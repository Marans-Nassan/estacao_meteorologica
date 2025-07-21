#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "lib/ssd1306.h"
#include "lib/matriz.h"
#include "lib/font.h"
#include "lib/bmp280.h"
#include "lib/aht20.h"
#include "lwipopts.h"

#define bot_a 5
#define bot_b 6
#define green_led 11
#define blue_led 12
#define red_led 13

#define interrupcoes(gpio) gpio_set_irq_enabled_with_callback(gpio, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);


void core1(void);
void init_leds(void);
void init_botoes(void);
void gpio_irq_handler(uint gpio, uint32_t events);

int main(){
    stdio_init_all();
    multicore_launch_core1(core1);
    init_leds();
    init_botoes();

    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}

void core1(void){
    while(true){

    }
}

void init_leds(void){
    for (uint8_t leds = 11; leds < 14; leds++){
        gpio_init(leds);
        gpio_set_dir(leds, GPIO_OUT);
        gpio_put(leds, 0);
    }
}

void init_botoes(void){
    for (uint8_t botoes = 5; botoes < 7; botoes++){
        gpio_init(botoes);
        gpio_set_dir(botoes, GPIO_IN);
        gpio_pull_up(botoes);
    }
}

void gpio_irq_handler(uint gpio, uint32_t events){
    
}