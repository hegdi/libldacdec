#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "bit_reader.h"
#include "huffCodes.h"
#include "bit_allocation.h"

#include "ldacdec.h"

#define LDAC_SYNCWORDBITS   (8)
#define LDAC_SYNCWORD       (0xAA)
/** Sampling Rate **/
#define LDAC_SMPLRATEBITS   (3)
/** Channel **/
#define LDAC_CHCONFIG2BITS  (2)
enum CHANNEL {
    MONO   = 0,
    DUAL   = 1,
    STEREO = 2
};
/** Frame Length **/
#define LDAC_FRAMELEN2BITS  (9)
/** Frame Status **/
#define LDAC_FRAMESTATBITS  (2)

/** Band Info **/
#define LDAC_NBANDBITS      (4)
#define LDAC_BAND_OFFSET    (2)

/** Band **/
#define LDAC_MAXNBANDS        16

/* Flag */
#define LDAC_FLAGBITS       (1)
#define LDAC_TRUE           (1)
#define LDAC_FALSE          (0)

 /* Mode */
#define LDAC_MODE_0         (0)
#define LDAC_MODE_1         (1)
#define LDAC_MODE_2         (2)
#define LDAC_MODE_3         (3)

/** Gradient Data **/
#define LDAC_GRADMODEBITS      (2)
#define LDAC_GRADOSBITS        (5)
#define LDAC_GRADQU0BITS       (6)
#define LDAC_GRADQU1BITS       (5)
#define LDAC_NADJQUBITS        (5)

/** Scale Factor Data **/
#define LDAC_SFCMODEBITS       1
#define LDAC_IDSFBITS          5
#define LDAC_NSFCWTBL          8
#define LDAC_SFCBLENBITS       2
#define LDAC_MINSFCBLEN_0      3
#define LDAC_MINSFCBLEN_2      2
#define LDAC_SFCWTBLBITS       3

#define LDAC_MINIDWL1          1
#define LDAC_MAXIDWL1         15
#define LDAC_MAXIDWL2         15

// Quantization Units
#define LDAC_MAXNQUS          34
#define LDAC_MAXGRADQU        50

/***************************************************************************************************
    Weighting Tables for Scale Factor Data
***************************************************************************************************/
const uint8_t gaa_sfcwgt_ldac[LDAC_NSFCWTBL][LDAC_MAXNQUS] = {
{
     1,  0,  0,  1,  1,  1,  2,  2,  2,  2,  2,  2,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8,  8,
},
{
     0,  1,  1,  2,  3,  4,  4,  4,  4,  5,  6,  6,  6,  6,  6,  7,
     7,  7,  7,  7,  7,  7,  8,  8,  8,  9, 10, 10, 11, 11, 12, 12, 12, 12,
},
{
     0,  1,  1,  2,  3,  3,  3,  3,  3,  4,  4,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  6,  6,  6,  7,  8,  9,  9, 10, 10, 11, 11, 11,
},
{
     0,  1,  3,  4,  5,  5,  6,  6,  6,  6,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  8,  8,  8,  8,  9,  9,  9, 10, 10, 10, 10,
},
{
     0,  1,  3,  4,  5,  5,  6,  7,  7,  8,  8,  9,  9, 10, 10, 10,
    10, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13,
},
{
     1,  0,  1,  2,  2,  3,  3,  4,  4,  5,  6,  7,  7,  8,  8,  8,
     9,  9,  9,  9,  9, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11,
},
{
     0,  0,  1,  1,  2,  2,  2,  2,  2,  3,  3,  3,  3,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  5,  5,  6,  7,  7,  7,  8,  9,  9,  9,  9,
},
{
     0,  0,  1,  2,  3,  4,  4,  5,  5,  6,  7,  7,  8,  8,  8,  8,
     9,  9,  9,  9,  9, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 12,
},
};

const uint8_t ga_nqus_ldac[LDAC_MAXNBANDS+1] = {
     0,  4,  8, 10, 12, 14, 16, 18, 20, 22, 24, 25, 26, 28, 30, 32, 34,
};


int initDecoder( ldacdec_t *this )
{
    InitHuffmanCodebooks();

    this->frame.channels[0].frame = &this->frame;
    this->frame.channels[1].frame = &this->frame;

    return 0;
}

