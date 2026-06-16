"""
Volante DIY - Monitor do AS5047P (bring-up da direcao)
======================================================
Le o console da S3 (UART0 -> CH343 -> COM3) e mostra, AO VIVO, o que o
`angle_task` do firmware emite a 1 Hz:

  W (....) angle: sem leitura valida (sensor sem pinos soldados/desligado)
  I (....) angle: raw=12345  ang=271.34 deg

Separa os dois casos, calcula graus, desenha uma barra 0-360 e detecta se
o angulo esta MEXENDO ou PARADO ao girar o ima. So LE -- nao escreve nada
na S3 e NAO reseta a placa (DTR/RTS fixados antes do open).

Uso:
  python angle_monitor.py                 -> COM3 @ 115200
  python angle_monitor.py --port COM5
  python angle_monitor.py --raw           -> mostra TODAS as linhas do console

Dependencia: pip install pyserial      (Ctrl+C p/ sair)
"""

import argparse
import re
import sys
import time
from collections import deque

import serial

# Windows: console cp1252 quebra ao imprimir ° / barras -> forca UTF-8
try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

CPR = 16384  # AS5047_CPR (14 bits)

# ─── Regex das linhas do angle_task ──────────────────────────────────────────
ANSI    = re.compile(r"\x1b\[[0-9;]*m")                       # cores do ESP-IDF
# Novo formato (firmware com diagnostico):
#   angle: word=0xFFFF par=OK errf=1 raw=16383 ang=359.98 deg INVALIDO
RE_DIAG = re.compile(
    r"angle:\s*word=0x([0-9A-Fa-f]{4})\s+par=(\w+)\s+errf=(\d)\s+"
    r"raw=\s*(-?\d+)\s+ang=\s*(-?\d+\.\d+)\s+deg\s+(\w+)")
RE_NONE = re.compile(r"angle:.*sem leitura valida")          # firmware antigo

def cause_for(word, par_ok, errf):
    """Traduz a palavra crua na causa fisica mais provavel."""
    if word == 0xFFFF:
        return "MISO ocioso ALTO -> MISO(G5) solto/sem solda, CS(G7) nao seleciona, ou CLK(G4) nao chega"
    if word == 0x0000:
        return ("MISO em BAIXO (0x0000) -> MISO(G5) em curto c/ GND ou no pino GND errado, "
                "ou CLK(G4) nao chega ao sensor (chip nao desloca os bits)")
    if not par_ok:
        return "paridade BAD -> bits chegam mas corrompidos: CLK/MOSI ruido, fio longo, ou solda fria"
    if errf:
        return "error-flag setado -> comando/framing: confira modo SPI/CS, ou baixe AS5047_SPI_HZ"
    return "frame invalido"

# ─── Estado p/ detectar movimento ────────────────────────────────────────────
# guarda (timestamp, raw) dos ultimos ~4 s p/ medir o quanto girou
hist = deque(maxlen=64)

def wrapped_delta(a, b):
    """Menor distancia angular entre dois raws (lida com a volta em 0/16384)."""
    d = (b - a) % CPR
    if d > CPR // 2:
        d -= CPR
    return d

def moving_span(now):
    """Maior excursao (em counts) nos ultimos 3 s. >~40 = esta girando."""
    recent = [r for (t, r) in hist if now - t <= 3.0]
    if len(recent) < 2:
        return 0
    span = 0
    for i in range(1, len(recent)):
        span += abs(wrapped_delta(recent[i - 1], recent[i]))
    return span

def bar(deg, width=28):
    pos = int(deg / 360.0 * (width - 1))
    return "[" + "-" * pos + "|" + "-" * (width - 1 - pos) + "]"

# ─── Bloco de ajuda quando o sensor nao responde ─────────────────────────────
HINT = (
    "\n  >> SEM LEITURA: o AS5047P nao respondeu no SPI. Suspeitos, em ordem:\n"
    "     1) MISO (G5) sem continuidade  2) sensor sem 3V3 / JP1 fora do 3.3V\n"
    "     3) fileira de baixo do header  4) CLK/MOSI/CSn trocados (G4/G6/G7)\n"
)

