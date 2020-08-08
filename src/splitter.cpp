#include "splitter.h"

#include <initializer_list>
#include <numeric>
#include <ratio>
#include <utility>
#include <iostream>
#include <iterator>
#include <chrono>


std::shared_ptr<ISplitter>    SplitterCreate(IN int _nMaxBuffers, IN int _nMaxClients)
{
    return std::make_shared<ISplitter>(_nMaxBuffers, _nMaxClients);
}

// ISplitter интерфейс

ISplitter::ISplitter(int _nMaxBuffers, int _nMaxClients)
    : m_nMaxBuffers(_nMaxBuffers)
    , m_nMaxClients(_nMaxClients)
{
    m_ClientsIdsBag.resize(m_nMaxClients);
    std::iota(std::begin(m_ClientsIdsBag), std::end(m_ClientsIdsBag), 1);
}

ISplitter::~ISplitter()
{
    m_Clients.clear();
    m_Frames.clear();
}

bool    ISplitter::SplitterInfoGet(OUT int* _pnMaxBuffers, OUT int* _pnMaxClients)
{
    *_pnMaxBuffers = m_nMaxBuffers;
    *_pnMaxClients = m_nMaxClients;

    return true;
}

// Кладём данные в очередь. Если какой-то клиент не успел ещё забрать свои данные, и количество буферов (задержка) для него больше максимального значения, то ждём пока не освободятся буфера (клиент заберет данные) в течении _nTimeOutMsec. Если по истечению времени данные так и не забраны, то удаляем старые данные для этого клиента, добавляем новые (по принципу FIFO) (*). Возвращаем код ошибки, который дает понять что один или несколько клиентов “пропустили” свои данные.
int    ISplitter::SplitterPut(IN const std::shared_ptr<std::vector<uint8_t>>& _pVecPut, IN int _nTimeOutMsec)
{
    WriteLock locker(m_Mutex);

    int res=0;

    this->m_Frames.push_back(_pVecPut);

    auto newBuf = --m_Frames.end();

    for( auto&& [nClientId, pNextFrame] : m_Clients)
    {
        if ( pNextFrame == m_Frames.end() )
        {
            pNextFrame = newBuf;
        }
    }

    locker.unlock();

    m_ConditionalVariable.notify_all();

    return res;
}

// По идентификатору клиента запрашиваем данные, если данных пока нет, то ожидаем _nTimeOutMsec пока не будут добавлены новые данные, в случае превышения времени ожидания - возвращаем ошибку.
int    ISplitter::SplitterGet(IN int _nClientID, OUT std::shared_ptr<std::vector<uint8_t>>& _pVecGet, IN int _nTimeOutMsec)
{
    if ( _nClientID > m_nMaxClients || _nClientID < 1 ) return ERR_BAD_CLIENT_ID;

    auto& pNextFrame = m_Clients[_nClientID];

    if ( pNextFrame == m_Frames.end() )
    {
        WriteLock locker(m_Mutex);

        std::chrono::milliseconds milliSeconds(_nTimeOutMsec);

        auto res = m_ConditionalVariable.wait_for(locker, milliSeconds);

        if ( res == std::cv_status::timeout ) return ERR_TIMEOUT;

        if ( pNextFrame == m_Frames.end() ) return ERR_SPOUROIUS_WAKEUP;
    }

    ReadLock read_locker(m_Mutex);

    _pVecGet = *pNextFrame;

    pNextFrame++;

    return 0;
}

// Сбрасываем все буфера, прерываем все ожидания.
int    ISplitter::SplitterFlush()
{
    WriteLock locker(m_Mutex);

    m_Frames.clear();

    for (auto&& [nClientId, pNextFrame] : m_Clients)
    {
        if ( pNextFrame != m_Frames.end() )
        {
            pNextFrame = m_Frames.end();
        }
    }
    return 0;
}

// Добавляем нового клиента - возвращаем уникальный идентификатор клиента.
bool    ISplitter::SplitterClientAdd(OUT int* _pnClientID)
{
    WriteLock locker(m_Mutex);

    if ( m_ClientsIdsBag.empty() ) return false;

    int id = m_ClientsIdsBag.front();

    *_pnClientID = id;

    m_ClientsIdsBag.pop_front();

    m_Clients.insert( {id, m_Frames.end()} );

    return true;
}

// Удаляем клиента по идентификатору, если клиент находиться в процессе ожидания буфера, то прерываем ожидание.
bool    ISplitter::SplitterClientRemove(IN int _nClientID)
{
    WriteLock locker(m_Mutex);

    auto pClient = m_Clients.find(_nClientID);

    if ( pClient == m_Clients.end() ) return false;

    m_ClientsIdsBag.push_front( pClient->first ); // возвращаем значок

    m_Clients.erase( pClient );

    return true;
}

// Перечисление клиентов, для каждого клиента возвращаем его идентификатор и количество буферов в очереди (задержку) для этого клиента.
bool    ISplitter::SplitterClientGetCount(OUT int* _pnCount)
{
    ReadLock read_locker(m_Mutex);

    *_pnCount  = m_Clients.size();

    return true;
}

bool    ISplitter::SplitterClientGetByIndex(IN int _nIndex, OUT int* _pnClientID, OUT int* _pnLatency)
{
    ReadLock read_locker(m_Mutex);

    if (_nIndex >= m_Clients.size()) return false;

    auto pClient = m_Clients.begin();

    std::advance(pClient, _nIndex);

    auto& [nClientId, pNextFrame] = *pClient;

    *_pnClientID = nClientId;

    *_pnLatency = std::distance( pNextFrame, m_Frames.end() );

    return true;
}

// Закрытие объекта сплиттера - все ожидания должны быть прерваны все вызовы возвращают соответствующую ошибку.
void    ISplitter::SplitterClose()
{
}



