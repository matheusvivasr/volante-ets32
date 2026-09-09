# Volante DIY — Mapa de Ligações

> Documento-fonte das conexões físicas do projeto (estilo netlist/esquemático).
> **Mantido junto com o `codigos/volante_projeto.json`.** Atualizar a cada mudança de fiação.
> Última atualização: **2026-09-08**.

---

## 1. Visão geral do sistema

```
                    ┌──────────────────────────────────────────────┐
   PC (ETS2) ◄──────┤ USB nativo  : HID gamepad (eixo + botões)     │
        ▲    USB ───┤ USB CH343   : COM3 (flash/console/telemetria) │  ESP32-S3
        │           │ CAN(TWAI) ──► barramento I2C ──► AS5600        │  (núcleo)
        │           │                                                 │
   telemetria       └──────────────────────────────────────────────┘
   (Python)                 │ CANH/CANL (barramento único)
                            ▼
                  ┌───────────────────┐    ┌───────────────────┐
                  │   PAINEL  (C3)    │    │  BOTOEIRA  (C3)   │
                  │ GC9A01 + OLED     │◄──►│  botões/joystick   │
                  │ 2 relés + 2 LEDs  │    │                    │
                  └───────────────────┘    └───────────────────┘
                       (mesmo barramento CAN pros 2 satélites)
```

Transporte: **software já migrado pra CAN/TWAI** (barramento único) nos 3 nós — a
UART ponto-a-ponto (3 fios por enlace) saiu de cena no firmware. **Fiação física
do barramento (SN65HVD230 + 120Ω) ainda pendente** — ver §10 pro status e a
receita de como plugar um nó.

---

## 2. Alimentação (regras de ouro)

| Regra | Detalhe |
|---|---|
| **S3 → USB do PC** | Energia + dados. **NÃO** ligar 5V externo no 5V/VBUS da S3 (jumper `IN-OUT` fica ABERTO). |
| **C3(s) → 5V externo** | Trilho de 5V (buck a partir de 12/24V), injetado via USB-C *power-only* OU no pino 5V. |
| **Relé (bobina)** | `JD-VCC` puxa **direto do trilho 5V** (não pela placa lógica). Cap 220–470µF no rail. |
| **GND COMUM** | Mesma referência p/ TODOS (obrigatório p/ UART/SPI). **Trilha grossa/curta serve** — não precisa estrela perfeita: ligue cargas pesadas (relé, LEDs, buck, FFB futuro) perto da fonte e os sinais sensíveis (sensor, ADC) c/ retorno próprio ao mesmo nó. Ver §9. |
| **Cada placa** | Faz seu próprio 3V3 pelo regulador onboard. Distribui-se **5V + GND**. |

```
[Fonte 12/24V] → [Buck → 5,0V] → ┬─ 5V ─► Painel C3 (USB-C)
                                 ├─ 5V ─► Botoeira C3 (USB-C)
                                 ├─ 5V ─► Relé JD-VCC  (+cap)
                                 └─ GND ─┬─ Painel C3 GND
                                         ├─ Botoeira C3 GND
                                         ├─ Relé GND
                                         └─ S3 GND   ◄── (só GND; 5V vem do USB do PC)
```

---

## 3. ESP32-S3-N16R8 (núcleo)  —  pinout *visto de cima, USB-C à ESQUERDA*

```
SUP: GND 5V 14 13 12 11 10 09 46 03 08 18 17 16 15 07 06 05 04 RST 3V3 3V3
INF: GND GND 19 20 21 47 48 45 00 35 36 37 38 39 40 41 42 02 01 RX  TX  GND
                                                                (44)(43)
```

