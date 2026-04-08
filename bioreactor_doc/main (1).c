/**
 * @file main.c
 * @brief Punto de entrada principal del sistema Bioreactor Fermentación Café.
 *
 * Inicializa los recursos FreeRTOS (cola e datos, mutex I2C) y lanza
 * las tres tareas concurrentes del sistema:
 *  - @ref tarea_sensores   : adquisición de datos y publicación en cola.
 *  - @ref tarea_pantalla   : visualización en LCD I2C con alertas parpadeantes.
 *  - @ref tarea_control_proceso : máquina de estados y control del LED RGB.
 *
 * @par Arquitectura de tareas
 * @code
 *  ┌──────────────────────────────────────────────────────┐
 *  │  app_main()                                          │
 *  │    │                                                 │
 *  │    ├─► tarea_sensores  (prio 5, stack 4096)          │
 *  │    │       │ xQueueSend(g_cola_datos)                │
 *  │    │       ▼                                         │
 *  │    ├─► tarea_pantalla  (prio 5, stack 4096)          │
 *  │    │       xQueueReceive / xQueuePeek                │
 *  │    │                                                 │
 *  │    └─► tarea_control_proceso (prio 5, stack 4096)    │
 *  │            xQueuePeek + GPIO RGB + botones           │
 *  └──────────────────────────────────────────────────────┘
 * @endcode
 *
 * @author  Equipo Bioreactor Popayán
 * @version 2.0
 * @date    2025
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"

#include "definiciones.h"
#include "sensores.h"
#include "i2c_lcd.h"
#include "rgb_control.h"
#include "botones.h"


/* ===========================================================
 * Variables globales
 * =========================================================== */

/** @brief Contador de segundos desde la última mezcla manual.
 *  Definido en sensores.c, leído aquí para sincronizar el "escudo visual". */
extern int seg_desde_ultima_mezcla;

/** @brief Estado actual de la máquina de estados del sistema. */
estado_sistema_t estado_actual_global = ESTADO_REPOSO;

/** @brief Cola FreeRTOS (tamaño 1) para pasar datos del sensor a pantalla/control. */
QueueHandle_t xColaDatos = NULL;

/** @brief Mutex que protege el bus I2C contra acceso simultáneo entre tareas. */
SemaphoreHandle_t xMutexI2C = NULL;

/** @brief Bandera de parpadeo para los mensajes de alerta en LCD. */
bool toggle_alerta = false;

/**
 * @brief Bandera volátil que indica que el operador acaba de realizar una mezcla manual.
 *
 * Se activa al pulsar el botón de mezcla y se desactiva automáticamente
 * cuando el contador @c seg_desde_ultima_mezcla confirma que el sensor
 * ya registró el cambio (evita el "parpadeo de alerta" falso en pantalla).
 */
volatile bool mezcla_manual_hecha = false;


/* ===========================================================
 * Tarea: Pantalla LCD
 * =========================================================== */

/**
 * @brief Tarea FreeRTOS que actualiza la pantalla LCD cada 500 ms.
 *
 * Comportamiento por estado del sistema:
 * - **ESTADO_REPOSO**: Muestra "BIOREACTOR V2.0 / LISTO - POPAYAN".
 * - **Otros estados**: Línea 1 → temperatura y humedad.
 *   Línea 2 → alterna entre pH+luz y el mensaje de alerta correspondiente.
 *
 * @par Tabla de códigos de alerta (campo @c codigo_alerta):
 * | Código | Mensaje LCD         | Significado                    |
 * |--------|---------------------|--------------------------------|
 * | 0      | pH + luz normal     | Sin alerta                     |
 * | 1      | !! TEMP ALTA !!     | Temperatura sobre TEMP_MAX_CAFE|
 * | 2      | !! TEMP BAJA !!     | Temperatura bajo TEMP_MIN_CAFE |
 * | 3      | !! PH ALTO    !!    | pH sobre PH_MAX                |
 * | 4      | !! PH BAJO    !!    | pH bajo PH_MIN                 |
 * | 5      | !! MUCHA LUZ !!     | Luz excesiva detectada         |
 * | 6      | MEZCLAR+PULSAR      | Tiempo sin mezcla superado     |
 * | 10–13  | T.BAJA/ALTA + pH   | Combinaciones temperatura+pH   |
 * | 20–28  | Combinaciones       | Combinaciones múltiples        |
 * | 99     | ALARMA:REV. TODO    | Múltiples parámetros críticos  |
 *
 * @param pvParameters  No utilizado (requerido por FreeRTOS).
 */
