#include "audio-processing.h"
#include <iostream>
#include <fftw3.h> // Se asume que has instalado la biblioteca FFTW3
#include <cmath> // Para log10 y sqrt

// Función principal del hilo de procesamiento de audio
void AudioProcessingThread(AudioData& sharedData, VisualizerData& sharedVisualizerData) {
    std::cout << "Hilo de procesamiento de señal iniciado." << std::endl;

    fftw_plan plan_forward;
    fftw_complex* fft_out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * (FFT_SIZE / 2 + 1));
    if (fft_out == NULL) {
        std::cerr << "Error: No se pudo asignar memoria para fft_out." << std::endl;
        return;
    }

    std::vector<double> dummy_in(FFT_SIZE);
    plan_forward = fftw_plan_dft_r2c_1d(FFT_SIZE, dummy_in.data(), fft_out, FFTW_MEASURE);

    while (true) {
        std::unique_lock<std::mutex> lock(sharedData.mtx);
        sharedData.cv.wait(lock);

        fftw_execute_dft_r2c(plan_forward, sharedData.in_data.data(), fft_out);

        lock.unlock();

        int write_index = sharedVisualizerData.write_buffer_index.load();

        // Optimización: El mutex aquí era redundante.
        // Se ha eliminado porque la técnica de doble búfer y el índice atómico
        // ya previenen las condiciones de carrera al escribir en el búfer.

        for (int i = 0; i < NUM_BARS; ++i) {
            double magnitude = sqrt(fft_out[i][0] * fft_out[i][0] + fft_out[i][1] * fft_out[i][1]);
            double scaled_value = 10.0 * log10(1 + magnitude);

            scaled_value = std::min(scaled_value, 50.0);
            scaled_value = std::max(scaled_value, 0.0);

            sharedVisualizerData.out_data[write_index][i] = scaled_value;
        }

        // Alternar el índice del búfer para que el hilo de renderizado lea del búfer que acaba de ser escrito.
        sharedVisualizerData.write_buffer_index.store(1 - write_index);

        // Notificar al hilo de renderizado que hay nuevos datos disponibles.
        sharedVisualizerData.cv.notify_one();
    }

    fftw_destroy_plan(plan_forward);
    fftw_free(fft_out);
}
