# Volante DIY modular para simuladores (ETS2)

Volante de simulador caseiro, modular e de baixo custo, construído por etapas
funcionais, com *force feedback* (FFB) planejado para o futuro. Jogo-alvo:
**Euro Truck Simulator 2**.

A arquitetura é **host + satélites**:

- **ESP32-S3** = núcleo/host. Lê o volante por SPI, lê os pedais, agrega o estado
  de todos os módulos e aparece para o PC como um **gamepad USB (HID)**. Roda o
  FFB no futuro.
- **ESP32-C3** = satélites (um por módulo). Cada um lê seus sensores/entradas
  locais (ou recebe telemetria, no caso do painel) e troca mensagens com a S3.

O **transporte** entre placas é abstraído: o formato lógico de mensagem é o mesmo
em qualquer meio físico. Hoje é **UART** (ponto-a-ponto); migra para **CAN/TWAI**
quando entrar um 3º módulo satélite e os transceivers `SN65HVD230` chegarem.

> Documentos-fonte complementares:
> - [`LIGACOES.md`](LIGACOES.md) — netlist/pinagem física detalhada (fonte oficial; atualizada a cada mudança de fiação).
> - [`codigos/volante_projeto.json`](codigos/volante_projeto.json) — estado completo do projeto, decisões e estrutura de arquivos.
>
> Este README é suficiente para **replicar** o software, o firmware e o hardware.
> Em caso de divergência de fiação, o `LIGACOES.md` prevalece.

---

## 1. Requisitos de hardware

### Placas e principais componentes

| Qtd | Componente | Papel |
|---|---|---|
| 1 | **ESP32-S3** (testado: N16R8, 16 MB flash / 8 MB PSRAM) | Núcleo/host USB-HID + leitura do volante e pedais |
| 1 | **ESP32-C3** (placa "0.42 OLED") | Painel de instrumentos |
| 1 | **ESP32-C3** (placa "0.42 OLED") | Botoeira |
| 1 | **AS5047P** (placa AS5047P-TS_EK_AB, AMS) | Encoder magnético de ângulo (direção), SPI |
| 1 | **Ímã diametral** (~6 mm, vem no kit do AS5047P) | Preso ao eixo, lido pelo sensor |
| 1 | **GC9A01** TFT redondo 240×240 (SPI) | Tela do painel |
| 1 | Módulo **2 relés** (opto, *active-low*, bobina SRD-05VDC) | Tic-tac do pisca |
| 2 | LEDs + resistores 330 Ω | Faróis baixo/alto no painel |
| 1 | Joystick analógico (KY-023 ou similar) | Controle de visão (na botoeira) |
| 8 | Botões momentâneos | Setas, buzina, faróis, etc. (na botoeira) |
| 2 | Botões momentâneos | Acelerador e freio provisórios (na S3) |
| 1 | Fonte **5 V** (≥1 A; carregador de celular serve nesta fase) | Trilho de 5 V dos satélites |
| — | Fios, mola de torção (centralização do volante), carcaça | Mecânica |

> **Futuro (CAN, ainda não adquirido):** 1× `SN65HVD230` por nó + 2× resistores 120 Ω.

### Regras de alimentação (importantes)

- **A S3 é alimentada pelo USB do PC.** Nunca injetar 5 V externo no pino 5V/VBUS
  da S3 — o jumper solável `IN-OUT` (perto de GPIO11/12) fica **ABERTO**.
- **Os C3 e periféricos** usam o **trilho de 5 V** da fonte externa.
- **GND comum obrigatório**: o GND de tudo (S3, C3s, fonte, relés, sensor) se junta
  em estrela, num único ponto.
- **Sensor AS5047P e joystick sempre em 3V3, nunca 5 V** (5 V estoura o ADC / sai
  da faixa do sensor).
- **Cursor de potenciômetro (pedais futuros) nunca em 5 V** — só 3V3/GND/GPIO.

---

## 2. Como instalar o firmware (passo a passo, do zero)

