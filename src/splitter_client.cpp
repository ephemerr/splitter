#include "splitter_client.h"
#include "splitter_definitions.h"

ISplitterClient::ISplitterClient( int _nId, TNextFrame _pNextFrame)
    : m_nId(_nId)
    , m_pNextFrame( _pNextFrame)
{
}

void ISplitterClient::SetNextFrame( TNextFrame _pNextFrame )
{
    const std::lock_guard<std::mutex> locker(m_Mutex);

    m_pNextFrame = _pNextFrame;
}

TFramePtr ISplitterClient::PopFrame()
{
    const std::lock_guard<std::mutex> locker(m_Mutex);

    auto res = *m_pNextFrame;

    m_pNextFrame++;

    return res;
}

void ISplitterClient::FrameIncrement()
{
    const std::lock_guard<std::mutex> locker(m_Mutex);

    m_pNextFrame++;
}
