#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
package.py - Monta o pacote ENXUTO de distribuicao (dist/) do Volante DIY.

Copia para dist/ apenas o necessario para GRAVAR (sem o lixo da pasta build/):
  - de cada modulo: flasher_args.json + os .bin referenciados + flash.py
  - na raiz: installer.py, 'Instalar Volante.bat', README.md, LIGACOES.md

Resultado: dist/ pronto para zipar e enviar. O amigo descompacta, da duplo-clique
no .bat (ou roda installer.py) e grava nas placas dele. So precisa de Python.

USO:
    python package.py            # atualiza dist/
    python package.py --zip      # atualiza dist/ e gera Volante_dist.zip
    python package.py --quiet    # silencioso (usado no gancho de build)

Roda AUTOMATICAMENTE apos cada build (gancho POST_BUILD nos CMakeLists). Em modo
--quiet nunca falha o build: erros viram aviso e o script sai com codigo 0.
"""

import sys
import os
import json
import shutil
import argparse

ROOT = os.path.dirname(os.path.abspath(__file__))
DIST = os.path.join(ROOT, "dist")

# Arquivos da raiz que vao para dist/ (se existirem).
ROOT_FILES = ["installer.py", "Instalar Volante.bat", "README.md", "LIGACOES.md"]


def discover_modules():
    """Subpastas com firmware compilado (build/flasher_args.json)."""
    mods = []
    for name in sorted(os.listdir(ROOT)):
        if name == "dist":
            continue
        args_json = os.path.join(ROOT, name, "build", "flasher_args.json")
        if os.path.isfile(args_json):
            mods.append(name)
    return mods


def copy_module(folder, quiet):
    """Copia o minimo do modulo para dist/<folder>/."""
    src_build = os.path.join(ROOT, folder, "build")
    args_json = os.path.join(src_build, "flasher_args.json")
    with open(args_json, encoding="utf-8") as f:
        cfg = json.load(f)

    dst_build = os.path.join(DIST, folder, "build")
    os.makedirs(dst_build, exist_ok=True)

    # flasher_args.json
    shutil.copy2(args_json, os.path.join(dst_build, "flasher_args.json"))

    # cada .bin referenciado (preservando subpasta: bootloader/, partition_table/)
    for _off, rel in cfg["flash_files"].items():
        src = os.path.join(src_build, rel)
        dst = os.path.join(dst_build, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)

    # flash.py do modulo (autonomo), se existir
    fp = os.path.join(ROOT, folder, "flash.py")
    if os.path.isfile(fp):
        shutil.copy2(fp, os.path.join(DIST, folder, "flash.py"))

    if not quiet:
        chip = cfg["extra_esptool_args"]["chip"]
        n = len(cfg["flash_files"])
        print(f"  + {folder:<22} {chip:<9} ({n} binarios)")


def build_dist(quiet=False):
    mods = discover_modules()
    if not mods:
        raise RuntimeError("nenhum modulo compilado (build/flasher_args.json) encontrado.")

    # Recria dist/ do zero para nao deixar restos de builds antigos.
    if os.path.isdir(DIST):
        shutil.rmtree(DIST)
    os.makedirs(DIST)

    if not quiet:
        print("Montando dist/ ...")
    for m in mods:
        copy_module(m, quiet)

    for fn in ROOT_FILES:
        src = os.path.join(ROOT, fn)
        if os.path.isfile(src):
            shutil.copy2(src, os.path.join(DIST, fn))

    write_readme()
    return mods


def write_readme():
    """Gera um LEIA-ME.txt curto na raiz do dist/."""
    txt = (
        "VOLANTE DIY - COMO INSTALAR O FIRMWARE\n"
        "======================================\n"
        "\n"
        "1) Tenha o Python 3 instalado (https://www.python.org/downloads/,\n"
        "   marque 'Add Python to PATH').\n"
        "2) Ligue UMA placa no PC com um cabo USB de DADOS.\n"
        "3) De duplo-clique em 'Instalar Volante.bat', escolha a placa no menu\n"
        "   e aguarde a gravacao. Repita para cada placa.\n"
        "\n"
        "IMPORTANTE: para funcionar, voce precisa das MESMAS placas\n"
        "(1x ESP32-S3 + 2x ESP32-C3) e da fiacao igual a do projeto.\n"
        "Veja README.md (secao 3) e LIGACOES.md para a pinagem/soldagem.\n"
        "\n"
        "Sem botao/menu: abra um terminal na pasta e rode 'python installer.py'.\n"
    )
    with open(os.path.join(DIST, "LEIA-ME.txt"), "w", encoding="utf-8") as f:
        f.write(txt)


def make_zip(quiet=False):
    base = os.path.join(ROOT, "Volante_dist")
    if os.path.exists(base + ".zip"):
        os.remove(base + ".zip")
    shutil.make_archive(base, "zip", DIST)
    if not quiet:
        size = os.path.getsize(base + ".zip") / 1024
        print(f"zip gerado: {base}.zip ({size:.0f} KB)")


def main():
    ap = argparse.ArgumentParser(description="Monta o pacote enxuto dist/.")
    ap.add_argument("--zip", action="store_true", help="tambem gera Volante_dist.zip.")
    ap.add_argument("--quiet", action="store_true", help="silencioso; nunca falha (p/ gancho de build).")
    args = ap.parse_args()

    try:
        build_dist(args.quiet)
        if args.zip:
            make_zip(args.quiet)
        if not args.quiet:
            print("OK.")
    except Exception as e:
        # Em build automatico, nao derrubar o build por causa do empacotamento.
        print(f"[package.py] aviso: {e}", file=sys.stderr)
        if not args.quiet:
            sys.exit(1)


if __name__ == "__main__":
    main()
