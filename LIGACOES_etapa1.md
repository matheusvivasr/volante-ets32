# Volante DIY — Ligações da Etapa 1 (Painel)

Placas envolvidas: **ESP32-S3 N16R8** (host) + **ESP32-C3 "0.42 OLED"** (painel) + **GC9A01** (TFT redondo 240×240).
Tudo em **3.3 V**, sem conversor de nível.

---

## 1. GC9A01 (TFT redondo) → ESP32-C3

| GC9A01 | Função          | C3 (GPIO) | Observação                          |
|--------|-----------------|-----------|-------------------------------------|
| VCC    | Alimentação     | 3V3       | —                                   |
| GND    | Terra           | GND       | —                                   |
| SCL    | Clock SPI       | GPIO4     | SCLK                                |
| SDA    | Dados SPI (MOSI)| GPIO3     | MOSI                                |
| CS     | Chip select     | GPIO7     | —                                   |
| DC     | Data/Command    | GPIO10    | —                                   |
| RST    | Reset           | 3V3       | reset por software (sem pino)       |
| BL     | Backlight       | 3V3       | sempre ligado                       |

> SPI a 40 MHz. Se houver chuvisco com jumpers longos, baixe `GC9A01_SPI_HZ` para 20 MHz em `display.h`.

## 2. UART S3 ↔ C3 (telemetria)

| S3 (GPIO)     | Sentido | C3 (GPIO)     | Sinal |
|---------------|:-------:|---------------|-------|
| GPIO17 (TX1)  |   →     | GPIO1 (RX1)   | dados S3→painel |
| GPIO18 (RX1)  |   ←     | GPIO0 (TX1)   | retorno painel→S3 |
| GND           |   —     | GND           | terra comum (obrigatório) |

> **Cruzado**: TX de um vai no RX do outro. 115200 baud, 8N1.

## 3. USB (PC)

| Placa | Porta USB | Para quê |
|-------|-----------|----------|
| S3    | USB nativo / CH340 | recebe telemetria do PC (`telemetry_reader.py`) **e** grava firmware |
| C3    | USB nativo (CH9102/JTAG) | só gravar firmware / monitor |

> Anote as portas COM no Gerenciador de Dispositivos: S3 = `COM?`, C3 = `COM?`.

---

## Diagrama

```
                 ┌──────────────────────────────┐
                 │              PC               │
                 │  ETS2 + scs-sdk-plugin        │
                 │  telemetry_reader.py          │
                 └──────────────┬───────────────┘
                                │ USB (serial)  COM da S3
                                ▼
        ┌─────────────────────────────────────────────┐
        │              ESP32-S3 N16R8 (host)           │
        │                                              │
        │   GPIO17 (TX1) ──────────────┐               │
        │   GPIO18 (RX1) ───────────┐  │               │
        │   GND ──────────────────┐ │  │               │
        └─────────────────────────┼─┼──┼──────────────┘
                                  │ │  │
                       GND comum  │ │  │  3 fios (TX/RX/GND)
                                  │ │  │
        ┌─────────────────────────┼─┼──┼──────────────┐
        │   GND ◄─────────────────┘ │  │              │
        │   GPIO0 (TX1) ◄───────────┘  │              │
        │   GPIO1 (RX1) ◄──────────────┘              │
        │                                              │
        │           ESP32-C3 "0.42 OLED" (painel)      │
        │                                              │
        │   GPIO4 (SCL) ───────┐                       │
        │   GPIO3 (SDA) ──────┐│                       │
        │   GPIO7 (CS)  ─────┐││                       │
        │   GPIO10 (DC) ────┐│││                       │
        │   3V3 / GND ─────┐││││                       │
        └──────────────────┼┼┼┼┼───────────────────────┘
                           │││││
                           ▼▼▼▼▼
        ┌─────────────────────────────────────────────┐
        │        GC9A01  TFT redondo 240×240           │
        │  VCC=3V3  GND  SCL=4  SDA=3  CS=7  DC=10      │
        │  RST=3V3  BL=3V3                             │
        │            (   velocímetro   )               │
        └─────────────────────────────────────────────┘
```

> OLED 0.42 embutido no C3 (I²C GPIO5/6): **não usado** na Etapa 1 — reservado para
> instrumentos analógicos com micro-servos no futuro.
