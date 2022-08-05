
#include <string.h>
// #include "protocol_examples_common.h"
#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"

#include "esp_log.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"

/* Retirado do arquivo sdkconfig */ 
#define CONFIG_ESP32_WIFI_STATIC_RX_BUFFER_NUM        10
#define CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM       32
#define CONFIG_EXAMPLE_WIFI_SCAN_RSSI_THRESHOLD     -127
#define CONFIG_ESP32_WIFI_TX_BUFFER_TYPE               1
#define CONFIG_ESP32_WIFI_DYNAMIC_TX_BUFFER_NUM       32

#define NR_OF_IP_ADDRESSES_TO_WAIT_FOR (s_active_interfaces)

#define EXAMPLE_DO_CONNECT CONFIG_EXAMPLE_CONNECT_WIFI

#define EXAMPLE_WIFI_SCAN_METHOD WIFI_FAST_SCAN
// #define EXAMPLE_WIFI_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN

#define EXAMPLE_WIFI_CONNECT_AP_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
// #define EXAMPLE_WIFI_CONNECT_AP_SORT_METHOD WIFI_CONNECT_AP_BY_SECURITY

// #define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
// #define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
// #define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
// #define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
// #define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_ENTERPRISE
// #define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
// #define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
// #define EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK

static int s_active_interfaces = 0;
static xSemaphoreHandle s_semph_get_ip_addrs;
static esp_netif_t *s_example_esp_netif = NULL;
// static esp_ip6_addr_t s_ipv6_addr;

// static const char *TAG = "conectar";

static esp_netif_t *wifi_start(void);
static void wifi_stop(void);

extern TAG;

/**
 * @Verifica a descricao do netif se contiver o prefixo especificado.
 * Todos os netifs criados dentro do componente de conexao comum sao prefixados com o modulo 'TAG', 
 * entao ele retorna 'true' se o netif especificado for de propriedade deste modulo.
 */
static bool is_our_netif(const char *prefix, esp_netif_t *netif)
{
    return strncmp(prefix, esp_netif_get_desc(netif), strlen(prefix) - 1) == 0;
}

/* Configurar conexao Wi-Fi. */
static void start(void)
{
    s_example_esp_netif = wifi_start();
    s_active_interfaces++;

    /* Cria semaforo se pelo menos uma interface estiver ativa. */
    s_semph_get_ip_addrs = xSemaphoreCreateCounting(NR_OF_IP_ADDRESSES_TO_WAIT_FOR, 0);
}

/* Derrubar conexao, liberar recursos. */
static void stop(void)
{
    wifi_stop();
    s_active_interfaces--;
}

static esp_ip4_addr_t s_ip_addr;

static void on_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    if (!is_our_netif(TAG, event->esp_netif)) 
	{
        ESP_LOGW(TAG, "Obteve IPv4 de outra interface \"%s\": ignorado", esp_netif_get_desc(event->esp_netif));
        return;
    }
    ESP_LOGI(TAG, "Obteve evento IPv4: Interface \"%s\" endereco: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
    memcpy(&s_ip_addr, &event->ip_info.ip, sizeof(s_ip_addr));
    xSemaphoreGive(s_semph_get_ip_addrs);
}


// static void on_got_ipv6(void *arg, esp_event_base_t event_base,
//                         int32_t event_id, void *event_data)
// {
//     ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
//     if (!is_our_netif(TAG, event->esp_netif)) {
//         ESP_LOGW(TAG, "Got IPv6 from another netif: ignored");
//         return;
//     }
//     esp_ip6_addr_type_t ipv6_type = esp_netif_ip6_get_addr_type(&event->ip6_info.ip);
//     ESP_LOGI(TAG, "Got IPv6 event: Interface \"%s\" address: " IPV6STR ", type: %s", esp_netif_get_desc(event->esp_netif),
//              IPV62STR(event->ip6_info.ip), s_ipv6_addr_types[ipv6_type]);
//     if (ipv6_type == EXAMPLE_CONNECT_PREFERRED_IPV6_TYPE) {
//         memcpy(&s_ipv6_addr, &event->ip6_info.ip, sizeof(s_ipv6_addr));
//         xSemaphoreGive(s_semph_get_ip_addrs);
//     }
// }

