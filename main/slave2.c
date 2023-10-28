#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"

static const char *TAG = "i2c-slave-2";


#define I2C_SLAVE_SCL_IO 22               /*!< gpio number for I2C master clock */
#define I2C_SLAVE_SDA_IO 21               /*!< gpio number for I2C master data  */
#define I2C_SLAVE_FREQ_HZ 400000       /*!< I2C master clock frequency */
#define I2C_SLAVE_TX_BUF_LEN 255                        /*!< I2C master doesn't need buffer */
#define I2C_SLAVE_RX_BUF_LEN 255                           /*!< I2C master doesn't need buffer */
#define ESP_SLAVE_ADDR 0x27
#define RW_TEST_LENGTH 128
#define WRITE_BIT I2C_MASTER_WRITE              /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ                /*!< I2C master read */
#define ACK_CHECK_EN 0x1                        /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0                       /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                             /*!< I2C ack value */
#define NACK_VAL 0x1                            /*!< I2C nack value */

int i2c_slave_port = 0;

static esp_err_t i2c_slave_init(void)
{
  
    i2c_config_t conf_slave = {
    .sda_io_num = I2C_SLAVE_SDA_IO,          // select GPIO specific to your project
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_io_num = I2C_SLAVE_SCL_IO,          // select GPIO specific to your project
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .mode = I2C_MODE_SLAVE,
    .slave.addr_10bit_en = 0,
    .slave.slave_addr = ESP_SLAVE_ADDR,      // address of your project
    .clk_flags = 0,
    };
    esp_err_t err = i2c_param_config(i2c_slave_port, &conf_slave);
    if (err != ESP_OK) {
        return err;
    }
    return i2c_driver_install(i2c_slave_port, conf_slave.mode, I2C_SLAVE_RX_BUF_LEN, I2C_SLAVE_TX_BUF_LEN, 0);
}


static void i2c_handle_task(void *pvParameters){
    ESP_LOGI(TAG, "i2c handle task start");
    uint8_t  received_data[I2C_SLAVE_RX_BUF_LEN] = {0};
    uint8_t transfer_data[32] = "0b010";
    while(1){
        vTaskDelay(pdMS_TO_TICKS(100));
        int len = i2c_slave_read_buffer(i2c_slave_port, received_data, I2C_SLAVE_RX_BUF_LEN, pdMS_TO_TICKS(100));
        if(len>0){
              ESP_LOG_BUFFER_CHAR(TAG,received_data,len);  
              i2c_reset_rx_fifo(i2c_slave_port);
              i2c_slave_write_buffer(i2c_slave_port,&transfer_data,32,pdMS_TO_TICKS(100));
              vTaskDelay(pdMS_TO_TICKS(1000));
            bzero(received_data,I2C_SLAVE_RX_BUF_LEN);
        }
    }
}

static esp_err_t create_i2c_handle_task(void){
    xTaskCreate(i2c_handle_task,"i2c_handle_task",1024*4,NULL,1,NULL);
    return ESP_OK;
}


void app_main(void)
{
    ESP_ERROR_CHECK(i2c_slave_init());
    ESP_ERROR_CHECK(create_i2c_handle_task());
}