> Esta seção assume que **você nunca mexeu com firmware**. Sistema usado nos
> testes: **Windows 11**.
>
> Há **dois caminhos**, escolha o seu:
> - **🟢 Caminho rápido (só usar o volante):** grava o firmware já pronto. Precisa
>   só de **Python** — sem ESP-IDF, sem compilador. **Comece em "2.A".**
> - **🔧 Caminho completo (modificar o código):** compila do zero. Precisa do
>   **ESP-IDF v5.4**. Veja "2.B".

---

### 2.A — Caminho rápido: gravar o firmware pronto (recomendado)

Para quem só quer **usar** o volante. **Pré-requisito único:** ter o **Python 3**
instalado ([python.org](https://www.python.org/downloads/) → marque *"Add Python
to PATH"* na instalação).

#### Mais simples ainda: clique duas vezes (Windows)

Na raiz do projeto há o **`Instalar Volante.bat`**. Dê **duplo-clique** nele:
abre o menu de instalação sem você precisar digitar nada no terminal. (Se o
Python não estiver instalado, ele avisa com o link para baixar.)

#### Pelo terminal: o instalador com menu

O `.bat` acima só chama o `installer.py`, na **raiz do projeto**: um menu único
onde você escolhe a placa e a porta. Ele instala o que precisa, detecta as portas
e grava.

```powershell
cd C:\Volante
python installer.py
```

O menu mostra os módulos disponíveis e, se houver mais de uma ESP ligada, deixa
você escolher a porta. Depois de gravar, ele pergunta se quer gravar outra placa —
prático para instalar as três em sequência.

```
=== Modulos disponiveis ===
  [1] Nucleo (ESP32-S3)  - volante + pedais + host USB-HID   (codigos/)
  [2] Painel (ESP32-C3)  - display GC9A01                    (codigos_c3_painel/)
  [3] Botoeira (ESP32-C3) - botoes + joystick                (codigos_c3_botoeira/)
  [0] Sair
Escolha o modulo:
```

Atalhos (sem menu, úteis para automatizar):
```powershell
python installer.py --list                  # lista módulos e portas
python installer.py -m codigos -p COM3      # grava direto o núcleo na COM3
python installer.py -m codigos --monitor    # grava e abre o monitor (Ctrl+C p/ sair)
```

> **Escalável:** o instalador descobre os módulos sozinho — qualquer pasta que
> tenha firmware compilado (`build/`) aparece no menu automaticamente. Para um
> nome bonito, adicione a pasta em `FRIENDLY_NAMES` no topo do `installer.py`.

#### Alternativa: gravar dentro da pasta de uma placa

Cada pasta de placa também tem um `flash.py` autônomo (mesma lógica, sem menu):

```powershell
cd C:\Volante\codigos          # ou codigos_c3_painel / codigos_c3_botoeira
python flash.py                # grava esta placa
python flash.py --monitor      # grava e mostra as mensagens
python flash.py --port COM5    # força a porta
```

> Se aparecer "Nenhuma porta serial encontrada": o cabo é só de carga (troque por
> um de dados) ou falta o driver USB-serial (CH343/CH340/CP210x) — instale o do
> fabricante e tente de novo.

#### Enviar o firmware para outra pessoa (pacote enxuto)

Há um pacote pronto e **leve** para distribuir: `Volante_dist.zip` (≈ 0,5 MB), com
só o essencial para gravar (binários + `installer.py` + `.bat` + README). É gerado
e **atualizado automaticamente a cada build** (gancho `package.py`); para gerar na
mão a qualquer momento:

```powershell
python package.py --zip
```

Quem receber o zip só precisa: ter **Python**, descompactar, dar duplo-clique em
`Instalar Volante.bat` e gravar. Para o firmware **funcionar de fato**, a pessoa
precisa das **mesmas placas** (1× ESP32-S3 + 2× ESP32-C3) e da **fiação igual** à
da seção 3 / `LIGACOES.md`.

Pronto — pule para o **Passo 5** (conferir se funcionou). Os passos 1–4 abaixo são
só para quem vai **compilar** o código.

---

### 2.B — Caminho completo: compilar do código-fonte

Necessário apenas se você quer **alterar o firmware**. Os passos 1 a 4 a seguir
instalam o ESP-IDF e compilam/gravam do zero.

### Passo 1 — Instalar o ESP-IDF v5.4

O ESP-IDF é a ferramenta oficial da Espressif que compila e grava o firmware.

1. Baixe o **instalador offline do ESP-IDF v5.4 para Windows** no site da Espressif
   (procure por *"ESP-IDF Windows Installer"*, versão **5.4**).
2. Rode o instalador e aceite as opções padrão. Ele instala tudo que é preciso:
   o compilador, o Python, o CMake e o Ninja. Pode demorar alguns minutos.
3. Ao terminar, você terá no Menu Iniciar um atalho chamado
   **"ESP-IDF 5.4 PowerShell"**. **É esse terminal que você vai usar** — ele já
   abre com tudo configurado. (Um PowerShell comum **não** funciona.)

### Passo 2 — Baixar este repositório

Baixe o projeto (botão *Code → Download ZIP* no GitHub, ou `git clone`) e extraia
numa pasta sem espaços/acentos no caminho, por exemplo `C:\Volante`.

### Passo 3 — Descobrir a porta COM da placa

Cada placa, quando ligada por USB, vira uma "porta COM" no Windows.

1. Ligue **uma** placa no PC com um cabo USB (cabo de **dados**, não só de carga).
2. Abra o **Gerenciador de Dispositivos** do Windows → seção **"Portas (COM e LPT)"**.
3. Anote o número que aparece, ex.: `COM3`. (Se nada aparecer, o cabo é só de
   carga ou falta o driver USB-serial — veja *Problemas comuns* abaixo.)

> Dica: ligue uma placa de cada vez para não confundir as portas.

### Passo 4 — Gravar as placas (uma de cada vez)

Abra o terminal **"ESP-IDF 5.4 PowerShell"**, entre na pasta de cada projeto e
rode os comandos. **Só uma placa ligada por vez.**

**A) Núcleo (ESP32-S3)** — ligue só a S3 e veja a porta (ex.: `COM3`):
```powershell
cd C:\Volante\codigos
idf.py -p COM3 flash
```

**B) Painel (ESP32-C3)** — desligue a S3, ligue o painel, veja a nova porta:
```powershell
cd C:\Volante\codigos_c3_painel
idf.py set-target esp32c3      # só na PRIMEIRA vez
idf.py -p COMx flash
```

