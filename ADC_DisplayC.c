#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "pico/bootrom.h"
#include "ws2812.pio.h"

// Definições
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define JOYSTICK_X_PIN 27 // Pino do eixo X do joystick
#define Botao_A 5
#define Botao_B 6
#define LED_R 13
#define LED_G 11
#define LED_B 12
#define BUZZER_PIN 21 // Pino do buzzer

#define IS_RGBW false
#define NUM_LEDS 25
#define WS2812_PIN 7

volatile bool system_active = false;
bool cor = true;
bool buzzer_ativo = false; // Estado do buzzer
ssd1306_t ssd; // Inicializa a estrutura do display

// Variáveis globais para a matriz de LEDs
uint32_t buffer_leds[NUM_LEDS] = {0};

// Função para enviar um pixel para a matriz de LEDs
static inline void enviar_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
    sleep_us(80); // Pequeno delay para estabilidade
}

// Função para converter RGB para GRB com redução de brilho
static inline uint32_t converter_rgb_para_grb(uint8_t r, uint8_t g, uint8_t b) {
    // Reduz o brilho em 95% (multiplica por 0.05)
    r = (uint8_t)(r * 0.05);
    g = (uint8_t)(g * 0.05);
    b = (uint8_t)(b * 0.05);
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

// Configura a matriz WS2812
void configurar_matriz_leds() {
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);
}

// Função para limpar a matriz de LEDs (todos desligados)
void limpar_matriz_leds() {
    for (int i = 0; i < NUM_LEDS; i++) {
        buffer_leds[i] = 0; // Define o valor GRB como 0 (desligado)
    }
    for (int i = 0; i < NUM_LEDS; i++) {
        enviar_pixel(buffer_leds[i]); // Envia o valor para cada LED
    }
}

// Função para desenhar um quadrado 3x3 na matriz de LEDs
void desenhar_quadrado_3x3(uint8_t r, uint8_t g, uint8_t b) {
    // Limpa o buffer de LEDs
    for (int i = 0; i < NUM_LEDS; i++) {
        buffer_leds[i] = 0;
    }

    // Define os LEDs do quadrado 3x3 no centro da matriz 5x5
    int leds_quadrado[] = {6, 7, 8, 11, 12, 13, 16, 17, 18};
    for (int i = 0; i < 9; i++) {
        buffer_leds[leds_quadrado[i]] = converter_rgb_para_grb(r, g, b);
    }

    // Envia os pixels para a matriz de LEDs
    for (int i = 0; i < NUM_LEDS; i++) {
        enviar_pixel(buffer_leds[i]);
    }
}

// Função para desenhar um retângulo com borda espessa
void draw_thick_rect(ssd1306_t *ssd, uint8_t x, uint8_t y, uint8_t width, uint8_t height, bool color, uint8_t thickness) {
    for (uint8_t t = 0; t < thickness; t++) {
        ssd1306_rect(ssd, x + t, y + t, width - 2 * t, height - 2 * t, color, !color);
    }
}

// Função para imprimir mensagem no display
void print_message(const char *message, uint8_t x, uint8_t y) {
    ssd1306_fill(&ssd, !cor); // Limpa o display
    draw_thick_rect(&ssd, 3, 3, 122, 58, cor, 2); // Desenha um retângulo com borda espessa
    ssd1306_draw_string(&ssd, message, x, y); // Desenha a mensagem
    ssd1306_send_data(&ssd); // Atualiza o display
}

// Função de tratamento de interrupções dos botões
void irq_handler(uint gpio, uint32_t events) {
    static absolute_time_t last_interrupt_time = {0};
    absolute_time_t now = get_absolute_time();

    // Debounce: ignora interrupções muito próximas no tempo
    if (absolute_time_diff_us(last_interrupt_time, now) < 200000) return;
    last_interrupt_time = now;

    if (gpio == Botao_A) {
        system_active = true;
        cor = !cor; // Alterna a cor do retângulo
        print_message("SISTEMA ATIVO", 15, 30);
    } else if (gpio == Botao_B) {
        reset_usb_boot(0, 0);
    }
}

// Função para ajustar o nível de PWM dos LEDs
void set_led_color(uint16_t r, uint16_t g, uint16_t b) {
    pwm_set_gpio_level(LED_R, r);
    pwm_set_gpio_level(LED_G, g);
    pwm_set_gpio_level(LED_B, b);
}

// Função para ativar o buzzer com PWM
void ativar_buzzer() {
    // Configura o PWM para o buzzer
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    uint channel = pwm_gpio_to_channel(BUZZER_PIN);

    // Configura a frequência do PWM para 1 kHz
    float div = 125.0f; // Divisor para 1 kHz (125 MHz / 125 = 1 MHz)
    pwm_set_clkdiv(slice_num, div);
    pwm_set_wrap(slice_num, 999); // 1 MHz / 1000 = 1 kHz
    pwm_set_chan_level(slice_num, channel, 200); // 10% de ciclo de trabalho
    pwm_set_enabled(slice_num, true);

    buzzer_ativo = true;
}

// Função para desativar o buzzer
void desativar_buzzer() {
    // Desativa o PWM e define o pino como baixo
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_set_enabled(slice_num, false);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    gpio_put(BUZZER_PIN, 0);

    buzzer_ativo = false;
}