| Função | Pino S3 | Vai para |
|---|---|---|
| UART0 TX (console/telem) | `TX` = GPIO43 | CH343 → COM3 |
| UART0 RX | `RX` = GPIO44 | CH343 → COM3 |
| USB nativo D− / D+ | GPIO19 / GPIO20 | porta USB nativa → HID |
| **CAN (TWAI) TX** *(era UART1 → Painel)* | GPIO17 | SN65HVD230 `D` |
| **CAN (TWAI) RX** *(era UART1 → Painel)* | GPIO18 | SN65HVD230 `R` |
| **I2C AS5600** SDA | GPIO6 | AS5600 `SDA` |
| **I2C AS5600** SCL | GPIO7 | AS5600 `SCL` |
| ~~SPI AS5047P CLK~~ **livre** (sensor trocado pro AS5600, I2C — ver §7.1) | GPIO4 | — |
| ~~SPI AS5047P MISO~~ **livre** | GPIO5 | — |
| Botão BOOT (calibração direção / HID btn 16) | GPIO0 | — (botão onboard) |
| ~~UART2 → Botoeira~~ **livre** (Botoeira migrou pro mesmo barramento CAN acima) | GPIO15 / GPIO16 | — |
| **Pedal Acelerador** | GPIO10 *(ADC1_CH9)* | botão→GND (hoje) / potenciômetro (futuro) |
| **Pedal Freio** | GPIO9 *(ADC1_CH8)* | botão→GND (hoje) / potenciômetro (futuro) |
| Alimentação sensor | `3V3` / `GND` | AS5600 `VCC` / `GND` |
| `DIR` do AS5600 | GND | fixa sentido de contagem |

> Jumper `IN-OUT` (perto de GPIO11/12) = **ABERTO**: pino 5V não dá saída. Não mexer.

> **Pedais provisórios (botões)**: cada botão liga `GPIO → botão → GND` (pull-up interno, ativo-baixo).
> Pinos GPIO10/9 são ADC-capazes: ao trocar pelos potenciômetros nas molas, basta ligar os extremos
> do pot em `3V3`/`GND` e o cursor no mesmo GPIO, e mudar `PEDAIS_USE_ADC` p/ 1 em `pedais.h`.
> **Cursor do pot NUNCA em 5V** — estoura o ADC da S3.

> **Calibração da direção (BOOT)**: segure BOOT >1,5 s p/ entrar; gire totalmente p/ esquerda + clique,
> totalmente p/ direita + clique, centre + clique (salva em NVS). Segurar >1,5 s durante o processo cancela.
> Eixos HID: X=direção · Y=acelerador · Rx=freio · Z/Rz=visão (joystick da botoeira) · btns 1-8=botoeira, 16=BOOT.

---

## 4. ESP32-C3-0.42 (Painel)  —  pinout *visto de cima, USB-C à DIREITA*

```
SUP (esq→dir): 0  1  2  TX(21) RX(20) 3V3 GND 5V
INF (esq→dir): 3  4  5  6      7      8   9   10
```

| Função | Pino C3 | Vai para |
|---|---|---|
| **CAN (TWAI) TX** *(era UART1 → S3)* | GPIO0 | SN65HVD230 `D` |
| **CAN (TWAI) RX** *(era UART1 → S3)* | GPIO1 | SN65HVD230 `R` |
| **Display** CS | GPIO3 | GC9A01 `CS` |
| **Display** DC | GPIO4 | GC9A01 `DC` |
| **Display** MOSI/SDA | GPIO7 | GC9A01 `SDA` |
| **Display** SCLK/SCL | GPIO10 | GC9A01 `SCL` |
| OLED 0.42 (interno) SDA/SCL | GPIO5 / GPIO6 | OLED embutido (I2C) |
| **Relé** IN1 (seta esq) | GPIO2 | Módulo relé `IN1` |
| **Relé** IN2 (seta dir) | GPIO8 | Módulo relé `IN2` |
| **LED** farol baixo | GPIO20 (pad RX) | resistor 330Ω → LED → GND |
| **LED** farol alto | GPIO21 (pad TX) | resistor 330Ω → LED → GND |
| Botão BOOT (troca tela) | GPIO9 | — (botão onboard) |
| Alimentação | `3V3` / `GND` / `5V` | display/relé/trilho |

---

## 5. Display GC9A01 (TFT redondo, 7 pads)

Silk *visto de frente (tela), pads embaixo*: `RST CS DC SDA SCL GND VCC`
**(Ao soldar POR TRÁS, a ordem espelha!)**

| GC9A01 | Painel C3 |
|---|---|
| `VCC` | 3V3 |
| `GND` | GND |
| `SCL` | GPIO10 |
| `SDA` | GPIO7 |
| `DC`  | GPIO4 |
| `CS`  | GPIO3 |
| `RST` | 3V3 (reset por software) |

