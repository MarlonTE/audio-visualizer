#include "audio-capture.h"
#include <iostream>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <stdio.h>
#include <vector>
#include <chrono>

#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "ole32.lib")

// Constantes globales de audio
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

// Función principal del hilo de captura de audio (WASAPI)
void AudioCaptureThread(AudioData& sharedData, VisualizerData& visualizerData) {
    HRESULT hr = S_OK;
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    IAudioClient* pAudioClient = NULL;
    IAudioCaptureClient* pCaptureClient = NULL;
    WAVEFORMATEX* pwfx = NULL;

    // Buffer temporal para evitar la reasignación de memoria.
    std::vector<double> local_buffer(FFT_SIZE);
    int buffer_position = 0;

    // 1. Inicializar la biblioteca COM para el hilo.
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "Error: No se pudo inicializar COM." << std::endl;
        goto cleanup;
    }

    // 2. Obtener un enumerador de dispositivos de audio.
    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator, NULL,
        CLSCTX_ALL, IID_IMMDeviceEnumerator,
        (void**)&pEnumerator);
    if (FAILED(hr)) {
        std::cerr << "Error: No se pudo crear el enumerador de dispositivos." << std::endl;
        goto cleanup;
    }

    // 3. Obtener el dispositivo de renderizado de audio por defecto.
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr)) {
        std::cerr << "Error: No se pudo obtener el dispositivo de audio por defecto." << std::endl;
        goto cleanup;
    }

    // 4. Activar el cliente de audio en el dispositivo de audio.
    hr = pDevice->Activate(
        IID_IAudioClient, CLSCTX_ALL,
        NULL, (void**)&pAudioClient);
    if (FAILED(hr)) {
        std::cerr << "Error: No se pudo activar el cliente de audio." << std::endl;
        goto cleanup;
    }

    // 5. Obtener el formato de onda del dispositivo.
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) {
        std::cerr << "Error: No se pudo obtener el formato de mezcla." << std::endl;
        goto cleanup;
    }

    // 6. Inicializar el cliente de audio para la captura de un loopback.
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK, // Captura de la salida de audio del sistema
        0, 0, pwfx, NULL);
    if (FAILED(hr)) {
        std::cerr << "Error: No se pudo inicializar el cliente de audio para loopback." << std::endl;
        goto cleanup;
    }

    // 7. Obtener el cliente de captura de audio.
    hr = pAudioClient->GetService(
        IID_IAudioCaptureClient,
        (void**)&pCaptureClient);
    if (FAILED(hr)) {
        std::cerr << "Error: No se pudo obtener el cliente de captura." << std::endl;
        goto cleanup;
    }

    hr = pAudioClient->Start();
    if (FAILED(hr)) {
        std::cerr << "Error: No se pudo iniciar el cliente de audio." << std::endl;
        goto cleanup;
    }

    std::cout << "Hilo de captura de audio iniciado." << std::endl;

    // Bucle principal que lee los datos de audio.
    while (!visualizerData.should_terminate.load()) {
        UINT32 numFramesInPacket = 0;
        hr = pCaptureClient->GetNextPacketSize(&numFramesInPacket);

        if (numFramesInPacket == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        BYTE* pData;
        UINT32 numFramesToRead;
        DWORD dwFlags;

        hr = pCaptureClient->GetBuffer(&pData, &numFramesToRead, &dwFlags, NULL, NULL);
        if (FAILED(hr)) {
            continue;
        }

        if (!(dwFlags & AUDCLNT_BUFFERFLAGS_SILENT)) {
            const float* pFloatData = reinterpret_cast<const float*>(pData);
            for (UINT32 i = 0; i < numFramesToRead; ++i) {
                if (buffer_position < FFT_SIZE) {
                    local_buffer[buffer_position++] = static_cast<double>(pFloatData[i]);
                }
                else {
                    break;
                }
            }
        }

        pCaptureClient->ReleaseBuffer(numFramesToRead);

        if (buffer_position >= FFT_SIZE) {
            std::unique_lock<std::mutex> lock(sharedData.mtx);
            std::copy(local_buffer.begin(), local_buffer.begin() + FFT_SIZE, sharedData.in_data.begin());
            lock.unlock();

            sharedData.cv.notify_one();
            buffer_position = 0;
        }
    }

cleanup:
    // Limpieza de recursos COM y de la memoria.
    if (pEnumerator) pEnumerator->Release();
    if (pDevice) pDevice->Release();
    if (pAudioClient) pAudioClient->Release();
    if (pCaptureClient) pCaptureClient->Release();
    if (pwfx) CoTaskMemFree(pwfx);
    CoUninitialize();
}
