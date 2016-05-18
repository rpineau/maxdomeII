//
//  maxdomeII.cpp
//  MaxDome II X2 plugin
//
//  Created by Rodolphe Pineau on 4/9/2016.
//
// most of the code comes from the INDI driver.
// The following is the original author header.
/*
 Max Dome II Driver
 Copyright (C) 2009 Ferran Casarramona (ferran.casarramona@gmail.com)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 */


#include "maxdomeII.h"
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
// #ifdef SB_MAC_BUILD
#include <unistd.h>
// #endif

CMaxDome::CMaxDome()
{
    // set some sane values
    pSerx = NULL;
    bIsConnected = false;

    mNbTicksPerRev = 0;

    mCurrentAzPosition = 0;
    mCurrentAzPositionInTicks = 0;

    mHomeAz = 0;
    mHomeAzInTicks = 0;

    mCloseShutterBeforePark = true;
    mShutterOpened = false;
    
    mParked = true;
    mHomed = false;
}

CMaxDome::~CMaxDome()
{

}

int CMaxDome::Init_Communication(void)
{
    char cMessage[MAX_BUFFER];
    unsigned long  nBytesWrite;
    int nReturn;
    int nErrorType = MD2_OK;

    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = ACK_CMD;
    cMessage[3] = checksum_MaxDomeII(cMessage, 3);

    printf("sending Init sequence to MaxDome II\n");
    nErrorType = pSerx->writeFile(cMessage, 4, nBytesWrite);
    
    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;


    if (nBytesWrite != 4)
        return MD2_CANT_CONNECT;
    printf("Reading response from Init sequence\n");
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (char)(ACK_CMD | TO_COMPUTER))
        return 0;

    return BAD_CMD_RESPONSE;	// Response don't match command

}

bool CMaxDome::Connect(const char *szPort)
{
    int err;
    unsigned tmpAz;
    unsigned tmpHomePosition;
    enum SH_Status tmpShutterStatus;
    enum AZ_Status tmpAzimuthStatus;

    // 19200 8N1
    printf("Opening port %s\n", szPort);
    if(pSerx->open(szPort,19200) == 0)
        bIsConnected = true;
    else
        bIsConnected = false;

    // Check to see if we can't even connect to the device
    if(!bIsConnected)
        return false;

    printf("Purging RxTx\n");
    // bIsConnected = GetFirmware(szFirmware);
    pSerx->purgeTxRx();

    printf("Purging RxTx Done\n");
    // init the comms
    err = Init_Communication();
    printf("err from Init_Communication = %d\n",err);
    if(err)
    {
        pSerx->close();
        bIsConnected = false;
        
        printf("bIsConnected = %d\n",bIsConnected);
        return bIsConnected;
    }

    // get the device status to make sure we're properly connected.
    err = Status_MaxDomeII(tmpShutterStatus, tmpAzimuthStatus, tmpAz, tmpHomePosition);

    if(err)
    {
        bIsConnected = false;
        pSerx->close();
    }

    // sset a few things on connect
    if (bIsConnected)
    {
        // if(mHomeOnCnnect)
        //      Home_Azimuth_MaxDomeII();
        err |= SetPark_MaxDomeII(mCloseShutterBeforePark, mParkAz);
        err |= SetTicksPerCount_MaxDomeII(mNbTicksPerRev);
        if(err)
        {
            bIsConnected = false; // if this fails we're not properly connectiong.
            pSerx->close();
        }
    }
    
    return bIsConnected;
}


void CMaxDome::Disconnect(void)
{
    if(bIsConnected)
    {
        Exit_Shutter_MaxDomeII();
        pSerx->close();
    }
    bIsConnected = false;
}

/*
	Calculates or checks the checksum
	It ignores first byte
	For security we limit the checksum calculation to MAX_BUFFER length

	@param cMessage Pointer to unsigned char array with the message
	@param nLen Length of the message
	@return Checksum byte
 */
