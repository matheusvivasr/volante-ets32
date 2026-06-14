#pragma once
#include <stdint.h>
#include <stdbool.h>

/*
 * calib.h – calibracao do eixo de direcao do volante (S3).
 *
 * O AS5047P e absoluto em UMA volta (0..16383). Como o volante pode girar mais
 * de 360 graus (lock-to-lock tipico de caminhao ~900 graus), este modulo conta
 * as voltas (multi-turn) detectando o "wrap" 16383<->0 a 50 Hz, formando um
 * angulo CONTINUO. A origem do continuo e o angulo lido no boot — assumimos que
 * a MOLA DE TORCAO deixa o volante CENTRADO ao ligar (decisao 2026-06-10), logo
 * a origem e consistente entre reinicios.
 *
 * Calibracao pelo botao BOOT (GPIO0, ativo-baixo):
 *   - SEGURAR BOOT >1.5s (parado, em IDLE) -> entra em modo calibracao.
 *   - Gire TOTALMENTE p/ ESQUERDA  + clique curto BOOT -> captura batente esq.
 *   - Gire TOTALMENTE p/ DIREITA   + clique curto BOOT -> captura batente dir.
 *   - CENTRE o volante              + clique curto BOOT -> captura centro + salva.
 *   - SEGURAR BOOT >1.5s em qualquer passo -> cancela (mantem calibracao antiga).
 * Os 3 pontos (esq/dir/centro) sao gravados em NVS e sobrevivem ao reset.
 *
 * Fora do modo calibracao, o clique curto do BOOT continua valendo como o
 * botao 16 do gamepad (teste). Tudo e logado pelo console (CDC/CH343).
 */

void calib_init(void);

// Atualiza o multi-turn com o angulo bruto (0..16383, ou <0 = sem sensor) e
// devolve a direcao mapeada -127..127 (0 = centro). Chamar a 50 Hz no hid_task.
int8_t calib_steering(int32_t raw);

// Maquina de estados do botao BOOT (debounce + curto/longo). Chamar a 50 Hz,
// depois de calib_steering() (usa o ultimo angulo continuo capturado).
void calib_button_poll(void);

// true enquanto algum passo de calibracao estiver em andamento.
bool calib_is_calibrating(void);

// true quando o BOOT deve valer como botao 16 do gamepad (so em IDLE).
bool calib_hid_button(void);
