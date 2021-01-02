#pragma once

#include <stdint.h>
//#include "unpack.h"

#if 0
At9Status CreateGradient(Block* block);
void CalculateMask(Channel* channel);
void CalculatePrecisions(Channel* channel);
#endif
extern const uint8_t gradientCurves[50][50];
void GenerateGradientCurves();
