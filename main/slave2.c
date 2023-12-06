#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "stdlib.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "regex.h"

static const char *TAG = "i2c-slave";

#define LED_PIN 2

#define I2C_SLAVE_SCL_IO 22               /*!< gpio number for I2C master clock */
#define I2C_SLAVE_SDA_IO 21               /*!< gpio number for I2C master data  */
#define I2C_SLAVE_FREQ_HZ 400000        /*!< I2C master clock frequency */
#define I2C_SLAVE_TX_BUF_LEN 255                        /*!< I2C master doesn't need buffer */
#define I2C_SLAVE_RX_BUF_LEN 255                           /*!< I2C master doesn't need buffer */
#define ESP_SLAVE_ADDR 0x0A
#define ESP_OTHER_SLAVE_ADDR 0x0B
#define I2C_MASTER_TX_BUF_DISABLE 0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0    
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

int useRegex(char* textToCheck) {
    regex_t compiledRegex;
    int reti;
    int actualReturnValue = -1;
    char messageBuffer[100];
    size_t nmatch = 1;
    regmatch_t pmatch[1];

    /* Compile regular expression */
    reti = regcomp(&compiledRegex, "^[0-9]+-\\[[0-9]+,[0-9]+,[0-9]+\\]$", REG_EXTENDED | REG_ICASE);
    if (reti) {
        // fprintf(stderr, "Could not compile regex\n");
        return -2;
    }

    /* Execute compiled regular expression */
    reti = regexec(&compiledRegex, textToCheck,nmatch, pmatch, 0);
    if (!reti) {
        // puts("Match");
        actualReturnValue = 0;
    } else if (reti == REG_NOMATCH) {
        // puts("No match");
        actualReturnValue = 1;
    } else {
        // regerror(reti, &compiledRegex, messageBuffer, sizeof(messageBuffer));
        // fprintf(stderr, "Regex match failed: %s\n", messageBuffer);
        actualReturnValue = -3;
    }

    /* Free memory allocated to the pattern buffer by regcomp() */
    regfree(&compiledRegex);
    return actualReturnValue;
}

static esp_err_t i2c_master_init(void)
{
  
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SLAVE_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SLAVE_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_SLAVE_FREQ_HZ,
        // .clk_flags = 0,          /*!< Optional, you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here. */
    };
    esp_err_t err = i2c_param_config(i2c_slave_port, &conf);
    if (err != ESP_OK) {
        return err;
    }
    return i2c_driver_install(i2c_slave_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

static esp_err_t i2c_master_send(uint8_t message[], int len)
{
    // ESP_LOGI(TAG, "Sending Message = %s", message);   
    
    esp_err_t ret; 
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ESP_OTHER_SLAVE_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write(cmd, message, len, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    
    ret = i2c_master_cmd_begin(i2c_slave_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

u_int8_t *messageParser(char *data) {
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
    int start = 0;
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

void swapCounter(uint8_t *old, uint8_t *new, uint8_t size) {
    uint8_t temp[size];
    for (int index = 0; index < (size); index++) {
        temp[index] = old[index];
        old[index] = new[index];
        new[index] = temp[index];
    }
}

uint8_t *vectorcalculateRecvTimeStamp(u_int8_t *recvTimeStamp, u_int8_t *counter, u_int8_t size) {
    for (int i = 0; i < size; i++) {
        counter[i] = (recvTimeStamp[i] > counter[i]) ? recvTimeStamp[i] : counter[i];
    }
    return counter;
}

uint8_t *vector_event(int processId, uint8_t *counter) {
    counter[processId] += 1;
    // char *localtimeString = vectorlocalTime(counter);
    // printf("Vector Clock = {[%d,%d,%d]}, LocalTime:%s", counter[0], counter[1], counter[2], localtimeString);
    return counter;
}

u_int8_t *vector_send_Message(uint8_t processId, uint8_t *counter) {
    counter[processId] += 1;
    char sendMessageData[255];
    sprintf(sendMessageData, "%d-[%d,%d,%d]",processId, counter[0], counter[1], counter[2]);
    size_t len = strlen(sendMessageData);
    uint8_t arr[len];
    memcpy(arr, sendMessageData, len);
    // uint8_t off_command[sizeof(sendMessageData)] = (uint8_t)atoi(sendMessageData);
    ESP_ERROR_CHECK(i2c_master_init());
    // const uint8_t off_command[] = "1-[255,255,255]";
    i2c_master_send(arr, sizeof(arr));
    ESP_ERROR_CHECK(i2c_driver_delete(i2c_slave_port));
    return counter;
}

u_int8_t *vector_receive_Message(uint8_t processId, uint8_t *counter) {
    uint8_t  received_data[I2C_SLAVE_RX_BUF_LEN] = {0};
    ESP_ERROR_CHECK(i2c_slave_init());
    i2c_slave_read_buffer(i2c_slave_port, received_data, I2C_SLAVE_RX_BUF_LEN, 100 / portTICK_PERIOD_MS);
    char *str = (char *)received_data;
    if(useRegex(str)==0){
        ESP_LOGI(TAG,"Received Data on regex:%s",str);
        uint8_t *receiveMessageData;
        receiveMessageData = messageParser(str);
        counter = vectorcalculateRecvTimeStamp(receiveMessageData, counter, 3);
    }
    ESP_ERROR_CHECK(i2c_driver_delete(i2c_slave_port));
    return counter;
}

void app_main(void)
{

    uint8_t  received_data[I2C_SLAVE_RX_BUF_LEN] = {0};
    uint8_t counter[3] = {0,0,0};
    int pid = 1;
    while(1)
    {
        swapCounter(counter, vector_receive_Message(pid, counter), 3);
        swapCounter(counter,vector_send_Message(pid,counter),3);
    }

}
