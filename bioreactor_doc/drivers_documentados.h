/* ============================================================
   ARCHIVO: botones.h
   ============================================================ */

/**
 * @file botones.h
 * @brief Interfaz del módulo de botones físicos del bioreactor.
 *
 * Expone funciones de inicialización y lectura con antirrebote
 * para el botón de inicio del proceso y el botón de mezcla manual.
 *
 * @author  Equipo Bioreactor Popayán
 * @version 2.0
 * @date    2025
 */

#ifndef BOTONES_H
#define BOTONES_H

#include <stdbool.h>

/**
 * @brief Inicializa los GPIOs de ambos botones como entradas con pull-up.
 *
 * Configura BOTON_INICIO y BOTON_MEZCLA (definidos en definiciones.h)
 * en modo entrada con resistencia pull-up interna activada.
 * Nivel lógico 0 = botón presionado.
 */
void botones_init(void);

/**
 * @brief Lee el estado del botón de inicio con antirrebote de 50 ms.
 * @retval true  El botón está presionado (confirmado tras espera antirrebote).
 * @retval false El botón no está presionado.
 */
bool boton_inicio_presionado(void);

/**
 * @brief Lee el estado del botón de mezcla con antirrebote de 50 ms.
 * @retval true  El botón está presionado (confirmado tras espera antirrebote).
 * @retval false El botón no está presionado.
 */
bool boton_mezcla_presionado(void);

#endif /* BOTONES_H */


/* ============================================================
   ARCHIVO: dht.h
   ============================================================ */

/**
 * @file dht.h
 * @brief Interfaz del driver para el sensor DHT22 (temperatura y humedad).
 *
 * Implementa el protocolo propietario del DHT22 mediante manipulación
 * directa de GPIO y temporización por software (bit-banging).
 *
 * @author  Equipo Bioreactor Popayán
 * @version 2.0
 * @date    2025
 *
 * @warning No modificar los delays internos del driver; el protocolo
 *          DHT22 es extremadamente sensible al timing.
 */

#ifndef DHT_H
#define DHT_H

#include "esp_err.h"

/** @brief Identificador del tipo de sensor DHT22. */
#define DHT_TYPE_DHT22 1

/**
 * @brief Lee temperatura y humedad del sensor DHT22.
 *
 * Genera la señal de inicio, recibe 40 bits de datos y valida
 * el checksum antes de convertir a valores físicos.
 *
 * @param[in]  gpio_num   Número del GPIO al que está conectado el sensor.
 * @param[out] humedad    Puntero donde se almacenará la humedad relativa (%).
 * @param[out] temperatura Puntero donde se almacenará la temperatura (°C).
 *
 * @retval ESP_OK    Lectura exitosa y checksum válido.
 * @retval ESP_FAIL  Error de comunicación (timeout) o checksum incorrecto.
 */
esp_err_t dht_read_data(int gpio_num, float *humedad, float *temperatura);

#endif /* DHT_H */


/* ============================================================
   ARCHIVO: ds18b20.h
   ============================================================ */

/**
 * @file ds18b20.h
 * @brief Interfaz del driver para el sensor de temperatura DS18B20 (1-Wire).
 *
 * Implementa el protocolo 1-Wire de Dallas/Maxim de forma manual
 * (bit-banging) sobre cualquier GPIO con resistencia pull-up externa.
 *
 * @author  Equipo Bioreactor Popayán
 * @version 2.0
 * @date    2025
 *
 * @note Requiere resistencia pull-up de 4.7 kΩ en la línea de datos.
 * @warning Los delays de temporización 1-Wire son críticos. No modificar.
 */

#ifndef DS18B20_H
#define DS18B20_H

#include "esp_err.h"

/**
 * @brief Inicializa el GPIO para comunicación 1-Wire con el DS18B20.
 *
 * Configura el pin como salida en drenador abierto con pull-up interno.
 *
 * @param[in] gpio_num  Número del GPIO conectado al pin DQ del DS18B20.
 * @retval ESP_OK    Configuración exitosa.
 * @retval ESP_FAIL  Error en la configuración del GPIO.
 */
