#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
flash.py - Grava o firmware desta placa de forma automatica.

USO SIMPLES (nao precisa de ESP-IDF, nem terminal especial):
    1. Ligue a placa no PC com um cabo USB de DADOS.
    2. Rode:   python flash.py
    O script instala o esptool sozinho (se faltar), descobre a porta COM
    automaticamente e grava os binarios ja prontos da pasta build/.

OPCOES:
    python flash.py --port COM5   # forca a porta (pula a deteccao automatica)
    python flash.py --monitor     # abre o monitor serial depois de gravar
    python flash.py --build       # recompila ANTES de gravar (requer ESP-IDF 5.4)

OBS: para apenas GRAVAR voce so precisa de Python + esptool (instalado aqui
automaticamente). O ESP-IDF completo so e necessario para RECOMPILAR (--build).
"""

import sys
import os
import json
import argparse
import importlib
import subprocess
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(HERE, "build")
ARGS_JSON = os.path.join(BUILD, "flasher_args.json")

# VIDs USB comuns das placas ESP32 e conversores serial (Espressif, CH34x, CP210x, FTDI)
KNOWN_VIDS = {0x303A, 0x1A86, 0x10C4, 0x0403}


def ensure(import_name, pip_name=None):
    """Importa um pacote; se faltar, instala via pip e importa de novo."""
    try:
        return importlib.import_module(import_name)
    except ImportError:
        pkg = pip_name or import_name
        print(f"[setup] instalando dependencia '{pkg}' (so na primeira vez)...")
        subprocess.check_call([sys.executable, "-m", "pip", "install", pkg])
        return importlib.import_module(import_name)


def detect_port(preferred=None):
    """Descobre a porta COM da placa. Pergunta se houver mais de uma."""
    if preferred:
        return preferred
    ensure("serial", "pyserial")
    from serial.tools import list_ports

    ports = list(list_ports.comports())
    if not ports:
        sys.exit("[erro] Nenhuma porta serial encontrada.\n"
                 "       Verifique: o cabo e de DADOS (nao so de carga)? O driver\n"
                 "       USB-serial (CH343/CH340/CP210x) esta instalado?")

    esp = [p for p in ports if p.vid in KNOWN_VIDS]
    cands = esp or ports

    if len(cands) == 1:
        p = cands[0]
        print(f"[porta] detectada automaticamente: {p.device}  ({p.description})")
        return p.device

    print("Foram encontradas varias portas. Deixe SO a sua placa ligada, ou escolha:")
    for i, p in enumerate(cands):
        print(f"  [{i}] {p.device}  {p.description}")
    try:
        sel = int(input("Numero da porta: ").strip())
        return cands[sel].device
    except (ValueError, IndexError):
        sys.exit("[erro] selecao invalida.")


def do_build():
    """Recompila o projeto (requer ESP-IDF 5.4 no PATH / terminal do IDF)."""
    idf = shutil.which("idf.py")
    if not idf:
        sys.exit("[erro] '--build' precisa do ESP-IDF 5.4 instalado.\n"
                 "       Abra o terminal 'ESP-IDF 5.4 PowerShell' e rode este script\n"
                 "       de dentro dele. Para apenas GRAVAR (sem recompilar), rode\n"
                 "       sem --build: os binarios ja prontos serao usados.")
    print("[build] compilando com idf.py ...")
    subprocess.check_call([idf, "build"], cwd=HERE)


def do_flash(port):
    """Grava os binarios listados em build/flasher_args.json via esptool."""
    ensure("esptool")
    with open(ARGS_JSON, encoding="utf-8") as f:
        cfg = json.load(f)

    extra = cfg["extra_esptool_args"]
    chip = extra["chip"]
    cmd = [sys.executable, "-m", "esptool",
           "--chip", chip, "-p", port, "-b", "460800",
           "--before", extra.get("before", "default_reset"),
           "--after", extra.get("after", "hard_reset"),
           "write_flash"] + cfg["write_flash_args"]

    # offset -> arquivo, em ordem crescente de offset
    for off, fn in sorted(cfg["flash_files"].items(), key=lambda kv: int(kv[0], 16)):
        cmd += [off, fn]

    print(f"[flash] gravando firmware ({chip}) em {port} ...")
    subprocess.check_call(cmd, cwd=BUILD)
    print("[ok] Firmware gravado com sucesso!")


def do_monitor(port, baud=115200):
    """Monitor serial simples (para ver os logs / a calibracao)."""
    ensure("serial", "pyserial")
    import serial
    print(f"[monitor] {port} @ {baud} bps  (Ctrl+C para sair)\n" + "-" * 50)
    try:
        with serial.Serial(port, baud, timeout=0.1) as s:
            while True:
                data = s.read(4096)
                if data:
                    sys.stdout.buffer.write(data)
                    sys.stdout.flush()
    except KeyboardInterrupt:
        print("\n[monitor] encerrado.")
    except Exception as e:
        print(f"\n[monitor] erro: {e}")


def main():
    ap = argparse.ArgumentParser(description="Grava o firmware desta placa ESP32.")
    ap.add_argument("--port", help="porta COM (ex.: COM5). Padrao: deteccao automatica.")
    ap.add_argument("--build", action="store_true", help="recompila antes (requer ESP-IDF).")
    ap.add_argument("--monitor", action="store_true", help="abre o monitor serial apos gravar.")
    args = ap.parse_args()

    if args.build:
        do_build()

    if not os.path.exists(ARGS_JSON):
        sys.exit(f"[erro] nao encontrei os binarios prontos em:\n       {ARGS_JSON}\n"
                 "       Rode com --build (precisa do ESP-IDF) ou obtenha a pasta build/ ja compilada.")

    port = detect_port(args.port)
    do_flash(port)

    if args.monitor:
        do_monitor(port)


if __name__ == "__main__":
    main()
