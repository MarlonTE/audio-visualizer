#include "config.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

// Usar el alias para simplificar el código
using json = nlohmann::json;

// Implementación de la función para cargar la configuración.
void LoadConfig(SharedConfigData& sharedConfigData, const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo de configuración: " << filename << std::endl;
        return;
    }

    json data;
    try {
        file >> data;

        // Cargar los valores desde el JSON a la estructura de configuración.
        std::unique_lock<std::mutex> lock(sharedConfigData.mtx);
        if (data.contains("estilos")) {
            const auto& estilos = data["estilos"];
            if (estilos.contains("decay_factor")) {
                sharedConfigData.config.decay_factor = estilos["decay_factor"].get<float>();
            }
            if (estilos.contains("smoothing_factor")) {
                sharedConfigData.config.smoothing_factor = estilos["smoothing_factor"].get<float>();
            }
            if (estilos.contains("amplitude_factor")) {
                sharedConfigData.config.amplitude_factor = estilos["amplitude_factor"].get<float>();
            }
            if (estilos.contains("reactivity_factor")) {
                sharedConfigData.config.reactivity_factor = estilos["reactivity_factor"].get<float>();
            }
            if (estilos.contains("base_color_rgb")) {
                sharedConfigData.config.base_color_rgb = estilos["base_color_rgb"].get<std::vector<float>>();
                if (sharedConfigData.config.base_color_rgb.size() != 3) {
                    sharedConfigData.config.base_color_rgb = { 0.0f, 0.9f, 0.3f }; // Valor por defecto
                }
            }
            // Leer el nuevo factor de agrupamiento
            if (estilos.contains("bin_grouping_factor")) {
                sharedConfigData.config.bin_grouping_factor = estilos["bin_grouping_factor"].get<float>();
            }
            else {
                sharedConfigData.config.bin_grouping_factor = 10.0f; // Valor por defecto
            }
        }
    }
    catch (const json::parse_error& e) {
        std::cerr << "Error de parseo del JSON en el archivo " << filename << ": " << e.what() << std::endl;
    }
}
