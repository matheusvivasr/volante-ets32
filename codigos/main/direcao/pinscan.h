#pragma once

/*
 * pinscan.h - Varredura de bring-up p/ descobrir a ORDEM dos fios do AS5047P.
 *
 * Testa as 24 permutacoes de (CLK, MISO, MOSI, CSn) sobre os 4 GPIOs candidatos
 * e identifica qual mapeamento devolve frames VALIDOS do sensor. Depois trava na
 * ordem vencedora e transmite o angulo ao vivo (TAG "angle", lido pelo
 * angle_monitor.py). NAO retorna -- e modo de bancada.
 *
 * Uso: defina AS5047_PINSCAN=1 em main.c, grave, leia o console (angle_monitor
 * --raw), anote a ordem MELHOR, ajuste as5047.h, e volte AS5047_PINSCAN=0.
 *
 * Se os fios foram p/ pinos DIFERENTES de G4..G7, edite CAND[] em pinscan.c.
 */

void as5047_pinscan(void);
