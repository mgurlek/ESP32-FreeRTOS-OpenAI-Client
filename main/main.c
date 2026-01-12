#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"


#define WIFI_SSID           "YOUR-WIFI"             //CHANGE HERE       
#define WIFI_PASSWORD       "YOUR-WIFI-PASSWORD"    //CHANGE HERE
#define OPENAI_API_KEY      "OPEN-AI-API-KEY"       //CHANGE HERE
#define OPENAI_MODEL        "gpt-3.5-turbo"         //YOU CAN CHANGE THE MODEL IF YOU WANT

#define BUTTON_GPIO         2                       // GPIO BUTTON 

// --- SCREEN SETTINGS---
#define PIN_NUM_MOSI 11
#define PIN_NUM_CLK  12
#define PIN_NUM_CS   10
#define PIN_NUM_DC   9
#define PIN_NUM_RST  8
#define PIN_NUM_BCKL 7
#define LCD_HOST     SPI2_HOST
#define LCD_WIDTH    128
#define LCD_HEIGHT   160
#define OFFSET_X     2                              //CHANGE THIS IIF YOU HAVE SCREEN OFFSET
#define OFFSET_Y     0                              //CHANGE THIS IIF YOU HAVE SCREEN OFFSET

// --- COLOR PALETTE ---
#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_NAVY    0x000F  
#define C_BLUE    0x001F  
#define C_RED     0xF800  
#define C_GREEN   0x07E0  
#define C_CYAN    0x07FF  
#define C_ORANGE  0xFD20  
#define C_PURPLE  0x780F  

static const char *TAG = "AI_PRO";


static QueueHandle_t xDisplayQueue;
static EventGroupHandle_t wifi_event_group;
static SemaphoreHandle_t xButtonSemaphore = NULL;
static const int WIFI_CONNECTED_BIT = BIT0;

typedef struct {
    char text[1024]; 
} DisplayMessage_t;

typedef struct {
    char *buffer;       
    int buffer_len;     
    int received_len;   
} OpenAI_Response_t;

static spi_device_handle_t spi; 


const uint8_t font5x7[][5] = {                                                      //FONT DATA
    {0,0,0,0,0}, {0,0,253,0,0}, {0,96,0,96,0}, {20,127,20,127,20}, 
    {36,42,127,42,18}, {35,19,8,100,98}, {54,73,85,34,80}, {0,5,3,0,0}, 
    {0,28,34,65,0}, {0,65,34,28,0}, {20,8,62,8,20}, {8,8,62,8,8}, 
    {0,80,48,0,0}, {8,8,8,8,8}, {0,96,96,0,0}, {32,16,8,4,2}, 
    {62,81,73,69,62}, {0,66,127,64,0}, {66,97,81,73,70}, {33,65,69,75,49}, 
    {24,20,18,127,16}, {39,69,69,69,57}, {60,74,73,73,48}, {1,113,9,5,3}, 
    {54,73,73,73,54}, {6,73,73,41,30}, {0,54,54,0,0}, {0,86,54,0,0}, 
    {8,20,34,65,0}, {20,20,20,20,20}, {0,65,34,20,8}, {2,1,81,9,6}, 
    {50,73,121,65,62}, {126,17,17,17,126}, {127,73,73,73,54}, {62,65,65,65,34}, 
    {127,65,65,34,28}, {127,73,73,73,65}, {127,9,9,9,1}, {62,65,73,73,122}, 
    {127,8,8,8,127}, {0,65,127,65,0}, {32,64,65,63,1}, {127,8,20,34,65}, 
    {127,64,64,64,64}, {127,2,12,2,127}, {127,4,8,16,127}, {62,65,65,65,62}, 
    {127,9,9,9,6}, {62,65,81,33,94}, {127,9,25,41,70}, {70,73,73,73,49}, 
    {1,1,127,1,1}, {63,64,64,64,63}, {31,32,64,32,31}, {63,64,56,64,63}, 
    {99,20,8,20,99}, {7,8,112,8,7}, {97,81,73,69,67}, {0,127,65,65,0}, 
    {2,4,8,16,32}, {0,65,65,127,0}, {4,2,1,2,4}, {64,64,64,64,64}, 
    {0,1,2,4,0}, {32,84,84,84,120}, {127,72,68,68,56}, {56,68,68,68,32}, 
    {56,68,68,72,127}, {56,84,84,84,24}, {8,126,9,1,2}, {12,82,82,82,62}, 
    {127,8,4,4,120}, {0,68,125,64,0}, {32,64,68,61,0}, {127,16,40,68,0}, 
    {0,65,127,64,0}, {124,4,24,4,120}, {124,8,4,4,120}, {56,68,68,68,56}, 
    {124,20,20,20,8}, {8,20,20,24,124}, {124,8,4,4,8}, {72,84,84,84,32}, 
    {4,63,68,64,32}, {60,64,64,32,124}, {28,32,64,32,28}, {60,64,48,64,60}, 
    {68,40,16,40,68}, {12,80,80,80,60}, {68,100,84,76,68}, {8,54,65,65,0}, 
    {0,0,119,0,0}, {0,65,65,54,8}, {8,12,6,3,1} 
};

