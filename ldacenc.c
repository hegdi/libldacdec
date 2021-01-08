#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include <samplerate.h>
#include <sndfile.h>

#include "ldacBT.h"
#if 0
static sf_count_t
sample_rate_convert (SNDFILE *infile, int converter, double src_ratio, int channels, double * gain, int normalize)
{	static float input [BUFFER_LEN] ;
	static float output [BUFFER_LEN] ;

	SRC_STATE	*src_state ;
	SRC_DATA	src_data ;
	int			error ;
	double		max = 0.0 ;
	sf_count_t	output_count = 0 ;

	sf_seek (infile, 0, SEEK_SET) ;

	/* Initialize the sample rate converter. */
	if ((src_state = src_new (converter, channels, &error)) == NULL)
	{	printf ("\n\nError : src_new() failed : %s.\n\n", src_strerror (error)) ;
		exit (1) ;
		} ;

    src_data.end_of_input = 0 ; /* Set this later. */

	/* Start with zero to force load in while loop. */
	src_data.input_frames = 0 ;
	src_data.data_in = input ;

	src_data.src_ratio = src_ratio ;

	src_data.data_out = output ;
	src_data.output_frames = BUFFER_LEN /channels ;

	while (1)
	{
		/* If the input buffer is empty, refill it. */
		if (src_data.input_frames == 0)
		{	src_data.input_frames = sf_readf_float (infile, input, BUFFER_LEN / channels) ;
			src_data.data_in = input ;

			/* The last read will not be a full buffer, so snd_of_input. */
			if (src_data.input_frames < BUFFER_LEN / channels)
				src_data.end_of_input = SF_TRUE ;
			} ;

		if ((error = src_process (src_state, &src_data)))
		{	printf ("\nError : %s\n", src_strerror (error)) ;
			exit (1) ;
			} ;

		/* Terminate if done. */
		if (src_data.end_of_input && src_data.output_frames_gen == 0)
			break ;

     	max = apply_gain (src_data.data_out, src_data.output_frames_gen, channels, max, *gain) ;

		/* Write output. */
		sf_writef_float (outfile, output, src_data.output_frames_gen) ;
		output_count += src_data.output_frames_gen ;

		src_data.data_in += src_data.input_frames_used * channels ;
		src_data.input_frames -= src_data.input_frames_used ;
	} ;

	src_delete (src_state) ;

	if (normalize && max > 1.0)
	{	*gain = 1.0 / max ;
		printf ("\nOutput has clipped. Restarting conversion to prevent clipping.\n\n") ;
		return -1 ;
		} ;

	return output_count ;
} /* sample_rate_convert */
#endif

#define BUFFER_LEN  (4096*2*2)

void do_ldac(SNDFILE *in, int channels, int sample_rate )
{
    float pcmSamples[128*channels];
    int pcm_used = 0;
    uint8_t ldacFrame[1024] = { 0 };
    int ldac_stream_size = 0;
    int ldac_frame_num = 0;
    HANDLE_LDAC_BT h = ldacBT_get_handle();
    float *buf = pcmSamples;
 
    assert( channels == 2 );
    int ret = ldacBT_init_handle_encode(h, 679, LDACBT_EQMID_HQ, LDACBT_CHANNEL_MODE_STEREO, LDACBT_SMPL_FMT_F32, sample_rate);
    if( ret < 0 )
    {
        printf("ldacBT_init_handler_encode failed! error code %d\n", ldacBT_get_error_code( h ) );
        return;
    }

    while( 1 )
    {
        for( int i=0; i<128*channels; ++i )
            pcmSamples[i] = 0.f;

        int count = sf_readf_float(in, pcmSamples, 128);
        
        if( count == 0 )
            buf = NULL;

        int error = ldacBT_encode(h, buf, &pcm_used, ldacFrame, &ldac_stream_size, &ldac_frame_num);
        if( error < 0 )
        {
            printf("ldacBT_encode failed : %d\n", ldacBT_get_error_code( h ) );
            break;
        }
        if( ldac_stream_size > 0 )
            printf("  pcm_used=%d ldac_stream_size=%d ldac_frame_num=%d\n", pcm_used, ldac_stream_size, ldac_frame_num);
        
        if( buf == NULL )
            break;
    }

    ldacBT_close_handle(h);
}