// Função para verificar o valor do ADC e atualizar os LEDs e o display
void check_adc_value(uint16_t adc_value) {
    // Definindo os valores mínimos e máximos do ADC
    uint16_t adc_min = 17;
    uint16_t adc_max = 4095;
    float corrente_max = 20.0; // Corrente máxima medida (20A)
    
    // Calculando a corrente
    float corrente = ((float)(adc_value - adc_min) / (adc_max - adc_min)) * corrente_max;
    char mensagem[50]; // Buffer para a mensagem formatada
    
    if ((adc_value >= 17 && adc_value <= 637) || (adc_value >= 3360 && adc_value <= 4095)) {
        // Zona Crítica
        set_led_color(2048, 0, 0); // Vermelho
        desenhar_quadrado_3x3(255, 0, 0); // Quadrado vermelho na matriz de LEDs
        print_message("ZONA CRITICA", 20, 30);
        sprintf(mensagem, "CORRENTE EM %.0fA", corrente);
        ssd1306_draw_string(&ssd, mensagem, 5, 40);
        ssd1306_send_data(&ssd);

        // Ativa o buzzer se não estiver ativo
        if (!buzzer_ativo) {
            ativar_buzzer();
        }

    } else if ((adc_value >= 638 && adc_value <= 1258) || (adc_value >= 2619 && adc_value <= 3359)) {
        // Zona de Alerta
        set_led_color(2048, 2048, 0); // Amarelo
        desenhar_quadrado_3x3(255, 255, 0); // Quadrado amarelo na matriz de LEDs
        print_message("ZONA DE ALERTA", 10, 30);
        sprintf(mensagem, "CORRENTE EM %.0fA", corrente);
        ssd1306_draw_string(&ssd, mensagem, 5, 40);
        ssd1306_send_data(&ssd);

        // Desativa o buzzer se estiver ativo
        if (buzzer_ativo) {
            desativar_buzzer();
        }

    } else if ((adc_value >= 1259 && adc_value <= 1877) || (adc_value >= 1878 && adc_value <= 2618)) {
        // Zona Segura
        set_led_color(0, 2048, 0); // Verde
        desenhar_quadrado_3x3(0, 255, 0); // Quadrado verde na matriz de LEDs
        sleep_ms(500);
        print_message("ZONA SEGURA", 20, 30);
        sprintf(mensagem, "CORRENTE EM %.0fA", corrente);
        ssd1306_draw_string(&ssd, mensagem, 5, 40);
        ssd1306_send_data(&ssd);

        // Desativa o buzzer se estiver ativo
        if (buzzer_ativo) {
            desativar_buzzer();
        }
    }
}

int main() {
    stdio_init_all();

    // Configuração dos botões
    gpio_init(Botao_A);
    gpio_set_dir(Botao_A, GPIO_IN);
    gpio_pull_up(Botao_A);
    gpio_set_irq_enabled_with_callback(Botao_A, GPIO_IRQ_EDGE_FALL, true, &irq_handler);

    gpio_init(Botao_B);
    gpio_set_dir(Botao_B, GPIO_IN);
    gpio_pull_up(Botao_B);
    gpio_set_irq_enabled_with_callback(Botao_B, GPIO_IRQ_EDGE_FALL, true, &irq_handler);

    // Configuração do display OLED
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, 128, 64, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd);

    // Inicialização do sistema - Placa BitDogLab energizada
    ssd1306_fill(&ssd, !cor); // Limpa o display
    draw_thick_rect(&ssd, 3, 3, 122, 58, cor, 2); // Desenha um retângulo com borda espessa
    ssd1306_draw_string(&ssd, "SISTEMA", 25, 20); // Desenha uma string
    ssd1306_draw_string(&ssd, "ENERGIZADO", 20, 30); // Desenha uma string
    ssd1306_send_data(&ssd);

    // Configuração do ADC (apenas eixo X)
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN); // Configura apenas o pino do eixo X
    adc_select_input(1); // Seleciona o canal 1 (eixo X)

    // Configuração dos LEDs PWM
    gpio_set_function(LED_R, GPIO_FUNC_PWM);
    gpio_set_function(LED_G, GPIO_FUNC_PWM);
    gpio_set_function(LED_B, GPIO_FUNC_PWM);

    uint slice_r = pwm_gpio_to_slice_num(LED_R);
    uint slice_g = pwm_gpio_to_slice_num(LED_G);
    uint slice_b = pwm_gpio_to_slice_num(LED_B);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0);
    pwm_config_set_wrap(&config, 4095);

    pwm_init(slice_r, &config, true);
    pwm_init(slice_g, &config, true);
    pwm_init(slice_b, &config, true);

    // Configuração da matriz de LEDs
    configurar_matriz_leds();
    sleep_ms(100); // Pequeno atraso para garantir que a matriz esteja pronta
    limpar_matriz_leds(); // Garante que a matriz de LEDs comece desligada

    // Configuração do buzzer
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    gpio_put(BUZZER_PIN, 0); // Garante que o buzzer comece desligado

    while (true) {
        if (system_active) {
            uint16_t adc_value = adc_read(); // Lê apenas o eixo X
            check_adc_value(adc_value);
            sleep_ms(250);
        }
    }
}