void clean_turkish_text(char *str) {
    char *src = str;
    char *dst = str;
    
    while (*src) {
        if ((unsigned char)*src == 0xC3 || (unsigned char)*src == 0xC4 || (unsigned char)*src == 0xC5) {
            unsigned char next = (unsigned char)*(src + 1);
            char replace = 0;
            
            
            if (*src == 0xC3) {
                if (next == 0xA7) replace = 'c'; // ç
                else if (next == 0x87) replace = 'C'; // Ç
                else if (next == 0xB6) replace = 'o'; // ö
                else if (next == 0x96) replace = 'O'; // Ö
                else if (next == 0xBC) replace = 'u'; // ü
                else if (next == 0x9C) replace = 'U'; // Ü
            } else if (*src == 0xC4) {
                if (next == 0x9F) replace = 'g'; // ğ
                else if (next == 0x9E) replace = 'G'; // Ğ
                else if (next == 0xB1) replace = 'i'; // ı
                else if (next == 0xB0) replace = 'I'; // İ
            } else if (*src == 0xC5) {
                if (next == 0x9F) replace = 's'; // ş
                else if (next == 0x9E) replace = 'S'; // Ş
            }

            if (replace) {
                *dst++ = replace;
                src += 2; 
            } else {
                *dst++ = *src++; 
            }
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

//SCREEN DRIVER

void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd) {
    spi_transaction_t t = { .length = 8, .tx_buffer = &cmd, .user = (void*)0 };
    spi_device_polling_transmit(spi, &t);
}

void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len) {
    if (len==0) return;
    spi_transaction_t t = { .length = len*8, .tx_buffer = data, .user = (void*)1 };
    spi_device_polling_transmit(spi, &t);
}

void lcd_spi_pre_transfer_callback(spi_transaction_t *t) {
    gpio_set_level(PIN_NUM_DC, (int)t->user);
}

void lcd_set_window(spi_device_handle_t spi, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    x0 += OFFSET_X; x1 += OFFSET_X;
    y0 += OFFSET_Y; y1 += OFFSET_Y;
    uint8_t col[] = {x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF};
    uint8_t row[] = {y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF};
    lcd_cmd(spi, 0x2A); lcd_data(spi, col, 4);
    lcd_cmd(spi, 0x2B); lcd_data(spi, row, 4);
    lcd_cmd(spi, 0x2C);
}

void lcd_fill_rect(spi_device_handle_t spi, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if ((x >= LCD_WIDTH) || (y >= LCD_HEIGHT)) return;
    if ((x + w - 1) >= LCD_WIDTH)  w = LCD_WIDTH  - x;
    if ((y + h - 1) >= LCD_HEIGHT) h = LCD_HEIGHT - y;
    
    lcd_set_window(spi, x, y, x + w - 1, y + h - 1);
    
    uint16_t *line_buf = (uint16_t*)malloc(w * 2);
    if (!line_buf) return;

    uint16_t swapped_color = (color << 8) | (color >> 8);
    for (int i = 0; i < w; i++) line_buf[i] = swapped_color;

    for (int i = 0; i < h; i++) {
        lcd_data(spi, (uint8_t*)line_buf, w * 2);
    }
    free(line_buf);
}

void lcd_fill_screen(spi_device_handle_t spi, uint16_t color) {
    lcd_fill_rect(spi, 0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

void draw_text_styled(spi_device_handle_t spi, int x, int y, const char *str, uint16_t color, uint16_t bg, uint8_t size) {
    int cur_x = x;
    int cur_y = y;
    int char_w = 6 * size; 
    int char_h = 8 * size; 

    while (*str) {
        uint8_t c = (uint8_t)*str;
        const uint8_t *bitmap = NULL;

        if (c == '\n') {
            cur_y += char_h + 2; cur_x = x; str++; continue;
        }
        if (cur_x + char_w > LCD_WIDTH) { 
            cur_y += char_h + 2; cur_x = x;
        }
        if (cur_y + char_h > LCD_HEIGHT) break; 

        if (c >= 32 && c <= 126) {
            bitmap = font5x7[c - 32];
        }

        if (bitmap) {
            int px_count = char_w * char_h;
            uint16_t *char_buf = (uint16_t*)malloc(px_count * 2);
            if (char_buf) {
                uint16_t color_swap = (color << 8) | (color >> 8);
                uint16_t bg_swap = (bg << 8) | (bg >> 8);
                for (int r = 0; r < 8; r++) { 
                    for (int s_y = 0; s_y < size; s_y++) { 
                        for (int col = 0; col < 5; col++) { 
                            int is_pixel = (bitmap[col] >> r) & 1;
                            for (int s_x = 0; s_x < size; s_x++) { 
                                int buf_idx = ((r * size + s_y) * char_w) + (col * size + s_x);
                                char_buf[buf_idx] = is_pixel ? color_swap : bg_swap;
                            }
                        }
                        for (int s_x = 0; s_x < size; s_x++) {
                             int buf_idx = ((r * size + s_y) * char_w) + (5 * size + s_x);
                             char_buf[buf_idx] = bg_swap;
                        }
                    }
                }
                lcd_set_window(spi, cur_x, cur_y, cur_x + char_w - 1, cur_y + char_h - 1);
                lcd_data(spi, (uint8_t*)char_buf, px_count * 2);
                free(char_buf);
            }
        }
        cur_x += char_w; str++;
    }
}

void lcd_init_setup() {
    gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_BCKL, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_BCKL, 1);

    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_MOSI, .sclk_io_num = PIN_NUM_CLK,
        .miso_io_num = -1, .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = 4096 
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 20 * 1000 * 1000,
        .mode = 0, .spics_io_num = PIN_NUM_CS, .queue_size = 7,
        .pre_cb = lcd_spi_pre_transfer_callback,
    };
    spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_bus_add_device(LCD_HOST, &devcfg, &spi);

    gpio_set_level(PIN_NUM_RST, 0); vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_NUM_RST, 1); vTaskDelay(pdMS_TO_TICKS(100));
    lcd_cmd(spi, 0x01); vTaskDelay(pdMS_TO_TICKS(150)); 
    lcd_cmd(spi, 0x11); vTaskDelay(pdMS_TO_TICKS(255)); 
    uint8_t color_mode[] = {0x05}; lcd_cmd(spi, 0x3A); lcd_data(spi, color_mode, 1);
    uint8_t madctl[] = {0xC0}; lcd_cmd(spi, 0x36); lcd_data(spi, madctl, 1); 
    lcd_cmd(spi, 0x29); 
}

