# -*- coding: utf-8 -*-
"""
Volante DIY - LAYOUT DE PLACA DEFINITIVO (prototipo real, footprints reais).

Projeto de placa de PROTOTIPO (perfboard), nao diagrama ilustrativo:
 - modulos ESP desenhados com a PINAGEM FISICA REAL (fileira SUP + INF), todos
   os pinos nas posicoes reais (fonte: LIGACOES.md secoes 3 e 4);
 - componentes com ENCAPSULAMENTO real: GC9A01 (header 7 pinos + tela redonda),
   AS5047P-TS_EK_AB (header 8x2, usa fileira de cima), modulo 2 reles
   (IN1/IN2/VCC/GND/JD-VCC + 2 reles), KY-023 (5 pinos + manche),
   botoes tateis (4 pernas), LED + resistor;
 - fios roteados pino-fisico -> pino-fisico (Manhattan; cruzamentos = jumpers,
   normais num prototipo);
 - 3 placas na ordem do cockpit: PAINEL (esq) | NUCLEO (centro) | BOTOEIRA (dir).

Numeracao 1-40 = LIGACOES.md (pedais = 39-40).
Uso: python layout_placas_final.py -> layout_placas_final.png + .pdf
"""

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Circle, Rectangle
from matplotlib.lines import Line2D

C = {
    "usb": "#7f7f7f", "pwr": "#d62728", "gnd": "#2b2b2b", "v3v3": "#ff7f0e",
    "spi": "#1f77b4", "uart": "#2ca02c", "disp": "#9467bd", "relay": "#8c564b",
    "led": "#e377c2", "btn": "#17becf", "analog": "#bcbd22", "mech": "#aa8800",
    "board": "#13477e", "s3": "#0f3a6b",
}

# pinagens fisicas reais (LIGACOES.md)
S3_TOP = ["GND","5V","14","13","12","11","10","9","46","3","8","18","17","16","15","7","6","5","4","RST","3V3","3V3"]
S3_BOT = ["GND","GND","19","20","21","47","48","45","0","35","36","37","38","39","40","41","42","2","1","44","43","GND"]
C3_TOP = ["0","1","2","21","20","3V3","GND","5V"]   # 21=TX 20=RX
C3_BOT = ["3","4","5","6","7","8","9","10"]


# ----------------------------------------------------------------- helpers ---
def board(ax, ncols, nrows, title, sub, xl=2.0, xr=2.0):
    xs = [c for c in range(ncols) for _ in range(nrows)]
    ys = [r for _ in range(ncols) for r in range(nrows)]
    ax.scatter(xs, ys, s=2.2, c="#d3d8e0", zorder=0)
    ax.add_patch(Rectangle((-0.5, -0.5), ncols, nrows, fill=False, ec="#9aa3b0", lw=1.2, zorder=0))
    ax.text((ncols-1)/2, nrows+2.0, title, ha="center", va="center", fontsize=12.5, fontweight="bold", color="#10243b")
    ax.text((ncols-1)/2, nrows+1.0, sub, ha="center", va="center", fontsize=8, color="#666")
    ax.set_xlim(-xl, ncols-1+xr); ax.set_ylim(-2.4, nrows+2.6)
    ax.set_aspect("equal"); ax.axis("off")


def used_pad(ax, c, r, num, col):
    ax.add_patch(Circle((c, r), 0.42, fc=col, ec="white", lw=1.0, zorder=8))
    ax.text(c, r, str(num), ha="center", va="center", color="white", fontsize=6.0, fontweight="bold", zorder=9)


def free_pad(ax, c, r):
    ax.add_patch(Circle((c, r), 0.24, fc="white", ec="#9aa3b0", lw=0.8, zorder=6))


