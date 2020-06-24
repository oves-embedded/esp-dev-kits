/* Camera Example

    This example code is in the Public Domain (or CC0 licensed, at your option.)
    Unless required by applicable law or agreed to in writing, this
    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "cam.h"
#include "ov2640.h"
#include "lcd.h"
#include "jpeg.h"
#include "board.h"

static const char *TAG = "main";

#define CAM_WIDTH   (320)
#define CAM_HIGH    (240)

static void cam_task(void *arg)
{
    lcd_config_t lcd_config = {
#ifdef CONFIG_LCD_ST7789
        .clk_fre         = 80 * 1000 * 1000, /*!< ILI9341 Stable frequency configuration */
#endif
#ifdef CONFIG_LCD_ILI9341
        .clk_fre         = 40 * 1000 * 1000, /*!< ILI9341 Stable frequency configuration */
#endif 
        .pin_clk         = LCD_CLK,
        .pin_mosi        = LCD_MOSI,
        .pin_dc          = LCD_DC,
        .pin_cs          = LCD_CS,
        .pin_rst         = LCD_RST,
        .pin_bk          = LCD_BK,
        .max_buffer_size = 2 * 1024,
        .horizontal      = 2 /*!< 2: UP, 3: DOWN */
    };

    lcd_init(&lcd_config);

    cam_config_t cam_config = {
        .bit_width = 8,
#ifdef CONFIG_CAMERA_JPEG_MODE
        .mode.jpeg = 1,
#else
        .mode.jpeg = 0,
#endif
        .xclk_fre = 16 * 1000 * 1000,
        .pin = {
            .xclk  = CAM_XCLK,
            .pclk  = CAM_PCLK,
            .vsync = CAM_VSYNC,
            .hsync = CAM_HSYNC,
        },
        .pin_data  = {CAM_D0, CAM_D1, CAM_D2, CAM_D3, CAM_D4, CAM_D5, CAM_D6, CAM_D7},
        .size = {
            .width = CAM_WIDTH,
            .high  = CAM_HIGH,
        },
        .max_buffer_size = 8 * 1024,
        .task_stack      = 1024,
        .task_pri        = configMAX_PRIORITIES
    };

    /*!< With PingPang buffers, the frame rate is higher, or you can use a separate buffer to save memory */
    cam_config.frame1_buffer = (uint8_t *)heap_caps_malloc(CAM_WIDTH * CAM_HIGH * 2 * sizeof(uint8_t), MALLOC_CAP_SPIRAM);
    cam_config.frame2_buffer = (uint8_t *)heap_caps_malloc(CAM_WIDTH * CAM_HIGH * 2 * sizeof(uint8_t), MALLOC_CAP_SPIRAM);

    cam_init(&cam_config);

    if (OV2640_Init(0, 1) != ESP_OK) {
        vTaskDelete(NULL);
        return;
    }

    if (cam_config.mode.jpeg) {
        OV2640_JPEG_Mode();
    } else {
        OV2640_RGB565_Mode(false);	/*!< RGB565 mode */
    }

    OV2640_ImageSize_Set(800, 600);
    OV2640_ImageWin_Set(0, 0, 800, 600);
    OV2640_OutSize_Set(CAM_WIDTH, CAM_HIGH);
    ESP_LOGI(TAG, "camera init done\n");
    cam_start();

    while (1) {
        uint8_t *cam_buf = NULL;
        size_t recv_len = cam_take(&cam_buf);
#ifdef CONFIG_CAMERA_JPEG_MODE

        int w, h;
        uint8_t *img = jpeg_decode(cam_buf, &w, &h);

        if (img) {
            ESP_LOGI(TAG, "jpeg: w: %d, h: %d\n", w, h);
            lcd_set_index(0, 0, w - 1, h - 1);
            lcd_write_data(img, w * h * sizeof(uint16_t));
            free(img);
        }

#else
        lcd_set_index(0, 0, CAM_WIDTH - 1, CAM_HIGH - 1);
        lcd_write_data(cam_buf, CAM_WIDTH * CAM_HIGH * 2);
#endif
        cam_give(cam_buf);
        /*!< Use a logic analyzer to observe the frame rate */
        gpio_set_level(LCD_BK, 1);
        gpio_set_level(LCD_BK, 0);
    }

    vTaskDelete(NULL);
}

void app_main()
{
    xTaskCreate(cam_task, "cam_task", 2048, NULL, configMAX_PRIORITIES, NULL);
}