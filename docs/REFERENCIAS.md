# Volante DIY — Referências dos componentes (pinagem oficial)

> Índice de **documentos de referência** usados para a pinagem e os desenhos técnicos
> do projeto. Cada item traz: doc-fonte, pinagem-chave, cuidado de tensão e onde está
> no projeto (seção do `LIGACOES.md`). Mantido junto com `LIGACOES.md`.
> Última atualização: **2026-06-15**.

---

## 1. ESP32-S3-N16R8 (núcleo / host) — `LIGACOES.md §3`

- **Ref:** [Espressif ESP32-S3-DevKitC-1 (oficial)](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/index.html) · [espboards.dev](https://www.espboards.dev/esp32/esp32-s3-devkitc-1/) · [Random Nerd](https://randomnerdtutorials.com/esp32-s3-devkitc-pinout-guide/)
- **Pinagem-chave:** UART0 console TX=43/RX=44; **USB nativo** D−/D+ = **GPIO19/20** (HID);
  SPI livre p/ sensor em G4/G5/G6/G7; pedais ADC G9/G10; BOOT=G0.
- **Tensão:** GPIO **3,3 V, não 5 V-tolerante**. Alimentar **só pelo USB do PC** (jumper IN-OUT aberto).
- **Cuidado:** GPIO 26–32 ocupados pela flash/PSRAM octal (N16R8) — não usar.

## 2. ESP32-C3-0.42 OLED (painel e botoeira) — `LIGACOES.md §4 e §8`

- **Ref:** [espboards.dev — ESP32-C3 OLED 0.42](https://www.espboards.dev/esp32/esp32-c3-oled-042/) · [Zephyr — 01space esp32c3_042_oled](https://docs.zephyrproject.org/latest/boards/01space/esp32c3_042_oled/doc/index.html) · [Cirkit Designer](https://docs.cirkitdesigner.com/component/d356ca80-2207-43dd-95de-361b47687a81/esp32c3-042-oled)
- **Pinagem-chave:** 13 GPIO; **OLED 0.42" (SSD1306, 72×40) interno via I²C em GPIO5(SDA)/GPIO6(SCL)**;
  expõe SUP `0 1 2 21 20 3V3 GND 5V`, INF `3 4 5 6 7 8 9 10`. USB-C + LED de power.
- **Tensão:** lógica **3,3 V**. GPIO2/8/9 são **strapping** (cuidado ao energizar).

## 3. GC9A01 — TFT redondo 1.28" 240×240 (painel) — `LIGACOES.md §5`

- **Ref:** [StudioPieters — guia GC9A01](https://www.studiopieters.nl/gc9a01/) · [DroneBot Workshop](https://dronebotworkshop.com/gc9a01/) · [ProtoSupplies](https://protosupplies.com/product/ips-lcd-128-round-pcb-gc9a01/)
- **Pinagem (7 pinos):** `VCC GND SCL SDA RES DC CS` (algumas variantes têm `BLK` de backlight).
- **No projeto:** SDA=G3, SCL=G4, CS=G7, DC=G10, VCC/RST=3V3, GND. SPI ~10 MHz.
- **Tensão:** **lógica 3,3 V apenas** (VCC aceita 3,3–5 V por ter regulador, mas dados são 3,3 V).
- **Cuidado:** exige **cap 100nF + 10µF** no VCC/GND da placa soldada (senão borra no flush DMA — ver §5).

## 4. AS5047P-TS_EK_AB — sensor de ângulo SPI — `LIGACOES.md §7`

- **Ref:** [Datasheet AS5047P DS000324 (ams-OSRAM)](https://look.ams-osram.com/m/d05ee39221f9857/original/AS5047P-DS000324.pdf) · [EK_AB Operation Manual](https://www.mouser.com/datasheet/2/588/AS5047P-TS_EK_AB_Operation-Manual_Rev.1.0-775823.pdf)
- **Header P1 (fileira de cima 1–8):** `5V 3V3 NC CSn CLK MOSI MISO GND` — **pino 3 = NC (off-by-one!)**.
- **No projeto:** CSn→G7, CLK→G4, MOSI→G6, MISO→G5, 3V3, GND. SPI modo 1, ≤10 MHz, CPR 16384.
- **Tensão:** **3,3 V OBRIGATÓRIO.** I/O seguem VDD (`VOH = VDD−0,5`): a 5 V o MISO sai ~4,5 V e
  **queima o GPIO5**. JP1 no 3V3 (R2 populado/R1 fora); VDD+VDD3V3 juntos + cap 1µF.
- **Ímã:** diametral N35H Ø8×3 mm, gap 0,5–2 mm, Bz 35–70 mT.

## 5. Módulo relé 2 canais (opto, SRD-05VDC-SL-C) — `LIGACOES.md §6`

- **Ref:** [Datasheet SRD-05VDC-SL-C (Songle, circuitbasics)](https://www.circuitbasics.com/wp-content/uploads/2015/11/SRD-05VDC-SL-C-Datasheet.pdf) · [Cirkit Designer](https://docs.cirkitdesigner.com/component/89153622-65b6-470a-ae1e-7e433bca3e0c/relay-srd-05vdc-sl-c)
- **Pinagem do módulo:** controle `IN1 IN2 VCC GND` + jumper `VCC↔JD-VCC`; saída relé SPDT `COM/NO/NC`.
- **No projeto:** IN1=G2 (seta esq), IN2=G8 (seta dir), VCC(lógico)=3V3, **JD-VCC=5V** (trilho), GND comum.
  **Jumper VCC↔JD-VCC REMOVIDO** (opto isolado). Active-low. Cap 220–470µF no rail.

## 6. KY-023 — joystick analógico (botoeira) — `LIGACOES.md §8`

- **Ref:** [espboards.dev — KY-023](https://www.espboards.dev/sensors/ky-023/) · [WatElectronics](https://www.watelectronics.com/ky-023-joystick-module/)
- **Pinagem (5 pinos):** `GND VCC VRx VRy SW` (2× pot 10 kΩ a 90°, centro ~½ VCC).
- **No projeto:** VRx→G3 (ADC1_CH3), VRy→G4 (ADC1_CH4), VCC=**3,3 V**, GND. SW não usado.
- **Tensão:** **VCC em 3,3 V** — 5 V no cursor estoura o ADC da C3.

## 7. SN65HVD230 — transceiver CAN (futuro, etapa 6) — `LIGACOES.md §10`

- **Ref:** [Datasheet SN65HVD230 (TI)](https://www.ti.com/lit/ds/symlink/sn65hvd230.pdf)
- **No projeto:** 1 por nó; CANH/CANL + 120 Ω em cada ponta; S3 TWAI TX=G17/RX=G18, C3 TX=G0/RX=G1.

---

> **Padrão de tensão do projeto:** tudo que é digital fala **3,3 V**. O **trilho 5 V** (buck) só
> alimenta C3(s) e a bobina do relé (JD-VCC). **Sensor/ADC nunca em 5 V.** GND comum obrigatório.
