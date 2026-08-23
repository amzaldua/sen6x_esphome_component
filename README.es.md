# Componente SEN6x para ESPHome

🌐 **Idiomas**: [English](README.md) | Español | Otros idiomas (por demanda vía Issues)

Componente ESPHome para la familia de sensores ambientales **Sensirion SEN6x**.

## Resumen

- Integración directa con ESPHome para sensores Sensirion SEN6x
- Sin scores IAQ propios ni interpretaciones de salud
- VOC/NOx expuestos estrictamente como índices Sensirion (indicadores relativos)
- Estimaciones TVOC opcionales según AppNotes oficiales de Sensirion
- Probado en hardware SEN66

## ¿Qué es esto?

Este componente proporciona integración directa de sensores Sensirion SEN6x con ESPHome y Home Assistant. Expone los datos del sensor tal como se documentan en la hoja de datos oficial de Sensirion, con estimaciones TVOC opcionales basadas en la Nota de Aplicación oficial de Sensirion "Compliance of Sensirion's VOC Sensors with Building Standards" (incluida en [docs/reference](docs/reference/AQI_BuildingStandards.pdf)).

### Principios de Diseño

- **Datos directos del sensor**: Las mediciones principales provienen directamente del hardware SEN6x
- **Solo derivaciones documentadas**: Los valores adicionales se derivan estrictamente usando AppNotes oficiales de Sensirion
- **Sin algoritmos propios**: Sin scores IAQ propietarios ni interpretaciones de salud
- **Comportamiento transparente**: Lo que configuras es lo que obtienes

## Modelos Soportados

| Modelo  | PM   | T/RH | VOC  | NOx  | CO2  | HCHO |
|---------|:----:|:----:|:----:|:----:|:----:|:----:|
| SEN62   | ✓    | ✓    | –    | –    | –    | –    |
| SEN63C  | ✓    | ✓    | –    | –    | ✓    | –    |
| SEN65   | ✓    | ✓    | ✓    | ✓    | –    | –    |
| SEN66   | ✓    | ✓    | ✓    | ✓    | ✓    | –    |
| SEN68   | ✓    | ✓    | ✓    | ✓    | –    | ✓    |
| SEN69C  | ✓    | ✓    | ✓    | ✓    | ✓    | ✓    |

El modelo se **auto-detecta** del sensor en tiempo de ejecución.