int decodeBand( frame_t *this, BitReaderCxt *br )
{
    this->nbrBands = ReadInt( br, LDAC_NBANDBITS ) + LDAC_BAND_OFFSET;
    printf("nbrBands:        %d\n", this->nbrBands );
    ReadInt( br, LDAC_FLAGBITS ); // unused

    this->quantizationUnitCount = ga_nqus_ldac[this->nbrBands];
    return 0;
}

int decodeGradient( frame_t *this, BitReaderCxt *br )
{
    this->gradientMode = ReadInt( br, LDAC_GRADMODEBITS );
    if( this->gradientMode == LDAC_MODE_0 )
    {
        this->gradientStartUnit  = ReadInt( br, LDAC_GRADQU0BITS );
        this->gradientEndUnit    = ReadInt( br, LDAC_GRADQU0BITS ) + 1;
        this->gradientStartValue = ReadInt( br, LDAC_GRADOSBITS );
        this->gradientEndValue   = ReadInt( br, LDAC_GRADOSBITS );
        printf("gradient\n\tqu [%3d,%3d]\n\tvalue [%3d,%3d]\n", 
                this->gradientStartUnit, this->gradientEndUnit, 
                this->gradientStartValue, this->gradientEndValue );
    } else
    {
        assert( 1 ); // todo
        this->gradientStartUnit  = ReadInt( br, LDAC_GRADQU1BITS );
        this->gradientEndUnit    = 31;
        this->gradientStartValue = ReadInt( br, LDAC_GRADOSBITS );
        this->gradientEndValue   = 31;
        printf("gradient\n\tqu [%3d,%3d\n\tvalue [%3d,%3d]\n", 
                this->gradientStartUnit, this->gradientEndUnit, 
                this->gradientStartValue, this->gradientEndValue );
   }
    
    this->gradientBoundary = ReadInt( br, LDAC_NADJQUBITS );
    return 0;
}

void calculateGradient( frame_t *this )
{
    int valueCount = this->gradientEndValue - this->gradientStartValue;
    int unitCount = this->gradientEndUnit - this->gradientStartUnit;
    
    for( int i=0; i<this->gradientEndUnit; ++i )
        this->gradient[i] = -this->gradientStartValue;
    for( int i=this->gradientEndUnit; i<this->quantizationUnitCount; ++i )
        this->gradient[i] = -this->gradientEndValue;

    if( unitCount > 0 )
    {
        const uint8_t *curve = gradientCurves[unitCount-1];
        if(  valueCount > 0 )
        {
            for( int i=this->gradientStartUnit; i<this->gradientEndUnit; ++i )
            {
                this->gradient[i] -= ((curve[i-this->gradientStartUnit] * (valueCount-1)) >> 8) + 1;
            }
        } else if( valueCount < 0 )
        {
            for( int i=this->gradientStartUnit; i<this->gradientEndUnit; ++i )
            {
                this->gradient[i] -= ((curve[i-this->gradientStartUnit] * (valueCount-1)) >> 8) + 1;
            }
        }
    }
    for( int i=0; i<this->quantizationUnitCount; ++i )
    {
        printf("%4d ", this->gradient[i] );
    }
    printf("\n");

}

void calculatePrecisionMask(channel_t* this)
{
	memset(this->precisionMask, 0, sizeof(this->precisionMask));
	for (int i = 1; i < this->frame->quantizationUnitCount; i++)
	{
		const int delta = this->scaleFactors[i] - this->scaleFactors[i - 1];
		if (delta > 1)
		{
			this->precisionMask[i] += min(delta - 1, 5);
		}
		else if (delta < -1)
		{
			this->precisionMask[i - 1] += min(delta * -1 - 1, 5);
		}
	}
}


