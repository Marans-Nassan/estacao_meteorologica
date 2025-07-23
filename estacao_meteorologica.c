#include <stdio.h>
#include <math.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "lib/ssd1306.h"
#include "lib/matriz.h"
#include "lib/font.h"
#include "lib/bmp280.h"
#include "lib/aht20.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"
#include "lwipopts.h"
#include "html_body.h"
#include "pico/cyw43_arch.h"


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
#define buzzer_a 21
#define rede "S.F.C 2"
#define senha "857aj431"
char ip_adr[20];


ssd1306_t ssd;
AHT20_Data data;
int32_t raw_temp_bmp;
int32_t raw_pressure;
struct bmp280_calib_param params;
double altitude = 0;
int temperature = 0;
int nivel_atual = 0;
float humidity = 0;
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

uint8_t slice = 0;
typedef struct {
    float dc;           // wrap value do PWM
    float div;          // divisor de clock
    bool alarm_state;   // estado da sirene
    bool alarm_react;   // permite reativar alarme
    alarm_id_t alarm_pwm;
} pwm_struct;
pwm_struct pw = {7812.5, 32.0, false, true, 0};

struct http_state{
    char response[8192];
    size_t len;
    size_t sent;
};

typedef struct {
    int min;
    int max;
    int offset;
} limites_t;

static limites_t limites ={0, 100, 0};

#define interrupcoes(gpio) gpio_set_irq_enabled_with_callback(gpio, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

void core1(void);
void init_leds(void);
void init_botoes(void);
void init_i2c0(void);
void init_i2c1(void);
void init_oled(void);
int16_t init_internet(void);
uint8_t endereco_ip(void);
void init_bmp280(void);
void init_aht20(void);
void pwm_setup(void);   // Configura o PWM
void pwm_on(uint8_t duty_cycle); // Ativa o pwm.
void pwm_off(void); // Desliga o pwm e o buzzer afim de evitar qualquer vazamento no periféricovoid gpio_irq_handler(uint gpio, uint32_t events);
void gpio_irq_handler(uint gpio, uint32_t events);
double calcular_altitude(double pressure);
int64_t variacao_temp(alarm_id_t, void *user_data);
static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
void parse_post_params(char *req, limites_t *limites);
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err);
static void start_http_server(void);

int main(){
    stdio_init_all();
    multicore_launch_core1(core1);
    init_leds();
    init_botoes();
    init_i2c0();
    init_bmp280();
    init_aht20();
    matriz_init();
    pwm_setup();
    init_internet();
    interrupcoes(bot_a);
    start_http_server();

    while (true) {
        // Leitura do BMP280
        bmp280_read_raw(i2c_port_a, &raw_temp_bmp, &raw_pressure);
        temperature = bmp280_convert_temp(raw_temp_bmp, &params);
        int32_t pressure = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params);

        // Checagem BMP280: valores plausíveis
        if (temperature < -4000 || temperature > 8500 || pressure < 30000 || pressure > 110000) {
            printf("Erro na leitura do BMP280!\n");
            c.error2 = false;
            sprintf(str_tmp1, "---");
            sprintf(str_alt, "---");
        } else {
            c.error2 = true;
            altitude = calcular_altitude(pressure);
            printf("Pressao = %.3f kPa\n", pressure / 1000.0);
            printf("Temperatura BMP: = %.2f C\n", temperature / 100.0);
            printf("Altitude estimada: %.2f m\n", altitude);
            sprintf(str_tmp1, "%.1fC", temperature / 100.0);
            sprintf(str_alt, "%.0fm", altitude);
        }

        // Leitura do AHT20
        if (aht20_read(i2c_port_a, &data)){
            printf("Temperatura AHT: %.2f C\n", data.temperature);
            printf("Umidade: %.2f %%\n\n\n", data.humidity);
            c.error3 = true;
        }
        else{
            printf("Erro na leitura do AHT10!\n\n\n");
            c.error3 = false;
        }
        if(!c.error2 && !c.error3) c.error1 = true;
        else c.error1 = false;



        sprintf(str_tmp1, "%.1fC", temperature / 100.0);  
        sprintf(str_alt, "%.0fm", altitude);  
        sprintf(str_tmp2, "%.1fC", data.temperature);  
        sprintf(str_umi, "%.1f%%", data.humidity);   
        matriz(0, c.error2, c.error3);
        matriz_x(c.error1, 0, 0);
        if ((temperature < 1000 || temperature > 4000) && !pw.alarm_state && pw.alarm_react) {
            pw.alarm_pwm = add_alarm_in_ms(3000, variacao_temp, NULL, false);
            pw.alarm_state = true;
            pw.alarm_react = false;
        }
        if (temperature >= 1000 && temperature <= 4000) {
            if (pw.alarm_state) {
                pwm_off();
                pw.alarm_state = false;
                cancel_alarm(pw.alarm_pwm);
            }
            pw.alarm_react = true;
        }
        nivel_atual = temperature;
        cyw43_arch_poll();
        sleep_ms(1000); // Aguarda 1 segundo entre leituras
    }
}

