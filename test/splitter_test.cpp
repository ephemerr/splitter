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

    int client[3] = {};

    SECTION("Put frames")
    {
        auto pFrame = std::make_shared<Frame>( 1000000 );

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
            for(int i=0; i<500; i++)
            {
                std::this_thread::sleep_for(100ms);

                CHECK( pSplitter->SplitterPut(pFrame, 1000) == 0 );
            }
        });

        putter.join();
    }
}


