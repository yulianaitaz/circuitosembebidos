/**
 * @file definiciones.h
 * @brief Configuración central del sistema Bioreactor Fermentación Café.
 *
 * Este archivo concentra todos los pines GPIO, constantes de límites físicos,
 * tiempos del sistema, la estructura de datos de fermentación y las variables
 * globales compartidas entre tareas FreeRTOS.
 *
 * @author  Equipo Bioreactor Popayán
 * @version 2.0
 * @date    2025
 *
 * @note Compatible con ESP-IDF v5.x sobre ESP32.
 */

#ifndef DEFINICIONES_H
#define DEFINICIONES_H

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/i2c.h"


/* ===========================================================
 * @defgroup pines_lcd Pines LCD I2C
 * @{
 * =========================================================== */

/** @brief GPIO para la línea de datos SDA del bus I2C (LCD). */
#define LCD_SDA_GPIO    21

/** @brief GPIO para la línea de reloj SCL del bus I2C (LCD). */
#define LCD_SCL_GPIO    22

/** @brief Número de puerto I2C usado por el LCD. */
#define I2C_MASTER_NUM  I2C_NUM_0

/** @brief Frecuencia del bus I2C en Hz (50 kHz). */
#define I2C_FREQ_HZ     50000

/** @brief Dirección I2C del módulo PCF8574 que controla el LCD. */
#define LCD_ADDR        0x27

/** @} */ /* fin grupo pines_lcd */


/* ===========================================================
 * @defgroup pines_rtc Pines RTC DS1302
 * @{
 * =========================================================== */

/** @brief GPIO para la línea RST (Reset/Enable) del DS1302. */
#define RTC_RST_GPIO    32

/** @brief GPIO para la línea de datos (DAT) del DS1302. */
#define RTC_DAT_GPIO    33

/** @brief GPIO para la línea de reloj (CLK) del DS1302. */
#define RTC_CLK_GPIO    25

/** @} */


/* ===========================================================
 * @defgroup pines_sd Pines Tarjeta SD (SPI)
 * @{
 * =========================================================== */

/** @brief GPIO MOSI del bus SPI para la tarjeta SD. */
#define SD_MOSI_GPIO    23

/** @brief GPIO MISO del bus SPI para la tarjeta SD. */
#define SD_MISO_GPIO    19

/** @brief GPIO de reloj SCK del bus SPI para la tarjeta SD. */
#define SD_SCK_GPIO     18

/** @brief GPIO Chip Select (CS) de la tarjeta SD. */
#define SD_CS_GPIO      5

/** @} */


/* ===========================================================
 * @defgroup pines_sensores Pines Sensores de Campo
 * @{
 * =========================================================== */

/** @brief GPIO de entrada analógica para el sensor de pH (ADC1 CH0). */
#define PH_ADC_GPIO     36

/** @brief GPIO de datos del sensor DHT22 (temperatura y humedad del aire). */
#define DHT_DATA_GPIO   4

/** @brief GPIO de datos del sensor DS18B20 (temperatura del café, protocolo 1-Wire). */
#define DS18B20_GPIO    16

/** @brief GPIO de entrada digital del sensor LDR (luz ambiental). */
#define LDR_GPIO        17

/** @} */


/* ===========================================================
 * @defgroup pines_salidas Pines de Salida (RGB y Botones)
 * @{
 * =========================================================== */

/** @brief GPIO para el canal Rojo del LED RGB. */
#define RGB_RED_GPIO    14

/** @brief GPIO para el canal Verde del LED RGB. */
#define RGB_GREEN_GPIO  26

/** @brief GPIO para el canal Azul del LED RGB. */
#define RGB_BLUE_GPIO   27

/** @brief GPIO del botón de inicio del proceso de fermentación. */
#define BOTON_INICIO    13

/** @brief GPIO del botón de mezcla manual. */
#define BOTON_MEZCLA    12

/** @} */


/* ===========================================================
 * @defgroup limites Límites Físicos del Proceso
 * @{
 * =========================================================== */

/** @brief Temperatura máxima permitida para el café (°C). Por encima → alerta. */
#define TEMP_MAX_CAFE       32.0f

