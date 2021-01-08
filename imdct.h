#pragma once

typedef struct {
	int Bits;
	int Size;
	double Scale;
	double ImdctPrevious[MAX_FRAME_SAMPLES];
	double* Window;
	double* SinTable;
	double* CosTable;
} Mdct;

void InitMdct();
void RunImdct(Mdct* mdct, float* input, float* output);

