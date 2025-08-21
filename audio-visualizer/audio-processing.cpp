#include "audio-processing.h"
#include "config.h"
#include <iostream>
#include <fftw3.h>
#include <cmath>
#include <numeric>

// Frecuencia de muestreo (Sample rate) del audio.
// Este valor es crucial para el cálculo de la resolución de la FFT.
const double SAMPLE_RATE = 44100.0;

// Función principal del hilo de procesamiento de audio
void AudioProcessingThread(AudioData& sharedData, VisualizerData& sharedVisualizerData, SharedConfigData& sharedConfigData) {
    std::cout << "Hilo de procesamiento de señal iniciado." << std::endl;

    fftw_plan plan_forward;
    fftw_complex* fft_out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * (FFT_SIZE / 2 + 1));
    if (fft_out == NULL) {
        std::cerr << "Error: No se pudo asignar memoria para fft_out." << std::endl;
        return;
    }

    std::vector<double> dummy_in(FFT_SIZE);
    plan_forward = fftw_plan_dft_r2c_1d(FFT_SIZE, dummy_in.data(), fft_out, FFTW_MEASURE);

    while (!sharedVisualizerData.should_terminate.load()) {
        std::unique_lock<std::mutex> lock(sharedData.mtx);
        sharedData.cv.wait(lock);

        if (sharedVisualizerData.should_terminate.load()) {
            break;
        }

        fftw_execute_dft_r2c(plan_forward, sharedData.in_data.data(), fft_out);

        lock.unlock();

        int write_index = sharedVisualizerData.write_buffer_index.load();

        // Obtener el número de barras de la variable atómica para el procesamiento.
        int num_bars = sharedVisualizerData.atomic_num_bars.load();

        // Obtener el factor de agrupamiento de la configuración.
        std::unique_lock<std::mutex> config_lock(sharedConfigData.mtx);
        float bin_grouping_factor = sharedConfigData.config.bin_grouping_factor;
        config_lock.unlock();

        // Calcular la resolución de frecuencia por bin de la FFT.
        const double bin_resolution = SAMPLE_RATE / FFT_SIZE;
        const double bins_per_bar = bin_grouping_factor / bin_resolution;

        // Limpiar el búfer antes de escribir en él
        sharedVisualizerData.out_data[write_index].resize(num_bars, 0.0);
        std::fill(sharedVisualizerData.out_data[write_index].begin(), sharedVisualizerData.out_data[write_index].end(), 0.0);

        for (int i = 0; i < num_bars; ++i) {
            double total_magnitude = 0.0;

            // Determinar los índices de los bins de la FFT a agrupar.
            int start_bin = static_cast<int>(i * bins_per_bar);
            int end_bin = static_cast<int>((i + 1) * bins_per_bar);

            // Asegurarse de no exceder los límites del array de la FFT.
            if (end_bin > FFT_SIZE / 2) {
                end_bin = FFT_SIZE / 2;
            }

            // Sumar las magnitudes de los bins de la FFT correspondientes a esta barra.
            for (int j = start_bin; j < end_bin; ++j) {
                total_magnitude += sqrt(fft_out[j][0] * fft_out[j][0] + fft_out[j][1] * fft_out[j][1]);
            }

            // Aplicar la escala logarítmica y otros factores.
            double scaled_value = 10.0 * log10(1 + total_magnitude);
            scaled_value = std::min(scaled_value, 50.0);
            scaled_value = std::max(scaled_value, 0.0);

            if (i < sharedVisualizerData.out_data[write_index].size()) {
                sharedVisualizerData.out_data[write_index][i] = scaled_value;
            }
        }

        sharedVisualizerData.write_buffer_index.store(1 - write_index);
        sharedVisualizerData.cv.notify_one();
    }

    fftw_destroy_plan(plan_forward);
    fftw_free(fft_out);
}
