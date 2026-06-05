"""
Volante DIY – Leitor de telemetria ETS2 → ESP32-S3
====================================================
Lê a shared memory do plugin SDK do ETS2 e envia os grupos
1 (painel), 2 (LEDs) e 3 (navegação) para a S3 via USB serial,
usando o mesmo framing do firmware (SOF + ID + LEN + PAYLOAD + SEQ + CRC).

Modos:
  python telemetry_reader.py --port COM3          → lê ETS2 real
  python telemetry_reader.py --port COM3 --sim    → simulador (sem ETS2)

Dependências:
  pip install pyserial
  Plugin ETS2 Telemetry SDK instalado no jogo (veja README)
"""

import argparse
import struct
import time
import math
import sys
import serial

# Windows: força UTF-8 no stdout (o console cp1252 crasha ao imprimir → ou °)
try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

# ─── IDs de mensagem (iguais ao firmware) ─────────────────────────────────────
MSG_TELEM_PAINEL = 0x100
MSG_TELEM_LEDS   = 0x101
MSG_TELEM_NAV    = 0x102

SOF = 0xAA

# ─── Framing ──────────────────────────────────────────────────────────────────
_seq = 0

def crc8(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b
    return crc

def build_frame(msg_id: int, payload: bytes) -> bytes:
    global _seq
    assert len(payload) <= 8
    id_lo = msg_id & 0xFF
    id_hi = (msg_id >> 8) & 0xFF
    hdr   = bytes([id_lo, id_hi, len(payload)])
    seq   = _seq & 0xFF
    _seq  = (_seq + 1) & 0xFF
    body  = hdr + payload + bytes([seq])
    return bytes([SOF]) + body + bytes([crc8(body)])

# ─── Serialização dos grupos ───────────────────────────────────────────────────
def pack_painel(speed_kmh: float, rpm: int, gear: int,
                fuel_pct: int, temp_c: int, seq_override=None) -> bytes:
    speed_x10 = int(speed_kmh * 10) & 0xFFFF
    rpm_u16   = max(0, min(65535, int(rpm)))
    gear_i8   = max(-128, min(127, int(gear)))
    fuel_u8   = max(0, min(100, int(fuel_pct)))
    temp_u8   = max(0, min(255, int(temp_c)))
    return struct.pack('<HHbBB', speed_x10, rpm_u16, gear_i8, fuel_u8, temp_u8)

def pack_leds(flags: int) -> bytes:
    return struct.pack('<H', flags & 0xFFFF)

def pack_nav(dist_m: int, eta_min: int, odometer_km: int) -> bytes:
    d = max(0, min(0xFFFFFF, int(dist_m)))
    e = max(0, min(65535, int(eta_min)))
    o = max(0, min(65535, int(odometer_km)))
    return struct.pack('<I', d)[:3] + struct.pack('<HH', e, o)

# ─── Flags de LEDs ────────────────────────────────────────────────────────────
LED_SETA_ESQ    = 1 << 0
LED_SETA_DIR    = 1 << 1
LED_PISCA_ALERT = 1 << 2
LED_FAROL_BAIXO = 1 << 3
LED_FAROL_ALTO  = 1 << 4
LED_LUZ_FREIO   = 1 << 5
LED_FREIO_MAO   = 1 << 6
LED_FREIO_MOTOR = 1 << 7
LED_CRUISE_ON   = 1 << 8
LED_BUZINA      = 1 << 9
LED_LIMPADOR    = 1 << 10
LED_LOW_FUEL    = 1 << 11

# ─── Leitura da shared memory do ETS2 ─────────────────────────────────────────
# Plugin: RenCloud scs-sdk-plugin v1.12.1 (PLUGIN_REVID 12).
# Shared memory: "Local\SCSTelemetry", 32 KiB, struct scsTelemetryMap_t.
# Offsets calculados a partir de scs-telemetry-common.hpp (zonas de offset fixo).
SHM_NAME = "Local\\SCSTelemetry"
SHM_SIZE = 32 * 1024
EXPECTED_REV = 12

# Offsets (bytes) dentro do scsTelemetryMap_t (little-endian, x86/x64)
OFF_SDK_ACTIVE   = 0      # bool  – plugin/jogo ativo
OFF_PLUGIN_REV   = 40     # uint  – telemetry_plugin_revision
OFF_GAME         = 52     # uint  – 0=unknown, 1=ETS2, 2=ATS
OFF_GEAR         = 504    # int   – truck_i.gear (assinado: <0 ré, 0 neutro)
OFF_FUEL_CAP     = 704    # float – config_f.fuelCapacity (litros)
OFF_SPEED        = 948    # float – truck_f.speed (m/s)
OFF_RPM          = 952    # float – engineRpm
OFF_FUEL         = 1000   # float – fuel (litros)
OFF_WATER_TEMP   = 1024   # float – waterTemperature (°C)
OFF_ODOMETER     = 1056   # float – truckOdometer (km)
OFF_ROUTE_DIST   = 1060   # float – routeDistance (m até o destino)
OFF_ROUTE_TIME   = 1064   # float – routeTime (s até o destino)
OFF_PARK_BRAKE   = 1566   # bool
OFF_MOTOR_BRAKE  = 1567   # bool  – freio motor
OFF_FUEL_WARNING = 1570   # bool  – combustível baixo
OFF_WIPERS       = 1577   # bool  – limpador
OFF_BLINK_L      = 1578   # bool  – blinkerLeftActive
OFF_BLINK_R      = 1579   # bool  – blinkerRightActive
OFF_BEAM_LOW     = 1583   # bool  – farol baixo
OFF_BEAM_HIGH    = 1584   # bool  – farol alto
OFF_BRAKE_LIGHT  = 1586   # bool  – luz de freio
OFF_HAZARD       = 1588   # bool  – pisca-alerta
OFF_CRUISE       = 1589   # bool  – cruise control

_plugin_logged = False

def _read_raw():
    """Abre a shared memory e devolve os bytes (ou None se indisponível)."""
    try:
        import mmap
        shm = mmap.mmap(-1, SHM_SIZE, tagname=SHM_NAME, access=mmap.ACCESS_READ)
        raw = shm.read(SHM_SIZE)
        shm.close()
        return raw
    except Exception:
        return None

def read_ets2_shm():
    """
    Lê a shared memory do plugin SCS/ETS2 (RenCloud scs-sdk-plugin).
    Retorna um dict com os campos ou None se o jogo/plugin não estiver ativo.
    """
    global _plugin_logged
    raw = _read_raw()
    if raw is None:
        return None

    def f(o): return struct.unpack_from('<f', raw, o)[0]
    def i(o): return struct.unpack_from('<i', raw, o)[0]
    def u(o): return struct.unpack_from('<I', raw, o)[0]
    def b(o): return raw[o] != 0

    if not b(OFF_SDK_ACTIVE):
        return None  # plugin presente mas SDK inativo (jogo no menu / fechando)

    if not _plugin_logged:
        rev, game = u(OFF_PLUGIN_REV), u(OFF_GAME)
        gname = {1: 'ETS2', 2: 'ATS'}.get(game, f'desconhecido({game})')
        warn = '' if rev == EXPECTED_REV else f'  [ATENCAO: esperava revid {EXPECTED_REV}]'
        print(f"\n[volante] plugin revid={rev} | jogo={gname}{warn}")
        _plugin_logged = True

    fuel_l, fuel_cap = f(OFF_FUEL), f(OFF_FUEL_CAP)
    fuel_pct = int(fuel_l / fuel_cap * 100) if fuel_cap > 0 else 0

    flags = (
        (LED_SETA_ESQ    if b(OFF_BLINK_L)      else 0) |
        (LED_SETA_DIR    if b(OFF_BLINK_R)      else 0) |
        (LED_PISCA_ALERT if b(OFF_HAZARD)       else 0) |
        (LED_FAROL_BAIXO if b(OFF_BEAM_LOW)     else 0) |
        (LED_FAROL_ALTO  if b(OFF_BEAM_HIGH)    else 0) |
        (LED_LUZ_FREIO   if b(OFF_BRAKE_LIGHT)  else 0) |
        (LED_FREIO_MAO   if b(OFF_PARK_BRAKE)   else 0) |
        (LED_FREIO_MOTOR if b(OFF_MOTOR_BRAKE)  else 0) |
        (LED_CRUISE_ON   if b(OFF_CRUISE)       else 0) |
        (LED_LIMPADOR    if b(OFF_WIPERS)       else 0) |
        (LED_LOW_FUEL    if b(OFF_FUEL_WARNING) else 0)
    )

    return {
        'speed_kmh': abs(f(OFF_SPEED)) * 3.6,   # m/s -> km/h (abs: ré mostra positivo)
        'rpm'      : max(0, int(f(OFF_RPM))),
        'gear'     : i(OFF_GEAR),
        'fuel_pct' : fuel_pct,
        'temp_c'   : int(f(OFF_WATER_TEMP)),
        'flags'    : flags,
        'dist_m'   : max(0, int(f(OFF_ROUTE_DIST))),
        'eta_min'  : max(0, int(f(OFF_ROUTE_TIME) / 60)),
        'odometer' : max(0, int(f(OFF_ODOMETER))),
    }

# ─── Dump cru p/ calibrar offsets (--debug) ──────────────────────────────────
def debug_dump():
    raw = _read_raw()
    if raw is None:
        print("[debug] shared memory indisponivel (ETS2 + plugin rodando?)")
        return
    f = lambda o: struct.unpack_from('<f', raw, o)[0]
    i = lambda o: struct.unpack_from('<i', raw, o)[0]
    u = lambda o: struct.unpack_from('<I', raw, o)[0]
    b = lambda o: raw[o] != 0
    if not b(OFF_SDK_ACTIVE):
        print("[debug] sdkActive=0 (jogo no menu / nao esta dirigindo)")
        return
    fuel, cap = f(OFF_FUEL), f(OFF_FUEL_CAP)
    pct = int(fuel / cap * 100) if cap > 0 else 0
    print("\n=== ETS2 raw  revid=%d game=%d paused=%d ===" % (u(OFF_PLUGIN_REV), u(OFF_GAME), raw[4]))
    print(" speed=%.1f m/s (%.1f km/h)  rpm=%.0f  gear=%d (dash@508=%d)"
          % (f(OFF_SPEED), abs(f(OFF_SPEED)) * 3.6, f(OFF_RPM), i(OFF_GEAR), i(508)))
    print(" fuel=%.0f/%.0f L (%d%%)  water=%.0f C" % (fuel, cap, pct, f(OFF_WATER_TEMP)))
    print(" rota: dist=%.0f m  time=%.0f s (%d min)  odo=%.0f km"
          % (f(OFF_ROUTE_DIST), f(OFF_ROUTE_TIME), f(OFF_ROUTE_TIME) / 60, f(OFF_ODOMETER)))
    print(" luzes: park=%d motorBrake=%d fuelWarn=%d  blinkL(act=%d,on=%d) blinkR(act=%d,on=%d)"
          % (b(OFF_PARK_BRAKE), b(OFF_MOTOR_BRAKE), b(OFF_FUEL_WARNING),
             b(OFF_BLINK_L), raw[1580], b(OFF_BLINK_R), raw[1581]))
    print("        beamLo=%d beamHi=%d brake=%d hazard=%d cruise=%d wipers=%d"
          % (b(OFF_BEAM_LOW), b(OFF_BEAM_HIGH), b(OFF_BRAKE_LIGHT),
             b(OFF_HAZARD), b(OFF_CRUISE), b(OFF_WIPERS)))

# ─── Modo simulador ────────────────────────────────────────────────────────────
class Simulator:
    def __init__(self):
        self.t = 0.0

    def read(self):
        self.t += 0.05
        t = self.t
        speed = max(0, 80 + 30 * math.sin(t * 0.3))
        rpm   = int(800 + 1200 * (speed / 110) + 200 * math.sin(t * 2))
        gear  = max(1, min(12, int(speed / 10)))
        fuel  = max(5, int(100 - (t * 0.1) % 95))
        temp  = int(85 + 10 * math.sin(t * 0.05))
        # Demo de indicadores: percorre TODOS os estados p/ validar os 2 reles
        # (esq=GPIO2, dir=GPIO8) e os telltales da tela. Ciclo de 24s, fases de 3s
        # (cada fase de seta da ~3 piscadas/cliques). Farol baixo sempre como base.
        phase = int(t / 3) % 8
        flags = LED_FAROL_BAIXO
        if   phase == 0: flags |= LED_SETA_ESQ      # rele ESQUERDO (GPIO2)
        elif phase == 2: flags |= LED_SETA_DIR      # rele DIREITO (GPIO8)
        elif phase == 4: flags |= LED_PISCA_ALERT   # ALERTA: os dois reles juntos
        elif phase == 6:                            # showcase dos telltales da tela
            flags |= (LED_FAROL_ALTO | LED_LUZ_FREIO | LED_FREIO_MAO |
                      LED_FREIO_MOTOR | LED_CRUISE_ON | LED_BUZINA | LED_LIMPADOR)
        # fases impares (1,3,5,7) = pausa, tudo apagado (so o farol baixo de base)
        if fuel < 15:
            flags |= LED_LOW_FUEL
        dist = max(0, int(150000 - t * 20))
        eta  = max(0, int(dist / (speed / 3.6 + 1) / 60)) if speed > 0 else 999
        return {
            'speed_kmh': speed,
            'rpm'      : rpm,
            'gear'     : gear,
            'fuel_pct' : fuel,
            'temp_c'   : temp,
            'flags'    : flags,
            'dist_m'   : dist,
            'eta_min'  : eta,
            'odometer' : int(t * 0.02),
        }

# ─── Loop principal ───────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description='Volante DIY – telemetria ETS2 → S3')
    parser.add_argument('--port', default='COM3', help='Porta serial da S3 (ex: COM3)')
    parser.add_argument('--baud', type=int, default=115200)
    parser.add_argument('--sim',  action='store_true', help='Modo simulador (sem ETS2)')
    parser.add_argument('--debug', action='store_true', help='Modo real: imprime campos crus p/ calibrar offsets')
    parser.add_argument('--hz',   type=float, default=20.0, help='Taxa de envio em Hz (padrão: 20)')
    args = parser.parse_args()

    interval = 1.0 / args.hz
    sim = Simulator() if args.sim else None

    def open_serial(first=False):
        while True:
            try:
                s = serial.Serial(args.port, args.baud, timeout=1)
                print(f"\n[volante] conectado em {args.port} @ {args.baud} baud")
                return s
            except Exception as e:
                if first:
                    print(f"[volante] aguardando {args.port} ({e})... Ctrl+C p/ sair")
                    first = False
                time.sleep(1.0)

    print(f"[volante] conectando em {args.port} @ {args.baud} baud...")
    ser = open_serial(first=True)
    print(f"[volante] {'SIMULADOR' if args.sim else 'ETS2 REAL'} | {args.hz} Hz -> {args.port}")
    print("[volante] Ctrl+C para sair\n")

    last_nav_t = 0.0
    last_dbg = 0.0
    no_game_warned = False

    while True:
        try:
            t0 = time.monotonic()

            if sim:
                d = sim.read()
            else:
                d = read_ets2_shm()
                if d is None:
                    if not no_game_warned:
                        print("[volante] aguardando ETS2 / plugin de telemetria...")
                        no_game_warned = True
                    time.sleep(0.5)
                    continue
                no_game_warned = False
                if args.debug and time.monotonic() - last_dbg > 0.5:
                    debug_dump()
                    last_dbg = time.monotonic()

            # Envia painel (0x100) a cada ciclo
            p = pack_painel(d['speed_kmh'], d['rpm'], d['gear'],
                            d['fuel_pct'], d['temp_c'])
            ser.write(build_frame(MSG_TELEM_PAINEL, p))

            # Envia LEDs (0x101) a cada ciclo
            l = pack_leds(d['flags'])
            ser.write(build_frame(MSG_TELEM_LEDS, l))

            # Envia navegação (0x102) a 1 Hz
            now = time.monotonic()
            if now - last_nav_t >= 1.0:
                n = pack_nav(d['dist_m'], d['eta_min'], d['odometer'])
                ser.write(build_frame(MSG_TELEM_NAV, n))
                last_nav_t = now

            # Log resumido no terminal
            print(f"\r[{d['speed_kmh']:5.1f} km/h | {d['rpm']:5d} RPM | "
                  f"marcha {d['gear']:+d} | comb {d['fuel_pct']:3d}% | "
                  f"{d['temp_c']:3d} C | flags 0x{d['flags']:03X}]", end='', flush=True)

            # Dorme o resto do intervalo
            elapsed = time.monotonic() - t0
            sleep_t = interval - elapsed
            if sleep_t > 0:
                time.sleep(sleep_t)

        except serial.SerialException as e:
            # porta caiu (S3 resetou/USB piscou) → reconecta e continua
            print(f"\n[volante] porta caiu ({e}); reconectando...")
            try:
                ser.close()
            except Exception:
                pass
            ser = open_serial()
        except KeyboardInterrupt:
            print("\n[volante] encerrado.")
            break

    try:
        ser.close()
    except Exception:
        pass

if __name__ == '__main__':
    main()
