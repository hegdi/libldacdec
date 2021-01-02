#pragma once

#define MAX_FRAME_SAMPLES 256

typedef struct {
	int Bits;
	int Size;
	double Scale;
	double ImdctPrevious[MAX_FRAME_SAMPLES];
	double* Window;
	double* SinTable;
	double* CosTable;
} Mdct;

void InitMdctTables(int frameSizePower);
void RunImdct(Mdct* mdct, float* input, float* output);