esp_err_t ds18b20_init(int gpio_num);

/**
 * @brief Lee la temperatura del sensor DS18B20.
 *
 * Secuencia: Reset → Skip ROM (0xCC) → Convert T (0x44) →
 * espera 800 ms → Reset → Skip ROM → Read Scratchpad (0xBE) →
 * lee 2 bytes y convierte a grados Celsius.
 *
 * @param[in]  gpio_num    GPIO conectado al sensor.
 * @param[out] temperatura Puntero donde se guarda la temperatura (°C).
 *
 * @retval ESP_OK    Temperatura leída correctamente.
 * @retval ESP_FAIL  Sin respuesta del sensor en el bus 1-Wire.
 */
esp_err_t ds18b20_leer_temp(int gpio_num, float *temperatura);

#endif /* DS18B20_H */


/* ============================================================
   ARCHIVO: ds1302.h
   ============================================================ */

/**
 * @file ds1302.h
 * @brief Interfaz del driver para el RTC DS1302 (Reloj en Tiempo Real).
 *
 * Implementa comunicación serial 3 hilos (CLK/DAT/RST) con el DS1302
 * usando bit-banging sobre GPIOs. Incluye validación de datos y
 * fallback por simulación de tiempo cuando el RTC falla.
 *
 * @author  Equipo Bioreactor Popayán
 * @version 2.0
 * @date    2025
 */

#ifndef DS1302_H
#define DS1302_H

#include "esp_err.h"
#include <stdint.h>

/**
 * @brief Estructura que representa un instante de tiempo del RTC DS1302.
 */
typedef struct {
    uint8_t  segundo;  /**< Segundos [0–59]. */
    uint8_t  minuto;   /**< Minutos  [0–59]. */
    uint8_t  hora;     /**< Horas    [0–23]. */
    uint8_t  dia;      /**< Día del mes [1–31]. */
    uint8_t  mes;      /**< Mes [1–12]. */
    uint16_t anio;     /**< Año completo (ej: 2025). */
} rtc_tiempo_t;

/**
 * @brief Inicializa los GPIOs y el chip DS1302.
 *
 * @param[in] clk  GPIO para la línea de reloj (CLK).
 * @param[in] dat  GPIO para la línea de datos (DAT/IO).
 * @param[in] rst  GPIO para la línea de habilitación (RST/CE).
 */
void ds1302_init(int clk, int dat, int rst);

/**
 * @brief Lee la hora y fecha actuales del DS1302.
 *
 * Si los datos recibidos son inválidos (mes > 12, día > 31, etc.),
 * el driver simula el avance del tiempo sumando 5 segundos al último
 * valor válido conocido.
 *
 * @param[out] tiempo  Puntero a la estructura donde se almacenará la lectura.
 */
void ds1302_leer_tiempo(rtc_tiempo_t *tiempo);

/**
 * @brief Ajusta la hora y fecha en el DS1302.
 *
 * Desbloquea el chip, escribe los registros en modo ráfaga (burst write)
 * y vuelve a bloquear.
 *
 * @param[in] tiempo  Puntero con los valores de hora/fecha a escribir.
 */
void ds1302_fijar_tiempo(rtc_tiempo_t *tiempo);

#endif /* DS1302_H */


/* ============================================================
   ARCHIVO: i2c_lcd.h
   ============================================================ */

/**
 * @file i2c_lcd.h
 * @brief Interfaz del driver para pantalla LCD 16x2 por I2C (PCF8574).
 *
 * Controla una pantalla LCD HD44780 conectada a través del expansor
 * de puertos PCF8574 vía bus I2C. Usa el nuevo driver i2c_master
 * de ESP-IDF v5.x.
 *
 * @author  Equipo Bioreactor Popayán
 * @version 2.0
 * @date    2025
 *
 * @note El LCD opera en modo 4 bits. La dirección I2C y los pines
 *       están definidos en definiciones.h (LCD_ADDR, LCD_SDA_GPIO, LCD_SCL_GPIO).
 */