//BUTTON INTERRUPT

static void IRAM_ATTR button_isr_handler(void* arg) {
    xSemaphoreGiveFromISR(xButtonSemaphore, NULL);
}

void system_monitor_task(void *pvParameters) {
    wifi_ap_record_t ap_info;
    DisplayMessage_t msg;
    static bool is_monitor_active = false;

    while(1) {
        if (xSemaphoreTake(xButtonSemaphore, portMAX_DELAY) == pdTRUE) { 
            vTaskDelay(pdMS_TO_TICKS(200)); 
            
            if (gpio_get_level(BUTTON_GPIO) == 0) {
                
                is_monitor_active = !is_monitor_active;

                if (is_monitor_active) {
                    uint32_t free_heap = esp_get_free_heap_size();
                    int8_t rssi = -100;
                    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) rssi = ap_info.rssi;
                    

                    snprintf(msg.text, sizeof(msg.text), 
                             "SISTEM DURUMU\n"
                             "----------------\n"
                             "RAM: %lu Byte\n"
                             "WiFi: %d dBm\n"
                             ,
                             free_heap, rssi);
                } 
                else {
                    snprintf(msg.text, sizeof(msg.text), 
                             "AI MODU AKTIF\n\n"
                             "Soru sormak icin\n"
                             "telefona geciniz.\n\n"
                             "Baglanti: ONLINE"); 
                }
                
                xQueueSend(xDisplayQueue, &msg, 0);
                
                while(gpio_get_level(BUTTON_GPIO) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
            }
        }
    }
}