**C) Botoeira (ESP32-C3)** — desligue o painel, ligue a botoeira:
```powershell
cd C:\Volante\codigos_c3_botoeira
idf.py set-target esp32c3      # só na PRIMEIRA vez
idf.py -p COMx flash
```

Troque `COM3`/`COMx` pelo número que você viu no Passo 3. O primeiro build de cada
placa demora mais (compila tudo e baixa dependências como o `esp_tinyusb`); os
seguintes são rápidos.

### Passo 5 — Conferir se funcionou

1. Para **ver as mensagens** de uma placa (útil para a calibração da direção):
   ```powershell
   idf.py -p COM3 monitor      # sair do monitor: Ctrl + ]
   ```
2. Para o **volante aparecer no jogo**: ligue a S3 pela **porta USB NATIVA** dela
   (a outra porta USB, separada da de gravação). No Windows, abra `joy.cpl` —
   o controle "Volante" deve aparecer e os eixos/botões reagir.

> Atalho: dentro do terminal do ESP-IDF, o próprio `flash.py` também recompila
> antes de gravar, se você quiser tudo num comando só:
> ```powershell
> python flash.py --build      # compila (precisa do ESP-IDF) e depois grava
> ```

### Atalho: scripts prontos (opcional)

A pasta raiz tem scripts PowerShell que automatizam build/flash/monitor
(`build_s3.ps1`, `flash_s3_nomon.ps1`, `monitor_s3.ps1`, e equivalentes para
painel/botoeira). **Atenção:** eles foram escritos com caminhos fixos do PC
original (`C:\Users\Vivas\...`). Para usá-los, abra o `.ps1` e troque esses
caminhos pelos seus (ou use os comandos `idf.py` do Passo 4 / o `flash.py`, que
não dependem desses scripts).

