#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
installer.py - Instalador do Volante DIY (menu no terminal).

Ferramenta unica para gravar o firmware em qualquer placa do projeto:
escolha o MODULO e a PORTA num menu simples. Nao precisa de ESP-IDF para
gravar (so do esptool, instalado automaticamente).

USO:
    python installer.py                 # abre o menu interativo
    python installer.py --list          # lista modulos e portas e sai
    python installer.py -m codigos -p COM3   # grava direto, sem menu (scriptavel)
    python installer.py -m codigos --monitor # grava e abre o monitor serial

ESCALABILIDADE: qualquer subpasta que tenha 'build/flasher_args.json' aparece
no menu automaticamente. Para um nome bonito, adicione em FRIENDLY_NAMES abaixo;
sem isso, usa o nome da pasta.
"""

import sys
import os
import json
import argparse
import importlib
import subprocess

ROOT = os.path.dirname(os.path.abspath(__file__))

# Nomes amigaveis (opcional). Chave = nome da pasta. Novos modulos sem entrada
# aqui aparecem com o proprio nome da pasta.
FRIENDLY_NAMES = {
    "codigos": "Nucleo (ESP32-S3)  - volante + pedais + host USB-HID",
    "codigos_c3_painel": "Painel (ESP32-C3)  - display GC9A01",
    "codigos_c3_botoeira": "Botoeira (ESP32-C3) - botoes + joystick",
}

# VIDs USB de placas ESP32 / conversores serial comuns.
KNOWN_VIDS = {0x303A, 0x1A86, 0x10C4, 0x0403}


# ─── utilidades ──────────────────────────────────────────────────────────────
def ensure(import_name, pip_name=None):
    """Importa um pacote; instala via pip se faltar."""
    try:
        return importlib.import_module(import_name)
    except ImportError:
        pkg = pip_name or import_name
        print(f"[setup] instalando dependencia '{pkg}' (so na primeira vez)...")
        subprocess.check_call([sys.executable, "-m", "pip", "install", pkg])
        return importlib.import_module(import_name)


def discover_modules():
    """Acha subpastas com firmware compilado (build/flasher_args.json)."""
    mods = []
    for name in sorted(os.listdir(ROOT)):
        path = os.path.join(ROOT, name)
        args_json = os.path.join(path, "build", "flasher_args.json")
        if os.path.isdir(path) and os.path.isfile(args_json):
            try:
                with open(args_json, encoding="utf-8") as f:
                    chip = json.load(f)["extra_esptool_args"]["chip"]
            except Exception:
                chip = "?"
            mods.append({
                "folder": name,
                "label": FRIENDLY_NAMES.get(name, name),
                "chip": chip,
                "args_json": args_json,
                "build_dir": os.path.join(path, "build"),
            })
    return mods


def list_ports():
    """Lista portas serie; coloca as 'cara de ESP' primeiro."""
    ensure("serial", "pyserial")
    from serial.tools import list_ports as lp
    ports = list(lp.comports())
    ports.sort(key=lambda p: (p.vid not in KNOWN_VIDS, p.device))
    return ports


# ─── acoes ───────────────────────────────────────────────────────────────────
def flash_module(mod, port):
    """Grava os binarios do modulo na porta dada via esptool."""
    ensure("esptool")
    with open(mod["args_json"], encoding="utf-8") as f:
        cfg = json.load(f)
    extra = cfg["extra_esptool_args"]
    cmd = [sys.executable, "-m", "esptool",
           "--chip", extra["chip"], "-p", port, "-b", "460800",
           "--before", extra.get("before", "default_reset"),
           "--after", extra.get("after", "hard_reset"),
           "write_flash"] + cfg["write_flash_args"]
    for off, fn in sorted(cfg["flash_files"].items(), key=lambda kv: int(kv[0], 16)):
        cmd += [off, fn]
    print(f"\n[flash] {mod['label']}  ->  {port}  ({extra['chip']})\n")
    subprocess.check_call(cmd, cwd=mod["build_dir"])
    print(f"\n[ok] '{mod['folder']}' gravado com sucesso em {port}!")


def monitor(port, baud=115200):
    ensure("serial", "pyserial")
    import serial
    print(f"\n[monitor] {port} @ {baud} bps  (Ctrl+C para sair)\n" + "-" * 50)
    try:
        with serial.Serial(port, baud, timeout=0.1) as s:
            while True:
                data = s.read(4096)
                if data:
                    sys.stdout.buffer.write(data)
                    sys.stdout.flush()
    except KeyboardInterrupt:
        print("\n[monitor] encerrado.")


# ─── menu interativo ─────────────────────────────────────────────────────────
def ask(prompt):
    try:
        return input(prompt).strip()
    except (EOFError, KeyboardInterrupt):
        print()
        sys.exit(0)


def choose_module(mods):
    print("\n=== Modulos disponiveis ===")
    for i, m in enumerate(mods, 1):
        print(f"  [{i}] {m['label']}   ({m['folder']}/)")
    print("  [0] Sair")
    while True:
        sel = ask("Escolha o modulo: ")
        if sel == "0":
            sys.exit(0)
        if sel.isdigit() and 1 <= int(sel) <= len(mods):
            return mods[int(sel) - 1]
        print("  opcao invalida.")


def choose_port(ports):
    if not ports:
        sys.exit("[erro] Nenhuma porta serial encontrada. Ligue a placa com um cabo de DADOS\n"
                 "       e confira o driver USB-serial (CH343/CH340/CP210x).")
    if len(ports) == 1:
        p = ports[0]
        print(f"[porta] usando a unica detectada: {p.device}  ({p.description})")
        return p.device
    print("\n=== Portas detectadas (ESP primeiro) ===")
    for i, p in enumerate(ports, 1):
        flag = "  <- ESP?" if p.vid in KNOWN_VIDS else ""
        print(f"  [{i}] {p.device}  {p.description}{flag}")
    while True:
        sel = ask("Escolha a porta: ")
        if sel.isdigit() and 1 <= int(sel) <= len(ports):
            return ports[int(sel) - 1].device
        print("  opcao invalida.")


def print_list(mods, ports):
    print("=== Modulos (firmware compilado encontrado) ===")
    for m in mods:
        print(f"  - {m['folder']:<22} {m['chip']:<9} {m['label']}")
    print("\n=== Portas serie ===")
    for p in ports:
        flag = "  <- ESP?" if p.vid in KNOWN_VIDS else ""
        print(f"  - {p.device:<8} {p.description}{flag}")


# ─── main ────────────────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(description="Instalador do Volante DIY.")
    ap.add_argument("-m", "--module", help="nome da pasta do modulo (pula o menu).")
    ap.add_argument("-p", "--port", help="porta COM (pula a deteccao/menu).")
    ap.add_argument("--monitor", action="store_true", help="abre o monitor apos gravar.")
    ap.add_argument("--list", action="store_true", help="lista modulos e portas e sai.")
    args = ap.parse_args()

    mods = discover_modules()
    if not mods:
        sys.exit("[erro] Nenhum modulo com firmware compilado (build/flasher_args.json).\n"
                 "       Compile primeiro (ESP-IDF: 'idf.py build' em cada pasta).")

    if args.list:
        print_list(mods, list_ports())
        return

    print("============================================")
    print("   Volante DIY - Instalador de firmware")
    print("============================================")

    # 1) modo direto (scriptavel)
    if args.module:
        mod = next((m for m in mods if m["folder"] == args.module), None)
        if not mod:
            sys.exit(f"[erro] modulo '{args.module}' nao encontrado. Use --list para ver os nomes.")
        port = args.port or choose_port(list_ports())
        flash_module(mod, port)
        if args.monitor:
            monitor(port)
        return

    # 2) modo menu (loop: permite gravar varias placas em sequencia)
    while True:
        mod = choose_module(mods)
        port = args.port or choose_port(list_ports())
        try:
            flash_module(mod, port)
        except subprocess.CalledProcessError:
            print("\n[erro] a gravacao falhou. Confira a porta e se a placa certa esta ligada.")
        if ask("\nGravar outra placa? (s/N): ").lower() != "s":
            print("Concluido.")
            break


if __name__ == "__main__":
    main()
