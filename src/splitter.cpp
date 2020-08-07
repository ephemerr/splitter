#include "splitter.h"

#include <initializer_list>
#include <numeric>
#include <utility>
#include <iostream>
#include <iterator>


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
    int res=0;

    this->m_Frames.push_back(_pVecPut);

    auto newBuf = --m_Frames.end();

    for( auto &x : m_WaitingClients )
    {
        auto client = m_Clients.find(x);

        client->second = newBuf;
    }

    m_WaitingClients.clear();

    return res;
}

// Сбрасываем все буфера, прерываем все ожидания.
int    ISplitter::SplitterFlush()
{
    m_Frames.clear();

    for (auto&& [first, second] : m_Clients)
    {
        if ( second != m_Frames.end() )
        {
            second = m_Frames.end();

            m_WaitingClients.insert( first );
        }
    }
    return 0;
}

// Добавляем нового клиента - возвращаем уникальный идентификатор клиента.
bool    ISplitter::SplitterClientAdd(OUT int* _pnClientID)
{
    if ( m_ClientsIdsBag.empty() ) return false;

    int id = m_ClientsIdsBag.front();

    *_pnClientID = id;

    m_ClientsIdsBag.pop_front();

    m_Clients.insert( {id, m_Frames.end()} );

    m_WaitingClients.insert( id );

    return true;
}

// Удаляем клиента по идентификатору, если клиент находиться в процессе ожидания буфера, то прерываем ожидание.
bool    ISplitter::SplitterClientRemove(IN int _nClientID)
{
    auto client = m_Clients.find(_nClientID);

    if ( client == m_Clients.end() ) return false;

    m_ClientsIdsBag.push_front( client->first ); // возвращаем значок

    m_Clients.erase( client );

    return true;
}

// Перечисление клиентов, для каждого клиента возвращаем его идентификатор и количество буферов в очереди (задержку) для этого клиента.
bool    ISplitter::SplitterClientGetCount(OUT int* _pnCount)
{
    *_pnCount  = m_Clients.size();

    return true;
}

bool    ISplitter::SplitterClientGetByIndex(IN int _nIndex, OUT int* _pnClientID, OUT int* _pnLatency)
{
    if (_nIndex >= m_Clients.size()) return false;

    auto client = m_Clients.begin();

    std::advance(client, _nIndex);

    *_pnClientID = client->first;

    *_pnLatency = std::distance( client->second, m_Frames.end() );

    return true;
}

// По идентификатору клиента запрашиваем данные, если данных пока нет, то ожидаем _nTimeOutMsec пока не будут добавлены новые данные, в случае превышения времени ожидания - возвращаем ошибку.
int    ISplitter::SplitterGet(IN int _nClientID, OUT std::shared_ptr<std::vector<uint8_t>>& _pVecGet, IN int _nTimeOutMsec)
{
}

// Закрытие объекта сплиттера - все ожидания должны быть прерваны все вызовы возвращают соответствующую ошибку.
void    ISplitter::SplitterClose()
{
}