def module(ax, x0, rbot, rtop, top, bot, title, ut, ub, col):
    n = len(top)
    ax.add_patch(FancyBboxPatch((x0-0.7, rbot-0.7), (n-1)+1.4, (rtop-rbot)+1.4,
                 boxstyle="round,pad=0.02,rounding_size=0.25", lw=1.8, ec=col, fc=col+"14", zorder=3))
    ax.text(x0+(n-1)/2, (rbot+rtop)/2, title, ha="center", va="center",
            fontsize=11, fontweight="bold", color=col, zorder=6)
    for i, nm in enumerate(top):
        c = x0+i
        (used_pad(ax, c, rtop, ut[i][0], ut[i][1]) if i in ut else free_pad(ax, c, rtop))
        ax.text(c, rtop+0.62, nm, rotation=90, ha="center", va="bottom", fontsize=4.7,
                color=("#10243b" if i in ut else "#888"), zorder=6)
    for i, nm in enumerate(bot):
        c = x0+i
        (used_pad(ax, c, rbot, ub[i][0], ub[i][1]) if i in ub else free_pad(ax, c, rbot))
        ax.text(c, rbot-0.62, nm, rotation=90, ha="center", va="top", fontsize=4.7,
                color=("#10243b" if i in ub else "#888"), zorder=6)
    return (lambda i: (x0+i, rtop)), (lambda i: (x0+i, rbot))


def header(ax, x0, y, names, title, col, down=True):
    n = len(names)
    ax.add_patch(FancyBboxPatch((x0-0.6, y-0.55), (n-1)+1.2, 1.7,
                 boxstyle="round,pad=0.02,rounding_size=0.2", lw=1.4, ec=col, fc=col+"14", zorder=3))
    ax.text(x0+(n-1)/2, y+1.35, title, ha="center", va="bottom", fontsize=8, fontweight="bold", color="#10243b", zorder=6)
    co = {}
    for i, nm in enumerate(names):
        c = x0+i
        ax.add_patch(Circle((c, y), 0.34, fc=col, ec="white", lw=0.9, zorder=8))
        yl = y-0.55 if down else y+0.55
        va = "top" if down else "bottom"
        ax.text(c, yl, nm, rotation=90, ha="center", va=va, fontsize=5.0, color="#333", zorder=6)
        co[nm] = (c, y)
    return co


def tactile(ax, c, r, label, col):
    ax.add_patch(FancyBboxPatch((c-0.45, r-0.45), 2.9, 2.9, boxstyle="round,pad=0.02,rounding_size=0.4",
                 lw=1.2, ec=col, fc=col+"14", zorder=3))
    for lc, lr in [(c, r), (c+2, r), (c, r+2), (c+2, r+2)]:
        ax.add_patch(Circle((lc, lr), 0.24, fc="white", ec="#888", lw=0.8, zorder=7))
    ax.text(c+1, r+1, label, ha="center", va="center", fontsize=6.0, fontweight="bold", color="#10243b", zorder=6)
    return (c, r), (c+2, r+2)   # (perna GPIO, perna GND)


def led(ax, c, r, label, col):
    ax.add_patch(Rectangle((c+0.55, r-0.22), 1.1, 0.44, fc="#e0cf8c", ec="#8a7a55", lw=0.8, zorder=5))
    ax.add_patch(Circle((c+2.3, r), 0.42, fc=col+"66", ec=col, lw=1.2, zorder=5))
    ax.add_patch(Circle((c, r), 0.24, fc="white", ec="#888", lw=0.8, zorder=7))
    ax.add_patch(Circle((c+3.1, r), 0.24, fc="white", ec="#888", lw=0.8, zorder=7))
    ax.text(c+1.55, r+0.62, label, ha="center", va="bottom", fontsize=6.0, color="#10243b", zorder=6)
    return (c, r), (c+3.1, r)   # (anodo<-GPIO via R, catodo->GND)


