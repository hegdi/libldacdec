#include "huffCodes.h"
#include "utility.h"
#include <stdint.h>

int ReadHuffmanValue(const HuffmanCodebook* huff, BitReaderCxt* br, int isSigned)
{
	const int code = PeekInt(br, huff->MaxBitSize);
	const unsigned char value = huff->Lookup[code];
	const int bits = huff->Bits[value];
	br->Position += bits;
	return isSigned ? SignExtend32(value, huff->ValueBits) : value;
}

void DecodeHuffmanValues(int* spectrum, int index, int bandCount, const HuffmanCodebook* huff, const int* values)
{
	const int valueCount = bandCount >> huff->ValueCountPower;
	const int mask = (1 << huff->ValueBits) - 1;

	for (int i = 0; i < valueCount; i++)
	{
		int value = values[i];
		for (int j = 0; j < huff->ValueCount; j++)
		{
			spectrum[index++] = SignExtend32(value & mask, huff->ValueBits);
			value >>= huff->ValueBits;
		}
	}
}

void InitHuffmanCodebook(const HuffmanCodebook* codebook)
{
	const int huffLength = codebook->Length;
	if (huffLength == 0) return;

	unsigned char* dest = codebook->Lookup;

	for (int i = 0; i < huffLength; i++)
	{
		if (codebook->Bits[i] == 0) continue;
		const int unusedBits = codebook->MaxBitSize - codebook->Bits[i];

		const int start = codebook->Codes[i] << unusedBits;
		const int length = 1 << unusedBits;
		const int end = start + length;

		for (int j = start; j < end; j++)
		{
			dest[j] = i;
		}
	}
}

static const uint8_t ScaleFactorsA3Bits[8] =
{
	2, 2, 4, 6, 6, 5, 3, 2
};

static const uint16_t ScaleFactorsA3Codes[8] =
{
	0x00, 0x01, 0x0E, 0x3E, 0x3F, 0x1E, 0x06, 0x02
};

static const uint8_t ScaleFactorsA4Bits[16] =
{
	2, 2, 4, 5, 6, 7, 8, 8, 8, 8, 8, 8, 6, 5, 4, 2
};

static const uint16_t ScaleFactorsA4Codes[16] =
{
	0x01, 0x02, 0x00, 0x06, 0x0F, 0x13, 0x23, 0x24, 0x25, 0x22, 0x21, 0x20, 0x0E, 0x05, 0x01, 0x03
};

static const uint8_t ScaleFactorsA5Bits[32] =
{
	2, 3, 3, 4, 5, 5, 6, 7, 7, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 6, 5, 5, 4, 3
};

static const uint16_t ScaleFactorsA5Codes[32] =
{
	0x02, 0x01, 0x07, 0x0D, 0x0C, 0x18, 0x1B, 0x21, 0x3F, 0x6A, 0x6B, 0x68, 0x73, 0x79, 0x7C, 0x7D,
	0x7A, 0x7B, 0x78, 0x72, 0x44, 0x45, 0x47, 0x46, 0x69, 0x38, 0x20, 0x1D, 0x19, 0x09, 0x05, 0x00
};

static const uint8_t ScaleFactorsA6Bits[64] =
{
	3, 3, 4, 4, 5, 5, 6, 6, 6, 7, 7, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 7, 7, 7, 6, 6, 5, 5, 5, 4, 4, 4
};

static const uint16_t ScaleFactorsA6Codes[64] =
{
	0x00, 0x01, 0x04, 0x05, 0x12, 0x13, 0x2E, 0x2F, 0x30, 0x66, 0x67, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
	0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
	0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA,
	0xFB, 0xFC, 0xFD, 0xFE, 0xFF, 0x68, 0x69, 0x6A, 0x31, 0x32, 0x14, 0x15, 0x16, 0x06, 0x07, 0x08
};

static const uint8_t ScaleFactorsB2Bits[4] =
{
	1, 2, 0, 2
};

static const uint16_t ScaleFactorsB2Codes[4] =
{
	0x00, 0x03, 0x00, 0x02
};

