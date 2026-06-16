"""
Volante DIY - Monitor de bancada do AS5047P (teste por conta propria)
=====================================================================
Le o console da S3 (chip CH343) e mostra AO VIVO o estado do sensor, pra voce
mexer fio / soldar / girar o ima e ver na hora o que acontece. Veredito:

  FLUTUANDO (0xFFFF) .... linha aberta / sem contato no DO(MISO)
  EM BAIXO  (0x0000) .... curto p/ GND
  INSTAVEL ............... o valor MUDA toda hora = mau contato (BALANCE o fio
                          procurando o ponto que estabiliza)
  FIXO (artefato) ....... mesmo valor invalido repetindo -> NAO e angulo real
  VALIDO ang=XX.X ....... leitura boa! gira o ima -> MEXENDO

Acha a porta sozinho (CH343, VID 1A86). Por padrao NAO reseta a placa.
Use --reset p/ reiniciar a S3 e capturar a varredura/diagnostico do boot.

Uso:
  python sensor_monitor.py            # so monitora (sem reset)
  python sensor_monitor.py --reset    # reinicia e pega o scan do boot
  python sensor_monitor.py --raw      # mostra TODAS as linhas do console
  python sensor_monitor.py --port COM7  # forca a porta

Dep: pip install pyserial      (Ctrl+C p/ sair)
"""

import argparse
import re
import sys
import time
from collections import Counter, deque

import serial
import serial.tools.list_ports as list_ports

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace", line_buffering=True)
except Exception:
    pass

CPR = 16384
ANSI    = re.compile(r"\x1b\[[0-9;]*m")
RE_RESP = re.compile(r"(?:angle|word)=0x([0-9A-Fa-f]{4})")          # palavra-resposta
RE_ERRFL = re.compile(r"ERRFL=0x([0-9A-Fa-f]{4})\s*\[FRERR=(\d)\s+INVCOMM=(\d)\s+PARERR=(\d)\]")
# linhas informativas do firmware de scan/diagnostico (mostradas como vieram)
RE_INFO = re.compile(r"(pinscan|DIAG|SCAN|MELHOR|VIVO|FASE|modo \d)")

def par_even(w):
    return (bin(w).count("1") & 1) == 0

def find_ch343():
    for p in list_ports.comports():
        if p.vid == 0x1A86 or (p.hwid and "1A86" in p.hwid.upper()):
            return p.device
    return None

def classify(word):
    """Devolve (categoria, detalhe, angulo_ou_None) p/ uma palavra de 16 bits."""
    if word == 0xFFFF:
        return "FLUTUANDO", "linha aberta / sem contato", None
    if word == 0x0000:
        return "EM BAIXO", "curto p/ GND?", None
    errf = (word >> 14) & 1
    raw  = word & 0x3FFF
    ang  = raw * 360.0 / CPR
    if par_even(word) and not errf:
        return "VALIDO", f"ang={ang:6.2f}", ang
    return "FIXO", f"errf={errf} raw={raw} (artefato, nao e angulo)", None

def main():
    ap = argparse.ArgumentParser(description="Monitor de bancada do AS5047P (S3 via CH343)")
    ap.add_argument("--port", default=None, help="porta serial (padrao: acha a CH343 sozinho)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--reset", action="store_true", help="reinicia a S3 (pega o scan do boot)")
    ap.add_argument("--raw", action="store_true", help="mostra todas as linhas do console")
    args = ap.parse_args()

    def open_port(first=False):
        while True:
            port = args.port or find_ch343()
            if port:
                try:
                    s = serial.Serial()
                    s.port = port
                    s.baudrate = args.baud
                    s.timeout = 0.2
                    s.dtr = False
                    s.rts = False
                    s.open()
                    print(f"\n[volante] conectado em {port} @ {args.baud}"
                          f"{' (com reset)' if args.reset else ' (sem reset)'}\n")
                    if args.reset:
                        s.rts = True; time.sleep(0.12); s.rts = False
                    return s
                except Exception as e:
                    if first:
                        print(f"[volante] falha ao abrir {port} ({e})...")
            elif first:
                print("[volante] CH343 nao encontrada. Plugue a S3 (porta de flash/log)... Ctrl+C p/ sair")
            first = False
            time.sleep(1.0)

    print("[volante] monitor do AS5047P. Mexa fio / gire o ima e observe. Ctrl+C p/ sair.")
    ser = open_port(first=True)

    hist = deque(maxlen=200)      # (t, word) p/ estabilidade
    ang_hist = deque(maxlen=200)  # (t, ang) p/ deteccao de movimento
    tally = Counter()             # contagem total por palavra
    last_cat = None
    last_errfl = None

    def verdict(now):
        recent = [(t, w) for (t, w) in hist if now - t <= 3.0]
        words = [w for _, w in recent]
        if not words:
            return "—", "aguardando", 0
        distinct = set(words)
        flips = sum(1 for i in range(1, len(words)) if words[i] != words[i-1])
        last = words[-1]
        if flips >= 3 and len(distinct) >= 2:
            tops = ", ".join(f"0x{w:04X}x{words.count(w)}" for w in sorted(distinct))
            return "INSTAVEL", f"mau contato — BALANCE o fio  [{tops}]", flips
        cat, det, _ = classify(last)
        if cat == "VALIDO":
            recent_ang = [a for (t, a) in ang_hist if now - t <= 2.0]
            mov = "MEXENDO" if (recent_ang and max(recent_ang) - min(recent_ang) > 2.0) else "parado"
            return "VALIDO", f"{det}  [{mov}]", flips
        return cat, det, flips

    while True:
        try:
            line = ser.readline().decode("utf-8", errors="replace").strip()
            if not line:
                continue
            line = ANSI.sub("", line)

            if args.raw:
                print(line)

            # linhas de scan/diagnostico do boot: mostra como vieram
            if RE_INFO.search(line) and "angle" not in line:
                if not args.raw:
                    print(line)
                continue

            m = RE_RESP.search(line)
            if not m:
                continue
            word = int(m.group(1), 16)
            now = time.monotonic()
            hist.append((now, word))
            tally[word] += 1
            cat0, _, ang = classify(word)
            if ang is not None:
                ang_hist.append((now, ang))

            ef = RE_ERRFL.search(line)
            errfl_txt = ""
            if ef:
                fr, inv, par = ef.group(2), ef.group(3), ef.group(4)
                errfl_txt = f"  ERRFL[FR={fr} INV={inv} PAR={par}]"
                last_errfl = (fr, inv, par)

            cat, det, flips = verdict(now)
            if cat != last_cat:
                print(f"\n>> {cat}: {det}")
                last_cat = cat
            print(f"\r {cat:9s} word=0x{word:04X}{errfl_txt}  {det}   n={sum(tally.values())} ",
                  end="", flush=True)

        except serial.SerialException as e:
            print(f"\n[volante] porta caiu ({e}); reconectando...")
            try: ser.close()
            except Exception: pass
            ser = open_port()
        except KeyboardInterrupt:
            print("\n\n[volante] encerrado. Resumo das palavras vistas:")
            for w, c in tally.most_common(8):
                cat, det, _ = classify(w)
                print(f"   0x{w:04X}  x{c:<5d}  {cat} ({det})")
            if last_errfl:
                print(f"   ultimo ERRFL: FRERR={last_errfl[0]} INVCOMM={last_errfl[1]} PARERR={last_errfl[2]}")
            break

    try: ser.close()
    except Exception: pass

if __name__ == "__main__":
    main()