> **Fiação reordenada em 2026-09-08** pra rodar em linha reta na fileira de baixo do
> C3 (`3,4,[5,6=OLED],7,[8,9=relé/BOOT],10`), sem cruzar fio: como o SPI do ESP32 é
> roteado por software (GPIO matrix), qualquer papel (CS/DC/SDA/SCL) pode ir em
> qualquer um desses 4 pinos — só o `#define` em `display.h` precisa acompanhar.
> Pinos físicos usados continuam os mesmos de sempre (3,4,7,10); só o papel de cada
> um mudou. GPIO5/6 (OLED embutido) e GPIO8 (relé seta direita) **não entraram** na
> troca — não têm pino livre pra ceder (ver mapa completo no `codigos_c3_painel/main/*.h`).

> **Desacoplamento OBRIGATÓRIO (descoberto 2026-06-14, na validação da placa soldada):**
> soldar **100 nF (cerâmico "104") + 10 µF** entre `VCC` e `GND`, colados nos pads do
> display, pernas curtas. **Sem isso, na placa soldada aparece borrão vertical do
> conteúdo** durante o flush do SPI (queda de tensão transitória na rajada DMA). Na
> protoboard não dava por causa dos retornos curtos. Pegadinhas: **o bip de continuidade
> no GND prova só DC** — o que conta na rajada é retorno **curto/grosso + cap local**;
> baixar o clock (10→6 MHz) **NÃO resolve** (o gargalo é alimentação, não a velocidade).
> Com o cap, **10 MHz roda limpo**. (Retenção de imagem em tela estática é traço do
> painel; some com o conteúdo dinâmico do jogo — não é defeito de solda nem de código.)

---

## 6. Módulo 2 relés + LEDs de farol (no Painel C3)

**Módulo de relé** = opto active-low, relé SRD-05VDC (bobina 5V). Jumper VCC↔JD-VCC **removido**.

| Módulo relé | Liga em |
|---|---|
| `IN1` | C3 GPIO2 (seta esq) |
| `IN2` | C3 GPIO8 (seta dir) |
| `VCC` (lógico) | 3V3 |
| `JD-VCC` (bobina) | **5V** (trilho) |
| `GND` | GND comum |

**LEDs de farol** (ativo-alto): `GPIO20 → 330Ω → LED baixo → GND` · `GPIO21 → 330Ω → LED alto → GND`.

---

## 7. Sensor de ângulo: AS5600 (I2C)

> **Status: sensor ATIVO desde 2026-06-16** (commit `230008a`), substituindo o
> AS5047P (SPI) da §7.1. Fonte do driver: `codigos/main/direcao/as5600.h`.

| AS5600 | S3 |
|---|---|
| `VCC` | 3V3 |
| `GND` | GND |
| `SDA` | GPIO6 |
| `SCL` | GPIO7 |
| `DIR` | GND (fixa o sentido de contagem) |
| `OUT`, `GPO` | não usados |

Endereço I2C fixo `0x36`. Ângulo bruto: `RAW ANGLE` (regs `0x0C`/`0x0D`, 12 bits,
4096 contagens/volta — o AS5047P antigo tinha 16384, 14 bits; multi-turn e
calibração usam a mesma lógica, só muda o CPR). Status do ímã: reg `0x0B`
(`MD`=detectado, `ML`=fraco, `MH`=forte). Ímã diametral centrado sobre o chip,
gap 0,5–3 mm.

**GPIO4 e GPIO5 (antigo `CLK`/`MISO` do AS5047P) ficaram livres** — o AS5600 é
I2C (só 2 fios de sinal) contra os 4 do SPI do sensor anterior.

### 7.1 Sensor anterior (arquivado): AS5047P-TS_EK_AB (SPI)

> Trocado pelo AS5600 em 2026-06-16. Código (`as5047.c/.h`) mantido no disco,
> **fora do build**, caso o projeto volte a usar um encoder SPI de maior
> resolução. Fonte da sequência de pinos: datasheet AS5047P (DS000324) e
> Operation Manual do EK_AB.

**Header P1 = 2×8. Usar SÓ a fileira de cima (pinos 1–8).** A de baixo (pinos 9–16
= `B A I/PWM TEST NC W/PWM V U`) **não é usada**.

Sequência física da fileira de cima — **pino 3 é `NC` (pular! é a armadilha de off-by-one):**