void button_init(void) {
    xButtonSemaphore = xSemaphoreCreateBinary();
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1; 
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);
}


void gui_task(void *pvParameters) {
    DisplayMessage_t msg;
    
    lcd_fill_screen(spi, C_BLACK);
    lcd_fill_rect(spi, 0, 0, LCD_WIDTH, 25, C_NAVY); 
    draw_text_styled(spi, 20, 8, "AI ASISTAN", C_WHITE, C_NAVY, 1);
    draw_text_styled(spi, 10, 60, "Sistem\nBaslatiliyor...", C_CYAN, C_BLACK, 1);

    while(1) {
        if (xQueueReceive(xDisplayQueue, &msg, portMAX_DELAY) == pdPASS) {
            
            
            clean_turkish_text(msg.text);                                                                       //FOR TURKISH CHAR'S

            
            lcd_fill_rect(spi, 0, 26, LCD_WIDTH, LCD_HEIGHT-26, C_BLACK);
            vTaskDelay(pdMS_TO_TICKS(10)); 
            
            uint16_t header_color = C_NAVY;
            char header_text[32] = "BILGI";
            uint16_t text_color = C_WHITE;

            if (strstr(msg.text, "Baglandi") || strstr(msg.text, "IP") || strstr(msg.text, "ONLINE")) {
                header_color = C_GREEN; strcpy(header_text, "ONLINE"); text_color = C_GREEN;
            } 
            else if (strstr(msg.text, "DUSUNUYOR") || strstr(msg.text, "Bekle")) {
                header_color = C_ORANGE; strcpy(header_text, "AI DUSUNUYOR"); text_color = C_ORANGE;
            }
            else if (strstr(msg.text, "SISTEM")) {
                header_color = C_PURPLE; strcpy(header_text, "SISTEM"); text_color = C_WHITE;
            }
            else if (strstr(msg.text, "Hata") || strstr(msg.text, "Koptu")) {
                header_color = C_RED; strcpy(header_text, "! HATA !"); text_color = C_RED;
            }
            else {
                header_color = C_BLUE; strcpy(header_text, "CEVAP"); text_color = C_CYAN; 
            }

            lcd_fill_rect(spi, 0, 0, LCD_WIDTH, 25, header_color);
            int center_x = (LCD_WIDTH - (strlen(header_text) * 6))/2;
            if(center_x < 0) center_x = 0;
            draw_text_styled(spi, center_x, 8, header_text, C_WHITE, header_color, 1);

            draw_text_styled(spi, 5, 35, msg.text, text_color, C_BLACK, 1); 
        }
    }
}


esp_err_t _openai_event_handler(esp_http_client_event_t *evt) {                                                         //OPEN AI CLIENT
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        OpenAI_Response_t *resp = (OpenAI_Response_t *)evt->user_data;
        if (resp->received_len + evt->data_len < resp->buffer_len) {
            memcpy(resp->buffer + resp->received_len, evt->data, evt->data_len);
            resp->received_len += evt->data_len;
            resp->buffer[resp->received_len] = 0; 
        }
    }
    return ESP_OK;
}

