#ifndef __LDACDEC_H_
#define __LDACDEC_H_

#define MAX_QUANT_UNITS     (34)

#define container_of( ptr, type, member ) ({                \
        const typeof( ((type*)0)->member ) *__mptr = (ptr); \
        (type *)((char*)__ptr - offsetof(type, memeber));   \
        })

#define min( a, b )             \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
       _a < _b ? _a : _b; });

#define max( a, b )             \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
       _a > _b ? _a : _b; });


typedef struct Frame frame_t;
typedef struct Channel channel_t;

struct Channel {
    frame_t *frame;
    int scaleFactorMode;
    int scaleFactorBitlen;
    int scaleFactorOffset;
    int scaleFactorWeight;

    int scaleFactors[MAX_QUANT_UNITS];

    int precisions[MAX_QUANT_UNITS];
    int precisionsFine[MAX_QUANT_UNITS];
    int precisionMask[MAX_QUANT_UNITS];
};

struct Frame {
    int sampleRateId;
    int channelConfigId;
    int frameLength;
    int frameStatus;

    int nbrBands;
    
    // gradient data
    int gradient[MAX_QUANT_UNITS];
    int gradientMode;
    int gradientStartUnit;
    int gradientEndUnit;
    int gradientStartValue;
    int gradientEndValue;
    int gradientBoundary;
  
    int quantizationUnitCount; 

    int channelCount;
    channel_t channels[2];
};


typedef struct {
    frame_t frame;


} ldacdec_t;

#endif // __LDACDEC_H_