void calculatePrecisions( channel_t *this )
{
    frame_t *frame = this->frame;
    
    for( int i=0; i<frame->quantizationUnitCount; ++i )
    {
        switch( frame->gradientMode )
        {
            case LDAC_MODE_0:
            {
                int precision = this->scaleFactors[i] + frame->gradient[i];
                precision = max( precision, LDAC_MINIDWL1 );
                this->precisions[i] = precision;
                break;
            }
            case LDAC_MODE_1:
            {
                int precision = this->scaleFactors[i] + frame->gradient[i] + this->precisionMask[i];
                if( precision > 0 )
                    precision /= 2;
                precision = max( precision, LDAC_MINIDWL1 );
                this->precisions[i] = precision;
                break;
            }
            case LDAC_MODE_2:
            {
                int precision = this->scaleFactors[i] + frame->gradient[i] + this->precisionMask[i];
                if( precision > 0 )
                    precision = ( precision * 3 ) / 8;
                precision = max( precision, LDAC_MINIDWL1 );
                this->precisions[i] = precision;
                break;
            }
            case LDAC_MODE_3:
            {
                int precision = this->scaleFactors[i] + frame->gradient[i] + this->precisionMask[i];
                if( precision > 0 )
                    precision /= 4;
                precision = max( precision, LDAC_MINIDWL1 );
                this->precisions[i] = precision;
                break;
            }
            default:
                assert(1);
                break;
        }
    }
    
    for( int i=0; i<frame->gradientBoundary; ++i )
    {
        this->precisions[i]++;
    }

    for( int i=0; i<frame->quantizationUnitCount; ++i )
    {
        this->precisionsFine[i] = 0;
        if( this->precisions[i] > LDAC_MAXIDWL1 )
        {
            this->precisionsFine[i] = this->precisions[i] - LDAC_MAXIDWL1;
            this->precisions[i] = LDAC_MAXIDWL1;
        }
    }

    printf("precisions =\n");
    for( int i=0; i<frame->quantizationUnitCount; ++i )
    {
        printf("%3d, ", this->precisions[i] );
    }
    printf("\n");
    
    printf("precisionsFine =\n");
    for( int i=0; i<frame->quantizationUnitCount; ++i )
    {
        printf("%3d, ", this->precisionsFine[i] );
    }
    printf("\n");
}

int decodeScaleFactor0( channel_t *this, BitReaderCxt *br )
{
    printf("%s\n", __FUNCTION__ );
    frame_t *frame = this->frame;
    this->scaleFactorBitlen = ReadInt( br, LDAC_SFCBLENBITS ) + LDAC_MINSFCBLEN_0;
    this->scaleFactorOffset = ReadInt( br, LDAC_IDSFBITS );
    this->scaleFactorWeight = ReadInt( br, LDAC_SFCWTBLBITS );

    printf("scale factor bitlen: %3d\n", this->scaleFactorBitlen );
    printf("scale factor offset: %3d\n", this->scaleFactorOffset );
    printf("scale factor weight: %3d\n", this->scaleFactorWeight );

    const uint8_t *weightTable = gaa_sfcwgt_ldac[this->scaleFactorWeight];
    const HuffmanCodebook* codebook = &HuffmanScaleFactorsUnsigned[this->scaleFactorBitlen];
    this->scaleFactors[0] = ReadInt( br, this->scaleFactorBitlen ) + this->scaleFactorOffset;
    for( int i=1; i<frame->quantizationUnitCount; ++i )
    {
        int diff = ReadHuffmanValue( codebook, br, 1 );
        printf("%2d, ", diff );
        this->scaleFactors[i] = this->scaleFactors[i-1] + diff;
        this->scaleFactors[i-1] -= weightTable[i-1]; // cancel weights
    }
    this->scaleFactors[frame->quantizationUnitCount-1] -= weightTable[frame->quantizationUnitCount-1];

    printf("\n");

    for( int i=0; i<frame->quantizationUnitCount; ++i )
    {
        printf("%2d, ", this->scaleFactors[i] );
    }
    printf("\n");
    return 0;
}

int decodeScaleFactor1( channel_t *this, BitReaderCxt *br )
{
    printf("%s\n", __FUNCTION__ );
    assert( 1 );
    return 0;
}

int decodeScaleFactor2( channel_t *this, BitReaderCxt *br )
{
    printf("%s\n", __FUNCTION__ );
    frame_t *frame = this->frame;
    channel_t *other = &frame->channels[0];

    this->scaleFactorBitlen = ReadInt( br, LDAC_SFCBLENBITS ) + LDAC_MINSFCBLEN_2;
    printf("scale factor bitlen: %d\n", this->scaleFactorBitlen );
    const HuffmanCodebook* codebook = &HuffmanScaleFactorsSigned[this->scaleFactorBitlen];
    for( int i=0; i<frame->quantizationUnitCount; ++i )
    {
       int diff = ReadHuffmanValue( codebook, br, 1 );
       printf("%2d, ", diff );
       this->scaleFactors[i] = other->scaleFactors[i] + diff;
    }
    printf("\n");
    for( int i=0; i<frame->quantizationUnitCount; ++i )
    {
        printf("%2d, ", this->scaleFactors[i] );
    }
    printf("\n");
 
    return 0;
}