| P1 | Pad EK_AB | S3 | `#define` em as5047.h |
|---|---|---|---|
| 1 | `5V`   | — (não usar) | — |
| 2 | `3V3`  | 3V3 | (alimentação) |
| 3 | `NC`   | — (pular) | — |
| 4 | `CSn`  | GPIO7 | `AS5047_PIN_CS    7` |
| 5 | `CLK`  | GPIO4 | `AS5047_PIN_SCLK  4` |
| 6 | `MOSI` | GPIO6 | `AS5047_PIN_MOSI  6` |
| 7 | `MISO` | GPIO5 | `AS5047_PIN_MISO  5` |
| 8 | `GND`  | GND | (comum) |

**Alimentação 3,3 V OBRIGATÓRIA** (nunca 5 V no sensor com ligação direta): os I/O do
AS5047P seguem o VDD → `VOH = VDD − 0,5`. A 5 V o `MISO` sairia ~4,5 V e **queima o
GPIO5** (S3 não é 5 V-tolerante); e o VIH do sensor a 5 V = 3,5 V, acima dos 3,3 V que
o S3 entrega (nem acionaria). A 3,3 V: VOH ≈ 2,8 V, tudo no mesmo domínio. No EK_AB:
**JP1 no lado 3V3** (R2 populado / R1 fora); no chip, `VDD` e `VDD3V3` **amarrados** +
cap **1 µF** em `VDD3V3`→GND.

> SPI modo 1 (CPOL=0, CPHA=1), ≤10 MHz, frames 16 bits MSB-first, paridade par (bit15)
> + error-flag (bit14). Ímã **diametral** (ref. N35H Ø8×3 mm) centralizado sobre o chip,
> gap 0,5–2 mm (crítico p/ linearidade), Bz 35–70 mT. Fileira de baixo (A/B/I/PWM/TEST)
> não usada.

---

## 8. Botoeira (ESP32-C3 0.42)  —  pinout *visto de cima, USB-C à DIREITA*

```
SUP (esq→dir): 0  1  2  TX(21) RX(20) 3V3 GND 5V
INF (esq→dir): 3  4  5  6      7      8   9   10
```

Enlace: **CAN (TWAI)** — Botoeira GPIO0/1 no mesmo barramento CANH/CANL da S3 e
do Painel (§10), **não** mais um enlace ponto-a-ponto próprio (a antiga UART2
da S3, GPIO15/16, ficou livre — ver §3). Console da Botoeira movido p/
**USB-JTAG** (libera GPIO20/21 p/ botões). OLED 0.42 não usado (GPIO5/6 viram
botões). Mensagem **0x040** (4 bytes): `[botões uint16][view_x int8][view_y int8]`.

### Link + alimentação

| Botoeira C3 | Vai para |
|---|---|
| `GPIO0` (CAN TX) | SN65HVD230 `D` (próprio da Botoeira) |
| `GPIO1` (CAN RX) | SN65HVD230 `R` (próprio da Botoeira) |
| `GND` | GND comum |
| `5V` ou `3V3` | trilho de alimentação |

### Botões (ativo-baixo: GPIO → botão → GND; pull-up interno)

| Botão (bit) | GPIO C3 |
|---|---|
| seta esquerda (0) | GPIO5 |
| seta direita (1) | GPIO6 |
| pisca-alerta (2) | GPIO7 |
| buzina (3) | GPIO10 |
| farol alto (4) | GPIO20 |
| farol baixo (5) | GPIO21 |
| limpador (6) | GPIO2 ⚠️ strapping |
| freio-motor (7) | GPIO8 ⚠️ strapping |

> ⚠️ GPIO2/8 são strapping: **não segure** os botões de limpador/freio-motor durante a energização.

### Joystick analógico (controle de visão) — KY-023 ou similar

| Joystick | Vai para |
|---|---|
| `VCC` | **3.3V** ⚠️ (NÃO 5V — estoura o ADC da C3) |
| `GND` | GND |
| `VRx` (horizontal) | C3 `GPIO3` (ADC1_CH3) |
| `VRy` (vertical) | C3 `GPIO4` (ADC1_CH4) |
| `SW` (clique) | *(não usado por ora)* |

No S3 (UART2 → 0x040): 8 botões → **botões HID 1–8**; view_x/view_y → **eixos Z / Rz** (olhar no ETS2).

---

## 9. Zonas físicas e comprimento de fios

