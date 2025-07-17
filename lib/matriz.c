#include "matriz.h"
#include "ws2812.pio.h"


typedef struct pixeis {
    uint8_t cor1, cor2, cor3;     // cores no formato GRB
} pixeis;

pixeis leds [matriz_led_pins];    // Array de LEDs da matriz

PIO pio;                          // PIO usado para controlar WS2812
uint sm;                          // State machine do PIO

void matriz_init(void){

    uint offset = pio_add_program(pio0, &ws2812_program);
    pio = pio0;
    
    sm = pio_claim_unused_sm(pio, false);
        if(sm < 0){
            pio = pio1;
            sm = pio_claim_unused_sm(pio, true);
        }
    
    ws2812_program_init(pio, sm, offset, matriz_led, 800000.f);
}

void setled(const uint index, const uint8_t r, const uint8_t g, const uint8_t b){
    leds[index].cor1 = g;
    leds[index].cor2 = r;
    leds[index].cor3 = b;
}

void display(){
    for (uint i = 0; i < matriz_led_pins; ++i) {
        pio_sm_put_blocking(pio, sm, leds[i].cor1);
        pio_sm_put_blocking(pio, sm, leds[i].cor2);
        pio_sm_put_blocking(pio, sm, leds[i].cor3);
        }
    sleep_us(100); 
}

void matriz(uint8_t r, uint8_t g, uint8_t b){
    const uint8_t digit_leds[] = {24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    size_t count = sizeof(digit_leds)/sizeof (digit_leds[0]);
        for (size_t i = 0; i < count; ++i) {
            setled(digit_leds[i], r, g, b); // RGB
        }
        display();    
}