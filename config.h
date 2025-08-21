#pragma once

#include <vector>
#include <mutex> // Incluye el encabezado para std::mutex
#include <string> // Incluye el encabezado para std::string

// Estructura para almacenar la configuración del visualizador.
// Esto permite que el visualizador lea los valores de un archivo
// de configuración externo, haciendo los estilos más flexibles.
struct VisualizerConfig {
    // Factor de decaimiento para que las barras caigan de forma suave
    float decay_factor;
    // Factor de suavizado para un movimiento tipo "ola" entre las barras
    float smoothing_factor;
    // Factor de amplitud para controlar la altura de las barras
    float amplitude_factor;
    // Factor de reactividad para ajustar la respuesta logarítmica
    float reactivity_factor;
    // Color base en formato RGB
    std::vector<float> base_color_rgb;
    // Factor de agrupamiento de bins para controlar el ancho de banda por barra.
    float bin_grouping_factor;
};

// Estructura de datos compartida para pasar la configuración entre hilos.
// El mutex asegura que el acceso a la configuración sea seguro,
// especialmente si se actualiza dinámicamente.
struct SharedConfigData {
    VisualizerConfig config;
    std::mutex mtx;
};

// Prototipo de la función que carga la configuración desde un archivo JSON.
void LoadConfig(SharedConfigData& sharedConfigData, const std::string& filename);