> **Princípio:** *digitaliza perto, transmite digital longe.* Sinal analógico ou
> rápido fica CURTO e LOCAL na própria placa; só digital lento (UART/CAN) cruza a
> cabine. É o motivo de a arquitetura ser host+satélites.

```
  ZONA COLUNA/EIXO ───────────┐        ZONA DASHBOARD ───────────┐
  AS5600 (no eixo) ─I2C────────┤        Painel C3 + GC9A01 + OLED │
   ≤10cm─ ESP32-S3 (host) ◄────┼──CAN───+ 2 relés + 2 LEDs        │
            │  +FFB futuro      │ (CANH/CANL, mesmo barramento    │
            │ pedais ~1-1.5m    │  da Botoeira) ZONA CONSOLE LATERAL ┤
            ▼                   │        Botoeira C3 + 8 botões    │
  ZONA CHÃO/PEDAL ◄────CAN──────┼───────►+ joystick (local ≤15cm) │
  (botão hoje / satélite        │                                 │
   C3 quando virar pot)         └── GND comum + trilho 5V em tudo ─┘
```

| Enlace | Sinais | Veloc. | Comprimento ± | Crít. | Onde |
|---|---|---|---|---|---|
| S3 ↔ AS5600 | SDA/SCL +3V3/GND | I2C | **≤10 cm** ideal, máx ~20-30 cm | 🔴 | **S3 colado na coluna** (sensor preso ao eixo) |
| S3 ↔ Painel ↔ Botoeira (**mesmo barramento CAN**) | CANH/CANL +GND | 500 kbps (TWAI) | dezenas de m (típico ≤100 m @500kbps, ISO11898) | 🟢 | qualquer nó longe OK; 120Ω só nas 2 pontas físicas (§10) |
| S3 ↔ Pedais (botão hoje) | 2 GPIO +GND | digital | ≤ ~1 m (>1 m: pull-up ext. 4k7 +100nF) | 🟡 | chão ~1-1.5 m, ok p/ botão |
| S3 ↔ Pedais (pot futuro) | 2 ADC +3V3/GND | analóg. | **≤ ~30 cm ao ADC** | 🔴 | NÃO esticar → **satélite C3 no chão** |
| Botoeira ↔ Joystick | 2 ADC +3V3/GND | analóg. | ≤ 10-20 cm (local) | 🔴 | **local na botoeira** (já é) |
| Buck 5V → C3s/relé | 5V +GND | potência | por queda de tensão (bitola), ~1-2 m c/ 20 AWG sub-amp | — | cargas pesadas perto da fonte |

**Regra — fio direto na S3 vs placa própria:**

- **Fio direto** se: digital/lento **OU** trecho curto **OU** for o SPI do sensor (já tem de ficar perto da S3).
- **Satélite C3 próprio** se: **longe E analógico/muitos fios** → digitaliza na origem, manda 2 fios CAN (CANH/CANL) +GND. Fio analógico longo = antena de ruído + queda; CAN longo = só nível diferencial, aguenta.

**Fio (independe da topologia):**

- CAN longo: CANH/CANL **trançados entre si**, GND à parte (não precisa trançar com o par). 120Ω só nas 2 pontas físicas do barramento (§10) — nunca em nó do meio.
- I2C do sensor (AS5600): o mais curto possível; cabo longo pede pull-up externo mais forte nos SDA/SCL.
- 5V: bitola pela corrente + comprimento (queda), não pela velocidade.

> **Status (2026-09-08):** o CAN já é o transporte ativo no firmware dos 3 nós
> (§10) — os limites de comprimento acima já valem pro CAN, não mais UART.
> Falta só a fiação física (SN65HVD230 + 120Ω) pra esse relaxamento virar
> realidade na bancada; até lá, nenhum nó troca mensagem de verdade.

---

## 10. Barramento CAN (TWAI)

> **Status (2026-09-08): software migrado e verificado nos 3 nós; fiação física
> pendente.** Os `CMakeLists.txt` dos 3 firmwares já compilam os backends CAN
> (não mais UART) — confirmado rodando de verdade no Painel C3 (log de boot:
> `can_bus: TWAI/CAN init OK (TX=GPIO0 RX=GPIO1, 500 kbps)`) e compilação limpa
> na S3 e na Botoeira. **Falta só cabear o SN65HVD230 em cada nó** — sem
> transceiver físico, cada nó fica "conversando sozinho" (driver TWAI de pé,
> sem tráfego). Módulos SN65HVD230 e resistores de 120Ω já em mãos (mat/2026).

