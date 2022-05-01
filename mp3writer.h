/****************************************************************
 *                                                              *
 * @copyright  Copyright (c) 2020 SDR-Technologies SAS          *
 * @author     Sylvain AZARIAN - s.azarian@sdr-technologies.fr  *
 * @project    SDR Virtual Machine                              *
 *                                                              *
 * Code propriete exclusive de la société SDR-Technologies SAS  *
 *                                                              *
 ****************************************************************/


#ifndef MP3WRITER_H
#define MP3WRITER_H

#include "vmplugins.h"
#include "vmtypes.h"
#include "ConsumerProducer.h"
#include "iqaccumulator.h"
#include <lame/lame.h>
#include <thread>

typedef struct {
    bool stop = false ;
    lame_global_flags *gfp;
    ConsumerProducerQueue<CpxBlock *> *in_queue ;
    FILE *fd ;
} ThreadParams ;


class MP3Writer : public IJSClass
{
public:
    MP3Writer() = default;

    const char* Name() ;
    const char* JSTypeName() ;
    MP3Writer* allocNewInstance(ISDRVirtualMachineEnv *host) ;
    void deleteInstance( IJSClass *instance ) ;
    void declareMethods( ISDRVirtualMachineEnv *host ) ;

    void init();
    void push( CpxBlock *b );
    bool openfile( const char *filename );
    bool closefile();

private:
     ConsumerProducerQueue<CpxBlock *> *queue ;
     lame_global_flags *gfp;
     int m_outSampleRate ;

     std::thread *encoder ;
     ThreadParams params ;
};


#endif // MP3WRITER_H