signed char CMaxDome::checksum_MaxDomeII(char *cMessage, int nLen)
{
    int nIdx;
    char nChecksum = 0;

    for (nIdx = 1; nIdx < nLen && nIdx < MAX_BUFFER; nIdx++)
    {
        nChecksum -= cMessage[nIdx];
    }

    return nChecksum;
}

/*
	Reads a response from MAX DOME II
	It verifies message sintax and checksum

	@param cMessage Pointer to a buffer to receive the message
	@return 0 if message is Ok. -1 if no response or no start caracter found. -2 invalid declared message length. -3 message too short. -4 checksum error
 */
int CMaxDome::ReadResponse_MaxDomeII(char *cMessage)
{
    unsigned long nBytesRead;
    int nLen = MAX_BUFFER;
    char nChecksum;
    int nErrorType = MD2_OK;
    *cMessage = 0x00;
    cMessage[13] = 0x00;

    // Look for a 0x01 starting character, until time out occurs or MAX_BUFFER characters was read
    while (*cMessage != 0x01 && nErrorType == MD2_OK )
    {
        nErrorType = pSerx->readFile(cMessage, 1, nBytesRead, MAX_TIMEOUT);
        if (nBytesRead !=1) // timeout
            nErrorType = MD2_CANT_CONNECT;
        printf("[ReadResponse_MaxDomeII] nErrorType = %d\n", nErrorType);
    }

    if (nErrorType != MD2_OK || *cMessage != 0x01)
        return MD2_CANT_CONNECT;

    // Read message length
    nErrorType = pSerx->readFile(cMessage + 1, 1, nBytesRead, MAX_TIMEOUT);

    if (nErrorType != MD2_OK || nBytesRead!=1 || cMessage[1] < 0x02 || cMessage[1] > 0x0E)
        return -2;

    nLen = cMessage[1];
    // Read the rest of the message
    nErrorType = pSerx->readFile(cMessage + 2, nLen, nBytesRead, MAX_TIMEOUT);

    if (nErrorType != MD2_OK || nBytesRead != nLen)
        return -3;

    nChecksum = checksum_MaxDomeII(cMessage, nLen + 2);
    if (nChecksum != 0x00)
        return -4;
    return 0;
}

/*
	Abort azimuth movement

	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Abort_Azimuth_MaxDomeII(void)
{
    char cMessage[MAX_BUFFER];
    // int nErrorType;
    unsigned long  nBytesWrite;
    int nReturn;
    int nErrorType = MD2_OK;

    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = ABORT_CMD;
    cMessage[3] = checksum_MaxDomeII(cMessage, 3);

    nErrorType = pSerx->writeFile(cMessage, 4, nBytesWrite);

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;
    

    if (nBytesWrite != 4)
        return MD2_CANT_CONNECT;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (char)(ABORT_CMD | TO_COMPUTER))
        return 0;

    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Move until 'home' position is detected

	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Home_Azimuth_MaxDomeII(void)
{
    char cMessage[MAX_BUFFER];
    int nErrorType;
    unsigned long  nBytesWrite;;
    int nReturn;

    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = HOME_CMD;
    cMessage[3] = checksum_MaxDomeII(cMessage, 3);

    nErrorType = pSerx->writeFile(cMessage, 4, nBytesWrite);

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (char)(HOME_CMD | TO_COMPUTER))
        return 0;

    return BAD_CMD_RESPONSE;	// Response doesn't match command
}

/*
	Go to a new azimuth position

	@param nDir Direcction of the movement. 0 E to W. 1 W to E
	@param nTicks Ticks from home position in E to W direction.
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Goto_Azimuth_MaxDomeII(int nDir, int nTicks)
{
    char cMessage[MAX_BUFFER];
    int nErrorType=0;
    unsigned long  nBytesWrite;;
    int nReturn;

    cMessage[0] = 0x01;
    cMessage[1] = 0x05;		// Will follow 5 bytes more
    cMessage[2] = GOTO_CMD;
    cMessage[3] = (char)nDir;
    // Note: we not use nTicks >> 8 in order to remain compatible with both little-endian and big-endian procesors
    cMessage[4] = (char)(nTicks / 256);
    cMessage[5] = (char)(nTicks % 256);
    cMessage[6] = checksum_MaxDomeII(cMessage, 6);

    nErrorType = pSerx->writeFile(cMessage, 7, nBytesWrite);

    if (nBytesWrite != 7)
        return MD2_CANT_CONNECT;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (char)(GOTO_CMD | TO_COMPUTER))
    {
        mGotoTicks = nTicks;
        mHomed = false;
        mParked = false;
        return 0;
    }

    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Go to a new azimuth position
 
    @param : new azimuth position
    @return : error code from Goto_Azimuth_MaxDomeII(int nDir, int nTicks)

*/
int CMaxDome::Goto_Azimuth_MaxDomeII(double newAz)
{
    int dir;
    int ticks;
    int err=0;

    AzToTicks(newAz, dir, ticks);
    err = Goto_Azimuth_MaxDomeII(dir, ticks);
    return err;
}

