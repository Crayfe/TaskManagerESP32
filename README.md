# Task Manager ESP32

Sistema de gestión de tareas programables basado en ESP32 con pantalla OLED, tarjeta SD y periféricos de control. Diseñado para automatizaciones en el hogar, proyectos de cultivo inteligente o cualquier sistema que requiera tareas programadas con relativa flexibilidad.

---

## Características destacadas

- 📅 **Programación de tareas desde archivo SD**
- 🧠 **Tipos de tareas personalizadas**: motor, sensor, alarma...
- 🌐 **Sincronización horaria vía NTP**
- 📟 **Pantalla OLED integrada** con visualización en tiempo real
- 🧰 **Expansible y modular**: código pensado para escalar
- 🔧 **WiFi configurable sin reprogramar**, leyendo credenciales desde la SD
- 🌡️ **Lectura de sensor DHT22** para temperatura y humedad
- 📢 **Alarmas acústicas** mediante zumbador (piezoeléctrico)

---

## ⚙️ Hardware necesario
Para el desarrollo de este proyecto se está utilizando una placa de desarrollo casera donde se integra la mayor parte del hardware. Entre todo suma:
- ESP32
- Pantalla OLED I2C (128x64, SSD1306)
- Módulo lector de tarjeta microSD
- Sensor DHT22
- Piezoeléctrico
- Montaje Darlington con transistor TIP120 para controlar motores o bombas (pin 17)
- Encoder rotativo y pulsadores

---

## 🗂️ Estructura de archivos en tarjeta SD

### `wifi_config.txt`
Este archivo permite almacenar y cambiar una red WiFi sin necesidad de reprogramar el ESP32.

Estructura:
```txt
ssid=TU_SSID
password=TU_PASSWORD
```


### `tasks.txt`
Este archivo permite almacenar tareas que luego el software es capaz de parsear y ejecutar, pudiendo editar y añadir las tareas que se deseen.

El formato de las tareas es el siguiente:
```
LABEL;TYPE;ENABLED;DD-MM-YYYY;HH:MM;REPEAT_SEC;[EXTRA_PARAMS]
```

#### Campos comunes

- LABEL:	Nombre de la tarea
- TYPE	Tipo: motor, alarm, sensor
- ENABLED	true o false
- DD-MM-YYYY	Fecha de ejecución
- HH:MM	Hora de ejecución
- REPEAT_SEC	Intervalo de repetición en segundos (0 = única vez)

#### Parámetros adicionales por tipo [EXTRA_PARAMS]
- motor: duración en segundos
- alarm: duración del sonido en segundos
- sensor: sin parámetros adicionales

#### Ejemplo:
```
Riego diario;motor;true;10-06-2025;08:00;86400;15
Alarma matutina;alarm;true;11-06-2025;07:30;86400;3
Medición temp;sensor;true;10-06-2025;09:00;3600
```

---

## 🧠 Lógica de funcionamiento
1. Al iniciar, el sistema:
	- Carga credenciales WiFi desde la SD.
	- Se conecta y sincroniza la hora por NTP.
	- Carga las tareas programadas desde tasks.txt.
2. Cada segundo que pasa:
	- Verifica si hay tareas pendientes de ejecutar.
	- Ejecuta la tarea cuando corresponde.
	- La reprograma si es periódica
	- Actualiza la pantalla OLED

---

### 🖥️ Visualización OLED
La pantalla muestra:
- Hora actual (sincronizada por NTP).
- Las próximas 2 tareas programadas (no entran más).
- Durante la ejecución de una tarea se muestra el label de la tarea que se está ejecutando.

---

## 🛠️ ¿Futuras mejoras?
- Mejorar la implementación de diferentes tareas.
- Interfaz para modificar tareas desde el propio ESP32 con un menú interactivo mostrable en la OLED y controlado con un encoder rotativo.
- Habilitar un servidor web para hacer una configuración de tareas remota.
- Dividir el código del proyecto en múltiples archivos (.h / .cpp) para mejorar el mantenimiento del código.
- Log de ejecuciones y de cambios de estado en SD.
- Sustituir o complementar NTP con un RTC.
- Gestión de energía con baterías externas y aprovechar el modo deep sleep del ESP32.