#ifndef I2C_LCD_H
#define I2C_LCD_H

#include <stdint.h>

/**
 * @brief Inicializa el bus I2C y la pantalla LCD.
 *
 * Configura el bus I2C en modo maestro, registra el dispositivo PCF8574
 * y ejecuta la secuencia de inicialización HD44780 en 4 bits:
 * modo 2 líneas, cursor OFF, incremento automático, limpieza inicial.
 */
void lcd_init(void);

/**
 * @brief Envía un comando de control al LCD (RS = 0).
 *
 * Ejemplos de uso: limpiar pantalla (0x01), mover cursor, configurar modo.
 *
 * @param[in] comando  Byte de comando según datasheet HD44780.
 */
void lcd_send_cmd(uint8_t comando);

/**
 * @brief Envía un carácter de datos al LCD para visualización (RS = 1).
 *
 * @param[in] dato  Código ASCII del carácter a mostrar.
 */
void lcd_send_data(uint8_t dato);

/**
 * @brief Escribe una cadena de texto en la posición actual del cursor.
 *
 * @param[in] texto  Puntero a la cadena terminada en '\0' a mostrar.
 */
void lcd_send_string(char *texto);

/**
 * @brief Mueve el cursor a una posición específica del LCD.
 *
 * @param[in] fila     Fila de destino: 0 = primera fila, 1 = segunda fila.
 * @param[in] columna  Columna de destino: 0–15 para LCD 16x2.
 */
void lcd_put_cur(int fila, int columna);

/**
 * @brief Limpia toda la pantalla y retorna el cursor a la posición (0,0).
 */
void lcd_clear(void);

#endif /* I2C_LCD_H */


/* ============================================================
   ARCHIVO: ph_sensor.h
   ============================================================ */

/**
 * @file ph_sensor.h
 * @brief Interfaz del driver para el sensor analógico de pH.
 *
 * Lee el voltaje de la sonda de pH a través del ADC1 del ESP32
 * (canal 0, GPIO 36) y lo convierte a unidades de pH mediante
 * una fórmula lineal calibrable.
 *
 * @author  Equipo Bioreactor Popayán
 * @version 2.0
 * @date    2025
 *
 * @note La fórmula por defecto es: pH = 3.5 × V_adc (aproximada).
 *       Para mayor precisión calibrar con soluciones tampón conocidas.
 */

#ifndef PH_SENSOR_H
#define PH_SENSOR_H

#include "esp_err.h"

/**
 * @brief Inicializa el ADC1 para la lectura del sensor de pH.
 *
 * Configura la unidad ADC1, atenuación de 12 dB (rango 0–3.3 V)
 * y resolución por defecto. El canal queda fijo en ADC_CHANNEL_0 (GPIO36).
 *
 * @param[in] gpio_num  GPIO del sensor (informativo; el canal ADC es fijo).
 */
void ph_init(int gpio_num);

/**
 * @brief Lee el valor de pH actual del sensor.
 *
 * Realiza una conversión ADC de un solo disparo, convierte el valor
 * crudo a voltaje y aplica la fórmula de calibración.
 *
 * @param[in] gpio_num  GPIO del sensor (informativo; el canal ADC es fijo).
 *
 * @return Valor de pH calculado (≥ 0.0).
 * @retval -1.0  Error de lectura ADC.
 */
float ph_leer(int gpio_num);

#endif /* PH_SENSOR_H */


/* ============================================================
   ARCHIVO: ldr.h
   ============================================================ */

/**
 * @file ldr.h
 * @brief Interfaz del driver para el sensor de luz ambiental (LDR).
 *
 * Lee el estado digital del LDR conectado como divisor de voltaje.
 * La salida es invertida respecto al nivel físico del GPIO.
 *
 * @author  Equipo Bioreactor Popayán
 * @version 2.0
 * @date    2025
 */