### Problemas comuns

| Sintoma | Causa provável / solução |
|---|---|
| A placa não aparece como porta COM | Cabo é só de carga → use cabo de **dados**. Ou falta o driver USB-serial (CH343/CH340/CP210x): instale o driver do fabricante. |
| `idf.py` não é reconhecido | Você abriu um PowerShell comum. Use o atalho **"ESP-IDF 5.4 PowerShell"**. |
| Falha ao conectar / `Failed to connect` na hora de gravar | Porta COM errada, ou outra placa ligada. Confirme a porta; deixe **só uma** placa conectada. Em algumas placas, segure o botão **BOOT** ao iniciar a gravação. |
| Gravou na S3 mas não aparece no jogo | O cabo está na porta de **gravação** (CH343). Para o jogo, use a **USB nativa** da S3. |
| Texto embaralhado no monitor | *Baud rate* errado — o monitor do `idf.py` já usa o correto; é só rodar `idf.py -p COMx monitor`. |

### Versões e dependências (requirements)

O firmware foi compilado e validado com as versões abaixo. A **única que você
precisa instalar manualmente é o ESP-IDF v5.4**; o resto é resolvido sozinho.

| Componente | Versão | Onde é fixado |
|---|---|---|
| **ESP-IDF** | **5.4** | instalado por você (Passo 1); é o limitador principal |
| `esp_tinyusb` (HID da S3) | 1.7.6 | `codigos/dependencies.lock` (resolvido de `^1.4.0` no `idf_component.yml`) |
| `tinyusb` (dep. transitiva) | 0.19.0 | `codigos/dependencies.lock` |
| Toolchain Xtensa (S3) + RISC-V (C3), CMake, Ninja | acompanham o IDF 5.4 | instalador do ESP-IDF |
| `pyserial` (leitor PC) | ≥ 3.5 | `pc_reader/requirements.txt` |

Notas importantes para reproduzir o mesmo build:

- **Componentes do IDF são travados pelo `dependencies.lock`.** Esse arquivo
  (em `codigos/`) registra o *hash* e a versão exata de cada componente baixado.
  No primeiro build, o `idf.py` lê esse lock e baixa exatamente as mesmas versões —
  você não precisa instalar `esp_tinyusb` à mão. **Não apague o `dependencies.lock`**
  se quiser o build idêntico ao testado.
- **Os projetos do painel e da botoeira não têm dependências externas** (sem
  `idf_component.yml`): usam apenas componentes nativos do ESP-IDF, então basta o
  IDF 5.4 instalado.
- **Python (só para a telemetria do PC):**
  ```powershell
  pip install -r pc_reader/requirements.txt
  ```
- **Outras versões do IDF (5.0–5.3 / 5.5+)** provavelmente compilam, mas não foram
  testadas. Se algo quebrar, use a **5.4** — foi com ela que tudo foi validado.

---

## 3. Pinagem / ligações

> Tabelas resumidas. A fonte completa e sempre atualizada é o [`LIGACOES.md`](LIGACOES.md).

### 3.1 ESP32-S3 (núcleo)

