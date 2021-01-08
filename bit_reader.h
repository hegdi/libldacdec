#pragma once

#include <stdint.h>

typedef struct {
	const uint8_t * Buffer;
	int Position;
} BitReaderCxt;

// Make MSVC compiler happy. Leave const in for value parameters

void InitBitReaderCxt(BitReaderCxt* br, const void * buffer);
uint32_t PeekInt(BitReaderCxt* br, const int bits);
uint32_t ReadInt(BitReaderCxt* br, const int bits);
int32_t ReadSignedInt(BitReaderCxt* br, const int bits);
int32_t ReadOffsetBinary(BitReaderCxt* br, const int bits);
void AlignPosition(BitReaderCxt* br, const unsigned int multiple);
