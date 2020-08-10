#ifndef SPLITTER_CLIENT_H
#define SPLITTER_CLIENT_H

#include "splitter_definitions.h"

class ISplitterClient
{
public:

    ISplitterClient( int _nId, TNextFrame _pFrame);

    TNextFrame NextFrame( ) { return m_pNextFrame; };

    void SetNextFrame( TNextFrame );

    TFramePtr PopFrame();

    void FrameIncrement();

private:
    int m_nId;
    TNextFrame m_pNextFrame;
    std::mutex m_Mutex;
};

typedef std::shared_ptr<ISplitterClient> ClientPtr;

#endif /*SPLITTER_CLIENT_H*/
