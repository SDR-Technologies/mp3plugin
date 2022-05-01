/****************************************************************
 *                                                              *
 * @copyright  Copyright (c) 2020 SDR-Technologies SAS          *
 * @author     Sylvain AZARIAN - s.azarian@sdr-technologies.fr  *
 * @project    SDR Virtual Machine                              *
 *                                                              *
 * Code propriete exclusive de la société SDR-Technologies SAS  *
 *                                                              *
 ****************************************************************/

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <samplerate.h>
#include "vmtoolbox.h"
#include "mp3writer.h"

#define MP3_BUFFER_SIZE (32768)
#define MP3_AUDIO_SAMPLERATE (48000)

const char JSTypeNameStr[] = "MP3Writer" ;

void encoder_thread( ThreadParams *params );

const char* MP3Writer::Name() {
    return "MP3Writer";
}

const char* MP3Writer::JSTypeName() {
    return( (const char*) JSTypeNameStr );
}



// This is called when a new instance is needed for the JS code
// example : var x = new Log();
// make init here
MP3Writer* MP3Writer::allocNewInstance(ISDRVirtualMachineEnv *host) {
    (void)host ;
    MP3Writer* result = new MP3Writer();
    result->init();
    return( result );
}

// This is called when the allocated instance is no longer required
void MP3Writer::deleteInstance(IJSClass *instance) {
    delete instance ;
}

void MP3Writer::init() {
    queue = new ConsumerProducerQueue<CpxBlock *>(10);
    gfp = lame_init();
    m_outSampleRate = 0 ;
    encoder = nullptr ;
    params.gfp = gfp ;
    params.in_queue = queue ;
    params.stop = false ;
    params.fd = nullptr ;
}

// we explain to the VM the methods we will add to this object
// method bridges are plain C function.
// each method exposed must have on C bridge function
int push_method( void *stack ) ;
int openfile_method( void *stack );
int closefile_method( void *stack );

void MP3Writer::declareMethods( ISDRVirtualMachineEnv *host ) {
    host->addMethod( (const char *)"write", push_method, true);
    host->addMethod( (const char *)"openfile", openfile_method, true);
    host->addMethod( (const char *)"closefile", closefile_method, false);
}

int openfile_method( void *stack ) {
    int n = vmtools->getStackSize( stack );
    if( n == 0 ) {
        vmtools->throwException( stack, (char *)"Missing argument !");
        return(0);
    }
    const char *filename   = vmtools->getString( stack, 0 );

    MP3Writer* object = (MP3Writer *)vmtools->getObject(stack);
    bool ok = object->openfile( filename );
    vmtools->pushBool( stack, ok );
    return(1);
}

int closefile_method( void* stack ) {
    MP3Writer* object = (MP3Writer *)vmtools->getObject(stack);
    bool ok = object->closefile();
    vmtools->pushBool( stack, ok );
    return(1);
}


bool MP3Writer::openfile(const char *filename ) {
    params.fd = fopen(filename, "w");
    if( params.fd == nullptr ) {
        fprintf( stderr, "Could not open file [%s]\n", filename );
        fflush( stderr );
        return( false );
    }

    return( true );
}

