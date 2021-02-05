#include <assert.h>
#include <string.h>

#include "ldacdec.h"
#include "log.h"
#include "utility.h"
#include "bit_reader.h"
#include "imdct.h"
#include "huffCodes.h"
#include "spectrum.h"
#include "bit_allocation.h"

#define LDAC_SYNCWORDBITS   (8)
#define LDAC_SYNCWORD       (0xAA)
/** Sampling Rate **/
#define LDAC_SMPLRATEBITS   (3)
/** Channel **/
#define LDAC_CHCONFIG2BITS  (2)
enum CHANNEL {
    MONO   = 0,
    STEREO = 1
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
#define LDAC_MINSFCBLEN_1      2
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
static const uint8_t gaa_sfcwgt_ldac[LDAC_NSFCWTBL][LDAC_MAXNQUS] = {
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

static const uint8_t ga_nqus_ldac[LDAC_MAXNBANDS+1] = {
     0,  4,  8, 10, 12, 14, 16, 18, 20, 22, 24, 25, 26, 28, 30, 32, 34,
};


int ldacdecInit( ldacdec_t *this )
{
    InitHuffmanCodebooks();
    InitMdct();

    memset( &this->frame.channels[0].mdct, 0, sizeof( Mdct ) ); 
    memset( &this->frame.channels[1].mdct, 0, sizeof( Mdct ) );
    
    this->frame.channels[0].frame = &this->frame;
    this->frame.channels[1].frame = &this->frame;

    return 0;
}

static int decodeBand( frame_t *this, BitReaderCxt *br )
{
    this->nbrBands = ReadInt( br, LDAC_NBANDBITS ) + LDAC_BAND_OFFSET;
    LOG("nbrBands:        %d\n", this->nbrBands );
    ReadInt( br, LDAC_FLAGBITS ); // unused

    this->quantizationUnitCount = ga_nqus_ldac[this->nbrBands];
    return 0;
}

static int decodeGradient( frame_t *this, BitReaderCxt *br )
{
    this->gradientMode = ReadInt( br, LDAC_GRADMODEBITS );
    if( this->gradientMode == LDAC_MODE_0 )
    {
        this->gradientStartUnit  = ReadInt( br, LDAC_GRADQU0BITS );
        this->gradientEndUnit    = ReadInt( br, LDAC_GRADQU0BITS ) + 1;
        this->gradientStartValue = ReadInt( br, LDAC_GRADOSBITS );
        this->gradientEndValue   = ReadInt( br, LDAC_GRADOSBITS );
        LOG("gradient\n\tqu [%3d,%3d]\n\tvalue [%3d,%3d]\n", 
                this->gradientStartUnit, this->gradientEndUnit, 
                this->gradientStartValue, this->gradientEndValue );
    } else
    {
        this->gradientStartUnit  = ReadInt( br, LDAC_GRADQU1BITS );
        this->gradientEndUnit    = 26;
        this->gradientStartValue = ReadInt( br, LDAC_GRADOSBITS );
        this->gradientEndValue   = 31;
        LOG("gradient\n\tqu [%3d,%3d\n\tvalue [%3d,%3d]\n", 
                this->gradientStartUnit, this->gradientEndUnit, 
                this->gradientStartValue, this->gradientEndValue );
    }
    
    this->gradientBoundary = ReadInt( br, LDAC_NADJQUBITS );
    return 0;
}

static void calculateGradient( frame_t *this )
{
    int valueCount = this->gradientEndValue - this->gradientStartValue;
    int unitCount = this->gradientEndUnit - this->gradientStartUnit;
    
    memset( this->gradient, 0, sizeof( this->gradient ) );

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
    
    LOG_ARRAY_LEN( this->gradient, "%3d, ", this->quantizationUnitCount );
}

static void calculatePrecisionMask(channel_t* this)
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


static void calculatePrecisions( channel_t *this )
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
                assert(0);
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

    LOG_ARRAY_LEN( this->precisions, "%3d, ", frame->quantizationUnitCount );
    LOG_ARRAY_LEN( this->precisionsFine, "%3d, ", frame->quantizationUnitCount );
}

static int decodeScaleFactor0( channel_t *this, BitReaderCxt *br )
{
    LOG_FUNCTION();
    frame_t *frame = this->frame;
    this->scaleFactorBitlen = ReadInt( br, LDAC_SFCBLENBITS ) + LDAC_MINSFCBLEN_0;
    this->scaleFactorOffset = ReadInt( br, LDAC_IDSFBITS );
    this->scaleFactorWeight = ReadInt( br, LDAC_SFCWTBLBITS );

    LOG("scale factor bitlen = %d\n", this->scaleFactorBitlen );
    LOG("scale factor offset = %d\n", this->scaleFactorOffset );
    LOG("scale factor weight = %d\n", this->scaleFactorWeight );

    const int mask = (1<<this->scaleFactorBitlen)-1;
    const uint8_t *weightTable = gaa_sfcwgt_ldac[this->scaleFactorWeight];
    const HuffmanCodebook* codebook = &HuffmanScaleFactorsUnsigned[this->scaleFactorBitlen];
    this->scaleFactors[0] = ReadInt( br, this->scaleFactorBitlen );
//    printf("diff =\n    ");
    for( int i=1; i<frame->quantizationUnitCount; ++i )
    {
        int diff = ReadHuffmanValue( codebook, br, 1 );
       
//        printf("%2d, ", diff );
        this->scaleFactors[i] = (this->scaleFactors[i-1] + diff) & mask;
        this->scaleFactors[i-1] += this->scaleFactorOffset - weightTable[i-1]; // cancel weights
    }
    this->scaleFactors[frame->quantizationUnitCount-1] += this->scaleFactorOffset - weightTable[frame->quantizationUnitCount-1];

    LOG_ARRAY_LEN( this->scaleFactors, "%2d, ", frame->quantizationUnitCount );
    return 0;
}

static int decodeScaleFactor1( channel_t *this, BitReaderCxt *br )
{
    LOG_FUNCTION();
    frame_t *frame = this->frame;
    this->scaleFactorBitlen = ReadInt( br, LDAC_SFCBLENBITS ) + LDAC_MINSFCBLEN_1;
    
    if( this->scaleFactorBitlen > 4 )
    {
        for( int i=0; i<frame->quantizationUnitCount; ++i )
        {
            this->scaleFactors[i] = ReadInt( br, LDAC_IDSFBITS );
        }
    } else
    {
        this->scaleFactorOffset = ReadInt( br, LDAC_IDSFBITS );
        this->scaleFactorWeight = ReadInt( br, LDAC_SFCWTBLBITS );
        const uint8_t *weightTable = gaa_sfcwgt_ldac[this->scaleFactorWeight];
        for( int i=0; i<frame->quantizationUnitCount; ++i )
        {
            this->scaleFactors[i] = ReadInt( br, this->scaleFactorBitlen ) - weightTable[i] + this->scaleFactorOffset;
        }
    }
    return 0;
}

int decodeScaleFactor2( channel_t *this, BitReaderCxt *br )
{
    LOG_FUNCTION();
    frame_t *frame = this->frame;
    channel_t *other = &frame->channels[0];

    this->scaleFactorBitlen = ReadInt( br, LDAC_SFCBLENBITS ) + LDAC_MINSFCBLEN_2;
    LOG("scale factor bitlen: %d\n", this->scaleFactorBitlen );
    const HuffmanCodebook* codebook = &HuffmanScaleFactorsSigned[this->scaleFactorBitlen];
//    printf("diff =\n");
    for( int i=0; i<frame->quantizationUnitCount; ++i )
    {
       int diff = ReadHuffmanValue( codebook, br, 1 );
//       printf("%2d, ", diff );
       this->scaleFactors[i] = other->scaleFactors[i] + diff;
    }
    
    LOG_ARRAY_LEN( this->scaleFactors, "%2d, ", frame->quantizationUnitCount );
    return 0;
}

int decodeScaleFactors( frame_t *this, BitReaderCxt *br, int channelNbr )
{
    LOG_FUNCTION();
    channel_t *channel = &this->channels[channelNbr];
    channel->scaleFactorMode = ReadInt( br, LDAC_SFCMODEBITS );
    LOG("scale factor mode = %d\n", channel->scaleFactorMode );
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

enum {
    CHANNEL_1CH = 1,
    CHANNEL_2CH = 2,
};

static const char gaa_block_setting_ldac[4][4]=
{
    {CHANNEL_1CH, 1, MONO},
    {CHANNEL_2CH, 2, MONO, MONO},
    {CHANNEL_2CH, 1, STEREO},
    {0, 0, 0},
};

static void pcmFloatToShort( frame_t *this, int16_t *pcmOut )
{
    int i=0;
    for(int smpl=0; smpl<this->frameSamples; ++smpl )
    {
        for( int ch=0; ch<this->channelCount; ++ch, ++i )
        {
            pcmOut[i] = Clamp16(Round(this->channels[ch].pcm[smpl]));
        }
    }
}

static const int channelConfigIdToChannelCount[] = { 1, 2, 2 };

int ldacdecGetChannelCount( ldacdec_t *this )
{
    return channelConfigIdToChannelCount[this->frame.channelConfigId];
}

static const unsigned short sampleRateIdToSamplesPower[] = {
    7, 7, 8, 8
};

static const int sampleRateIdToFrequency[] = { 44100, 48000, 88200, 96000 };

int ldacdecGetSampleRate( ldacdec_t *this )
{
    return sampleRateIdToFrequency[this->frame.sampleRateId];
}

static int decodeFrame( frame_t *this, BitReaderCxt *br )
{
    int syncWord = ReadInt( br, LDAC_SYNCWORDBITS );
    if( syncWord != LDAC_SYNCWORD )
        return -1;

    this->sampleRateId = ReadInt( br, LDAC_SMPLRATEBITS );
    this->channelConfigId = ReadInt( br, LDAC_CHCONFIG2BITS );
    this->frameLength = ReadInt( br, LDAC_FRAMELEN2BITS ) + 1;
    this->frameStatus = ReadInt( br, LDAC_FRAMESTATBITS );
    
    this->channelCount = channelConfigIdToChannelCount[this->channelConfigId];
    this->frameSamplesPower = sampleRateIdToSamplesPower[this->sampleRateId];
    this->frameSamples = 1<<this->frameSamplesPower;

    this->channels[0].mdct.Bits = this->frameSamplesPower;
    this->channels[1].mdct.Bits = this->frameSamplesPower;

    LOG("sampleRateId:    %d\n", this->sampleRateId );
    LOG("   sample rate:  %d\n", sampleRateIdToFrequency[this->sampleRateId] );
    LOG("   samplePower:  %d\n", this->frameSamplesPower );
    LOG("channelConfigId: %d\n", this->channelConfigId );
    LOG("frameLength:     %d\n", this->frameLength );
    LOG("frameStatus:     %d\n", this->frameStatus );

    return 0;
}

int ldacDecode( ldacdec_t *this, uint8_t *stream, int16_t *pcm, int *bytesUsed )
{
    BitReaderCxt brObject;
    BitReaderCxt *br = &brObject;
    InitBitReaderCxt( br, stream );

    frame_t *frame = &this->frame;
   
    int ret = decodeFrame( frame, br );
    if( ret < 0 )
        return -1;
   
    for( int block = 0; block<gaa_block_setting_ldac[frame->channelConfigId][1]; ++block )
    {
        decodeBand( frame, br );
        decodeGradient( frame, br );
        calculateGradient( frame );
        
        for( int i=0; i<frame->channelCount; ++i )
        {
            channel_t *channel = &frame->channels[i];
            decodeScaleFactors( frame, br, i );
            calculatePrecisionMask( channel ); 
            calculatePrecisions( channel );

            decodeSpectrum( channel, br );
            decodeSpectrumFine( channel, br );
            dequantizeSpectra( channel );
            scaleSpectrum( channel );

            RunImdct( &channel->mdct, channel->spectra, channel->pcm );
        }
        AlignPosition( br, 8 );

        pcmFloatToShort( frame, pcm );
    }
    AlignPosition( br, (frame->frameLength)*8 + 24 );

    if( bytesUsed != NULL )
        *bytesUsed = br->Position / 8;
    return 0;
}

// for packet loss concealment
static const int sa_null_data_size_ldac[2] = {
    11, 15,
};

static const uint8_t saa_null_data_ldac[2][15] = {
    {0x07, 0xa0, 0x16, 0x00, 0x20, 0xad, 0x51, 0x45, 0x14, 0x50, 0x49},
    {0x07, 0xa0, 0x0a, 0x00, 0x20, 0xad, 0x51, 0x41, 0x24, 0x93, 0x00, 0x28, 0xa0, 0x92, 0x49},
};

int ldacNullPacket( ldacdec_t *this, uint8_t *output, int *bytesUsed )
{
    frame_t *frame = &this->frame;
    uint8_t *ptr = output;

    for( int block = 0; block<gaa_block_setting_ldac[frame->channelConfigId][1]; ++block )
    {
        const int channelType = gaa_block_setting_ldac[frame->channelConfigId][2];
        const int size = sa_null_data_size_ldac[channelType];
        memcpy( ptr, saa_null_data_ldac[channelType], size );
        ptr += size;
    }

    *bytesUsed = frame->frameLength + 3;
}