def main():
    ap = argparse.ArgumentParser(description="Monitor do AS5047P (direcao) via console da S3")
    ap.add_argument("--port", default="COM3", help="porta serial da S3 (padrao COM3)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--raw", action="store_true", help="imprime TODAS as linhas do console")
    args = ap.parse_args()

    def open_serial(first=False):
        while True:
            try:
                # configura DTR/RTS ANTES de abrir p/ NAO pulsar o reset da S3
                s = serial.Serial()
                s.port = args.port
                s.baudrate = args.baud
                s.timeout = 1.0
                s.dtr = False
                s.rts = False
                s.open()
                print(f"[volante] conectado em {args.port} @ {args.baud} (somente leitura, sem reset)\n")
                return s
            except Exception as e:
                if first:
                    print(f"[volante] aguardando {args.port} ({e})... Ctrl+C p/ sair")
                    first = False
                time.sleep(1.0)

    print(f"[volante] monitor do AS5047P -> {args.port}. Gire o ima. Ctrl+C p/ sair.")
    ser = open_serial(first=True)

    valid = 0
    invalid = 0
    last_state = None        # 'ok' | 'none' -> imprime dica só na transicao p/ 'none'
    rmin = rmax = None

    while True:
        try:
            line = ser.readline().decode("utf-8", errors="replace").strip()
            if not line:
                continue
            line = ANSI.sub("", line)

            if args.raw:
                print(line)

            m = RE_DIAG.search(line)
            if m:
                word   = int(m.group(1), 16)
                par_ok = m.group(2).upper() == "OK"
                errf   = int(m.group(3))
                raw    = int(m.group(4))
                deg    = float(m.group(5))
                ok     = m.group(6).upper() == "VALIDO"
                # 0x0000 e 0xFFFF passam na paridade por acaso, mas sao linhas
                # presas (baixo/alto) -> nunca sao leitura real do sensor.
                if word in (0x0000, 0xFFFF):
                    ok = False

                if ok:
                    now = time.monotonic()
                    hist.append((now, raw))
                    valid += 1
                    rmin = raw if rmin is None else min(rmin, raw)
                    rmax = raw if rmax is None else max(rmax, raw)
                    span = moving_span(now)
                    mov = "MEXENDO" if span > 40 else "parado "
                    print(f"\r OK  word=0x{word:04X}  raw={raw:5d}/{CPR}  ang={deg:6.2f}°  "
                          f"{bar(deg)}  min={rmin:5d} max={rmax:5d}  {mov}   ",
                          end="", flush=True)
                    last_state = "ok"
                else:
                    invalid += 1
                    if last_state != ("bad:%04X" % word):
                        print(f"\n  ✗ word=0x{word:04X} par={'OK' if par_ok else 'BAD'} "
                              f"errf={errf}  ->  {cause_for(word, par_ok, errf)}")
                        last_state = "bad:%04X" % word
                    print(f"\r ✗  INVALIDO  word=0x{word:04X}   leituras invalidas={invalid}   ",
                          end="", flush=True)
                continue

            if RE_NONE.search(line):           # firmware antigo (sem dump de word)
                invalid += 1
                if last_state != "none":
                    print(HINT)
                print(f"\r ✗  SEM LEITURA  (firmware antigo)   invalidas={invalid}   ",
                      end="", flush=True)
                last_state = "none"
                continue

            # outras linhas do boot (init do SPI etc.) so aparecem com --raw

        except serial.SerialException as e:
            print(f"\n[volante] porta caiu ({e}); reconectando...")
            try:
                ser.close()
            except Exception:
                pass
            ser = open_serial()
        except KeyboardInterrupt:
            print(f"\n[volante] encerrado. validas={valid} invalidas={invalid}")
            break

    try:
        ser.close()
    except Exception:
        pass

if __name__ == "__main__":
    main()
