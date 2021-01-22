#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include <samplerate.h>
#include <sndfile.h>

#include <unistd.h>
#include <getopt.h>

#include <string.h>

#include "ldacBT.h"

void do_ldac(SNDFILE *in, SF_INFO *info, FILE *out, int eqmid )
{
    const int channels = info->channels;
    float pcmSamples[128*channels];
    int pcm_used = 0;
    uint8_t ldacFrame[1024] = { 0 };
    int ldac_stream_size = 0;
    int ldac_frame_num = 0;
    HANDLE_LDAC_BT h = ldacBT_get_handle();
    float *buf = pcmSamples;
   
    const int bytesPeerFrame = sizeof(float) * channels;
    fflush(stdout);
    assert( channels == 2 );
    int ret = ldacBT_init_handle_encode(h, 679, eqmid, LDACBT_CHANNEL_MODE_STEREO, LDACBT_SMPL_FMT_F32, info->samplerate);
    if( ret < 0 )
    {
        printf("ldacBT_init_handler_encode failed! error code %d\n", ldacBT_get_error_code( h ) );
        return;
    }
    size_t frameCount = 0;
    while( 1 )
    {
        if( buf == NULL )
            break;
        
        int count = sf_readf_float( in, pcmSamples, 128 );
        frameCount += count;
        
        // pad samples in case we can't fill all at the end of file
        if( count < 128 )
        {
            memset( (uint8_t*)pcmSamples+bytesPeerFrame*count, 0, (128-count) * bytesPeerFrame ); 
        }

        // flush codec if no samples left
        if( count == 0 )
            buf = NULL;

        int error = ldacBT_encode(h, buf, &pcm_used, ldacFrame, &ldac_stream_size, &ldac_frame_num);
        if( error < 0 )
        {
            printf("ldacBT_encode failed : %d\n", ldacBT_get_error_code( h ) );
            break;
        }
        if( ldac_stream_size > 0 )
            fwrite( ldacFrame, ldac_stream_size, 1, out );
        
    }
    printf("done.\n");
    ldacBT_close_handle(h);
}

