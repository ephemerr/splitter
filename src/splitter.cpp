#include "splitter.h"

#include <initializer_list>
#include <memory>
#include <numeric>
#include <ratio>
#include <utility>
#include <iostream>
#include <iterator>
#include <chrono>
#include <thread>

#include "easylogging++.h"

INITIALIZE_EASYLOGGINGPP

using namespace std::chrono_literals;

std::shared_ptr<ISplitter>    SplitterCreate(IN int _nMaxBuffers, IN int _nMaxClients)
{
    return std::make_shared<ISplitter>(_nMaxBuffers, _nMaxClients);
}

// ISplitter интерфейс

ISplitter::ISplitter(int _nMaxBuffers, int _nMaxClients)
    : m_nMaxBuffers(_nMaxBuffers)
    , m_nMaxClients(_nMaxClients)
{
    if (m_nMaxClients > 0 && m_nMaxClients > 0)
    {
        m_bIsClosed = false;
    }
    else
    {
        return;
    }
    m_ClientsIdsBag.resize(m_nMaxClients);
    std::iota(std::begin(m_ClientsIdsBag), std::end(m_ClientsIdsBag), 1);
}

ISplitter::~ISplitter()
{
    this->SplitterClose();

    m_Clients.clear();
    m_Frames.clear();
}

bool    ISplitter::SplitterInfoGet(OUT int* _pnMaxBuffers, OUT int* _pnMaxClients)
{
    if ( m_bIsClosed ) return false;

    *_pnMaxBuffers = m_nMaxBuffers;
    *_pnMaxClients = m_nMaxClients;

    return true;
}

// Кладём данные в очередь. Если какой-то клиент не успел ещё забрать свои данные, и количество буферов (задержка) для него больше максимального значения, то ждём пока не освободятся буфера (клиент заберет данные) в течении _nTimeOutMsec. Если по истечению времени данные так и не забраны, то удаляем старые данные для этого клиента, добавляем новые (по принципу FIFO) (*). Возвращаем код ошибки, который дает понять что один или несколько клиентов “пропустили” свои данные.
int    ISplitter::SplitterPut(IN const std::shared_ptr<std::vector<uint8_t>>& _pVecPut, IN int _nTimeOutMsec)
{
    LOG(DEBUG);

    std::list<int> slowClients;

    // add frame, check slow and quick clients
    TWriteLock write_locker(m_Mutex);

    if ( m_bIsClosed ) return ERR_SPLITTER_IS_CLOSED;

    this->m_Frames.push_back(_pVecPut);

    auto newBuf = --m_Frames.end();

    for( auto&& [nClientId, pClient] : m_Clients)
    {
        if ( pClient->NextFrame() == m_Frames.end() )
        {
            pClient->SetNextFrame( newBuf );
        }

        if ( pClient->NextFrame()  == m_Frames.begin() )
        {
            slowClients.push_back( nClientId );
        }
    }

    LOG(DEBUG) << "Notify waiting clients";

    m_NewFrameUploaded.notify_all();

    // wait for slow

    if ( m_bIsClosed ) return ERR_SPLITTER_IS_CLOSED;

    if ( m_Frames.size() <= m_nMaxBuffers ) return 0;

    if ( not slowClients.empty() )
    {
        LOG(DEBUG) << "Wait for slow clients to get their data";

        m_NoSlowClients.wait_for(write_locker, _nTimeOutMsec*1ms);

        if ( m_bIsClosed ) return ERR_SPLITTER_IS_CLOSED;

        if ( m_Frames.empty() ) return 0;

        slowClients = SlowClients();
    }

    // remove last frame

    int res = 0;

    for (auto& id : slowClients)
    {
        auto ppClient = m_Clients.find(id);

        auto pClient = ppClient->second;

        pClient->FrameIncrement();

        res = ERR_FORCED_FRAMES_REMOVE;
    }

    LOG(DEBUG) << "Pop oldest frame";

    m_Frames.pop_front();

    return res;
}