/*
	Ask Max Dome status

	@param nShutterStatus Returns shutter status
	@param nAzimuthStatus Returns azimuth status
	@param nAzimuthPosition Returns azimuth current position (in ticks from home position)
	@param nHomePosition Returns last position where home was detected
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Status_MaxDomeII(enum SH_Status &nShutterStatus, enum AZ_Status &nAzimuthStatus, unsigned &nAzimuthPosition, unsigned &nHomePosition)
{
    unsigned char cMessage[MAX_BUFFER];
    int nErrorType;
    unsigned long  nBytesWrite;;
    int nReturn;

    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = STATUS_CMD;
    cMessage[3] = checksum_MaxDomeII((char*)cMessage, 3);
    
    printf("Sending data to MaxDome II\n");
    nErrorType = pSerx->writeFile(cMessage, 4, nBytesWrite);

    printf("Sending data to MaxDome II nErrorType = %d\n", nErrorType);

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;

    nReturn = ReadResponse_MaxDomeII((char*)cMessage);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (unsigned char)(STATUS_CMD | TO_COMPUTER))
    {
        nShutterStatus = (enum SH_Status)cMessage[3];
        nAzimuthStatus = (enum AZ_Status)cMessage[4];
        nAzimuthPosition = (unsigned)(((unsigned)cMessage[5]) * 256 + ((unsigned)cMessage[6]));
        nHomePosition = ((unsigned)cMessage[7]) * 256 + ((unsigned)cMessage[8]);
        mCurrentAzPositionInTicks = nAzimuthPosition;
        TicksToAz(mCurrentAzPositionInTicks, mCurrentAzPosition);
        mHomeAzInTicks = nHomePosition;
        return 0;
    }

    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Ack comunication

	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Ack_MaxDomeII(void)
{
    char cMessage[MAX_BUFFER];
    int nErrorType = MD2_OK;
    unsigned long  nBytesWrite;;
    int nReturn;

    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = ACK_CMD;
    cMessage[3] = checksum_MaxDomeII(cMessage, 3);

    nErrorType = pSerx->writeFile(cMessage, 4, nBytesWrite);

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;

    nReturn = ReadResponse_MaxDomeII(cMessage);

    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (char)(ACK_CMD | TO_COMPUTER))
        return 0;

    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Set park coordinates and if need to park before to operating shutter

	@param nParkOnShutter 0 operate shutter at any azimuth. 1 go to park position before operating shutter
	@param nTicks Ticks from home position in E to W direction.
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::SetPark_MaxDomeII(int nParkOnShutter, int nTicks)
{
    char cMessage[MAX_BUFFER];
    int nErrorType;
    unsigned long  nBytesWrite;;
    int nReturn;

    cMessage[0] = 0x01;
    cMessage[1] = 0x05;		// Will follow 5 bytes more
    cMessage[2] = SETPARK_CMD;
    cMessage[3] = (char)nParkOnShutter;
    // Note: we not use nTicks >> 8 in order to remain compatible with both little-endian and big-endian procesors
    cMessage[4] = (char)(nTicks / 256);
    cMessage[5] = (char)(nTicks % 256);
    cMessage[6] = checksum_MaxDomeII(cMessage, 6);

    nErrorType = pSerx->writeFile(cMessage, 7, nBytesWrite);

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != MD2_OK)
        return nReturn;

    if (cMessage[2] == (char)(SETPARK_CMD | TO_COMPUTER))
    {
        mParkAzInTicks = mHomeAzInTicks + nTicks;
        mCloseShutterBeforePark = nParkOnShutter;
        TicksToAz(mParkAzInTicks, mParkAz);
        return MD2_OK;
    }
    return BAD_CMD_RESPONSE;	// Response don't match command
}

int CMaxDome::SetPark_MaxDomeII(int nParkOnShutter, double dAz)

{
    int err;
    int nTicks;
    int dir;

    AzToTicks(dAz, dir, (int &)nTicks);
    err = SetPark_MaxDomeII(nParkOnShutter, nTicks);
    return err;
}


int CMaxDome::Sync_Dome(double dAz)
{
    int err = 0;
    int dir;

    mCurrentAzPosition = dAz;
    AzToTicks(dAz, dir, (int &)mCurrentAzPositionInTicks);
    return err;
}

//
// send the dome to its park position
//
int CMaxDome::Park_MaxDomeII(void)
{
    int nErrorType;

    nErrorType = Goto_Azimuth_MaxDomeII(mParkAz);
    return nErrorType;
}

int CMaxDome::Unpark(void)
{
    mParked = false;
    return 0;
}

/*
 Set ticks per turn of the dome

 @param nTicks Ticks from home position in E to W direction.
 @return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::SetTicksPerCount_MaxDomeII(int nTicks)
{
    char cMessage[MAX_BUFFER];
    int nErrorType;
    unsigned long  nBytesWrite;;
    int nReturn;

    cMessage[0] = 0x01;
    cMessage[1] = 0x04;		// Will follow 4 bytes more
    cMessage[2] = TICKS_CMD;
    // Note: we do not use nTicks >> 8 in order to remain compatible with both little-endian and big-endian procesors
    cMessage[3] = (char)(nTicks / 256);
    cMessage[4] = (char)(nTicks % 256);
    cMessage[5] = checksum_MaxDomeII(cMessage, 5);

    nErrorType = pSerx->writeFile(cMessage, 6, nBytesWrite);
    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (char)(TICKS_CMD | TO_COMPUTER))
    {
        mNbTicksPerRev = nTicks;
        return 0;
    }
    return BAD_CMD_RESPONSE;	// Response don't match command
}

///////////////////////////////////////////////////////////////////////////
//
//  Shutter commands
//
///////////////////////////////////////////////////////////////////////////

/*
	Opens the shutter fully

	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Open_Shutter_MaxDomeII()
{
    char cMessage[MAX_BUFFER];
    int nErrorType;
    unsigned long  nBytesWrite;;
    int nReturn;

    cMessage[0] = 0x01;
    cMessage[1] = 0x03;		// Will follow 3 bytes more
    cMessage[2] = SHUTTER_CMD;
    cMessage[3] = OPEN_SHUTTER;
    cMessage[4] = checksum_MaxDomeII(cMessage, 4);

    nErrorType = pSerx->writeFile(cMessage, 5, nBytesWrite);

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (char)(SHUTTER_CMD | TO_COMPUTER))
        return 0;

    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Opens the upper shutter only

	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Open_Upper_Shutter_Only_MaxDomeII()
{
    char cMessage[MAX_BUFFER];
    int nErrorType;
    unsigned long  nBytesWrite;;
    int nReturn;

    cMessage[0] = 0x01;
    cMessage[1] = 0x03;		// Will follow 3 bytes more
    cMessage[2] = SHUTTER_CMD;
    cMessage[3] = OPEN_UPPER_ONLY_SHUTTER;
    cMessage[4] = checksum_MaxDomeII(cMessage, 4);

    nErrorType = pSerx->writeFile(cMessage, 5, nBytesWrite);

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (char)(SHUTTER_CMD | TO_COMPUTER))
        return 0;

    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Close the shutter

	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Close_Shutter_MaxDomeII()
{
    char cMessage[MAX_BUFFER];
    int nErrorType;
    unsigned long  nBytesWrite;;
    int nReturn;

    cMessage[0] = 0x01;
    cMessage[1] = 0x03;		// Will follow 3 bytes more
    cMessage[2] = SHUTTER_CMD;
    cMessage[3] = CLOSE_SHUTTER;
    cMessage[4] = checksum_MaxDomeII(cMessage, 4);

    nErrorType = pSerx->writeFile(cMessage, 5, nBytesWrite);

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (char)(SHUTTER_CMD | TO_COMPUTER))
        return 0;

    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Abort shutter movement

	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Abort_Shutter_MaxDomeII()
{
    char cMessage[MAX_BUFFER];
    int nErrorType;
    unsigned long  nBytesWrite;;
    int nReturn;

    cMessage[0] = 0x01;
    cMessage[1] = 0x03;		// Will follow 3 bytes more
    cMessage[2] = SHUTTER_CMD;
    cMessage[3] = ABORT_SHUTTER;
    cMessage[4] = checksum_MaxDomeII(cMessage, 4);

    nErrorType = pSerx->writeFile(cMessage, 5, nBytesWrite);

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;
    
    if (cMessage[2] == (char)(SHUTTER_CMD | TO_COMPUTER))
        return 0;

    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Exit shutter (?) Normally send at program exit
	
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Exit_Shutter_MaxDomeII()
{
    char cMessage[MAX_BUFFER];
    int nErrorType;
    unsigned long  nBytesWrite;;
    int nReturn;

    cMessage[0] = 0x01;
    cMessage[1] = 0x03;		// Will follow 3 bytes more
    cMessage[2] = SHUTTER_CMD;
    cMessage[3] = EXIT_SHUTTER;
    cMessage[4] = checksum_MaxDomeII(cMessage, 4);

    nErrorType = pSerx->writeFile(cMessage, 5, nBytesWrite);

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;
    
    if (cMessage[2] == (char)(SHUTTER_CMD | TO_COMPUTER))
        return 0;

    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Convert pdAz to number of ticks from home and direction.

 */
