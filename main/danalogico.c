#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"

#define ADC_UNIT 		ADC_UNIT_1 // Se escoge la unidad de adc a utilizar, que sería la 1 en este caso
#define _ADC_UNIT_STR(unit)	#unit
#define ADC_UNIT_STR(unit)	_ADC_UNIT_STR(ADC_UNIT)
#define ADC_CONV_MODE		ADC_CONV_SINGLE_UNIT_1 // Se indica que solamente se utilizará el ADC 1
#define ADC_ATTEN		ADC_ATTEN_DB_0 // Atenuación del ADC, checar hoja de datos para ver más atenuaciones
#define ADC_BIT_WIDTH		SOC_ADC_DIGI_MAX_BITWIDTH // Resolución máxima (12 bits en este caso)
#define ADC_OUTPUT_TYPE		ADC_DIGI_OUTPUT_FORMAT_TYPE2 // Se escoge la configuración de los datos de salida
#define ADC_GET_CHANNEL(p_data) ((p_data)->type2.channel)
#define ADC_GET_DATA(p_data)	((p_data)->type2.data)
#define ADC_READ_LENGHT		4

uint16_t contador = 0;

static TaskHandle_t s_task_handle;

static adc_channel_t channels[1] = {ADC_CHANNEL_4}; // Lista dónde estarán contenidos los canales a utilizar
static const char *TAG = "EJERCICIO";

// 						  Parámetros(Manejador del adc			   , puntero a la estructura de config	   , No utilizado, puntero a datos adicionales)
static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data){ // Callback ADC
	BaseType_t mustYield = pdFALSE; // 
	vTaskNotifyGiveFromISR(s_task_handle, &mustYield); // Se notifica a 's_task_handle que la conversión del ADC ha terminado
	return (mustYield == pdTRUE);
}

// Configuración de los ADC a utilizar
void adc_init(adc_channel_t *channels, uint8_t channel_num, adc_continuous_handle_t *out_handle){

	// Configuración del handle del adc
	adc_continuous_handle_t handle = NULL; // Se le asigna un valor nulo al identificador del adc

	adc_continuous_handle_cfg_t adc_config = {
		.max_store_buf_size = 1024,// Máximo de datos que se pueden almacenar en el pool en bytes
		.conv_frame_size = ADC_READ_LENGHT, //Largo del frame de conversión
	};

	ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle)); //Se inicializa y configura el manejador interno utilizando la iformación de adc_config

	// Se configuran los canales por individual y se genera el arreglo que se cargará a la configuración del adc en modo continuo
	adc_digi_pattern_config_t adc_pattern[1] = {0};
	for (uint8_t i = 0; i < channel_num; i++){
		adc_pattern[i].atten = ADC_ATTEN;
		int io_channel;
		adc_continuous_channel_to_io(ADC_UNIT, channels[i],&io_channel);
		adc_pattern[i].channel = channels[i];
		adc_pattern[i].unit = ADC_UNIT;
		adc_pattern[i].bit_width = ADC_BIT_WIDTH;

		ESP_LOGI(TAG, "adc_pattern[%d].atten is :%"PRIx8, i, adc_pattern[i].atten);
		ESP_LOGI(TAG, "adc_pattern[%d].channel is :%"PRIx8, i, adc_pattern[i].channel);
		ESP_LOGI(TAG, "adc_pattern[%d].unit is :%"PRIx8, i, adc_pattern[i].unit);
	}

	adc_continuous_config_t dig_config = {
		.pattern_num = channel_num, // Número de canales ADC a utilizar
		.adc_pattern = adc_pattern,
		.sample_freq_hz = 1000, // MAX 83333 - MIN 611
		.conv_mode = ADC_CONV_MODE,
		.format = ADC_OUTPUT_TYPE,
	};

	ESP_ERROR_CHECK(adc_continuous_config(handle,&dig_config));
	*out_handle = handle;
	ESP_LOGI(TAG, "Se ha generado la primera configuración correctamente");
}

void app_main(void){
	
	esp_err_t ret; // Variale utilizada para almacenar el resultado de operaciones de funciones y comprobación de errores.
	uint32_t ret_num = 0; // Variable que se utilizará para almacenar el número de bytes leídos en una lectura de ADC
	uint8_t result[ADC_READ_LENGHT] = {0}; // Este arreglo almacenará los datos leídos del ADC
	memset(result, 0xcc, ADC_READ_LENGHT); // memset establece en el arreglo 'result' el valor 0xcc en 256 elementos (en este caso todo el arreglo)

	s_task_handle = xTaskGetCurrentTaskHandle(); // Se obtiene el identificador de la tarea actual (app_main) y se almacena en s_task_handle

	adc_continuous_handle_t adc_handle = NULL; // Se declara el handle del adc que será el manejador de la controlador ADC continuo
  //adc_init(Canales analógicos, número de canales, dirección del identificador)
	adc_init(channels, sizeof(channels) / sizeof(adc_channel_t), &adc_handle); // Se inicializan los canales ADC

	adc_continuous_evt_cbs_t cbs = { // Se genera la estructura que contiene la función en dónde estará la fución a ejecutar del callback
		.on_conv_done = s_conv_done_cb,
	};

	ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle, &cbs, NULL)); // Se registra la estuctura de callbacks
	ESP_ERROR_CHECK(adc_continuous_start(adc_handle)); // Se inicia el controlador ADC continuo

	while(1){
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Se bloquea la tarea hasta que se reciba la notificación del ADC
		char unit[] = ADC_UNIT_STR(ADC_UNIT); // Se genera la cadena de caractéres de la unidad de ADC
			while(1){
				ret = adc_continuous_read(adc_handle, result, ADC_READ_LENGHT, &ret_num, 0);
				if (ret == ESP_OK){
					contador++;
					if (contador == 40){
						ESP_LOGI("TASK", "ret is %x, ret_num is %"PRIu32" bytes", ret, ret_num);
						for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
							adc_digi_output_data_t *p = (adc_digi_output_data_t*)&result[i];
							uint32_t chan_num = ADC_GET_CHANNEL(p);
							uint32_t data = ADC_GET_DATA(p);
							float voltage = (float)data * 0.98 / 4095.0;
							/* Check the channel number validation, the data is invalid if the channel num exceed the maximum channel */
							if (chan_num < SOC_ADC_CHANNEL_NUM(ADC_UNIT)) {
								ESP_LOGI(TAG, "Unit: %s, Channel: %"PRIu32", Value: %"PRIu32", Voltage: %.3f", unit, chan_num, data,voltage);
							} else {
								ESP_LOGW(TAG, "Invalid data [%s_%"PRIu32"_%"PRIx32"]", unit, chan_num, data);
							}
						}
						contador = 0;
						vTaskDelay(1);
					}
					vTaskDelay(1);
				} else if (ret == ESP_ERR_TIMEOUT) {
					//We try to read `EXAMPLE_READ_LEN` until API returns timeout, which means there's no available data
					break;
				}
			}
	}
	ESP_ERROR_CHECK(adc_continuous_stop(adc_handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(adc_handle));
}
