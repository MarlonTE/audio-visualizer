#pragma once

#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "config.h"

// Tamaño de la ventana de la FFT.
const int FFT_SIZE = 4096;

// Estructura de datos compartida entre el hilo de captura y el de procesamiento
struct AudioData {
    std::mutex mtx;
    std::condition_variable cv;
    std::vector<double> in_data; // Vector para almacenar la entrada de audio
};

// Estructura de datos compartida entre el hilo de procesamiento y el de renderizado
struct VisualizerData {
    std::mutex mtx;
    std::condition_variable cv;
    // Dos búferes para la técnica de doble amortiguación
    std::vector<double> out_data[2];
    // Índice atómico para indicar qué búfer es el que se está escribiendo actualmente
    std::atomic<int> write_buffer_index;
    // Variable atómica para comunicar el número de barras entre hilos
    std::atomic<int> atomic_num_bars;
};