#ifndef LDR_H
#define LDR_H

#include "esp_err.h"

/**
 * @brief Inicializa el GPIO del LDR como entrada digital sin pull-up.
 *
 * @param[in] gpio_num  Número del GPIO conectado al LDR.
 */
void ldr_init(int gpio_num);

/**
 * @brief Lee el estado de iluminación actual.
 *
 * La señal se invierte lógicamente respecto al nivel del GPIO,
 * según la polaridad del divisor de voltaje utilizado.
 *
 * @param[in] gpio_num  GPIO del LDR.
 * @retval 1  Luz detectada en el ambiente.
 * @retval 0  Ambiente oscuro (sin luz).
 */
int ldr_leer_estado(int gpio_num);

#endif /* LDR_H */


/* ============================================================
   ARCHIVO: rgb_control.h
   ============================================================ */

/**
 * @file rgb_control.h
 * @brief Interfaz del driver de control del LED RGB de alertas.
 *
 * Proporciona control digital ON/OFF (sin PWM) de un LED RGB de
 * cátodo común para señalizar visualmente el estado del sistema.
 *
 * @par Esquema de colores del sistema:
 * | Color    | Estado del Sistema |
 * |----------|--------------------|
 * | Verde    | REPOSO / normal    |
 * | Azul     | MONITOREO activo   |
 * | Amarillo | ALERTA (parpadeo)  |
 * | Rojo     | ALARMA (parpadeo)  |
 *
 * @author  Equipo Bioreactor Popayán
 * @version 2.0
 * @date    2025
 */

#ifndef RGB_CONTROL_H
#define RGB_CONTROL_H

/**
 * @brief Inicializa los tres GPIOs del LED RGB como salidas digitales.
 *
 * @param[in] rojo   GPIO para el canal rojo.
 * @param[in] verde  GPIO para el canal verde.
 * @param[in] azul   GPIO para el canal azul.
 */
void rgb_init(int rojo, int verde, int azul);

/** @brief Apaga el LED RGB (todos los canales en LOW). */
void rgb_apagar(void);

/** @brief Enciende el LED en color ROJO (canal R alto, G y B bajos). */
void rgb_rojo(void);

/** @brief Enciende el LED en color VERDE (canal G alto, R y B bajos). */
void rgb_verde(void);

/** @brief Enciende el LED en color AZUL (canal B alto, R y G bajos). */
void rgb_azul(void);

/**
 * @brief Enciende el LED en color AMARILLO (canales R y G altos, B bajo).
 * @note  El amarillo se obtiene mezclando rojo y verde digitalmente.
 */
void rgb_amarillo(void);

#endif /* RGB_CONTROL_H */


/* ============================================================
   ARCHIVO: sensores.h
   ============================================================ */

/**
 * @file sensores.h
 * @brief Interfaz de la tarea principal de adquisición de sensores.
 *
 * Declara la tarea FreeRTOS que integra todos los sensores del bioreactor,
 * calcula el código de alerta correspondiente y publica los datos
 * en la cola global @c xColaDatos.
 *
 * @author  Equipo Bioreactor Popayán
 * @version 2.0
 * @date    2025
 */

#ifndef SENSORES_H
#define SENSORES_H

/**
 * @brief Tarea FreeRTOS de adquisición de datos del bioreactor.
 *
 * Inicializa todos los sensores (DHT22, DS18B20, pH, LDR, RTC DS1302)
 * y entra en un bucle de lectura periódica. Calcula el código de alerta
 * combinado y publica un @ref datos_fermentacion_t en @c xColaDatos.
 *
 * @param pvParameters  No utilizado (firma estándar FreeRTOS).
 *
 * @note Esta función es creada como tarea con @c xTaskCreate() en @c app_main().
 */
void tarea_sensores(void *pvParameters);

#endif /* SENSORES_H */