| Função | Pino S3 | Liga em |
|---|---|---|
| AS5047P CLK | GPIO4 | sensor `CLK` |
| AS5047P MISO | GPIO5 | sensor `MISO` (pull-up interno) |
| AS5047P MOSI | GPIO6 | sensor `MOSI` |
| AS5047P CSn | GPIO7 | sensor `CSn` |
| Sensor alimentação | 3V3 / GND | sensor `3V3` / `GND` |
| UART1 → Painel TX | GPIO17 | Painel C3 GPIO1 (RX) |
| UART1 → Painel RX | GPIO18 | Painel C3 GPIO0 (TX) |
| UART2 → Botoeira TX | GPIO15 | Botoeira C3 GPIO1 (RX) |
| UART2 → Botoeira RX | GPIO16 | Botoeira C3 GPIO0 (TX) |
| **Pedal acelerador** | **GPIO10** (ADC1_CH9) | botão → GND (futuro: potenciômetro) |
| **Pedal freio** | **GPIO9** (ADC1_CH8) | botão → GND (futuro: potenciômetro) |
| Botão BOOT (calibração / HID btn 16) | GPIO0 | botão onboard |
| USB HID+CDC | USB nativa (GPIO19/20) | cabo → PC |
| GND comum | GND | estrela com fonte 5 V, C3s, sensor |

SPI do sensor: **modo 1, 8 MHz**. Pedais (botão): `GPIO → botão → GND` (pull-up
interno, *active-low*). GPIO9/10 são ADC-capazes — os mesmos pontos servem para
os potenciômetros futuros (basta `PEDAIS_USE_ADC=1`).

### 3.2 ESP32-C3 Painel + display GC9A01

| Função | Pino C3 | Liga em |
|---|---|---|
| UART1 → S3 TX | GPIO0 | S3 GPIO18 (RX) |
| UART1 → S3 RX | GPIO1 | S3 GPIO17 (TX) |
| Display MOSI/SDA | GPIO3 | GC9A01 `SDA` |
| Display SCLK/SCL | GPIO4 | GC9A01 `SCL` |
| Display CS | GPIO7 | GC9A01 `CS` |
| Display DC | GPIO10 | GC9A01 `DC` |
| Relé IN1 (seta esq) | GPIO2 | módulo relé `IN1` |
| Relé IN2 (seta dir) | GPIO8 | módulo relé `IN2` |
| LED farol baixo | GPIO20 | 330 Ω → LED → GND |
| LED farol alto | GPIO21 | 330 Ω → LED → GND |
| Botão BOOT (troca tela) | GPIO9 | botão onboard |
| Alimentação | 3V3 / GND / 5V | display / relé / trilho |

GC9A01: `VCC`→3V3, `GND`→GND, `RST`→3V3 (reset por software), `BL` interno.
Display rodando a **10 MHz** (40 MHz com jumpers vira chuvisco). Relé: bobina
(`JD-VCC`) puxa do **5 V** do trilho; lógica (`VCC`) em 3V3, jumper `VCC↔JD-VCC`
removido.

### 3.3 ESP32-C3 Botoeira

| Função | Pino C3 | Liga em |
|---|---|---|
| UART → S3 TX | GPIO0 | S3 GPIO16 (RX2) |
| UART → S3 RX | GPIO1 | S3 GPIO15 (TX2) |
| Botão seta esquerda (bit 0) | GPIO5 | botão → GND |
| Botão seta direita (bit 1) | GPIO6 | botão → GND |
| Botão pisca-alerta (bit 2) | GPIO7 | botão → GND |
| Botão buzina (bit 3) | GPIO10 | botão → GND |
| Botão farol alto (bit 4) | GPIO20 | botão → GND |
| Botão farol baixo (bit 5) | GPIO21 | botão → GND |
| Botão limpador (bit 6) | GPIO2 ⚠️ | botão → GND |
| Botão freio-motor (bit 7) | GPIO8 ⚠️ | botão → GND |
| Joystick VRx | GPIO3 (ADC) | cursor X |
| Joystick VRy | GPIO4 (ADC) | cursor Y |
| Joystick VCC / GND | **3.3V** / GND | ⚠️ nunca 5 V |
| Alimentação | 5V / GND | trilho |

Botões *active-low* (pull-up interno). ⚠️ GPIO2/8 são *strapping*: não segure os
botões de limpador/freio-motor durante a energização. Console na **USB-JTAG**
(libera GPIO20/21 para botões).

---

## 4. Estrutura do firmware

### 4.1 S3 — `codigos/`