void tarea_pantalla(void *pvParameters)
{
    datos_fermentacion_t d_cache = {0};
    char l1[17], l2[17];

    lcd_init();
    lcd_clear();

    while (1)
    {
        /* Consumir el dato más reciente de la cola (sin bloqueo). */
        if (xQueueReceive(xColaDatos, &d_cache, 0) == pdTRUE)
        {
            /* Escudo visual: si el operador acaba de mezclar, ignorar alerta 6. */
            if (mezcla_manual_hecha && d_cache.causa_alerta == 6)
                d_cache.causa_alerta = 0;
        }

        if (estado_actual_global == ESTADO_REPOSO)
        {
            lcd_put_cur(0, 0); lcd_send_string("BIOREACTOR V2.0");
            lcd_put_cur(1, 0); lcd_send_string("LISTO - POPAYAN");
        }
        else
        {
            /* Línea 1: temperatura del café + humedad del aire */
            snprintf(l1, 17, "T:%4.1fC H:%2.0f%% ", d_cache.temp_cafe, d_cache.hum_aire);
            lcd_put_cur(0, 0);
            lcd_send_string(l1);

            if (d_cache.causa_alerta == 0)
            {
                /* Sin alerta: mostrar pH y estado de luz */
                snprintf(l2, 17, "pH:%4.2f L:%s  ",
                         d_cache.ph_valor,
                         d_cache.luz_nivel ? "ON " : "OFF");
                lcd_put_cur(1, 0);
                lcd_send_string(l2);
            }
            else
            {
                /* Con alerta: alternar entre mensaje de alerta y datos de pH/luz */
                toggle_alerta = !toggle_alerta;
                lcd_put_cur(1, 0);

                if (toggle_alerta)
                {
                    switch (d_cache.causa_alerta)
                    {
                        case 1:  lcd_send_string("!! TEMP ALTA !!"); break;
                        case 2:  lcd_send_string("!! TEMP BAJA !!"); break;
                        case 3:  lcd_send_string("!! PH ALTO    !!"); break;
                        case 4:  lcd_send_string("!! PH BAJO    !!"); break;
                        case 5:  lcd_send_string("!! MUCHA LUZ !!"); break;
                        case 6:  lcd_send_string("MEZCLAR+PULSAR "); break;
                        case 10: lcd_send_string("T.BAJA + PH.BAJ"); break;
                        case 11: lcd_send_string("T.BAJA + PH.ALT"); break;
                        case 12: lcd_send_string("T.ALTA + PH.BAJ"); break;
                        case 13: lcd_send_string("T.ALTA + PH.ALT"); break;
                        case 20: lcd_send_string("T.BAJA + LUZ   "); break;
                        case 21: lcd_send_string("T.BAJA + MEZCLA"); break;
                        case 22: lcd_send_string("T.ALTA + LUZ   "); break;
                        case 23: lcd_send_string("T.ALTA + MEZCLA"); break;
                        case 24: lcd_send_string("PH.BAJ + LUZ   "); break;
                        case 25: lcd_send_string("PH.BAJ + MEZCLA"); break;
                        case 26: lcd_send_string("PH.ALT + LUZ   "); break;
                        case 27: lcd_send_string("PH.ALT + MEZCLA"); break;
                        case 28: lcd_send_string("LUZ + MEZCLA   "); break;
                        case 99: lcd_send_string("ALARMA:REV. TODO"); break;
                        default: lcd_send_string("ERROR NO DEF.  "); break;
                    }
                }
                else
                {
                    snprintf(l2, 17, "pH:%4.2f L:%s  ",
                             d_cache.ph_valor,
                             d_cache.luz_nivel ? "ON " : "OFF");
                    lcd_send_string(l2);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


/* ===========================================================
 * Tarea: Control del Proceso
 * =========================================================== */

/**
 * @brief Tarea FreeRTOS que gestiona la máquina de estados y el LED RGB.
 *
 * Se ejecuta cada 200 ms para garantizar respuesta rápida a botones.
 *
 * @par Máquina de estados interna:
 * @code
 *  REPOSO ──(btn inicio)──► MONITOREO
 *                               │
 *                    (alerta > 0)│
 *                               ▼
 *                           ALERTA
 *                               │
 *               (ticks > TIEMPO_MAX * 5)│ o (codigo == 99)
 *                               ▼
 *                           ALARMA
 * @endcode
 *
 * @par Indicación visual (LED RGB):
 * | Estado      | LED            |
 * |-------------|----------------|
 * | REPOSO      | Verde fijo     |
 * | MONITOREO   | Azul fijo      |
 * | ALERTA      | Amarillo parpad|
 * | ALARMA      | Rojo parpadeante|
 *
 * @param pvParameters  No utilizado (requerido por FreeRTOS).
 */
void tarea_control_proceso(void *pvParameters)
{
    datos_fermentacion_t datos_vivos;
    int ticks_en_alerta = 0;
    int toggle_visual   = 0;

    rgb_init();
    botones_init();

    while (1)
    {
        /* --- Botón mezcla: reset instantáneo del contador --- */
        if (boton_mezcla_presionado())
        {
            seg_desde_ultima_mezcla = 0;
            mezcla_manual_hecha     = true;
            ticks_en_alerta         = 0;
            printf("[SISTEMA] Mezcla manual: contador reseteado.\n");
        }

        if (estado_actual_global == ESTADO_REPOSO)
        {
            color_verde();
            if (boton_inicio_presionado())
                estado_actual_global = ESTADO_MONITOREO;
        }
        else
        {
            /* Espiar la cola sin extraer el dato (también lo necesita tarea_pantalla) */
            if (xQueuePeek(xColaDatos, &datos_vivos, 0) == pdTRUE)
            {
                /* Suprimir alerta de mezcla si el operador ya actuó */
                if (mezcla_manual_hecha && datos_vivos.causa_alerta == 6)
                    datos_vivos.causa_alerta = 0;

                if (datos_vivos.causa_alerta == 99)
                {
                    estado_actual_global = ESTADO_ALARMA;
                }
                else if (datos_vivos.causa_alerta > 0)
                {
                    ticks_en_alerta++;
                    if (ticks_en_alerta >= (TIEMPO_MAX_PERSISTENCIA * 5))
                        estado_actual_global = ESTADO_ALARMA;
                    else
                        estado_actual_global = ESTADO_ALERTA;
                }
                else
                {
                    estado_actual_global = ESTADO_MONITOREO;
                    ticks_en_alerta = 0;
                    /* Bajar el escudo visual sólo cuando el sensor confirmó la normalidad */
                    if (seg_desde_ultima_mezcla < 5)
                        mezcla_manual_hecha = false;
                }
            }

            /* Actualizar LED RGB según estado */
            toggle_visual = !toggle_visual;
            switch (estado_actual_global)
            {
                case ESTADO_MONITOREO: color_azul();    break;
                case ESTADO_ALERTA:    toggle_visual ? color_amarillo() : color_azul();  break;
                case ESTADO_ALARMA:    toggle_visual ? color_rojo()     : color_azul();  break;
                default:               color_verde();   break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}


/* ===========================================================
 * Punto de entrada del firmware
 * =========================================================== */

/**
 * @brief Función principal del firmware (equivalente a @c main en ESP-IDF).
 *
 * Secuencia de arranque:
 * 1. Espera 1 segundo para estabilizar periféricos tras el encendido.
 * 2. Crea la cola @c xColaDatos (capacidad = 1 mensaje).
 * 3. Crea el mutex @c xMutexI2C para proteger el bus I2C.
 * 4. Lanza las tres tareas FreeRTOS con prioridad 5 y stack de 4096 bytes.
 *
 * @note Si la creación de la cola falla (RAM insuficiente), el sistema
 *       no iniciará las tareas y se quedará detenido silenciosamente.
 */
void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(1000));   /* Estabilización de periféricos */

    xColaDatos = xQueueCreate(1, sizeof(datos_fermentacion_t));
    xMutexI2C  = xSemaphoreCreateMutex();

    if (xColaDatos != NULL)
    {
        xTaskCreate(tarea_sensores,        "TareaSensores", 4096, NULL, 5, NULL);
        xTaskCreate(tarea_pantalla,        "TareaPantalla", 4096, NULL, 5, NULL);
        xTaskCreate(tarea_control_proceso, "TareaControl",  4096, NULL, 5, NULL);

        printf("[INFO] Bioreactor Popayán: Sistema listo.\n");
    }
}