void CMaxDome::AzToTicks(double pdAz, int &dir, int &ticks)
{
    dir = 0;

    ticks = floor(0.5 + (pdAz - mHomeAz) * mNbTicksPerRev / 360.0);
    while (ticks > mNbTicksPerRev) ticks -= mNbTicksPerRev;
    while (ticks < 0) ticks += mNbTicksPerRev;

    // find the dirrection with the shortest path
    if( (mCurrentAzPosition < pdAz) && (mCurrentAzPosition <(pdAz -180)))
    {
        dir = 1;
    }
    else if (mCurrentAzPosition > pdAz)
    {
        dir = 1;
    }

}


/*
	Convert ticks from home to Az
 
*/

void CMaxDome::TicksToAz(int ticks, double &pdAz)
{
    
    pdAz = mHomeAzInTicks + ticks * 360.0 / mNbTicksPerRev;
    while (pdAz < 0) pdAz += 360;
    while (pdAz >= 360) pdAz -= 360;
}


int CMaxDome::IsGoToComplete(bool &complete)
{
    int err = 0;
    unsigned tmpAz;
    unsigned tmpHomePosition;
    enum SH_Status tmpShutterStatus;
    enum AZ_Status tmpAzimuthStatus;

    err = Status_MaxDomeII(tmpShutterStatus, tmpAzimuthStatus, tmpAz, tmpHomePosition);
    if(err)
        return err;

    if((mGotoTicks == tmpAz) && (tmpAzimuthStatus == As_IDLE || tmpAzimuthStatus == As_IDLE2))
        complete = true;
    else
        complete = false;

    return err;
}