int main( int argc, char *args[] )
{
    HANDLE_LDAC_BT h;

    if( argc < 2 )
    {
        printf( "usage:\n%s <filename>\n", args[0] );
        return EXIT_FAILURE;
    }
    const char *fileName = args[1];
    printf("audio file: \"%s\"\n", fileName );
    SNDFILE *in = NULL;
    SF_INFO sfinfo = { 0 };
    in = sf_open( fileName, SFM_READ, &sfinfo );
    if( in == NULL )
    {
        printf("sndfile: can't open file: %s\n", sf_strerror( NULL ) );
        return EXIT_FAILURE;
    }
    int channels = sfinfo.channels;
    printf("samplerate: %d\n", sfinfo.samplerate );
    printf("channels:   %d\n", channels );
    printf("format:     %06x\n", sfinfo.format );

    // resampling
    int converter = SRC_SINC_BEST_QUALITY;
    int new_sample_rate = 96000;
    int error = 0;
    SRC_STATE *src_state = NULL;
    SRC_DATA  src_data = { 0 };
    float pcmSamples[BUFFER_LEN] = { 0 };
    float output[BUFFER_LEN*2] = { 0 };
    float src_ratio = 1.f;

	/* Initialize the sample rate converter. */
	if ((src_state = src_new (converter, channels, &error)) == NULL)
	{	
        printf ("\n\nError : src_new() failed : %s.\n\n", src_strerror (error)) ;
		return EXIT_FAILURE;
	}

	if (new_sample_rate > 0)
	{	
        src_ratio = (1.0 * new_sample_rate) / sfinfo.samplerate ;
	}

	if (fabs (src_ratio - 1.0) < 1e-20)
	{	
        printf ("Target samplerate and input samplerate are the same. Exiting.\n");
        do_ldac( in, channels, sfinfo.samplerate  );
		return EXIT_SUCCESS;
	}

	printf ("SRC Ratio     : %f\n", src_ratio) ;
	printf ("Converter     : %s\n\n", src_get_name (converter)) ;

	if (src_is_valid_ratio (src_ratio) == 0)
	{	
        printf ("Error : Sample rate change out of valid range.\n");
		return EXIT_FAILURE;
	}

    h = ldacBT_get_handle();
    assert( channels == 2 );
    int ret = ldacBT_init_handle_encode(h, 679, LDACBT_EQMID_HQ, LDACBT_CHANNEL_MODE_STEREO, LDACBT_SMPL_FMT_F32, new_sample_rate);
    if( ret < 0 )
    {
        printf("ldacBT_init_handler_encode failed! error code %d\n", ldacBT_get_error_code( h ) );
        return EXIT_FAILURE;
    }


    FILE *out = fopen( "test.ldac", "wb" );

    src_data.end_of_input = 0;
    src_data.input_frames = 0;
    src_data.src_ratio = src_ratio;
    src_data.data_in = pcmSamples;
    src_data.data_out = output;
    src_data.output_frames = 128;
    int pcm_used = 0;
    int ldac_stream_size = 0;
    int ldac_frame_num = 0;
    uint8_t ldacFrame[1024] = { 0 };


    while( 1 )
    {
        /* If the input buffer is empty, refill it. */
        if (src_data.input_frames == 0)
        {   
            src_data.input_frames = sf_readf_float (in, pcmSamples, BUFFER_LEN / channels) ;
            src_data.data_in = pcmSamples;

            /* The last read will not be a full buffer, so snd_of_input. */
            if (src_data.input_frames < BUFFER_LEN / channels)
                src_data.end_of_input = SF_TRUE;
        }
        printf("src_data.input_frames: %ld\n", src_data.input_frames ); 
		if ((error = src_process (src_state, &src_data)))
		{	
            printf ("src_process faild : %s\n", src_strerror (error)) ;
			break;
		}

		/* Terminate if done. */
		if (src_data.end_of_input && src_data.output_frames_gen == 0)
        {
            printf("done.\n");
			break;
        }
        
        if( src_data.output_frames_gen == 0 )
            continue;
        printf("%ld\n", src_data.output_frames_gen );
        assert( src_data.output_frames_gen == 128 );
#if 1       
        error = ldacBT_encode(h, output, &pcm_used, ldacFrame, &ldac_stream_size, &ldac_frame_num);
        if( error < 0 )
        {
            printf("ldacBT_encode failed : %d\n", ldacBT_get_error_code( h ) );
            break;
        }
        if( ldac_stream_size > 0 )
        {
            printf("  pcm_used=%d ldac_stream_size=%d ldac_frame_num=%d\n", pcm_used, ldac_stream_size, ldac_frame_num);
            fwrite( ldacFrame, ldac_stream_size, 1, out ); 
        }
#endif    	

        src_data.data_in += src_data.input_frames_used * channels;
		src_data.input_frames -= src_data.input_frames_used;
    }
   
    ldacBT_close_handle(h);
   
    src_delete(src_state); 

    sf_close( in );
    return EXIT_SUCCESS;
}

