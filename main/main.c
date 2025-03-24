/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

QueueHandle_t xQueueTime, xQueueDistance;
SemaphoreHandle_t xSemaphore;

const int ECHO_PIN = 16;
const int TRIGGER_PIN = 17;

void pin_callback(uint gpio, uint32_t events) {
    uint64_t start_time;
    uint64_t end_time;

    if (events == GPIO_IRQ_EDGE_RISE) {
        start_time = to_us_since_boot(get_absolute_time());
        xQueueSendFromISR(xQueueTime, &start_time, 0);
    } else if (events == GPIO_IRQ_EDGE_FALL) {
        end_time = to_us_since_boot(get_absolute_time());
        xQueueSendFromISR(xQueueTime, &end_time, 0);
    }
}

void trigger_task(void *p) {
    while (1) {
        gpio_put(TRIGGER_PIN, 1);
        vTaskDelay(1 / (portTICK_PERIOD_MS * 1000));
        gpio_put(TRIGGER_PIN, 0);

        xSemaphoreGive(xSemaphore);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void echo_task(void *p) {
    int dt;
    float distancia;
    uint64_t start_time = 0;
    uint64_t end_time = 0;
    uint64_t time = 0;
    int par = 0;

    while (1) {
        if (xQueueReceive(xQueueTime, &time, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (par % 2 != 0) {
                end_time = time;
            } else {
                start_time = time;
            }
            par++;

            if (start_time > 0 && end_time > 0) {
                dt = end_time - start_time;
                distancia = (dt * 0.0343) / 2;
                start_time = 0;
                end_time = 0;
                xQueueSend(xQueueDistance, &distancia, pdMS_TO_TICKS(10));
            }
        }
    }
}

void oled1_btn_led_init(void) {
    gpio_init(LED_1_OLED);
    gpio_set_dir(LED_1_OLED, GPIO_OUT);

    gpio_init(LED_2_OLED);
    gpio_set_dir(LED_2_OLED, GPIO_OUT);

    gpio_init(LED_3_OLED);
    gpio_set_dir(LED_3_OLED, GPIO_OUT);

    gpio_init(BTN_1_OLED);
    gpio_set_dir(BTN_1_OLED, GPIO_IN);
    gpio_pull_up(BTN_1_OLED);

    gpio_init(BTN_2_OLED);
    gpio_set_dir(BTN_2_OLED, GPIO_IN);
    gpio_pull_up(BTN_2_OLED);

    gpio_init(BTN_3_OLED);
    gpio_set_dir(BTN_3_OLED, GPIO_IN);
    gpio_pull_up(BTN_3_OLED);
}

void oled_task(void *p) {
    ssd1306_init();
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);
    oled1_btn_led_init();

    while (1) {
        if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
            float distancia = 0;
            if (xQueueReceive(xQueueDistance, &distancia, pdMS_TO_TICKS(1000)) == pdTRUE) {
                gfx_clear_buffer(&disp);
                char str[50];
                if (distancia < 300) {
                    sprintf(str, "Distancia: %.2f cm", distancia);
                } else {
                    sprintf(str, "Falha");
                }
                gfx_draw_string(&disp, 0, 0, 1, str);
                int size = (int) distancia * 128 / 300;
                gfx_draw_line(&disp, 0, 27, size, 27);
                vTaskDelay(pdMS_TO_TICKS(50));
                gfx_show(&disp);
            }
        } else {
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "Falha");
            gfx_show(&disp);
        }
    }
}

int main() {
    stdio_init_all();

    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);

    gpio_put(TRIGGER_PIN, 0);

    gpio_set_irq_enabled_with_callback(
        ECHO_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &pin_callback);

    xQueueTime = xQueueCreate(64, sizeof(uint64_t));
    xQueueDistance = xQueueCreate(64, sizeof(float));
    xSemaphore = xSemaphoreCreateBinary();

    if (xSemaphore == NULL) {
        printf("falha em criar o semaforo \n");
    }

    xTaskCreate(trigger_task, "Trigger", 256, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo", 256, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1) {
    }
}
