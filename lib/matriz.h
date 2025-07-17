#ifndef MATRIZ_H
#define MATRIZ_H

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"

#define matriz_led 7
#define matriz_led_pins 25

/* pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)
Adicionar a linha acima ao cmakelists.txt para gerar o cabeçalho do PIO */

void matriz_init(void); // Inicializa a matriz de LED WS2812
void setled(const uint index, const uint8_t r, const uint8_t g, const uint8_t b); // Define a cor de um LED específico no array
void display(); // Envia os valores de cor armazenados no array para os LEDs fisicamente
void matriz(uint8_t r, uint8_t g, uint8_t b); // Acende todos os LEDs da matriz com a mesma cor

#endif 