Projeto ESP-IDF (target `esp32s3`). O componente `main/` é organizado em
**sub-pastas por subsistema**. O `CMakeLists.txt` lista cada `.c` por caminho
relativo e adiciona cada sub-pasta ao `INCLUDE_DIRS`, então os `#include`
continuam por nome simples (`#include "calib.h"`).

```
codigos/main/
├── main.c              ← app_main + tasks (HID 50 Hz, telemetria, log de ângulo)
├── CMakeLists.txt
├── idf_component.yml   ← dependência: espressif/esp_tinyusb (HID)
├── direcao/            ← volante (sensor de ângulo + calibração)
│   ├── as5047.c/.h     ← driver SPI do AS5047P (encoder magnético 14 bits)
│   └── calib.c/.h      ← calibração da direção pelo BOOT + multi-turn + NVS
├── pedais/
│   └── pedais.c/.h     ← acelerador/freio (botões hoje, potenciômetros depois)
├── botoeira/
│   └── botoeira_rx.c/.h← recebe o estado da botoeira (msg 0x040 via UART2)
├── painel/
│   └── telemetry.c/.h  ← imagem de telemetria em RAM + reemissão p/ o painel
├── transporte/
│   ├── transport.c/.h  ← framing UART (SOF+ID+LEN+PAYLOAD+SEQ+CRC8)
│   └── transport_can.c ← backend CAN/TWAI (GUARDADO, fora do build até a etapa CAN)
└── usb/
    └── usb_hid.c/.h    ← gamepad USB HID pela USB nativa (TinyUSB)
```

| Módulo | Função |
|---|---|
| **`main.c`** | Inicia tudo (NVS, transporte, telemetria, sensor, calibração, pedais, botoeira, HID). `hid_task` a 50 Hz monta o relatório do gamepad; `reemit_task` reenvia telemetria ao painel a cada 50 ms; `angle_task` loga o ângulo a 1 Hz. |
| **`direcao/as5047`** | Driver SPI do encoder magnético AS5047P (absoluto, 14 bits/volta). Valida paridade e *error-flag*; retorna `-1` se não houver sensor (evita travar o eixo). |
| **`direcao/calib`** | Calibração da direção pelo botão **BOOT** e contagem *multi-turn* (o encoder é absoluto em 1 volta; o volante gira mais que isso). Salva esquerda/direita/centro em **NVS** (sobrevive ao reset). |
| **`pedais/pedais`** | Acelerador e freio na própria S3. Macro **`PEDAIS_USE_ADC`** escolhe entre 2 botões (agora) e 2 potenciômetros (futuro), nos mesmos pinos. |
| **`botoeira/botoeira_rx`** | Recebe por UART2 o estado da botoeira (8 botões + joystick de visão) e disponibiliza para o `hid_task`. |
| **`painel/telemetry`** | Guarda a telemetria do jogo em RAM (thread-safe) e a reemite serializada para o painel C3. |
| **`transporte/transport`** | Camada de comunicação entre placas (framing UART com CRC8). Mesmo formato lógico que o CAN futuro. |
| **`transporte/transport_can`** | Versão CAN/TWAI do transporte. Pronta mas fora do build até a migração. |
| **`usb/usb_hid`** | Faz a S3 aparecer no PC como gamepad via USB nativa (TinyUSB). |

**Mapa dos eixos/botões HID:**

| Entrada | Eixo / botão HID |
|---|---|
| Volante (AS5047P calibrado) | eixo **X** |
| Acelerador (pedais) | eixo **Y** |
| Freio (pedais) | eixo **Rx** |
| Joystick de visão (botoeira) | eixos **Z** / **Rz** |
| Botões da botoeira | botões **1–8** |
| Botão BOOT (fora da calibração) | botão **16** |

**Calibração da direção (pelo botão BOOT):**

1. **Segure BOOT por >1,5 s** (parado) para entrar no modo calibração.
2. Gire **totalmente para a esquerda** e dê um **clique curto** no BOOT.
3. Gire **totalmente para a direita** e dê um **clique curto**.
4. **Centre** o volante e dê um **clique curto** — os 3 pontos são salvos em NVS.

