#include "splitter_definitions.h"
#include <functional>
#include <iostream>
#include <mutex>
#define CATCH_CONFIG_MAIN

#include <memory>
#include <type_traits>
#include <thread>

#include "catch.hpp"

#include "splitter.h"

using namespace std::chrono_literals;

TEST_CASE( "Splitter", "[splitter]" )
{
    const int nMaxClients = 10;
    const int nMaxBufs = 10;

    auto pSplitter = SplitterCreate(nMaxBufs, nMaxClients);

    SECTION("Initialization")
    {
        REQUIRE( std::is_same_v<decltype(pSplitter), std::shared_ptr<ISplitter>> );

        int nBufs = 0;
        int nClients = 0;

        pSplitter->SplitterInfoGet(&nBufs, &nClients);

        REQUIRE( nBufs == nMaxBufs );
        REQUIRE( nClients == nMaxClients );
    }

    SECTION("Add and remove clients")
    {
        int nClientId = 0;

        std::set<int> ClientsIds;

        // check remove before addition
        REQUIRE_FALSE( pSplitter->SplitterClientRemove( 1 ) );

        for(int i=0; i<nMaxClients; i++)
        {
            REQUIRE( pSplitter->SplitterClientAdd(&nClientId) );
            REQUIRE( nClientId >=1  );
            REQUIRE( nClientId <= nMaxClients );
            // check if ID is unique
            REQUIRE( ClientsIds.find( nClientId ) == ClientsIds.end() );

            ClientsIds.insert( nClientId );
        }
        // check limit excess
        REQUIRE_FALSE( pSplitter->SplitterClientAdd(&nClientId) );

        int nCliensCount = 0;

        pSplitter->SplitterClientGetCount( &nCliensCount );

        REQUIRE( nCliensCount == nMaxClients );

        for(int i=0; i<nMaxClients; i++)
        {
            int nLatency = -1;
            REQUIRE( pSplitter->SplitterClientGetByIndex( i, &nClientId, &nLatency ) );
            auto client = ClientsIds.find( nClientId );
            REQUIRE( client != ClientsIds.end() );
            ClientsIds.erase( client );
        }
        REQUIRE(ClientsIds.size() == 0);

        // check remove by wrong id
        REQUIRE_FALSE( pSplitter->SplitterClientRemove( 0 ) );

        for(int i=0; i<nMaxClients; i++)
        {
            REQUIRE( pSplitter->SplitterClientRemove( i+1 ) );
        }
        REQUIRE_FALSE( pSplitter->SplitterClientRemove( 1 ) );

    }

    SECTION("Put frames")
    {
        int client[3] = {};

        auto pFrame = std::make_shared<TFrame>( 1000000 );

        int nClientId = 0;

        int nLatency = -1;

        for(int i=0; i<3; i++)
        {
            REQUIRE( pSplitter->SplitterClientAdd(&nClientId) );

            client[i] = nClientId;

            REQUIRE( pSplitter->SplitterPut(pFrame, 1000) == 0 );
        }

        int nCliensCount = 0;

        pSplitter->SplitterClientGetCount( &nCliensCount );

        REQUIRE( nCliensCount == 3 );

        for(int i=0; i<nCliensCount; i++)
        {
            CHECK( pSplitter->SplitterClientGetByIndex( i, &nClientId, &nLatency ) );

            CHECK( nClientId == i + 1 );

            CHECK( nLatency == nCliensCount - i);

            for(int j=0; j<nLatency; j++)
            {
                CHECK( pSplitter->SplitterGet(nClientId, pFrame, 1) == 0 );
            }
            CHECK( pSplitter->SplitterGet(nClientId, pFrame, 100) == ISplitter::ERR_TIMEOUT );
        }

        std::thread putter(
        [&] {
            for(int i=0; i<15; i++)
            {
                std::this_thread::sleep_for(100ms);

                if (i >= nMaxBufs)
                {
                    CHECK( pSplitter->SplitterPut(pFrame, 1000) == ISplitter::ERR_FORCED_FRAMES_REMOVE );
                } else
                {
                    CHECK( pSplitter->SplitterPut(pFrame, 1000) == 0 );
                }
            }
        });

        putter.join();
    }

    SECTION("Async")
    {
        std::cout << "Async tests" << std::endl;

        int client[3] = {};

        auto pSplitter = SplitterCreate(nMaxBufs, nMaxClients);

        int nClientId = 0;

        int nLatency = -1;

        for(int i=0; i<3; i++)
        {
            REQUIRE( pSplitter->SplitterClientAdd(&nClientId) );

            client[i] = nClientId;
        }

        auto pFrameIn1 = std::make_shared<TFrame>( 1000000 );
        auto pFrameIn2 = std::make_shared<TFrame>( 1000000 );
        auto pFrame1 = std::make_shared<TFrame>( 1000000 );
        auto pFrame2 = std::make_shared<TFrame>( 1000000 );
        auto pFrame3 = std::make_shared<TFrame>( 1000000 );

        std::function<void(int, int, TFramePtr)> putLoop = [&](int _nNum, int _nInterval, TFramePtr _pFrameIn)
        {
            for(int i=0; i<_nNum; i++)
            {
                std::this_thread::sleep_for(_nInterval*1ms);

                CHECK( pSplitter->SplitterPut(_pFrameIn, 1000) == 0 );
            }
        };

        std::function<void(int, int, TFramePtr, int)> getLoop = [&](int _nNum, int _nInterval, TFramePtr _pFrameOut, int _nClientId )
        {
            for(int i=0; i<_nNum; i++)
            {
                std::this_thread::sleep_for(_nInterval*1ms);

                CHECK( pSplitter->SplitterGet(_nClientId, _pFrameOut, 1000) == 0 );
            }
        };

        std::thread g1( getLoop, 1000, 100,  pFrame1, client[0]);
        std::thread g2( getLoop, 1000, 10,  pFrame2, client[1]);
        std::thread g3( getLoop, 1000, 1,  pFrame3, client[2]);
        std::thread p1( putLoop, 500, 10,  pFrameIn1 );
        std::thread p2( putLoop, 500, 1,  pFrameIn2 );

        p1.join();
        p2.join();
        g1.join();
        g2.join();
        g3.join();
    }
}