void core1(void){
    sleep_ms(5000);
    bool cor = true;
    init_i2c1();
    init_oled();
    
    while(true){
        ssd1306_fill(&ssd, !cor);                          
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);      
        ssd1306_line(&ssd, 3, 25, 123, 25, cor);           
        ssd1306_line(&ssd, 3, 37, 123, 37, cor);            
        ssd1306_draw_string(&ssd, " Meteorologia", 8, 6);  
        ssd1306_draw_string(&ssd, ip_adr, 20, 16);
        ssd1306_draw_string(&ssd, "BMP280  AHT10", 10, 28); 
        ssd1306_line(&ssd, 63, 25, 63, 60, cor);            
        
        if(c.error2) {
            ssd1306_draw_string(&ssd, str_tmp1, 14, 41);             
            ssd1306_draw_string(&ssd, str_alt, 14, 52);             
        } else {
            ssd1306_draw_string(&ssd, "---", 14, 41);
            ssd1306_draw_string(&ssd, "---", 14, 52);
        }
        
        if(c.error3) {
            ssd1306_draw_string(&ssd, str_tmp2, 73, 41);             
            ssd1306_draw_string(&ssd, str_umi, 73, 52);
        } else {
            ssd1306_draw_string(&ssd, "---", 73, 41);
            ssd1306_draw_string(&ssd, "---", 73, 52);
        }
        
        ssd1306_send_data(&ssd);                            
        sleep_ms(1000); 
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

int16_t init_internet(void){
    
    if (cyw43_arch_init()) {
        printf("Falha ao inicializar Wi-Fi\n");
        gpio_put(red_led, 1);
        return -1;
    } else gpio_put(red_led, 0);

    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(rede, senha, CYW43_AUTH_WPA2_AES_PSK, 20000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        strcpy(ip_adr, "SEM CONEXAO");
        gpio_put(blue_led, 1);
        cyw43_arch_deinit();
        return -1;
    } else gpio_put(blue_led, 0);

    if(endereco_ip()) gpio_put(green_led, 1);
    else gpio_put(green_led, 0);
    return 0;
}

uint8_t endereco_ip(void){
        cyw43_arch_lwip_begin();
        struct netif *netif = &cyw43_state.netif[0];
        snprintf(ip_adr, sizeof(ip_adr), "%s", ip4addr_ntoa(netif_ip4_addr(netif)));
        cyw43_arch_lwip_end();
        return 1;
}

void pwm_setup(void) {
    gpio_set_function(buzzer_a, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(buzzer_a);
    pwm_set_clkdiv(slice, pw.div);
    pwm_set_wrap(slice, pw.dc);
    pwm_set_enabled(slice, false);
}

void pwm_on(uint8_t duty_cycle) {
    gpio_set_function(buzzer_a, GPIO_FUNC_PWM);
    pwm_set_gpio_level(buzzer_a, (uint16_t)((pw.dc * duty_cycle) / 100));
    pwm_set_enabled(slice, true);
}

void pwm_off(void) {
    pwm_set_enabled(slice, false);
    gpio_set_function(buzzer_a, GPIO_FUNC_SIO);
    gpio_put(buzzer_a, 0);
}

void gpio_irq_handler(uint gpio, uint32_t events){
    uint64_t current_time = to_ms_since_boot(get_absolute_time());
    static uint64_t last_time_a = 0, last_time_b = 0;
    if(gpio == bot_a && (current_time - last_time_a > 300)) {
        pwm_off();
        pw.alarm_state = false;
        pw.alarm_react = false;
        cancel_alarm(pw.alarm_pwm);
        last_time_a = current_time;
    }
}

double calcular_altitude(double pressure){
    return 44330.0 * (1.0 - pow(pressure / press_nivel_mar, 0.1903));
}

int64_t variacao_temp(alarm_id_t, void *user_data){
    pwm_on(50);
    return 1;
}

static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len){
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;

    if (hs->sent >= hs->len) {
        tcp_close(tpcb);
        free(hs);
        return ERR_OK;
    }

    size_t remaining = hs->len - hs->sent;
    size_t chunk = remaining > 1024 ? 1024 : remaining;
    err_t err = tcp_write(tpcb, hs->response + hs->sent, chunk, TCP_WRITE_FLAG_COPY);
    if (err == ERR_OK) {
        tcp_output(tpcb);
    } else {
        printf("Erro ao enviar chunk restante: %d\n", err);
    }

    return ERR_OK;
}

static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err){
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}