esp_err_t example_connect(void)
{
#if EXAMPLE_DO_CONNECT
    if (s_semph_get_ip_addrs != NULL) 
	{
        return ESP_ERR_INVALID_STATE;
    }
#endif
    start();
    ESP_ERROR_CHECK(esp_register_shutdown_handler(&stop));
    ESP_LOGI(TAG, "Aguardando IP.");
    for (int i = 0; i < NR_OF_IP_ADDRESSES_TO_WAIT_FOR; ++i) 
	{
        xSemaphoreTake(s_semph_get_ip_addrs, portMAX_DELAY);
    }
    // Observar as interfaces ativas e imprimir IPs de "nossos" netifs.
    esp_netif_t *netif = NULL;
    esp_netif_ip_info_t ip;
    for (int i = 0; i < esp_netif_get_nr_of_ifs(); ++i) 
	{
        netif = esp_netif_next(netif);
        if (is_our_netif(TAG, netif)) 
		{
            ESP_LOGI(TAG, "Conectado a %s", esp_netif_get_desc(netif));
            ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip));

            ESP_LOGI(TAG, "- IPv4 Endereco: " IPSTR, IP2STR(&ip.ip));
        }
    }
    return ESP_OK;
}

esp_err_t example_disconnect(void)
{
    if (s_semph_get_ip_addrs == NULL) 
	{
        return ESP_ERR_INVALID_STATE;
    }
    vSemaphoreDelete(s_semph_get_ip_addrs);
    s_semph_get_ip_addrs = NULL;
    stop();
    ESP_ERROR_CHECK(esp_unregister_shutdown_handler(&stop));
    return ESP_OK;
}

static void on_wifi_disconnect(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "Wi-Fi disconectado, tentando reconectar...");
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_STARTED) 
	{
        return;
    }
    ESP_ERROR_CHECK(err);
}

// static void on_wifi_connect(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data)
// {
//     esp_netif_create_ip6_linklocal(esp_netif);
// }

static esp_netif_t *wifi_start(void)
{
    char *desc;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    // Prefixar a descricao da interface com o modulo TAG.
    // Atencao: a interface desc e' usada em testes para capturar detalhes reais da conexao (IP, gw, mask).
    asprintf(&desc, "%s: %s", TAG, esp_netif_config.if_desc);
    esp_netif_config.if_desc = desc;
    esp_netif_config.route_prio = 128;
    esp_netif_t *netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    free(desc);
    esp_wifi_set_default_wifi_sta_handlers();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL));


    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = 
	{
        .sta = 
		{
            .ssid = "IoT_AP",
            .password = "12345678",
            .scan_method = EXAMPLE_WIFI_SCAN_METHOD,
            .sort_method = EXAMPLE_WIFI_CONNECT_AP_SORT_METHOD,
            .threshold.rssi = CONFIG_EXAMPLE_WIFI_SCAN_RSSI_THRESHOLD,
            .threshold.authmode = EXAMPLE_WIFI_SCAN_AUTH_MODE_THRESHOLD,
        },
    };
    ESP_LOGI(TAG, "Conectando a %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();
    return netif;
}

esp_netif_t *get_example_netif(void)
{
    return s_example_esp_netif;
}

static void wifi_stop(void)
{
    esp_netif_t *wifi_netif = get_example_netif_from_desc("sta");
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip));

    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) 
	{
        return;
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(wifi_netif));
    esp_netif_destroy(wifi_netif);
    s_example_esp_netif = NULL;
}

esp_netif_t *get_example_netif_from_desc(const char *desc)
{
    esp_netif_t *netif = NULL;
    char *expected_desc;
    asprintf(&expected_desc, "%s: %s", TAG, desc);
    while ((netif = esp_netif_next(netif)) != NULL) 
	{
        if (strcmp(esp_netif_get_desc(netif), expected_desc) == 0) 
		{
            free(expected_desc);
            return netif;
        }
    }
    free(expected_desc);
    return netif;
}
