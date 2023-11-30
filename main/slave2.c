#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "time.h"
#include "unistd.h"
#include "string.h"
#include "stdlib.h"
#include "stdint.h"

static const char *TAG = "i2c-slave";


#define I2C_SLAVE_SCL_IO 22               /*!< gpio number for I2C master clock */
#define I2C_SLAVE_SDA_IO 21               /*!< gpio number for I2C master data  */
#define I2C_SLAVE_FREQ_HZ 400000       /*!< I2C master clock frequency */
#define I2C_SLAVE_TX_BUF_LEN 512                        /*!< I2C master doesn't need buffer */
#define I2C_SLAVE_RX_BUF_LEN 512                          /*!< I2C master doesn't need buffer */
#define ESP_SLAVE_ADDR 0x32
#define RW_TEST_LENGTH 128
#define WRITE_BIT I2C_MASTER_WRITE              /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ                /*!< I2C master read */
#define ACK_CHECK_EN 0x1                        /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0                       /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                             /*!< I2C ack value */
#define NACK_VAL 0x1                            /*!< I2C nack value */

int i2c_slave_port = 0;

char* vectorlocalTime(u_int8_t* counter);
u_int8_t* vectorcalculateRecvTimeStamp(u_int8_t* recvTimeStamp, uint8_t* counter,  uint8_t size);
u_int8_t* vector_event(uint8_t processId, uint8_t* counter);
u_int8_t* vector_send_Message(u_int8_t processId, u_int8_t *counter, char * rx_data);

uint8_t *messageParser(char *data) {
    static uint8_t c[3] = {-1, -1, -1};
    int startArrayIndex = 0;
    int endArrayIndex = 0;
    for (int i = 0; i < strlen(data); i++) {
        if (data[i] == '[') {
            startArrayIndex = i + 1;
        } else if (data[i] == ']') {
            endArrayIndex = i;
            break;
        }
    }
    char substring[endArrayIndex - startArrayIndex];
    u_int8_t start = 0;
    strncpy(substring, data + startArrayIndex, endArrayIndex - startArrayIndex);
    char *token = strtok(substring, ",");
    start = 0;
    while (token != NULL) {
        c[start] = atoi(token);
        token = strtok(NULL, ",");
        start++;
    }
    return c;

}

void swapCounter(uint8_t *old, uint8_t *new, int size) {
    uint8_t temp[size];
    for (uint8_t index = 0; index < (size); index++) {
        temp[index] = old[index];
        old[index] = new[index];
        new[index] = temp[index];
    }
}

char *vectorlocalTime(uint8_t *counter) {
    time_t now = time(NULL);
    char *timeStr = ctime(&now);
    char *lt = asctime(localtime((const time_t *) &timeStr));
    char timeStrCounter[300];
    sprintf(timeStrCounter, "Vector Clock = {[%d,%d,%d]}, LocalTime:%s", counter[0], counter[1], counter[2], lt);
    return lt;
}

u_int8_t *vectorcalculateRecvTimeStamp(uint8_t *recvTimeStamp, uint8_t *counter, uint8_t size) {
    for (uint8_t i = 0; i < size; i++) {
        counter[i] = (recvTimeStamp[i] > counter[i]) ? recvTimeStamp[i] : counter[i];
    }
    return counter;
}

u_int8_t *vector_event(u_int8_t processId, u_int8_t *counter) {
    counter[processId] += 1;
    char *localtimeString = vectorlocalTime(counter);
    printf("Vector Clock = {[%d,%d,%d]}, LocalTime:%s", counter[0], counter[1], counter[2], localtimeString);
    return counter;
}


void assign_uint8_t_to_char(uint8_t *uint8_array, char *char_array, size_t size) {
    for (size_t i = 0; i < size; i++) {
        char_array[i] = (char) uint8_array[i];
    }
}


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
    uint8_t counter[3] = {0,0,0};
    uint8_t transfer_data[32] = "0b001";
     char rx_data[I2C_SLAVE_RX_BUF_LEN]={0};
    while(1){
        uint8_t  received_data[I2C_SLAVE_RX_BUF_LEN] = {0};
        vTaskDelay(pdMS_TO_TICKS(100));
        int len = i2c_slave_read_buffer(i2c_slave_port, received_data, I2C_SLAVE_RX_BUF_LEN, pdMS_TO_TICKS(100));
        if(len>0){
            counter[1] += 1;
            char sendMessageData[128];
            sprintf(sendMessageData, "%d-[%d,%d,%d]",1, counter[0], counter[1], counter[2]);
            ESP_LOGI(TAG,"%s",sendMessageData);
            ESP_LOG_BUFFER_CHAR(TAG,received_data,len);  
            assign_uint8_t_to_char(received_data,rx_data,I2C_SLAVE_RX_BUF_LEN);
            u_int8_t *receiveMessageData;
            ESP_LOGI(TAG,"Received data: %s",rx_data);
            // receiveMessageData = messageParser(rx_data);
            // ESP_LOGI(TAG,"Received Datas:%s--%s",rx_data,received_data);
            // swapCounter(counter,receiveMessageData,3);
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