int CMaxDome::IsOpenComplete(bool &complete)
{
    int err=0;
    unsigned tmpAz;
    unsigned tmpHomePosition;
    enum SH_Status tmpShutterStatus;
    enum AZ_Status tmpAzimuthStatus;

    err = Status_MaxDomeII(tmpShutterStatus, tmpAzimuthStatus, tmpAz, tmpHomePosition);
    if(err)
        return err;

    if( tmpShutterStatus == Ss_OPEN)
    {
        complete = true;
        mShutterOpened = true;
    }
    else
        complete = false;

    return err;

}

int CMaxDome::IsCloseComplete(bool &complete)
{
    int err=0;
    unsigned tmpAz;
    unsigned tmpHomePosition;
    enum SH_Status tmpShutterStatus;
    enum AZ_Status tmpAzimuthStatus;

    err = Status_MaxDomeII(tmpShutterStatus, tmpAzimuthStatus, tmpAz, tmpHomePosition);
    if(err)
        return err;

    if( tmpShutterStatus == Ss_CLOSED)
    {
        complete = true;
        mShutterOpened = false;
    }
    else
        complete = false;
 
    return err;
}


int CMaxDome::IsParkComplete(bool &complete)
{
    int err=0;
    unsigned tmpAz;
    unsigned tmpHomePosition;
    enum SH_Status tmpShutterStatus;
    enum AZ_Status tmpAzimuthStatus;

    err = Status_MaxDomeII(tmpShutterStatus, tmpAzimuthStatus, tmpAz, tmpHomePosition);
    if(err)
        return err;

    if((mParkAzInTicks == tmpAz) && (tmpAzimuthStatus == As_IDLE || tmpAzimuthStatus == As_IDLE2))
    {
        complete = true;
        mParked = true;
    }
    else
        complete = false;

    return err;

}

