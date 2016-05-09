//
//  maxdomeII.cpp
//  MaxDome II
//
//  Created by Rodolphe Pineau on 4/9/2016.
//
//

#include "maxdomeII.h"
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
// #ifdef SB_MAC_BUILD
#include <unistd.h>
// #endif

CMaxDome::CMaxDome()
{

}

CMaxDome::~CMaxDome()
{

}

bool CMaxDome::Connect(const char *szPort)
{
    if(pSerx->open(szPort) == 0)
        bIsConnected = true;
    else
        bIsConnected = false;

    // Check to see if we can't even connect to the device
    if(!bIsConnected)
        return false;

    // bIsConnected = GetFirmware(szFirmware);

    if(!bIsConnected)
        pSerx->close();

    return bIsConnected;
}


void CMaxDome::Disconnect(void)
{
    if(bIsConnected)
        pSerx->close();

    bIsConnected = false;
}
