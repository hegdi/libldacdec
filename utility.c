#include "utility.h"
#include <limits.h>

int Max(int a, int b) { return a > b ? a : b; }
int Min(int a, int b) { return a > b ? b : a; }

uint32_t BitReverse32(uint32_t value, int bitCount)
{
	value = ((value & 0xaaaaaaaa) >> 1) | ((value & 0x55555555) << 1);
	value = ((value & 0xcccccccc) >> 2) | ((value & 0x33333333) << 2);
	value = ((value & 0xf0f0f0f0) >> 4) | ((value & 0x0f0f0f0f) << 4);
	value = ((value & 0xff00ff00) >> 8) | ((value & 0x00ff00ff) << 8);
	value = (value >> 16) | (value << 16);
	return value >> (32 - bitCount);
}

int32_t SignExtend32(int32_t value, int bits)
{
	const int shift = 8 * sizeof(int32_t) - bits;
	return (value << shift) >> shift;
}

int16_t Clamp16(int value)
{
	if (value > SHRT_MAX)
		return SHRT_MAX;
	if (value < SHRT_MIN)
		return SHRT_MIN;
	return (int16_t)value;
}

int Round(double x)
{
	x += 0.5;
	return (int)x - (x < (int)x);
}