def wire(ax, p0, p1, col, via_r=None, via_c=None, lw=1.7):
    x0, y0 = p0; x1, y1 = p1
    if via_r is not None:
        pts = [(x0, y0), (x0, via_r), (x1, via_r), (x1, y1)]
    elif via_c is not None:
        pts = [(x0, y0), (via_c, y0), (via_c, y1), (x1, y1)]
    else:
        pts = [(x0, y0), (x1, y0), (x1, y1)]
    ax.plot([p[0] for p in pts], [p[1] for p in pts], color=col, lw=lw,
            solid_capstyle="round", solid_joinstyle="round", zorder=2)


def gpib(ax, c, r, text, col):  # rotulo de barramento que sai da placa
    ax.annotate("", xy=(c, r), xytext=(c+(1.6 if "->" in text else -1.6), r),
                arrowprops=dict(arrowstyle="->", color=col, lw=1.7), zorder=4)
    ha = "left" if "->" in text else "right"
    dx = 1.8 if "->" in text else -1.8
    ax.text(c+dx, r, text, ha=ha, va="center", fontsize=7.0, color=col, fontweight="bold", zorder=5)


# =================================================================== figura ==
fig = plt.figure(figsize=(30, 13), dpi=150)
gs = fig.add_gridspec(1, 3, width_ratios=[26, 34, 28], wspace=0.04,
                      left=0.008, right=0.992, top=0.90, bottom=0.05)
axP, axN, axB = (fig.add_subplot(gs[0, i]) for i in range(3))
fig.suptitle("Volante DIY - Layout de placa DEFINITIVO (prototipo real · footprints e pinagem fisica · ordem do cockpit · 1-40 = LIGACOES.md)",
             fontsize=14.5, fontweight="bold", color="#10243b", y=0.965)


# --------------------------------------------------------------- PAINEL C3 ---
def placa_painel(ax):
    board(ax, 27, 26, "PLACA 1 - PAINEL C3 (esq.)", "ESP32-C3 0.42 + GC9A01 + 2 reles + 2 LEDs", xr=3.5)
    ut = {0:(13,C["uart"]),1:(12,C["uart"]),2:(22,C["relay"]),3:(26,C["led"]),
          4:(25,C["led"]),5:(20,C["v3v3"]),6:(5,C["gnd"]),7:(2,C["pwr"])}
    ub = {0:(16,C["disp"]),1:(17,C["disp"]),4:(18,C["disp"]),5:(23,C["relay"]),7:(19,C["disp"])}
    T, B = module(ax, 3, 7, 12, C3_TOP, C3_BOT, "ESP32-C3\nPainel", ut, ub, C["board"])
    ax.text(6.5, 13.3, "OLED 0.42 (G5/G6 I2C) · BOOT G9 = troca tela", ha="center",
            fontsize=5.8, style="italic", color="#555")

    # GC9A01 (header 7 pinos + tela redonda) -- canto sup. dir.
    gc = header(ax, 16, 19, ["RST","CS","DC","SDA","SCL","GND","VCC"], "GC9A01 (TFT redondo)", C["disp"])
    ax.add_patch(Circle((19, 23.0), 2.1, fc=C["disp"]+"22", ec=C["disp"], lw=1.3, zorder=3))
    wire(ax, B(0), gc["SDA"], C["disp"], via_r=15.5)   # G3 -> SDA
    wire(ax, B(1), gc["SCL"], C["disp"], via_r=15.0)   # G4 -> SCL
    wire(ax, B(4), gc["CS"],  C["disp"], via_r=16.0)   # G7 -> CS
    wire(ax, B(7), gc["DC"],  C["disp"], via_r=16.5)   # G10 -> DC
    wire(ax, T(5), gc["VCC"], C["v3v3"], via_r=17.5)   # 3V3 -> VCC
    wire(ax, T(5), gc["RST"], C["v3v3"], via_r=17.0)   # 3V3 -> RST
    wire(ax, T(6), gc["GND"], C["gnd"],  via_r=14.5)   # GND -> GND

    # Modulo 2 reles (inf. dir.)
    rl = header(ax, 16, 4, ["IN1","IN2","VCC","GND","JDVC"], "Modulo Rele 5V 2ch opto (setas)", C["relay"], down=False)
    ax.add_patch(Rectangle((21.2, 3.4), 1.5, 2.2, fc=C["relay"]+"33", ec=C["relay"], lw=1.0, zorder=3))
    ax.add_patch(Rectangle((23.0, 3.4), 1.5, 2.2, fc=C["relay"]+"33", ec=C["relay"], lw=1.0, zorder=3))
    ax.text(23.5, 2.6, "optoacoplador active-low (SRD-05VDC) · jumper VCC<->JDVC removido",
            ha="center", fontsize=5.0, style="italic", color="#555")
    wire(ax, T(2), rl["IN1"], C["relay"], via_c=13.0)  # G2 -> IN1
    wire(ax, B(5), rl["IN2"], C["relay"], via_c=12.0)  # G8 -> IN2
    wire(ax, T(5), rl["VCC"], C["v3v3"], via_c=11.5)
    wire(ax, T(6), rl["GND"], C["gnd"],  via_c=11.0)
    wire(ax, T(7), rl["JDVC"], C["pwr"], via_c=14.0)   # 5V -> JD-VCC

    # LEDs farol (topo)
    a1, k1 = led(ax, 11, 22, "LED baixo", C["led"])
    a2, k2 = led(ax, 11, 24, "LED alto", C["led"])
    wire(ax, T(4), a1, C["led"], via_r=20.5)   # G20 -> LED baixo
    wire(ax, T(3), a2, C["led"], via_r=21.0)   # G21 -> LED alto
    wire(ax, k1, T(6), C["gnd"], via_r=20.0)
    wire(ax, k2, k1, C["gnd"])

    # alimentacao + UART (off-board)
    ax.annotate("", xy=(3, 13.6), xytext=(1.0, 13.6),
                arrowprops=dict(arrowstyle="->", color=C["uart"], lw=1.7), zorder=4)
    ax.text(0.8, 13.6, "UART1 <-> S3 (W1): G0/G1", ha="right", va="center",
            fontsize=6.5, color=C["uart"], fontweight="bold")
    ax.annotate("", xy=(9, 5.0), xytext=(9, 3.0),
                arrowprops=dict(arrowstyle="->", color=C["pwr"], lw=1.7), zorder=4)
    ax.text(9, 2.6, "5V / GND (trilho)", ha="center", va="top",
            fontsize=6.5, color=C["pwr"], fontweight="bold")


