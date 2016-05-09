//
//  maxdomeII.h
//  MaxDome II
//
//  Created by Rodolphe Pineau on 4/9/2016.
//
//

#ifndef __MAXDOMEII__
#define __MAXDOMEII__

//#include "MySerx/SerxInterface.h"
#include "../../licensedinterfaces/serxinterface.h"

class CMaxDome
{
public:
    CMaxDome();
    ~CMaxDome();

    bool        Connect(const char *szPort);
    void        Disconnect(void);
    bool        IsConnected(void) { return bIsConnected; }


protected:
    bool        bIsConnected;

    SerXInterface        *pSerx;

};

#endif
