CONTROL DE AXIS-LPC1769
Asignatura: Electrónica Digital III — Universidad Nacional de Córdoba

Integrantes: Adriel Omar Scheffer, Daniel Arias

Profesor: Marcos Javier Blasco

Descripción General del Proyecto

AXIS-LPC1769 es un sistema de posicionamiento lineal de un eje controlado desde PC. Un motor paso a paso NEMA-17 acciona una varilla roscada milimétrica que desplaza un carro a lo largo de un recorrido de (((250mm))). Un encoder incremental óptico ISC3806 montado en el extremo opuesto de la varilla provee retroalimentación de posición y velocidad en tiempo real. La comunicación con la PC se realiza vía UART, desde donde el operador puede visualizar RPM, velocidad lineal y posición del carro, así como comandar movimientos en ambas direcciones con velocidad variable seleccionada mediante potenciómetro.

Arquitectura del Sistema:


Especificaciones Eléctricas:
ParámetroValorTensión de alimentación del motor12V DC
Tensión de alimentación lógica 3.3V DC
Tensión de alimentación encoder 5V DC
Corriente máxima por fase NEMA-17 1.5A (configurada en A4988)
Corriente lógica LPC1769 (típica)~150mAPull-up
señales encoder en Resistencias externas (((2.2kΩ))) a 3.3V

Parámetros Mecánicos
Valor paso de varilla 1 mm/vuelta
Recorrido total 250 mm
Pasos del motor (paso completo) 200 pasos/vuelta
Resolución de posición (motor)0.005 mm/paso
PPR del encoder1000
Resolución de posición (encoder, 4X)0.00025 mm/cuenta

Herramientas y Entorno de Desarrollo
IDEMCUXpresso IDE v11.8SDK / Biblioteca de periféricosLPCOpen v2.10CompiladorARM GCC (incluido en MCUXpresso)
comunicacion UART:

Periféricos del LPC1769 Utilizados
QEI (Quadrature Encoder Interface): lectura de posición absoluta acumulada y velocidad angular del encoder ISC3806. Configurado en modo cuadratura 4X con timer de captura de velocidad cada 10ms.
ADC: conversión del potenciómetro en el canal AD0.0 (P0.23). El valor de 12 bits se mapea a una de 4 velocidades de crucero predefinidas.
SysTick: base de tiempo de 100µs para la generación de pulsos STEP al driver A4988. Implementa la rampa de aceleración/deceleración trapezoidal.
UART: comunicación serie con la PC a 115200 baudios para recepción de comandos y transmisión de telemetría (posición, velocidad, RPM).
NVIC: gestión de prioridades de interrupción. SysTick configurado en prioridad máxima para garantizar el timing del generador de pasos.
GPIO: control de señales STEP/DIR hacia el A4988 y lectura de finales de carrera.

Estrategia de Concurrencia ((()))
El sistema no utiliza RTOS. La concurrencia se resuelve mediante un esquema de superloop con interrupción periódica:
La interrupción del SysTick (cada 100µs) tiene prioridad máxima y se encarga exclusivamente de la generación de pulsos STEP y el cálculo de la rampa trapezoidal. Es la tarea de tiempo crítico del sistema.
El loop principal se ocupa de las tareas no críticas en tiempo: lectura del ADC, procesamiento de comandos UART y actualización de la telemetría. Al ser tareas de baja frecuencia (escala de milisegundos) no interfieren con el timing del motor.
Las variables compartidas entre la ISR y el loop principal están declaradas como volatile para evitar optimizaciones incorrectas del compilador.

Proceso de Integración y Desarrollo
Etapa 1 — Motor: se validó el generador de pasos y la rampa trapezoidal de forma aislada, verificando que el NEMA-17 no perdiera pasos en arranque y frenado a distintas velocidades.
Etapa 2 — Encoder: se integró el periférico QEI y se verificó la correcta lectura de posición y velocidad comparando con movimientos de distancia conocida sobre la varilla.
Etapa 3 — ADC: se incorporó la lectura del potenciómetro y se validó la selección de las 4 velocidades de crucero.
Etapa 4 — UART: se implementó el protocolo de comandos y la transmisión de telemetría, verificando que la comunicación serie no introdujera latencia perceptible en el control del motor.
Etapa 5 — Integración final: se ejecutaron pruebas de movimiento completo (homing + desplazamiento a posición + retorno) verificando repetibilidad de posición y ausencia de pérdida de pasos.

Posibles Etapas Siguientes
Mejora mecánica: reemplazar la varilla roscada milimétrica por un husillo a bolas recirculantes, lo que reduciría la fricción, eliminaría el backlash y permitiría velocidades de desplazamiento más altas con mayor precisión de posicionamiento.

Interfaz gráfica: desarrollar una GUI en Python con visualización de posición y velocidad en gráficos dinámicos en tiempo real, con capacidad de persistir los datos registrados en archivos para análisis posterior.

Memoria no volátil: implementar almacenamiento de la posición actual en memoria EEPROM para preservar la referencia de posición ante cortes de energía, eliminando la necesidad de ejecutar el homing al reiniciar el sistema.

Lazo cerrado PID: evolucionar el control actual (lazo abierto con verificación por encoder) a un controlador PID completo que corrija desviaciones de posición en tiempo real ante perturbaciones externas como carga variable sobre el carro.

PCB dedicada: migrar el prototipo de protoboard a un circuito impreso con separación de planos de masa digital y de potencia, desacople distribuido y ruteo diferencial de las señales del encoder, cumpliendo con criterios básicos de compatibilidad electromagnética (EMC).











