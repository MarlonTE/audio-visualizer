#pragma once

#include "common.h"

// Prototypes of the functions in audio-processing.cpp
// This is the declaration that the compiler needs to find when compiling main.cpp.
void AudioProcessingThread(AudioData& sharedData, VisualizerData& sharedVisualizerData);

