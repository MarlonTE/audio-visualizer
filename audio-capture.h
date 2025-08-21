#pragma once

#include "common.h"

// Prototypes of the functions in audio-capture.cpp
// This is the declaration that the compiler needs to find when compiling main.cpp.
void AudioCaptureThread(AudioData& sharedData, VisualizerData& visualizerData);
