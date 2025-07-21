#include <stdio.h>
#include <math.h>
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

#define i2c_port_a i2c0
#define i2c_sda_a 0
#define i2c_scl_a 1
#define press_nivel_mar 101325.0
#define bot_a 5
#define bot_b 6
#define green_led 11
#define blue_led 12
#define red_led 13
#define i2c_port_b i2c1
#define i2c_sda_b 14
#define i2c_scl_b 15
#define i2c_endereco 0x3c
ssd1306_t ssd;
AHT20_Data data;
int32_t raw_temp_bmp;
int32_t raw_pressure;
struct bmp280_calib_param params;
char str_tmp1[5]; 
char str_alt[5];   
char str_tmp2[5];  
char str_umi[5]; 
typedef struct checks{
    bool error1;
    bool error2;
    bool error3;
} checks; 
checks c = {false, false, false};

#define interrupcoes(gpio) gpio_set_irq_enabled_with_callback(gpio, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);


void core1(void);
void init_leds(void);
void init_botoes(void);
void init_i2c0(void);
void init_i2c1(void);
void init_oled(void);
void init_bmp280(void);
void init_aht20(void);
void gpio_irq_handler(uint gpio, uint32_t events);
double calcular_altitude(double pressure);

int main(){
    stdio_init_all();
    multicore_launch_core1(core1);
    init_leds();
    init_botoes();
    init_i2c0();
    init_bmp280();
    init_aht20();

    while (true) {
        // Leitura do BMP280
        bmp280_read_raw(i2c_port_a, &raw_temp_bmp, &raw_pressure);
        int32_t temperature = bmp280_convert_temp(raw_temp_bmp, &params);
        int32_t pressure = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params);

        // Cálculo da altitude
        double altitude = calcular_altitude(pressure);

        printf("Pressao = %.3f kPa\n", pressure / 1000.0);
        printf("Temperatura BMP: = %.2f C\n", temperature / 100.0);
        printf("Altitude estimada: %.2f m\n", altitude);

        // Leitura do AHT20
        if (aht20_read(i2c_port_a, &data)){
            printf("Temperatura AHT: %.2f C\n", data.temperature);
            printf("Umidade: %.2f %%\n\n\n", data.humidity);
            c.error1 = true;
        }
        else{
            printf("Erro na leitura do AHT10!\n\n\n");
            c.error1 = false;
        }


        sprintf(str_tmp1, "%.1fC", temperature / 100.0);  
        sprintf(str_alt, "%.0fm", altitude);  
        sprintf(str_tmp2, "%.1fC", data.temperature);  
        sprintf(str_umi, "%.1f%%", data.humidity);     
    }
}

void core1(void){
    bool cor = true;
    init_i2c1();
    init_oled();
    while(true){
        ssd1306_fill(&ssd, !cor);                          
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);      
        ssd1306_line(&ssd, 3, 25, 123, 25, cor);           
        ssd1306_line(&ssd, 3, 37, 123, 37, cor);            
        ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6);  
        ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 16);
        ssd1306_draw_string(&ssd, "BMP280  AHT10", 10, 28); 
        ssd1306_line(&ssd, 63, 25, 63, 60, cor);            
        ssd1306_draw_string(&ssd, str_tmp1, 14, 41);             
        ssd1306_draw_string(&ssd, str_alt, 14, 52);             
        ssd1306_draw_string(&ssd, str_tmp2, 73, 41);             
        ssd1306_draw_string(&ssd, str_umi, 73, 52);           
        ssd1306_send_data(&ssd);                            

        sleep_ms(50);
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

void init_i2c0(void){
    i2c_init(i2c_port_a, 400*1000);
    for(uint8_t init_i2c0 = 0 ; init_i2c0 < 2; init_i2c0 ++){
        gpio_set_function(init_i2c0, GPIO_FUNC_I2C);
        gpio_pull_up(init_i2c0);
    }
}

void init_i2c1(void){
    i2c_init(i2c_port_b, 400*1000);
    for(uint8_t init_i2c1 = 14 ; init_i2c1 < 16; init_i2c1 ++){
        gpio_set_function(init_i2c1, GPIO_FUNC_I2C);
        gpio_pull_up(init_i2c1);
    }
}

void init_oled(void){
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, i2c_endereco, i2c_port_b);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
}

void init_bmp280(void){
    bmp280_init(i2c_port_a);
    bmp280_get_calib_params(i2c_port_a, &params);
}

void init_aht20(void){
    aht20_reset(i2c_port_a);
    aht20_init(i2c_port_a);
}

void gpio_irq_handler(uint gpio, uint32_t events){
    
}

double calcular_altitude(double pressure){
    return 44330.0 * (1.0 - pow(pressure / press_nivel_mar, 0.1903));
}