#pragma once

#include "common.h"
#include "config.h"

// The main rendering thread function, now with a new parameter for shared configuration.
void RenderThread(VisualizerData& sharedVisualizerData, SharedConfigData& sharedConfigData);