void do_ldac_resample(SNDFILE *in, SF_INFO *info, int new_sample_rate, FILE *out, int eqmid )
{
    // resampling
    int converter = SRC_SINC_BEST_QUALITY;
    int error = 0;
    SRC_STATE *src_state = NULL;
    SRC_DATA  src_data = { 0 };
    float src_ratio = 1.f;
    int channels = info->channels;

	/* Initialize the sample rate converter. */
	if ((src_state = src_new (converter, channels, &error)) == NULL)
	{	
        printf ("Error : src_new() failed : %s.\n", src_strerror (error)) ;
		return;
	}

	if (new_sample_rate > 0)
	{	
        src_ratio = (1.0 * new_sample_rate) / info->samplerate ;
	}

	if (fabs (src_ratio - 1.0) < 1e-20)
	{	
        printf ("Target samplerate and input samplerate are the same. Exiting.\n");
		return;
	}

//	printf ("SRC Ratio     : %f\n", src_ratio) ;
//	printf ("Converter     : %s\n\n", src_get_name (converter)) ;

	if (src_is_valid_ratio (src_ratio) == 0)
	{	
        printf ("Error : Sample rate change out of valid range.\n");
		return;
	}

    HANDLE_LDAC_BT h = ldacBT_get_handle();
    assert( channels == 2 );
    int ret = ldacBT_init_handle_encode(h, 679, eqmid, LDACBT_CHANNEL_MODE_STEREO, LDACBT_SMPL_FMT_F32, new_sample_rate);
    if( ret < 0 )
    {
        printf("ldacBT_init_handler_encode failed! error code %d\n", ldacBT_get_error_code( h ) );
        return;
    }

    size_t frameCount = 0;
    const int bytesPeerFrame = sizeof(float) * channels;
    const ssize_t bufFrames = 1000000;
    float *pcmSamples = malloc( bytesPeerFrame * bufFrames ); 
    float output[128*2]; // max input for encoder
 
    src_data.end_of_input = SF_FALSE;
    src_data.input_frames = 0;
    src_data.src_ratio = src_ratio;
    src_data.data_in = pcmSamples;
    src_data.data_out = output;
    src_data.output_frames = 128;
    int pcm_used = 0;
    int ldac_stream_size = 0;
    int ldac_frame_num = 0;
    uint8_t ldacFrame[1024] = { 0 };
    float *ptrOut = output; 
    while( 1 )
    {
        /* Terminate if done. */
		if( ptrOut == NULL )
        {
            printf("done.\n");
            break;
        }

        if( src_data.end_of_input == SF_TRUE )
        {
            ptrOut = NULL;
        }
        
        /* If the input buffer is empty, refill it. */
        if (src_data.input_frames == 0)
        {   
            src_data.input_frames = sf_readf_float (in, pcmSamples, bufFrames) ;
            src_data.data_in = pcmSamples;
            frameCount += src_data.input_frames;
            /* The last read will not be a full buffer, so snd_of_input. */
            if (src_data.input_frames < bufFrames)
                src_data.end_of_input = SF_TRUE;
            printf("%3.0f%%", (float)frameCount/info->frames*100.f );
            fflush( stdout );
            printf("\b\b\b\b");
        }
		if ((error = src_process (src_state, &src_data)))
		{	
            printf ("src_process faild : %s\n", src_strerror (error)) ;
			break;
		}

        if( (ptrOut != NULL) && (src_data.output_frames_gen == 0) )
            continue;
        
        // pad last frame if needed
        if( (src_data.end_of_input == SF_TRUE) && (src_data.output_frames_gen < 128) )
        {
            memset( (uint8_t*)output + src_data.output_frames_gen * bytesPeerFrame, 0, (128-src_data.output_frames_gen) * bytesPeerFrame );
        }

        if( src_data.end_of_input == SF_FALSE )
            assert( src_data.output_frames_gen == 128 );

        error = ldacBT_encode(h, ptrOut, &pcm_used, ldacFrame, &ldac_stream_size, &ldac_frame_num);
        if( error < 0 )
        {
            printf("ldacBT_encode failed : %d\n", ldacBT_get_error_code( h ) );
            break;
        }
        if( ldac_stream_size > 0 )
            fwrite( ldacFrame, ldac_stream_size, 1, out ); 

        src_data.data_in += src_data.input_frames_used * channels;
		src_data.input_frames -= src_data.input_frames_used;
    }
    
    free( pcmSamples );
    
    ldacBT_close_handle(h);
   
    src_delete(src_state);
}


static char short_options[] = "hr:q:v";

static struct option long_options[] = {
    {"help",        no_argument,        NULL,   'h'},
    {"rate",        required_argument,  NULL,   'r'},
    {"eqmi",        required_argument,  NULL,   'q'},
    {"version",     no_argument,        NULL,   'v'},

    {0, 0, 0, 0}
};

static char *help_options[] = {
    "print (this) help.",
    "sample rate of encoded stream",
    "encode quality mode index",
    "print version",
};

static const int sampleRates[] =
{
    44100,
    48000,
    88200,
    96000,
};

static int sampleRateSupported( const int rate )
{
    for( size_t i=0; i<sizeof(sampleRates)/sizeof(int); ++i )
    {
        if( rate == sampleRates[i] )
            return 1;
    }
    return 0;
}


static int eqmiSupported( const int eqmi )
{
    int ret = 0;
    switch( eqmi )
    {
        case LDACBT_EQMID_HQ:
        case LDACBT_EQMID_SQ:
        case LDACBT_EQMID_MQ:
        case LDACBT_EQMID_ABR:
            ret = 1;
            break;
        default:
            break;
    }
    return ret;
}