# --------------------------------------------------------------- NUCLEO S3 ---
def placa_nucleo(ax):
    board(ax, 34, 30, "PLACA 2 - NUCLEO ESP32-S3 (centro)", "ESP32-S3-N16R8 · USB nativo -> PC (HID)", xl=2.5, xr=3.0)
    ut = {0:(5,C["gnd"]),6:(39,C["analog"]),7:(40,C["analog"]),
          11:(13,C["uart"]),12:(12,C["uart"]),13:(15,C["uart"]),14:(14,C["uart"]),
          15:(9,C["spi"]),16:(8,C["spi"]),17:(7,C["spi"]),18:(6,C["spi"]),20:(10,C["v3v3"])}
    ub = {2:("U",C["usb"]),3:("U",C["usb"]),8:("B",C["gnd"])}
    T, B = module(ax, 5, 6, 11, S3_TOP, S3_BOT, "ESP32-S3 (nucleo)", ut, ub, C["s3"])
    # USB-C nativo (conector na ponta esq)
    ax.add_patch(FancyBboxPatch((3.0, 7.4), 1.3, 2.2, boxstyle="round,pad=0.02", lw=1.3,
                 ec=C["usb"], fc=C["usb"]+"33", zorder=4))
    ax.text(3.6, 5.0, "USB-C\nnativo", ha="center", va="top", fontsize=5.6, color=C["usb"], fontweight="bold")
    gpib(ax, 3.0, 8.5, "<- PC (HID)", C["usb"])

    # AS5047P EK_AB (header 8x2; usa fileira de cima) -- sup. dir, perto do SPI
    spi = header(ax, 20, 20, ["CSn","MOSI","MISO","CLK","GND","3V3"], "AS5047P-TS EK_AB (SPI)", C["spi"])
    ax.add_patch(Circle((22.5, 24.4), 1.0, fc=C["mech"], ec="#7a5b00", lw=1.0, zorder=4))  # imã/chip
    ax.text(22.5, 26.0, "imã diametral no EIXO (verso) · so fileira de cima do 8x2",
            ha="center", fontsize=5.6, style="italic", color="#7a5b00")
    wire(ax, T(15), spi["CSn"],  C["spi"], via_r=16.0)   # G7 -> CSn
    wire(ax, T(16), spi["MOSI"], C["spi"], via_r=15.5)   # G6 -> MOSI
    wire(ax, T(17), spi["MISO"], C["spi"], via_r=15.0)   # G5 -> MISO
    wire(ax, T(18), spi["CLK"],  C["spi"], via_r=14.5)   # G4 -> CLK
    wire(ax, T(20), spi["3V3"],  C["v3v3"], via_r=17.0)  # 3V3 -> 3V3
    wire(ax, T(0),  spi["GND"],  C["gnd"], via_r=18.0)   # GND -> GND (longo)

    # Pedais (2 botoes tateis) -- esq, perto de G10/G9
    pa, pga = tactile(ax, 7, 14, "accel", C["analog"])
    pb, pgb = tactile(ax, 11, 14, "freio", C["analog"])
    wire(ax, T(6),  pa, C["analog"], via_r=13.2)   # G10 -> accel
    wire(ax, T(7),  pb, C["analog"], via_r=12.8)   # G9 -> freio
    wire(ax, pga, T(0), C["gnd"], via_r=17.5)       # accel -> GND
    wire(ax, pgb, pga, C["gnd"], via_r=16.6)

    # UART1 -> Painel (esq) ; UART2 -> Botoeira (dir)
    wire(ax, T(11), (0.0, 11.0), C["uart"], via_r=12.2)   # G18 -> esq
    wire(ax, T(12), (0.0, 11.6), C["uart"], via_r=12.6)   # G17 -> esq
    ax.text(-0.3, 11.3, "<- Painel\n(UART1 W1)", ha="right", va="center", fontsize=6.4, color=C["uart"], fontweight="bold")
    wire(ax, T(13), (33.0, 10.0), C["uart"], via_r=10.8)  # G16 -> dir
    wire(ax, T(14), (33.0, 9.4),  C["uart"], via_r=9.6)   # G15 -> dir
    ax.text(33.3, 9.7, "-> Botoeira\n(UART2 W2)", ha="left", va="center", fontsize=6.4, color=C["uart"], fontweight="bold")
    ax.text(5+8, 5.0, "BOOT G0 = calibracao · USB nativo D-/D+ = G19/G20", ha="center", va="top",
            fontsize=5.6, style="italic", color="#555")