int CMaxDome::IsUnparkComplete(bool &complete)
{
    int err=0;
    unsigned tmpAz;
    unsigned tmpHomePosition;
    enum SH_Status tmpShutterStatus;
    enum AZ_Status tmpAzimuthStatus;

    err = Status_MaxDomeII(tmpShutterStatus, tmpAzimuthStatus, tmpAz, tmpHomePosition);
    if(err)
        return err;

    if(tmpAzimuthStatus == As_IDLE || tmpAzimuthStatus == As_IDLE2)
    {
        complete = true;
        mParked = false;
    }
    else
        complete = false;

    return err;

}

int CMaxDome::IsFindHomeComplete(bool &complete)
{
    int err=0;
    unsigned tmpAz;
    unsigned tmpHomePosition;
    enum SH_Status tmpShutterStatus;
    enum AZ_Status tmpAzimuthStatus;

    err = Status_MaxDomeII(tmpShutterStatus, tmpAzimuthStatus, tmpAz, tmpHomePosition);
    if(err)
        return err;

    if((tmpHomePosition == tmpAz) && (tmpAzimuthStatus == As_IDLE || tmpAzimuthStatus == As_IDLE2))
    {
        complete = true;
        mHomed = true;
    }
    else
        complete = false;

    return err;

}

#pragma mark - Getter / Setter

int CMaxDome::getNbTicksPerRev()
{
    return mNbTicksPerRev;
}

void CMaxDome::setNbTicksPerRev(int nbTicksPerRev)
{
    mNbTicksPerRev = nbTicksPerRev;
    if(bIsConnected)
        SetTicksPerCount_MaxDomeII(mNbTicksPerRev);
}


double CMaxDome::getHomeAz()
{
    return mHomeAz;
}

void CMaxDome::setHomeAz(double dAz)
{
    mHomeAz = dAz;

}


double CMaxDome::getParkAz()
{
    return mParkAz;

}

void CMaxDome::setParkAz(double dAz)
{
    int dir;
    mParkAz = dAz;
    AzToTicks(dAz, dir, (int &)mParkAzInTicks);


}

bool CMaxDome::getCloseShutterBeforePark()
{
    return mCloseShutterBeforePark;
}

void CMaxDome::setCloseShutterBeforePark(bool close)
{
    mCloseShutterBeforePark = close;
}

double CMaxDome::getCurrentAz()
{
    return mCurrentAzPosition;
}

void CMaxDome::setCurrentAz(double dAz)
{
    mCurrentAzPosition = dAz;
}