bool MP3Writer::closefile() {
    while( queue->isEmpty() == false ) {
       std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CpxBlock *zero = (CpxBlock *)malloc( sizeof(CpxBlock));
    zero->length = 0 ;
    zero->data = nullptr ;
    queue->add( zero );

    params.stop = true ;
    if( encoder != nullptr ) {
        if( encoder->joinable() )
            encoder->join();
    }
    return(true);
}

void MP3Writer::push(CpxBlock *b) {
    if( b == nullptr )
        return ;
    if( params.fd == nullptr )
        return ;

    if( b->floatdata == false ) {
        fprintf( stderr, "MP3 plugin only accepts FLOAT IQ!\n") ;
        fflush(stderr);
        free( b->data );
        free(b);
        return ;
    }

    if( m_outSampleRate == 0 ) {
        m_outSampleRate = (int)b->samplerate ;
        lame_set_num_channels(gfp, 1);
        lame_set_in_samplerate(gfp, 48000);
        //lame_set_out_samplerate( gfp, 48000 );
        lame_set_VBR(gfp, vbr_off); //force CBR mode
        lame_set_brate(gfp, 48); //is 320

        lame_set_mode(gfp, MONO);
        lame_set_quality(gfp,5);   // 2=high  5 = medium  7=low
        if( lame_init_params(gfp) < 0 ) {
            m_outSampleRate = 0 ;
            fprintf( stderr, "MP3 configuration failed, check sample rate of incoming block !\n") ;
            fflush(stderr);
            free( b->data );
            free(b);
            return ;
        }
        id3tag_init( gfp );
        id3tag_set_title( gfp, "SDR-Technologies") ;
        id3tag_set_artist(gfp, "SDR-Technologies") ;
        id3tag_set_album(gfp,"SDRVM") ;
        id3tag_set_comment(gfp,"https://www.sdrtechnologies.fr/");

        if( encoder == nullptr ) {
            params.gfp = gfp ;
            params.in_queue = queue ;
            params.stop = false ;
            encoder = new std::thread( encoder_thread, &params );
        }
    }
    queue->add( b );
}

int push_method( void *stack ) {
    // Check that we have received the valid number of arguments
    int n = vmtools->getStackSize( stack );
    if( n < 1 ) {
        vmtools->throwException( stack, (char *)"Missing argument !");
        return(0);
    }
    // pop the argument
    CpxBlock *iq = vmtools->getIQData( stack, 0);
    // retrieve the "this" pointer for this method
    MP3Writer* object = (MP3Writer *)vmtools->getObject(stack);

    // call
    if( object != nullptr ) {
        object->push(iq);
    }
    return(0);
}

#define IQBLOCK_SIZE (4096)
void encoder_thread( ThreadParams *params ) {
    bool first = true ;
    int error = 0 ;
    TYPECPX iqbuffer[IQBLOCK_SIZE];
    float soundbuffer[IQBLOCK_SIZE];
    float resampled_buffer[IQBLOCK_SIZE*16];
    unsigned char* mp3buf ;
    CpxBlock *b = nullptr ;
    ConsumerProducerQueue<CpxBlock *> *queue = params->in_queue ;
    lame_global_flags *gfp = params->gfp ;
    IQAccumulator acc ;
    mp3buf = (unsigned char*)malloc( MP3_BUFFER_SIZE ) ;

    SRC_STATE* sstate = src_new(SRC_SINC_MEDIUM_QUALITY, 1, &error );
    SRC_DATA sdata ;

    sdata.data_in  = soundbuffer ;
    sdata.data_out = resampled_buffer ;
    sdata.end_of_input = 0 ;
    sdata.output_frames = IQBLOCK_SIZE*16 ;

    long total_samples = 0 ;

    while( params->stop == false ) {
        queue->consume(b);
        if( params->fd == nullptr ) {
            free( b->data );
            free(b);
            continue ;
        }
        if( b->length == 0 )
            continue ;

        if( first ) {
            first = false ;
            sdata.src_ratio = 48000.0/b->samplerate ; // upsample  to 48 kHz
        }
        acc.append((TYPECPX *)b->data, b->length);
        free( b->data );
        free(b);
        while( acc.maxRead() >= IQBLOCK_SIZE ) {
            int nb_samples = acc.get( IQBLOCK_SIZE, iqbuffer ) ;
            if( nb_samples == 0 ) {
                break ;
            }
            for( int i=0 ; i < nb_samples ; i++ ) {
                soundbuffer[i] = iqbuffer[i].I ;
            }
            sdata.input_frames = nb_samples ;
            src_process( sstate, &sdata );
            //fprintf( stderr, "SRC : in %ld ==> out %ld\n", sdata.input_frames, sdata.output_frames_gen ); fflush(stderr);
            if( sdata.output_frames_gen > 0 ) {
                nb_samples = lame_encode_buffer_ieee_float( gfp,
                                                                (const float *)resampled_buffer,
                                                                NULL,
                                                                sdata.output_frames_gen,
                                                                mp3buf, MP3_BUFFER_SIZE) ;
                if( nb_samples > 0 ) {
                    total_samples += nb_samples ;
                    fwrite( mp3buf, 1, nb_samples, params->fd );
                }
            }

        }
    }

    fclose( params->fd );
    params->fd = nullptr ;
    fprintf( stdout, "A total of %ld samples was written.\n", total_samples );
}
