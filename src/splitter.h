
#include <atomic>
#include <memory>
#include <vector>
#include <list>
#include <map>
#include <shared_mutex>
#include <condition_variable>

#define OUT
#define IN

typedef std::vector<uint8_t> Frame;
typedef std::shared_ptr<Frame> FramePtr;
typedef std::list<FramePtr> FrameBuf;
typedef FrameBuf::iterator Client;

typedef std::shared_mutex Lock;
typedef std::unique_lock< Lock >  WriteLock;
typedef std::shared_lock< Lock >  ReadLock;

class ISplitter
{
    // ISplitter интерфейс
public:

    enum ErrorCode {
        NO_ERROR=0
        ,ERR_BAD_CLIENT_ID
        ,ERR_SPOUROIUS_WAKEUP
        ,ERR_TIMEOUT
        ,ERR_BAD_FRAME_UPDATE
    };

    ISplitter(int _nMaxBuffers, int _nMaxClients);

    ~ISplitter();

    bool    SplitterInfoGet(OUT int* _pnMaxBuffers, OUT int* _pnMaxClients);

    // Кладём данные в очередь. Если какой-то клиент не успел ещё забрать свои данные, и количество буферов (задержка) для него больше максимального значения, то ждём пока не освободятся буфера (клиент заберет данные) в течении _nTimeOutMsec. Если по истечению времени данные так и не забраны, то удаляем старые данные для этого клиента, добавляем новые (по принципу FIFO) (*). Возвращаем код ошибки, который дает понять что один или несколько клиентов “пропустили” свои данные.
    int    SplitterPut(IN const std::shared_ptr<std::vector<uint8_t>>& _pVecPut, IN int _nTimeOutMsec);

    // Сбрасываем все буфера, прерываем все ожидания.
    int    SplitterFlush();

    // Добавляем нового клиента - возвращаем уникальный идентификатор клиента.
    bool    SplitterClientAdd(OUT int* _pnClientID);

    // Удаляем клиента по идентификатору, если клиент находиться в процессе ожидания буфера, то прерываем ожидание.
    bool    SplitterClientRemove(IN int _nClientID);

    // Перечисление клиентов, для каждого клиента возвращаем его идентификатор и количество буферов в очереди (задержку) для этого клиента.
    bool    SplitterClientGetCount(OUT int* _pnCount);
    bool    SplitterClientGetByIndex(IN int _nIndex, OUT int* _pnClientID, OUT int* _pnLatency);

    // По идентификатору клиента запрашиваем данные, если данных пока нет, то ожидаем _nTimeOutMsec пока не будут добавлены новые данные, в случае превышения времени ожидания - возвращаем ошибку.
    int    SplitterGet(IN int _nClientID, OUT std::shared_ptr<std::vector<uint8_t>>& _pVecGet, IN int _nTimeOutMsec);

    // Закрытие объекта сплиттера - все ожидания должны быть прерваны все вызовы возвращают соответствующую ошибку.
    void    SplitterClose();

private:

    Lock m_Mutex;
    std::condition_variable_any m_ConditionalVariable;
    FrameBuf m_Frames;
    std::map<int, Client> m_Clients;
    std::list<int> m_ClientsIdsBag;
    int m_nMaxBuffers{0};
    int m_nMaxClients{0};
};

std::shared_ptr<ISplitter>    SplitterCreate(IN int _nMaxBuffers, IN int _nMaxClients);