int ask_openai_direct(const char *question, char *response_buf, int buf_len) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", OPENAI_MODEL);
    cJSON *messages = cJSON_CreateArray();
    cJSON *msg_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(msg_obj, "role", "user");
    cJSON_AddStringToObject(msg_obj, "content", question);
    cJSON_AddItemToArray(messages, msg_obj);
    cJSON_AddItemToObject(root, "messages", messages);
    cJSON_AddNumberToObject(root, "max_tokens", 150); 
    char *json_body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    char *large_rx_buffer = (char*)malloc(8192);
    if (!large_rx_buffer) { free(json_body); return -1; }
    memset(large_rx_buffer, 0, 8192);

    OpenAI_Response_t response_data = { .buffer = large_rx_buffer, .buffer_len = 8192, .received_len = 0 };

    esp_http_client_config_t config = {
        .url = "https://api.openai.com/v1/chat/completions",
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 60000,
        .buffer_size_tx = 2048,
        .event_handler = _openai_event_handler, 
        .user_data = &response_data,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    char *auth_header = (char*)malloc(512);
    if(auth_header) snprintf(auth_header, 512, "Bearer %s", OPENAI_API_KEY);
    
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    if(auth_header) esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_post_field(client, json_body, strlen(json_body));

    ESP_LOGI(TAG, "OpenAI'a soruluyor...");
    esp_err_t err = esp_http_client_perform(client);
    
    int result = -1;

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP: %d, Data: %d byte", status, response_data.received_len);

        if (status == 200 && response_data.received_len > 0) {
            cJSON *resp_json = cJSON_Parse(large_rx_buffer);
            if (resp_json) {
                cJSON *choices = cJSON_GetObjectItem(resp_json, "choices");
                if (choices && cJSON_GetArraySize(choices) > 0) {
                    cJSON *first = cJSON_GetArrayItem(choices, 0);
                    cJSON *msg = cJSON_GetObjectItem(first, "message");
                    cJSON *cnt = cJSON_GetObjectItem(msg, "content");
                    if (cnt && cJSON_IsString(cnt)) {
                        strncpy(response_buf, cnt->valuestring, buf_len - 1);
                        response_buf[buf_len - 1] = '\0';
                        result = 0; 
                    }
                }
                cJSON_Delete(resp_json);
            }
        }
    } else {
        ESP_LOGE(TAG, "Baglanti Hatasi: %s", esp_err_to_name(err));
        snprintf(response_buf, buf_len, "Baglanti Yok");
    }

    if(auth_header) free(auth_header);
    free(json_body);
    free(large_rx_buffer);
    esp_http_client_cleanup(client);
    return result;
}

//WEB SERVER AND WIFI


static esp_err_t chat_get_handler(httpd_req_t *req)
{
    char query_str[1024] = {0};
    char question[512] = {0};
    char ai_response[1024] = {0}; 
    DisplayMessage_t msg;

    if (httpd_req_get_url_query_str(req, query_str, sizeof(query_str)) == ESP_OK) {
        if (httpd_query_key_value(query_str, "q", question, sizeof(question)) == ESP_OK) {
            
            
            snprintf(msg.text, sizeof(msg.text), "AI DUSUNUYOR...\n\nLutfen bekleyiniz,\ncevap hazirlaniyor...");
            xQueueSend(xDisplayQueue, &msg, 0);

            if (ask_openai_direct(question, ai_response, sizeof(ai_response)) == 0) {
                snprintf(msg.text, sizeof(msg.text), "%s", ai_response);
                xQueueSend(xDisplayQueue, &msg, 0);
                
                cJSON *root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, "response", ai_response);
                const char *json_resp = cJSON_PrintUnformatted(root);
                httpd_resp_set_type(req, "application/json");
                httpd_resp_send(req, json_resp, strlen(json_resp));
                cJSON_Delete(root);
                free((void*)json_resp);
            } else {
                httpd_resp_send_500(req);
                snprintf(msg.text, sizeof(msg.text), "API Hatasi!");
                xQueueSend(xDisplayQueue, &msg, 0);
            }
            return ESP_OK;
        }
    }
    httpd_resp_send(req, "Hata: ?q=soru eksik", HTTPD_RESP_USE_STRLEN);
    return ESP_FAIL;
}

static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 20480; 
    config.max_uri_handlers = 8;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t chat_uri = { .uri = "/chat_get", .method = HTTP_GET, .handler = chat_get_handler };
        httpd_register_uri_handler(server, &chat_uri);
        return server;
    }
    return NULL;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    DisplayMessage_t msg;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        snprintf(msg.text, sizeof(msg.text), "WiFi Koptu!\nBaglaniyor...");
        xQueueSend(xDisplayQueue, &msg, 0);
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(msg.text, sizeof(msg.text), "Baglandi!\nIP: " IPSTR "\n\nLinke Git:\nhttp://" IPSTR "/chat_get?q=soru", IP2STR(&event->ip_info.ip), IP2STR(&event->ip_info.ip));
        xQueueSend(xDisplayQueue, &msg, 0);
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init(void) {
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);
    wifi_config_t wifi_config = { .sta = { .ssid = WIFI_SSID, .password = WIFI_PASSWORD, .threshold.authmode = WIFI_AUTH_WPA2_PSK, }, };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase()); ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    lcd_init_setup();
    xDisplayQueue = xQueueCreate(5, sizeof(DisplayMessage_t));
    button_init();
    xTaskCreate(gui_task, "GUI_Task", 4096, NULL, 5, NULL);
    xTaskCreate(system_monitor_task, "SysMon_Task", 4096, NULL, 5, NULL);
    wifi_init();
    start_webserver();
}