> **Nota**: La disponibilidad de características depende del modelo. Ver [Estado de Pruebas](#estado-de-pruebas) para detalles de verificación.

## Conexión del Hardware

### Pinout del SEN6x

| Pin SEN6x | Función | Conexión ESP32 |
|-----------|---------|----------------|
| 1 (VDD) | Alimentación | 3.3V o 5V |
| 2 (GND) | Tierra | GND |
| 3 (SDA) | Datos I2C | GPIO21 (o cualquier SDA) |
| 4 (SCL) | Reloj I2C | GPIO22 (o cualquier SCL) |
| 5 (SEL) | Selección de interfaz | GND (para modo I2C) |
| 6 (NC) | No conectado | – |

> **Nota**: El SEN6x opera con lógica de 3.3V pero acepta alimentación de 5V. Las líneas I2C no deben exceder niveles lógicos de 3.3V. El pin 5 (SEL) debe conectarse a GND para modo I2C.

### Resistencias Pull-up Recomendadas

El bus I2C requiere resistencias pull-up en las líneas SDA y SCL. Muchas placas de desarrollo ESP32 incluyen pull-ups internos, pero para operación fiable:
- Usar resistencias pull-up externas de 4.7kΩ a 3.3V si hay problemas de comunicación

> **Importante**: Verifica la asignación de pines I2C de tu placa ESP32 específica. GPIO21/GPIO22 son los valores típicos pero pueden variar. Para especificaciones eléctricas completas del SEN6x, consulta la [hoja de datos oficial](docs/reference/datasheet.pdf).

## Instalación

Añade esto a tu YAML de ESPHome:

```yaml
external_components:
  - source: github://amzaldua/sen6x_esphome_component
    components: [sen6x]

i2c:
  sda: GPIO21
  scl: GPIO22

sen6x:
  id: sen6x_sensor
  address: 0x6B
  update_interval: 10s
```

## Configuración Básica

### Ejemplo Mínimo

```yaml
sensor:
  - platform: sen6x
    pm_2_5:
      name: "PM 2.5"
    temperature:
      name: "Temperatura"
    humidity:
      name: "Humedad"
    voc_index:
      name: "Índice VOC"
    co2:
      name: "CO2"
```

## Sensores Disponibles

### Datos Directos del Sensor

| Sensor | Unidad | Descripción |
|--------|--------|-------------|
| `pm_1_0` | µg/m³ | Material particulado ≤1.0 µm |
| `pm_2_5` | µg/m³ | Material particulado ≤2.5 µm |
| `pm_4_0` | µg/m³ | Material particulado ≤4.0 µm |
| `pm_10_0` | µg/m³ | Material particulado ≤10.0 µm |
| `temperature` | °C | Temperatura ambiente |
| `humidity` | % | Humedad relativa |
| `voc_index` | – | Índice VOC (1–500) |
| `nox_index` | – | Índice NOx (1–500) |
| `co2` | ppm | Concentración de CO2 |
| `formaldehyde` | ppb | Concentración de formaldehído (solo SEN68 / SEN69C) |
| `nc_0_5` a `nc_10_0` | #/cm³ | Concentración numérica |

### Estimaciones Derivadas (Calculadas, no medidas)

| Sensor | Unidad | Fuente |
|--------|--------|--------|
| `tvoc_well` | µg/m³ | Estándar WELL Building (equivalente Mølhave) |
| `tvoc_reset` | µg/m³ | Estándar RESET Air (equivalente Isobutileno) |
| `tvoc_ethanol` | ppb | Equivalente etanol |

> **Nota**: Estos valores no son mediciones directas del sensor. Se calculan usando las fórmulas oficiales de la Nota de Aplicación de Sensirion "AQI_BuildingStandards" (Marzo 2023).

## Limitaciones

### VOC y NOx son Índices, No Concentraciones

El Índice VOC y el Índice NOx son **indicadores relativos** (escala 1–500), no concentraciones absolutas de gases. Según la documentación de Sensirion:

- Índice = 100 representa la línea base promedio
- Índice > 100 indica niveles de VOC/NOx por encima del promedio
- Índice < 100 indica niveles por debajo del promedio

Los índices requieren un período de calentamiento inicial después del encendido y una fase de aprendizaje más larga para estabilidad óptima de la línea base, como se describe en la documentación de Sensirion.

### Estimaciones TVOC

Los sensores `tvoc_well`, `tvoc_reset` y `tvoc_ethanol` proporcionan concentraciones TVOC **estimadas** basadas en calibración de laboratorio con etanol. Según la AppNote de Sensirion:

> "Este enfoque es solo una simplificación ya que las composiciones reales de gases en interiores pueden variar significativamente con el tiempo y de un entorno a otro."

### Sin Evaluaciones de Salud

Este componente **no** proporciona:
- Evaluaciones de riesgo para la salud
- Puntuaciones o clasificaciones de calidad del aire
- Recomendaciones médicas

Los datos del sensor deben ser interpretados por profesionales cualificados cuando se usen para decisiones relacionadas con la salud.

## Estado de Pruebas

> **Importante**: Este componente ha sido desarrollado y probado únicamente en hardware **SEN66**.

| Modelo | Estado |
|--------|--------|
| SEN62 | ⚠️ No probado – comandos implementados según datasheet, no validados en hardware |
| SEN63C | ⚠️ No probado – comandos implementados según datasheet, no validados en hardware |
| SEN65 | ⚠️ No probado – comandos implementados según datasheet, no validados en hardware |
| **SEN66** | ✅ **Probado y verificado** |
| SEN68 | ⚠️ No probado – comandos implementados según datasheet, no validados en hardware |
| SEN69C | ⚠️ No probado – comandos implementados según datasheet, no validados en hardware |

### Notas

- Todos los comandos I2C han sido auditados contra la hoja de datos oficial de Sensirion
- Los modelos con formaldehído (SEN68, SEN69C) están implementados estrictamente según especificaciones del datasheet
- Al momento de escribir esto, SEN68 y SEN69C tienen disponibilidad limitada en el mercado
- Las pruebas y comentarios de la comunidad para otros modelos son bienvenidos

## Documentación

Ver la carpeta `/docs` para información detallada:

- [sensors.md](docs/sensors.md) – Especificaciones detalladas de sensores
- [voc_nox_indexes.md](docs/voc_nox_indexes.md) – Entendiendo los índices VOC/NOx
- [co2_calibration.md](docs/co2_calibration.md) – Guía de calibración CO2
- [home_assistant_presentation.md](docs/home_assistant_presentation.md) – Recomendaciones de dashboard

## Referencias

Este componente fue desarrollado usando la siguiente documentación oficial de Sensirion (obtenida de la página del producto SEN69C):

| Documento | Versión Referenciada | Incluido |
|-----------|---------------------|----------|
| Datasheet SEN6x | v0.92 (Diciembre 2025) | [docs/reference/datasheet.pdf](docs/reference/datasheet.pdf) |
| AppNote AQI Building Standards | v1.1 (Septiembre 2023) | [docs/reference/AQI_BuildingStandards.pdf](docs/reference/AQI_BuildingStandards.pdf) |

Páginas de productos online:
- [Familia de productos SEN6x](https://sensirion.com/search/products?q=SEN6x)
- [SEN66 (modelo probado)](https://sensirion.com/products/catalog/SEN66)
- [SEN69C (fuente del datasheet)](https://sensirion.com/products/catalog/SEN69C)

## Licencia

Licencia MIT

## Estado

**Estable** – Probado y verificado en hardware SEN66. Otros modelos requieren validación de la comunidad.