# -------------------------------------------------------------- BOTOEIRA C3 --
def placa_botoeira(ax):
    board(ax, 30, 28, "PLACA 3 - BOTOEIRA C3 (dir.)", "ESP32-C3 0.42 + 8 botoes + joystick KY-023", xl=3.0)
    ut = {0:(15,C["uart"]),1:(14,C["uart"]),2:(33,C["btn"]),3:(32,C["btn"]),
          4:(31,C["btn"]),5:(35,C["analog"]),6:(36,C["gnd"]),7:(3,C["pwr"])}
    ub = {0:(37,C["analog"]),1:(38,C["analog"]),2:(27,C["btn"]),3:(28,C["btn"]),
          4:(29,C["btn"]),5:(34,C["btn"]),7:(30,C["btn"])}
    T, B = module(ax, 3, 7, 12, C3_TOP, C3_BOT, "ESP32-C3\nBotoeira", ut, ub, C["board"])
    ax.annotate("", xy=(3, 13.6), xytext=(1.0, 13.6), arrowprops=dict(arrowstyle="->", color=C["uart"], lw=1.7), zorder=4)
    ax.text(0.8, 13.6, "UART2 <-> S3 (W2): G0/G1", ha="right", va="center", fontsize=6.4, color=C["uart"], fontweight="bold")
    ax.annotate("", xy=(9, 5.0), xytext=(9, 3.0), arrowprops=dict(arrowstyle="->", color=C["pwr"], lw=1.7), zorder=4)
    ax.text(9, 2.6, "5V / GND (trilho)", ha="center", va="top", fontsize=6.4, color=C["pwr"], fontweight="bold")

    # 8 botoes tateis (2 fileiras, dir.)
    pos = {  # nome: (col, row, pino_func, badge)
        "setaE": (14, 20, B(2)), "setaD": (18, 20, B(3)), "alrt": (22, 20, B(4)), "buz": (26, 20, B(7)),
        "farA":  (14, 15, T(4)), "farB":  (18, 15, T(3)), "limp": (22, 15, T(2)), "frMo": (26, 15, B(5)),
    }
    gnd_nodes = []
    for nm, (c, r, pinf) in pos.items():
        gp, gg = tactile(ax, c, r, nm, C["btn"])
        wire(ax, pinf, gp, C["btn"], via_r=r-1.3 if r > 16 else r-1.3)
        gnd_nodes.append(gg)
    # barra de GND dos botoes -> C3 GND (T(6))
    gy = 23.5
    ax.plot([14, 28], [gy, gy], color=C["gnd"], lw=2.0, zorder=2)
    for gg in gnd_nodes:
        ax.plot([gg[0], gg[0]], [gg[1], gy], color=C["gnd"], lw=1.2, zorder=2)
    wire(ax, (14, gy), T(6), C["gnd"], via_c=12.5)

    # Joystick KY-023 (5 pinos + manche)  VCC = 3V3 !
    js = header(ax, 22, 6, ["GND","+5V","VRx","VRy","SW"], "Joystick KY-023", C["analog"], down=False)
    ax.add_patch(Circle((24, 9.0), 1.3, fc=C["analog"]+"33", ec=C["analog"], lw=1.2, zorder=3))
    ax.text(24, 4.2, "VCC -> 3V3 (NAO 5V) · SW nao usado", ha="center", fontsize=5.6, style="italic", color="#a00")
    wire(ax, B(0), js["VRx"], C["analog"], via_c=13.5)   # G3 -> VRx
    wire(ax, B(1), js["VRy"], C["analog"], via_c=13.0)   # G4 -> VRy
    wire(ax, T(5), js["+5V"], C["v3v3"],   via_c=12.0)   # 3V3 -> VCC(+5V pad)
    wire(ax, T(6), js["GND"], C["gnd"],    via_c=11.5)


placa_painel(axP); placa_nucleo(axN); placa_botoeira(axB)

items = [("USB", C["usb"]), ("5V", C["pwr"]), ("GND", C["gnd"]), ("3V3", C["v3v3"]),
         ("SPI", C["spi"]), ("UART", C["uart"]), ("Display", C["disp"]), ("Rele", C["relay"]),
         ("LEDs", C["led"]), ("Botoes", C["btn"]), ("Analog/ADC", C["analog"]), ("Mecanico (imã/eixo)", C["mech"])]
fig.legend(handles=[Line2D([0],[0],color=c,lw=4,label=l) for l,c in items],
           loc="lower center", ncol=12, fontsize=8.2, frameon=True, framealpha=0.95,
           columnspacing=1.0, handlelength=1.5, bbox_to_anchor=(0.5, 0.004))

fig.savefig("layout_placas_final.png", dpi=150, bbox_inches="tight", facecolor="white")
fig.savefig("layout_placas_final.pdf", bbox_inches="tight", facecolor="white")
print("OK -> layout_placas_final.png / .pdf")