Os enlaces UART viram **um barramento CAN único**:

```
S3 ─[SN65HVD230]─┐                          ┌─[SN65HVD230]─ Painel C3
                 ├═══ CANH ════════════════ ┤
   120Ω ─────────┤                          ├───────── 120Ω
                 ├═══ CANL ════════════════ ┤
                 └── GND comum ─────────────┘  (+ Botoeira, Pedais, Câmbio...)
```

- S3 TWAI: TX=GPIO17, RX=GPIO18 (reaproveita pinos da antiga UART1).
- C3 (Painel e Botoeira) TWAI: TX=GPIO0, RX=GPIO1 (reaproveita pinos da antiga UART1).
- 120Ω em **cada uma das duas pontas físicas** do barramento — **não** um por nó.
  Com 2 nós só (bring-up), os 120Ω vão nas duas pontas do próprio trecho S3↔Painel;
  ao entrar um 3º nó no meio (Botoeira, depois Pedais/Câmbio), ele **não** leva
  resistor — só quem fica fisicamente nas pontas do fio leva.
- Framing do projeto (igual nos 3 nós): payload útil ≤ 7 bytes; o CAN faz
  SOF/CRC/ACK/arbitragem em hardware; o **último byte** do frame é o contador
  de sequência. IDs (11 bits): `0x020` pedais, `0x030` câmbio, `0x040` botoeira,
  `0x100–0x102` telemetria→painel, `0x7FF` heartbeat.

### Componente compartilhado: `components/can_bus`

O driver TWAI não é mais reimplementado por nó. Existe **um componente ESP-IDF
só** (`components/can_bus/`, referenciado nos 3 projetos via
`EXTRA_COMPONENT_DIRS` no `CMakeLists.txt` de topo) com a API:

```c
esp_err_t can_bus_init(int tx_gpio, int rx_gpio);              // sobe o driver, 500 kbps
esp_err_t can_bus_send(uint16_t id, const uint8_t *payload, uint8_t len);
void      can_bus_start_rx(can_bus_rx_cb_t cb);                 // task de recepção
```

Cada nó só tem um arquivo fino (`transport_can.c` na S3, `transport_rx_can.c`
no Painel, `transport_tx_can.c` na Botoeira) que chama essas 3 funções com os
pinos do próprio nó e traduz `can_bus_msg_t` <-> o `transport_msg_t` que o
`main.c` daquele nó já conhecia — **`main.c` de nenhum nó precisou mudar.**

### Receita pra plugar um nó CAN novo (pedais, câmbio, o que vier)

1. `idf_component_register` do novo projeto **não precisa declarar `REQUIRES`**
   pro `can_bus` — sem `REQUIRES`/`PRIV_REQUIRES` nenhum, o `main` do ESP-IDF já
   ganha todos os componentes de graça (é assim que os 3 nós atuais funcionam).
   Só **copie o `set(EXTRA_COMPONENT_DIRS ...)` do `CMakeLists.txt` de topo** de
   qualquer um dos 3 projetos existentes, apontando pra `../components`.
2. Escolha 2 GPIOs livres pro TX/RX do TWAI (na C3, se sobrar, `GPIO0/1` de novo
   não serve — cada nó no barramento tem seu próprio par de pinos locais; não é
   um pino compartilhado entre nós, só o CANH/CANL físico é compartilhado).
3. Escreva um `transport_*.c` fino chamando `can_bus_init()` +
   `can_bus_send()`/`can_bus_start_rx()` (copie o do nó mais parecido: Botoeira
   se só transmite, Painel se só recebe, S3 se os dois).
4. Cabeie o SN65HVD230 do nó novo: TX/RX do ESP → `D`/`R` do transceiver,
   `CANH`/`CANL` no barramento, GND comum. **120Ω só se esse nó for uma ponta
   física nova do barramento** (normalmente não é — ele entra no meio).
5. Novo ID de mensagem: siga a faixa livre depois de `0x102` (ou `0x0xx` pra
   entrada de sensor, seguindo o padrão de `0x020`/`0x030`/`0x040` já usado).