/** @brief Temperatura mínima permitida para el café (°C). Por debajo → alerta. */
#define TEMP_MIN_CAFE       15.0f

/** @brief Valor máximo de pH aceptable durante la fermentación. */
#define PH_MAX              4.5f

/** @brief Valor mínimo de pH aceptable durante la fermentación. */
#define PH_MIN              3.8f

/**
 * @brief Nivel mínimo de luz requerida en el ambiente.
 * @note  0 = sin umbral mínimo (cualquier nivel de luz se acepta).
 */
#define LUZ_MIN_REQUERIDA   0

/** @} */


/* ===========================================================
 * @defgroup tiempos Tiempos del Sistema (segundos)
 * @{
 * =========================================================== */

/**
 * @brief Tiempo máximo en segundos que una alerta puede persistir
 *        antes de escalar a estado ALARMA.
 */
#define TIEMPO_ALERTA_MAX         900

/**
 * @brief Intervalo de lectura del sensor de pH en segundos.
 */
#define TIEMPO_LECTURA_PH         900

/**
 * @brief Tiempo máximo de persistencia de una alerta.
 *        Usado en la lógica de escalado de estados.
 */
#define TIEMPO_MAX_PERSISTENCIA   900

/** @} */


/* ===========================================================
 * @defgroup estructuras Estructuras de Datos
 * @{
 * =========================================================== */

/**
 * @brief Paquete de datos de una lectura completa del proceso de fermentación.
 *
 * Esta estructura viaja a través de la cola FreeRTOS (g_cola_datos)
 * desde la tarea de sensores hacia las tareas de pantalla y control.
 */
typedef struct {
    float temperatura_aire;  /**< Temperatura del aire ambiente medida por DHT22 (°C). */
    float humedad_aire;      /**< Humedad relativa del aire medida por DHT22 (%). */
    float temperatura_cafe;  /**< Temperatura interna del café medida por DS18B20 (°C). */
    float valor_ph;          /**< Valor de pH medido por el sensor analógico. */
    int   nivel_luz;         /**< Estado del LDR: 1 = hay luz, 0 = oscuro. */
    char  timestamp[32];     /**< Marca de tiempo en formato "DD/MM/AAAA HH:MM:SS". */
    int   codigo_alerta;     /**< Código de la alerta activa (0 = sin alerta, ver tabla). */
} datos_fermentacion_t;

/** @} */


/* ===========================================================
 * @defgroup estados Estados del Sistema
 * @{
 * =========================================================== */

/**
 * @brief Máquina de estados principal del bioreactor.
 *
 * Flujo normal:
 * @code
 *  REPOSO → (botón inicio) → MONITOREO → (alerta detectada) →
 *  ALERTA → (persiste > TIEMPO_MAX) → ALARMA
 * @endcode
 */
typedef enum {
    ESTADO_REPOSO    = 0, /**< Sistema encendido pero sin proceso activo. LED verde fijo. */
    ESTADO_MONITOREO = 1, /**< Proceso activo, todos los parámetros normales. LED azul. */
    ESTADO_ALERTA    = 2, /**< Parámetro(s) fuera de rango. LED amarillo parpadeante. */
    ESTADO_ALARMA    = 3  /**< Alerta persistente o múltiple crítica. LED rojo parpadeante. */
} estado_sistema_t;

/** @} */


/* ===========================================================
 * @defgroup globales Variables Globales Compartidas
 * @{
 * =========================================================== */

/**
 * @brief Cola FreeRTOS de capacidad 1.
 *
 * La tarea de sensores publica @ref datos_fermentacion_t.
 * Las tareas de pantalla y control consumen/espían los datos.
 */
extern QueueHandle_t     g_cola_datos;

/**
 * @brief Mutex para proteger el bus I2C compartido entre tareas.
 *
 * Debe tomarse antes de cualquier operación sobre el LCD
 * o dispositivos I2C conectados al mismo bus.
 */
extern SemaphoreHandle_t g_mutex_i2c;

/**
 * @brief Estado global actual del sistema.
 *
 * Modificado por @c tarea_control_proceso().
 * Leído por @c tarea_pantalla() para seleccionar la pantalla.
 */
extern estado_sistema_t  g_estado_sistema;

/** @} */

#endif /* DEFINICIONES_H */