void parse_post_params(char *req, limites_t *limites) {
    char *body = strstr(req, "\r\n\r\n");
    if (body) {
        body += 4;
        char *token = strtok(body, "&");
        while (token) {
            if (strncmp(token, "min=", 4) == 0) {
                limites->min = atoi(token + 4);
            } 
            else if (strncmp(token, "max=", 4) == 0) {
                limites->max = atoi(token + 4);
            } 
            else if (strncmp(token, "offset=", 7) == 0) {
                limites->offset = atoi(token + 7);
            }
            token = strtok(NULL, "&");
        }
    }
}

static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    tcp_recved(tpcb, p->len);

    // Converta o payload em string C
    char *req = (char *)p->payload;

    struct http_state *hs = malloc(sizeof *hs);
    if (!hs) {
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_MEM;
    }
    memset(hs, 0, sizeof *hs);

    if (strstr(req, "GET /api/data")) {
        int aplicado = nivel_atual + limites.offset;
        if (aplicado < 0) aplicado = 0;
        if (aplicado > 100) aplicado = 100;

        char json_payload[256];
        int json_len = snprintf(json_payload, sizeof json_payload,
            "{\"min\":%d,\"max\":%d,\"offset\":%d,"
            "\"nivel_atual\":%d,"
            "\"temp_bmp\":%.1f,"
            "\"altitude\":%.1f,"
            "\"temp_aht\":%.1f,"
            "\"humidity\":%.1f}",
            limites.min,
            limites.max,
            limites.offset,
            aplicado,
            temperature / 100.0,   // Temperatura do BMP280 em °C
            altitude,               // Altitude em metros
            data.temperature,       // Temperatura do AHT20 em °C
            data.humidity          // Umidade do AHT20 em %
        );

        printf("Enviando JSON: %s\n", json_payload); // Debug
        
        hs->len = snprintf(hs->response, sizeof hs->response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            json_len, json_payload
        );

        tcp_arg(tpcb, hs);
        tcp_sent(tpcb, http_sent);
        tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
        pbuf_free(p);
        return ERR_OK;
    }

    if (strncmp(req, "GET /", 5) == 0) {
        extern const char HTML_BODY[];
        size_t L = strlen(HTML_BODY);
        hs->len = snprintf(hs->response, sizeof hs->response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %u\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            (unsigned)L, HTML_BODY
        );

        tcp_arg(tpcb, hs);
        tcp_sent(tpcb, http_sent);
        tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
        pbuf_free(p);
        return ERR_OK;
    }

    if (strstr(req, "POST /api/limites")) {
        parse_post_params(req, &limites);

        const char *resp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: 2\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{}";

        tcp_write(tpcb, resp, strlen(resp), TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
        pbuf_free(p);
        free(hs);
        return ERR_OK;
    }

    {
        const char *notf =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 9\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Not Found";

        tcp_write(tpcb, notf, strlen(notf), TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
        pbuf_free(p);
        free(hs);
        return ERR_OK;
    }
}   

static void start_http_server(void){
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb)
    {
        printf("Erro ao criar PCB TCP\n");
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP rodando na porta 80...\n");
}