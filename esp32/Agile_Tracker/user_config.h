#ifndef USER_CONFIG_H
#define USER_CONFIG_H

// Waveshare ESP32-C6-Touch-AMOLED-2.16
// Panel: 480x480 AMOLED, CO5300 (QSPI), CST9220 touch @ I2C 0x5A, AXP2101 PMIC

#define BSP_I2C_NUM             (I2C_NUM_0)
#define BSP_LCD_SPI_NUM         (SPI2_HOST)

#define BSP_I2C_SCL             (GPIO_NUM_7)
#define BSP_I2C_SDA             (GPIO_NUM_8)

#define BSP_I2S_SCLK            (GPIO_NUM_20)
#define BSP_I2S_MCLK            (GPIO_NUM_19)
#define BSP_I2S_LCLK            (GPIO_NUM_22)
#define BSP_I2S_DOUT            (GPIO_NUM_23)
#define BSP_I2S_DSIN            (GPIO_NUM_21)
#define BSP_POWER_AMP_IO        (GPIO_NUM_NC)

#define BSP_LCD_H_RES           (480)
#define BSP_LCD_V_RES           (480)
#define BSP_LCD_CS              (GPIO_NUM_15)
#define BSP_LCD_PCLK            (GPIO_NUM_0)
#define BSP_LCD_DATA0           (GPIO_NUM_1)
#define BSP_LCD_DATA1           (GPIO_NUM_2)
#define BSP_LCD_DATA2           (GPIO_NUM_3)
#define BSP_LCD_DATA3           (GPIO_NUM_4)
#define BSP_LCD_BITS_PER_PIXEL  (16)
#define BSP_LCD_DMASIZE         (BSP_LCD_H_RES * BSP_LCD_V_RES)

#define BSP_LCD_BACKLIGHT       (GPIO_NUM_NC)
#define BSP_LCD_RST             (GPIO_NUM_NC)
#define BSP_LCD_TOUCH_RST       (GPIO_NUM_11)
#define BSP_LCD_TOUCH_INT       (GPIO_NUM_5)
#define BSP_LCD_TOUCH_I2C_ADDR  (0x5A)   // CST9220 (CST92xx family)


#define BSP_SD_MOSI             (GPIO_NUM_1)
#define BSP_SD_MISO             (GPIO_NUM_2)
#define BSP_SD_CLK              (GPIO_NUM_0)
#define BSP_SD_CS               (GPIO_NUM_6)

#define LVGL_TICK_PERIOD_MS    2
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 5
#define LVGL_TASK_STACK_SIZE   (8 * 1024)
#define LVGL_TASK_PRIORITY     2

// Round 2.16" AMOLED: full-screen layout; pad text/cards inward where the circle clips
#define BSP_ROUND_HEADER_PAD_H  34
#define BSP_ROUND_CONTENT_PAD_H  14
#define BSP_ROUND_CONTENT_PAD_V  6

// Physical buttons (active low, internal pull-up)
#define BSP_BTN_BOOT_GPIO      (GPIO_NUM_9)   // BOOT
#define BSP_BTN_KEY_GPIO       (GPIO_NUM_10)  // KEY (+)
#define BSP_BTN_PWR_GPIO       (GPIO_NUM_18)  // PWR (AXP2101 power key, hold to off)
#endif // !USER_CONFIG_H