int decodeScaleFactors( frame_t *this, BitReaderCxt *br, int channelNbr )
{
    printf("%s\n", __FUNCTION__ );
    channel_t *channel = &this->channels[channelNbr];
    channel->scaleFactorMode = ReadInt( br, LDAC_SFCMODEBITS );
 
    if( channelNbr == 0 )
    {
        if( channel->scaleFactorMode == LDAC_MODE_0 )
            decodeScaleFactor0( channel, br );
        else
            decodeScaleFactor1( channel, br );
    } else
    {
        if( channel->scaleFactorMode == LDAC_MODE_0 )
            decodeScaleFactor0( channel, br );
        else
            decodeScaleFactor2( channel, br );
    }
    return 0;
}

int decodeSpectrum( frame_t *this, BitReaderCxt *br )
{
    return 0;
}

int decodeResidual( frame_t *this, BitReaderCxt *br )
{

    return 0;
}


int decode( ldacdec_t *this, uint8_t *stream, float *pcm, int *bytesUsed )
{
    BitReaderCxt brObject;
    BitReaderCxt *br = &brObject;
    InitBitReaderCxt( br, stream );

    frame_t *frame = &this->frame;
   
    int ret = decodeFrame( frame, br );
    if( ret < 0 )
        return -1;

    decodeBand( frame, br );
    decodeGradient( frame, br );
    calculateGradient( frame );
    for( int i=0; i<frame->channelCount; ++i )
    {
        channel_t *channel = &frame->channels[i];
        decodeScaleFactors( frame, br, i );
        calculatePrecisionMask( channel ); 
        calculatePrecisions( channel );

        decodeSpectrum( frame, br );
        decodeResidual( frame, br );
        return -1;
    }

    if( bytesUsed != NULL )
        *bytesUsed = br->Position / 8;
    return -1;
}

int channelConfigIdToChannelCount[] = { 1, 2, 2 };

int decodeFrame( frame_t *this, BitReaderCxt *br )
{
    int syncWord = ReadInt( br, LDAC_SYNCWORDBITS );
    if( syncWord != LDAC_SYNCWORD )
        return -1;

    this->sampleRateId = ReadInt( br, LDAC_SMPLRATEBITS );
    this->channelConfigId = ReadInt( br, LDAC_CHCONFIG2BITS );
    this->frameLength = ReadInt( br, LDAC_FRAMELEN2BITS ) + 1;
    this->frameStatus = ReadInt( br, LDAC_FRAMESTATBITS );
    
    this->channelCount = channelConfigIdToChannelCount[this->channelConfigId];

    printf("sampleRateId:    %d\n", this->sampleRateId );
    printf("channelConfigId: %d\n", this->channelConfigId );
    printf("frameLength:     %d\n", this->frameLength );
    printf("frameStatus:     %d\n", this->frameStatus );

    return 0;
}


#define BUFFER_SIZE (1024)

int main(int argc, char *args[] )
{
    if( argc < 2 )
    {
        printf("usage:\n\t%s <filename>\n", args[0] );
        return EXIT_SUCCESS;
    }
    const char *fileName = args[1];
    printf("opening \"%s\" ...\n", fileName );

    ldacdec_t dec;
    initDecoder( &dec );

    FILE *in = fopen( fileName, "rb" );
    if( in == NULL )
    {
        perror("can't open stream file");
        return EXIT_FAILURE;
    }

    uint8_t buf[BUFFER_SIZE];
    float pcm[BUFFER_SIZE];

    int bytesInBuffer = 0;
    while(1)
    {
        if( bytesInBuffer == 0 )
        {
            bytesInBuffer = fread( buf, 1, BUFFER_SIZE, in );
            if( bytesInBuffer == 0 )
                break;
        }

        int bytesUsed = 0;
        
        int ret = decode( &dec, buf, pcm, &bytesUsed ); 
        if( ret < 0 )
            break;
        
        if( bytesUsed > 0 )
        {
            memmove( buf, buf + bytesUsed, bytesUsed );
            bytesInBuffer -= bytesUsed;
        }
    }

    printf("done.\n");


    fclose(in);

    return EXIT_SUCCESS;
}