// По идентификатору клиента запрашиваем данные, если данных пока нет, то ожидаем _nTimeOutMsec пока не будут добавлены новые данные, в случае превышения времени ожидания - возвращаем ошибку.
int    ISplitter::SplitterGet(IN int _nClientID, OUT std::shared_ptr<std::vector<uint8_t>>& _pVecGet, IN int _nTimeOutMsec)
{
    TReadLock locker(m_Mutex);

    LOG(DEBUG);

    if ( m_bIsClosed ) return ERR_SPLITTER_IS_CLOSED;

    if ( _nClientID > m_nMaxClients || _nClientID < 1 ) return ERR_BAD_CLIENT_ID;

    auto ppClient = m_Clients.find(_nClientID);

    if ( ppClient == m_Clients.end() ) return ERR_BAD_CLIENT_ID;

    auto& pClient = ppClient->second;

    if ( pClient->NextFrame() == m_Frames.end() )
    {
        LOG(DEBUG) << "Wait for new data upload";

        auto res = m_NewFrameUploaded.wait_for(locker, _nTimeOutMsec*1ms);

        if ( m_bIsClosed ) return ERR_SPLITTER_IS_CLOSED;

        if ( res == std::cv_status::timeout ) return ERR_TIMEOUT;

        if ( pClient->NextFrame() == m_Frames.end() ) return ERR_SPOUROIUS_WAKEUP;
    }

    _pVecGet = pClient->PopFrame();

    if ( SlowClients().empty() )
    {
        LOG(DEBUG) << "Notify about unneeded oldest frame";

        m_NoSlowClients.notify_all();
    }

    return 0;
}

// Сбрасываем все буфера, прерываем все ожидания.
int    ISplitter::SplitterFlush()
{
    TWriteLock locker(m_Mutex);

    LOG(DEBUG);

    if ( m_bIsClosed ) return ERR_SPLITTER_IS_CLOSED;

    m_Frames.clear();

    for (auto&& [nClientId, pClient] : m_Clients)
    {
        if ( pClient->NextFrame() != m_Frames.end() )
        {
            pClient->SetNextFrame( m_Frames.end() );
        }
    }
    return 0;
}

// Добавляем нового клиента - возвращаем уникальный идентификатор клиента.
bool    ISplitter::SplitterClientAdd(OUT int* _pnClientID)
{
    TWriteLock locker(m_Mutex);

    LOG(DEBUG);

    if ( m_bIsClosed ) return false;

    if ( m_ClientsIdsBag.empty() ) return false;

    int id = m_ClientsIdsBag.front();

    *_pnClientID = id;

    m_ClientsIdsBag.pop_front();

    auto&& pClient = std::make_shared<ISplitterClient>(id, m_Frames.end() );

    m_Clients.insert( {id, pClient } );

    return true;
}

// Удаляем клиента по идентификатору, если клиент находиться в процессе ожидания буфера, то прерываем ожидание.
bool    ISplitter::SplitterClientRemove(IN int _nClientID)
{
    TWriteLock locker(m_Mutex);

    LOG(DEBUG);

    if ( m_bIsClosed ) return false;

    auto ppClient = m_Clients.find(_nClientID);

    if ( ppClient == m_Clients.end() ) return false;

    m_ClientsIdsBag.push_front( ppClient->first ); // возвращаем значок

    m_Clients.erase( ppClient );

    return true;
}

// Перечисление клиентов, для каждого клиента возвращаем его идентификатор и количество буферов в очереди (задержку) для этого клиента.
bool    ISplitter::SplitterClientGetCount(OUT int* _pnCount)
{
    TReadLock read_locker(m_Mutex);

    LOG(DEBUG);

    if ( m_bIsClosed ) return false;

    *_pnCount  = m_Clients.size();

    return true;
}

bool    ISplitter::SplitterClientGetByIndex(IN int _nIndex, OUT int* _pnClientID, OUT int* _pnLatency)
{
    TReadLock read_locker(m_Mutex);

    LOG(DEBUG);

    if ( m_bIsClosed ) return false;

    if (_nIndex >= m_Clients.size()) return false;

    auto ppClient = m_Clients.begin();

    std::advance(ppClient, _nIndex);

    auto& [nClientId, pClient] = *ppClient;

    *_pnClientID = nClientId;

    *_pnLatency = std::distance( pClient->NextFrame(), m_Frames.end() );

    return true;
}

// Закрытие объекта сплиттера - все ожидания должны быть прерваны все вызовы возвращают соответствующую ошибку.
void    ISplitter::SplitterClose()
{
    TWriteLock write_locker(m_Mutex);

    LOG(DEBUG);

    m_bIsClosed = true;

    m_NewFrameUploaded.notify_all();
    m_NoSlowClients.notify_all();
}

std::list<int> ISplitter::SlowClients()
{
    std::list<int> slowClients;

    for( auto&& [nClientId, pClient] : m_Clients)
    {
        if ( pClient->NextFrame()  == m_Frames.begin() )
        {
            slowClients.push_back( nClientId );
        }
    }
    return slowClients;
}