Segurar BOOT por >1,5 s durante o processo **cancela** (mantém a calibração
anterior). Sem calibração, o padrão é 900° *lock-to-lock*. O log de todo o
processo aparece no monitor serial.

### 4.2 Painel — `codigos_c3_painel/`

ESP32-C3 com display redondo **GC9A01**. Mostra velocidade, RPM, temperatura,
combustível e navegação em 3 telas (troca pelo botão BOOT). Aciona 2 relés
(tic-tac do pisca) e 2 LEDs de farol a partir da telemetria recebida da S3.

| Módulo | Função |
|---|---|
| `main.c` | Inicia display/oled/relés/luzes e a recepção; troca de tela; decodifica telemetria. |
| `display.c/.h` | Driver bare-metal do GC9A01 (framebuffer RGB565, fonte própria, 3 views). |
| `transport_rx.c/.h` | Recepção UART (mesmo framing da S3). |
| `relay.c/.h` | 2 relés do pisca (um por lado), em fase com os indicadores da tela. |
| `lights.c/.h` | LEDs de farol baixo/alto. |
| `oled.c/.h` | OLED 0.42 embutido (bateria de combustível). |
| `diag.c` | Diagnóstico/bring-up. |
| `leds.c/.h` | Desativado nesta placa (sem GPIO livre); mantido para reuso. |
| `transport_rx_can.c` | Versão CAN, guardada para a migração. |

### 4.3 Botoeira — `codigos_c3_botoeira/`

ESP32-C3 com 8 botões e um joystick analógico de visão. Envia o estado para a S3
(mensagem `0x040`) por UART.

| Módulo | Função |
|---|---|
| `main.c` | Lê entradas e envia `0x040` on-change + heartbeat. |
| `inputs.c/.h` | 8 botões (ativo-baixo) → bitmap; joystick por ADC → eixos. |
| `transport_tx.c/.h` | Envio UART (mesmo framing). Só TX. |

---

## 5. Leitor de telemetria do PC — `pc_reader/`

`telemetry_reader.py` lê a *shared memory* do plugin SCS SDK do ETS2 e envia a
telemetria para a S3 por serial, no mesmo framing do firmware. (Requer o plugin
SCS SDK instalado na pasta de plugins do ETS2.)

```powershell
pip install -r pc_reader/requirements.txt
python pc_reader/telemetry_reader.py --port COM3            # ETS2 real
python pc_reader/telemetry_reader.py --port COM3 --sim      # dados sintéticos (sem jogo)
```

---

## 6. Protocolo de mensagens

Framing UART (idêntico ao CAN futuro):

```
SOF(0xAA) | ID_LO | ID_HI | LEN | PAYLOAD(0..8) | SEQ | CRC8
```
`CRC8` = XOR acumulado sobre ID+LEN+PAYLOAD+SEQ.

| ID | Mensagem | Sentido |
|---|---|---|
| `0x040` | Botoeira: `[botões u16][view_x i8][view_y i8]` | Botoeira → S3 |
| `0x100` | Telemetria painel (velocidade, RPM, etc.) | S3 → Painel |
| `0x101` | Telemetria LEDs/telltales | S3 → Painel |
| `0x102` | Telemetria navegação | S3 → Painel |

---

## 7. Estado das etapas

- ✅ **Etapa 1 — Painel**: concluída e validada no ETS2 real.
- ✅ **Etapa 2 — Volante HID**: firmware completo (direção + calibração + pedais
  + botoeira + painel), gravado na S3. Falta validar em hardware no `joy.cpl`.
- ⏳ **Pedais**: provisórios como 2 botões na S3; viram potenciômetros nas molas
  depois (trocar `PEDAIS_USE_ADC`).
- ⏳ **CAN/TWAI**: migrar quando entrar um 3º módulo satélite e chegarem os
  `SN65HVD230` (código já guardado).
- ⏳ **FFB**: adiado; arquitetura já reserva o núcleo na S3. Centralização atual
  por mola de torção.