static const uint8_t ScaleFactorsB3Bits[8] =
{
	1, 3, 5, 6, 0, 6, 4, 2
};

static const uint16_t ScaleFactorsB3Codes[8] =
{
	0x01, 0x00, 0x04, 0x0B, 0x00, 0x0A, 0x03, 0x01
};

static const uint8_t ScaleFactorsB4Bits[16] =
{
	1, 3, 4, 5, 5, 7, 8, 8, 0, 8, 8, 7, 6, 6, 4, 3
};

static const uint16_t ScaleFactorsB4Codes[16] =
{
	0x01, 0x01, 0x04, 0x0E, 0x0F, 0x2C, 0x5A, 0x5D, 0x00, 0x5C, 0x5B, 0x2F, 0x15, 0x14, 0x06, 0x00
};

static const uint8_t ScaleFactorsB5Bits[32] =
{
	3, 3, 4, 4, 4, 4, 4, 4, 4, 5, 6, 7, 7, 7, 8, 8,
	8, 8, 8, 8, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 6, 3
};

static const uint16_t ScaleFactorsB5Codes[32] =
{
	0x00, 0x05, 0x07, 0x0C, 0x04, 0x02, 0x03, 0x05, 0x09, 0x10, 0x23, 0x33, 0x36, 0x6E, 0x60, 0x65,
	0x62, 0x61, 0x63, 0x64, 0x6F, 0x6D, 0x6C, 0x6B, 0x6A, 0x68, 0x69, 0x45, 0x44, 0x37, 0x1A, 0x07
};

static uint8_t ScaleFactorsA3Lookup[64];
static uint8_t ScaleFactorsA4Lookup[256];
static uint8_t ScaleFactorsA5Lookup[256];
static uint8_t ScaleFactorsA6Lookup[256];

static uint8_t ScaleFactorsB2Lookup[4];
static uint8_t ScaleFactorsB3Lookup[64];
static uint8_t ScaleFactorsB4Lookup[256];
static uint8_t ScaleFactorsB5Lookup[256];

HuffmanCodebook HuffmanScaleFactorsUnsigned[7] = {
	{0},
    {0},
    {0},
    {ScaleFactorsA3Bits, ScaleFactorsA3Codes, ScaleFactorsA3Lookup,  8, 1, 0, 3,  8, 6},
	{ScaleFactorsA4Bits, ScaleFactorsA4Codes, ScaleFactorsA4Lookup, 16, 1, 0, 4, 16, 8},
	{ScaleFactorsA5Bits, ScaleFactorsA5Codes, ScaleFactorsA5Lookup, 32, 1, 0, 5, 32, 8},
	{ScaleFactorsA6Bits, ScaleFactorsA6Codes, ScaleFactorsA6Lookup, 64, 1, 0, 6, 64, 8},
};

HuffmanCodebook HuffmanScaleFactorsSigned[6] = {
	{0},
	{0},
	{ScaleFactorsB2Bits, ScaleFactorsB2Codes, ScaleFactorsB2Lookup,  4, 1, 0, 2,  4, 2},
	{ScaleFactorsB3Bits, ScaleFactorsB3Codes, ScaleFactorsB3Lookup,  8, 1, 0, 3,  8, 6},
	{ScaleFactorsB4Bits, ScaleFactorsB4Codes, ScaleFactorsB4Lookup, 16, 1, 0, 4, 16, 8},
	{ScaleFactorsB5Bits, ScaleFactorsB5Codes, ScaleFactorsB5Lookup, 32, 1, 0, 5, 32, 8},
};

static void InitHuffmanSet(const HuffmanCodebook* codebooks, int count)
{
	for (int i = 0; i < count; i++)
	{
		InitHuffmanCodebook(&codebooks[i]);
	}
}

void InitHuffmanCodebooks()
{
	InitHuffmanSet(HuffmanScaleFactorsUnsigned, sizeof(HuffmanScaleFactorsUnsigned) / sizeof(HuffmanCodebook));
	InitHuffmanSet(HuffmanScaleFactorsSigned, sizeof(HuffmanScaleFactorsSigned) / sizeof(HuffmanCodebook));
}