#define EQMI( value ) case value: return #value;
static const char *eqmiToString( const int eqmi )
{
    switch( eqmi )
    {
        EQMI( LDACBT_EQMID_HQ )
        EQMI( LDACBT_EQMID_SQ )
        EQMI( LDACBT_EQMID_MQ )
        EQMI( LDACBT_EQMID_ABR )
        default: return "unknown";
    }
    return "unknown";
}

static void strip_ext( char *fname )
{
    char *end = fname + strlen(fname);

    while( end > fname && 
          *end != '.' && 
          *end != '\\' &&
          *end != '/' )
    {
        --end;
    }

    if( (end > fname && *end == '.') &&
        (*(end -1) != '\\' && *(end-1) != '/') )
    {
        *end = '\0';
    }
}

static void printVersion()
{
    printf("ldacenc %s\n", VERSION );
}

static void usage( char *progName )
{
    int i;
    printVersion();
    printf( "\nusage:\n" );
    printf( "%s [options] <filename>\n\n", progName );
    for( i=0; long_options[i].name != 0; i++)
    {
        printf("--%s|-%c\t\t%s\n", long_options[i].name, long_options[i].val, help_options[i] );
    }

    printf("\nsupported sample rates: ");
    for( size_t i=0; i<sizeof(sampleRates)/sizeof(int); ++i )
    {
        printf("%d ", sampleRates[i] );
    }
    printf("\n");
}

int main( int argc, char *args[] )
{
    int sampleRate = -1;
    int eqmi = LDACBT_EQMID_HQ;

    int c;
    while( (c = getopt_long( argc, args, short_options, long_options, NULL )) > 0 )
    {
        switch (c)
        {
            case 'r':
                sampleRate = atoi(optarg);
                if( !sampleRateSupported( sampleRate ) )
                {
                    printf("invalid sample rate!\n");
                    usage( args[0] );
                    return EXIT_FAILURE;
                }
                break;

            case 'q':
                eqmi = atoi(optarg);
                if( !eqmiSupported( eqmi ) )
                {
                    fprintf(stderr, "invalid encode quality mode index!\n");
                    usage( args[0] );
                    return EXIT_FAILURE;
                }
                break;
            
            case 'v':
                printVersion();
                break;
            
            case '?':
            case 'h':
            default:
                usage( args[0] );
                return EXIT_FAILURE;
        }
    }

    if( optind < argc )
    {
        printf("current settings:\n");
        printf("sample rate: %d\n", sampleRate );
        printf("encode quality mode index: %s\n", eqmiToString(eqmi) );

        do
        {    
            const char *fileName = args[optind];
            char outFileName[255] = { 0 };
            strncpy( outFileName, basename( fileName ), 254 );
            strip_ext( outFileName );
            strcat( outFileName, ".ldac" );

            printf("convert: \"%s\" -> \"%s\" ", fileName, outFileName );
            SNDFILE *in = NULL;
            SF_INFO sfinfo = { 0 };
            in = sf_open( fileName, SFM_READ, &sfinfo );
            if( in == NULL )
            {
                printf("sndfile: can't open file: %s\n", sf_strerror( NULL ) );
                return EXIT_FAILURE;
            }
            
            FILE *out = fopen( outFileName, "wb" );
            if( out == NULL )
            {
                perror("can't open output file!");
                return EXIT_FAILURE;
            }

            if( (sampleRate < 0) && sampleRateSupported( sfinfo.samplerate ) )
            {
                do_ldac( in, &sfinfo, out, eqmi ); 
            } else if( (sampleRate > 0) && (sampleRate != sfinfo.samplerate) )
            {
                do_ldac_resample( in, &sfinfo, sampleRate, out, eqmi );
            } else
            {
                printf( "not supported sample frequency (%dHz)\n", sfinfo.samplerate );
            }

            fclose( out );
            sf_close( in );
        } while( ++optind < argc );
    } else 
    {
        usage( args[0] );
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

