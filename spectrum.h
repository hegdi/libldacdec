#ifndef _SPECTRUM_H_
#define _SPECTRUM_H_

#include "ldacdec.h"
#include "bit_reader.h"

int decodeSpectrum( channel_t *this, BitReaderCxt *br );
int decodeSpectrumFine( channel_t *this, BitReaderCxt *br );

void dequantizeSpectra( channel_t *this );
void scaleSpectrum(channel_t* this);

#endif // _SPECTRUM_H_
