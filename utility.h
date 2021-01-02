#pragma once

#include <stdint.h>
#include <math.h>

#define FALSE 0
#define TRUE 1

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int Max(int a, int b);
int Min(int a, int b);
uint32_t BitReverse32(uint32_t value, int bitCount);
int32_t SignExtend32(int32_t value, int bits);
int16_t Clamp16(int value);
int Round(double x);

