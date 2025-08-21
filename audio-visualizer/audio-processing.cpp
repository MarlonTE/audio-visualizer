#include "audio-processing.h"
#include "config.h"
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

        for (int i = 0; i < NUM_BARS; ++i) {
            double magnitude = sqrt(fft_out[i][0] * fft_out[i][0] + fft_out[i][1] * fft_out[i][1]);

            // Implementación de la reactividad logarítmica para una respuesta más balanceada.
            double scaled_value = 10.0 * log10(1 + magnitude);

            sharedVisualizerData.out_data[write_index][i] = scaled_value;
        }

        sharedVisualizerData.write_buffer_index.store(1 - write_index);
        sharedVisualizerData.cv.notify_one();
    }

    fftw_destroy_plan(plan_forward);
    fftw_free(fft_out);
}
