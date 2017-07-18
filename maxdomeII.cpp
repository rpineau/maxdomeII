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

CMaxDome::CMaxDome()
{
    // set some sane values
    bDebugLog = true;
    
    pSerx = NULL;
    bIsConnected = false;
    
    mNbTicksPerRev = 0;

    mCurrentAzPosition = 0;
    mCurrentAzPositionInTicks = 0;
    
    mHomeAz = 0;
    mHomeAzInTicks = 0;

    mParkAzInTicks = 0;
    mParkAz = -1;

    mCloseShutterBeforePark = true;
    mShutterOpened = false;
    
    mParked = true;
    mHomed = false;
    mCalibrating = false;

#ifdef	MAXDOME_DEBUG
    Logfile = fopen(MAXDOME_LOGFILENAME, "w");
    ltime = time(NULL);
    char *timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CMaxDome Constructor Called.\n", timestamp);
    fflush(Logfile);
#endif

    
}

CMaxDome::~CMaxDome()
{
#ifdef	MAXDOME_DEBUG
    // Close LogFile
    if (Logfile) fclose(Logfile);
#endif

}

int CMaxDome::Connect(const char *pszPort)
{
    int nErr;
    int tmpAz;
    int tmpHomePosition;
    enum SH_Status tmpShutterStatus;
    enum AZ_Status tmpAzimuthStatus;

    if(!pSerx)
        return ERR_COMMNOLINK;

#ifdef MAXDOME_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CMaxDome::Connect Called %s\n", timestamp, pszPort);
    fflush(Logfile);
#endif

    // 19200 8N1
    //if(pSerx->open(szPort, 19200, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1") == 0)
    nErr = pSerx->open(pszPort, 19200, SerXInterface::B_NOPARITY);
    if(nErr) {
        bIsConnected = false;
        return nErr;
    }
    bIsConnected = true;

#ifdef MAXDOME_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CMaxDome::Connect connected to %s\n", timestamp, pszPort);
    fflush(Logfile);
#endif

    // Check to see if we can't even connect to the device
    if(!bIsConnected)
        return false;

    pSerx->purgeTxRx();

    // init the comms
    nErr = Init_Communication();
    if(nErr)
    {
        pSerx->close();
        bIsConnected = false;
        return bIsConnected;
    }

    if(mNbTicksPerRev) {
        nErr = SetTicksPerCount_MaxDomeII(mNbTicksPerRev);
        if(nErr) {
            snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::setNbTicksPerRev -> SetTicksPerCount_MaxDomeII] err = %d",nErr);
            mLogger->out(mLogBuffer);
            bIsConnected = false;

            return false;
        }
    }

    if(mParkAz != -1) {
        nErr = setParkAz(mCloseShutterBeforePark, mParkAz);
        if(nErr) {
            bIsConnected = false;
            return false;
        }
    }
    // get the device status to make sure we're properly connected.
    nErr = Status_MaxDomeII(tmpShutterStatus, tmpAzimuthStatus, tmpAz, tmpHomePosition);
    if(nErr)
    {
        bIsConnected = false;
        pSerx->close();
    }

    return nErr;
}


void CMaxDome::Disconnect(void)
{
    if(bIsConnected)
    {
        Exit_Shutter_MaxDomeII();
        pSerx->purgeTxRx();
        pSerx->close();
    }
    bIsConnected = false;
}

int CMaxDome::Init_Communication(void)
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    unsigned long  nBytesWrite;
    int nReturn;
    int nErrorType = MD2_OK;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = ACK_CMD;
    cMessage[3] = checksum_MaxDomeII(cMessage, 3);
    
#ifdef MAXDOME_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage,cHexMessage,cMessage[1]+2);
    fprintf(Logfile, "[%s] CMaxDome::Init_Communication sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Init_Communication] cMessage = %s",cHexMessage);
        mLogger->out(mLogBuffer);
    }

    nErrorType = pSerx->writeFile(cMessage, 4, nBytesWrite);
    pSerx->flushTx();

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;
    
    
    if (nBytesWrite != 4)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);

    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Init_Communication] response = %s\n",cHexMessage);
        mLogger->out(mLogBuffer);
    }

    if (nReturn != 0)
        return nReturn;
    
    if (cMessage[2] == (unsigned char)(ACK_CMD | TO_COMPUTER))
        return 0;
    
    return BAD_CMD_RESPONSE;	// Response don't match command
    
}


/*
	Calculates or checks the checksum
	It ignores first byte
	For security we limit the checksum calculation to MD_BUFFER_SIZE length
 
	@param cMessage Pointer to unsigned char array with the message
	@param nLen Length of the message
	@return Checksum byte
 */
signed char CMaxDome::checksum_MaxDomeII(unsigned char *cMessage, int nLen)
{
    int nIdx;
    char nChecksum = 0;
    
    for (nIdx = 1; nIdx < nLen && nIdx < MD_BUFFER_SIZE; nIdx++)
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
int CMaxDome::ReadResponse_MaxDomeII(unsigned char *cMessage)
{
    unsigned long nBytesRead;
    int nLen = MD_BUFFER_SIZE;
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    char nChecksum;
    int nErrorType = MD2_OK;
    
    memset(cMessage, 0, MD_BUFFER_SIZE);
    
    // Look for a 0x01 starting character, until time out occurs or MD_BUFFER_SIZE characters was read
    while (*cMessage != 0x01 && nErrorType == MD2_OK )
    {
        nErrorType = pSerx->readFile(cMessage, 1, nBytesRead, MAX_TIMEOUT);
        if (nBytesRead !=1) // timeout
            nErrorType = MD2_CANT_CONNECT;
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

#ifdef MAXDOME_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage,cHexMessage,cMessage[1]+2);
    fprintf(Logfile, "[%s] CMaxDome::ReadResponse_MaxDomeII got :\n%s\n\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    if (nErrorType != MD2_OK || nBytesRead != nLen) {
#ifdef MAXDOME_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CMaxDome::ReadResponse_MaxDomeII error reading response\n", timestamp);
        fflush(Logfile);
#endif
        return -3;
    }

    nChecksum = checksum_MaxDomeII(cMessage, nLen + 2);
    if (nChecksum != 0x00) {
#ifdef MAXDOME_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CMaxDome::ReadResponse_MaxDomeII checksum error in response\n", timestamp);
        fflush(Logfile);
#endif
        return -4;
    }
    return 0;
}

/*
	Abort azimuth movement
 
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Abort_Azimuth_MaxDomeII(void)
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    // int nErrorType;
    unsigned long  nBytesWrite;
    int nReturn;
    int nErrorType = MD2_OK;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = ABORT_CMD;
    cMessage[3] = checksum_MaxDomeII(cMessage, 3);

#ifdef MAXDOME_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage,cHexMessage,cMessage[1]+2);
    fprintf(Logfile, "[%s] CMaxDome::Abort_Azimuth_MaxDomeII sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,MD_BUFFER_SIZE);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Abort_Azimuth_MaxDomeII] cMessage = %s",cHexMessage);
        mLogger->out(mLogBuffer);
    }

    nErrorType = pSerx->writeFile(cMessage, 4, nBytesWrite);
    pSerx->flushTx();

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;
    
    
    if (nBytesWrite != 4)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Abort_Azimuth_MaxDomeII] response = %s\n",cHexMessage);
        mLogger->out(mLogBuffer);
    }

    if (nReturn != 0)
        return nReturn;
    
    if (cMessage[2] == (unsigned char)(ABORT_CMD | TO_COMPUTER))
        return 0;
    
    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Move until 'home' position is detected
 
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Home_Azimuth_MaxDomeII(void)
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    
    int nErrorType;
    unsigned long  nBytesWrite;;
    int nReturn;
    
    memset(cMessage, 0, MD_BUFFER_SIZE);
    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = HOME_CMD;
    cMessage[3] = checksum_MaxDomeII(cMessage, 3);
    
#ifdef MAXDOME_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage,cHexMessage,cMessage[1]+2);
    fprintf(Logfile, "[%s] CMaxDome::Home_Azimuth_MaxDomeII sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Home_Azimuth_MaxDomeII] cMessage = %s",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    
    nErrorType = pSerx->writeFile(cMessage, 4, nBytesWrite);
    pSerx->flushTx();

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    
    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Home_Azimuth_MaxDomeII] response = %s\n",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    
    if (nReturn != 0)
        return nReturn;
    
    if (cMessage[2] == (unsigned char)(HOME_CMD | TO_COMPUTER))
        return 0;
    
    return BAD_CMD_RESPONSE;	// Response doesn't match command
}

/*
	Go to a new azimuth position
 
	@param nDir Direcction of the movement. 1 E to W. 2 W to E
	@param nTicks Ticks from home position in E to W direction.
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Goto_Azimuth_MaxDomeII(int nDir, int nTicks)
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
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

#ifdef MAXDOME_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage,cHexMessage,cMessage[1]+2);
    fprintf(Logfile, "[%s] CMaxDome::Goto_Azimuth_MaxDomeII sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Goto_Azimuth_MaxDomeII] cMessage = %s",cHexMessage);
        mLogger->out(mLogBuffer);
    }

    nErrorType = pSerx->writeFile(cMessage, 7, nBytesWrite);
    pSerx->flushTx();

    if (nBytesWrite != 7)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;
    
    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Goto_Azimuth_MaxDomeII] response = %s\n",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    
    if (cMessage[2] == (unsigned char)(GOTO_CMD | TO_COMPUTER))
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
    unsigned dir;
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
int CMaxDome::Status_MaxDomeII(enum SH_Status &nShutterStatus, enum AZ_Status &nAzimuthStatus, int &nAzimuthPosition, int &nHomePosition)
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    int nErrorType;
    unsigned long  nBytesWrite;;
    int nReturn;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = STATUS_CMD;
    cMessage[3] = checksum_MaxDomeII(cMessage, 3);
    
#ifdef MAXDOME_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage,cHexMessage,cMessage[1]+2);
    fprintf(Logfile, "[%s] CMaxDome::Status_MaxDomeII sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Status_MaxDomeII] cMessage = %s",cHexMessage);
        mLogger->out(mLogBuffer);
    }

    nErrorType = pSerx->writeFile(cMessage, 4, nBytesWrite);
    pSerx->flushTx();

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Status_MaxDomeII] response = %s\n",cHexMessage);
        mLogger->out(mLogBuffer);
    }

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
    unsigned char cMessage[MD_BUFFER_SIZE];
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    int nErrorType = MD2_OK;
    unsigned long  nBytesWrite;;
    int nReturn;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = ACK_CMD;
    cMessage[3] = checksum_MaxDomeII(cMessage, 3);
    
#ifdef MAXDOME_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage,cHexMessage,cMessage[1]+2);
    fprintf(Logfile, "[%s] CMaxDome::Ack_MaxDomeII sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Ack_MaxDomeII] cMessage = %s",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    nErrorType = pSerx->writeFile(cMessage, 4, nBytesWrite);
    pSerx->flushTx();

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Ack_MaxDomeII] response = %s\n",cHexMessage);
        mLogger->out(mLogBuffer);
    }

    if (nReturn != 0)
        return nReturn;
    
    if (cMessage[2] == (unsigned char)(ACK_CMD | TO_COMPUTER))
        return 0;
    
    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
    this switch the park command to a sync command
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::SyncMode_MaxDomeII(void)
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    int nErrorType = MD2_OK;
    unsigned long  nBytesWrite;;
    int nReturn;

    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = SYMC_CMD;
    cMessage[3] = checksum_MaxDomeII(cMessage, 3);

#ifdef MAXDOME_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage,cHexMessage,cMessage[1]+2);
    fprintf(Logfile, "[%s] CMaxDome::SyncMode_MaxDomeII sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Ack_MaxDomeII] cMessage = %s",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    nErrorType = pSerx->writeFile(cMessage, 4, nBytesWrite);
    pSerx->flushTx();

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Ack_MaxDomeII] response = %s\n",cHexMessage);
        mLogger->out(mLogBuffer);
    }

    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (unsigned char)(SYMC_CMD | TO_COMPUTER))
        return 0;

    return BAD_CMD_RESPONSE;	// Response don't match command
}


/*
	Set park coordinates and if need to park before to operating shutter
 
	@param nParkOnShutter 0 operate shutter at any azimuth. 1 go to park position before operating shutter
	@param nTicks Ticks from home position in E to W direction.
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::SetPark_MaxDomeII_Ticks(unsigned nParkOnShutter, int nTicks)
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    int nErrorType;
    unsigned long  nBytesWrite;
    int nReturn;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x05;		// Will follow 5 bytes more
    cMessage[2] = SETPARK_CMD;
    cMessage[3] = (char)nParkOnShutter;
    // Note: we not use nTicks >> 8 in order to remain compatible with both little-endian and big-endian procesors
    cMessage[4] = (char)(nTicks / 256);
    cMessage[5] = (char)(nTicks % 256);
    cMessage[6] = checksum_MaxDomeII(cMessage, 6);
    
#ifdef MAXDOME_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage,cHexMessage,cMessage[1]+2);
    fprintf(Logfile, "[%s] CMaxDome::SetPark_MaxDomeII_Ticks sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::SetPark_MaxDomeII_Ticks] cMessage = %s",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    nErrorType = pSerx->writeFile(cMessage, 7, nBytesWrite);
    pSerx->flushTx();

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::SetPark_MaxDomeII_Ticks] response = %s\n",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    if (nReturn != MD2_OK)
        return nReturn;
    if (cMessage[2] == (unsigned char)(SETPARK_CMD | TO_COMPUTER))
    {
		mCloseShutterBeforePark = nParkOnShutter;
		return MD2_OK;
	}
    return BAD_CMD_RESPONSE;	// Response don't match command
}

int CMaxDome::Sync_Dome(double dAz)
{
    int err = 0;
    int nTicks;
    unsigned nDir;

    if(bDebugLog) {
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Sync_Dome] dAz = %3.2f",dAz);
        mLogger->out(mLogBuffer);
    }
    // this switch the park command to a sync command
    err = SyncMode_MaxDomeII();
    if (err)
        return err;

    // apparently it expect 360 - Az for the zync, so mNbTicksPerRev - nTicks
    AzToTicks(dAz, nDir, nTicks);
    err = SetPark_MaxDomeII_Ticks(mCloseShutterBeforePark, mNbTicksPerRev - nTicks);
    if (err)
        return err;

    mCurrentAzPosition = dAz;
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
    mCurrentAzPosition = mParkAz;
    return 0;
}

/*
 Set ticks per turn of the dome
 
 @param nTicks Ticks from home position in E to W direction.
 @return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::SetTicksPerCount_MaxDomeII(int nTicks)
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
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
    
#ifdef MAXDOME_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage,cHexMessage,cMessage[1]+2);
    fprintf(Logfile, "[%s] CMaxDome::SetTicksPerCount_MaxDomeII sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::SetTicksPerCount_MaxDomeII] cMessage = %s",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    nErrorType = pSerx->writeFile(cMessage, 6, nBytesWrite);
    pSerx->flushTx();

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::SetTicksPerCount_MaxDomeII] response = %s\n",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    if (nReturn != 0)
        return nReturn;
    
    if (cMessage[2] == (unsigned char)(TICKS_CMD | TO_COMPUTER))
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
    unsigned char cMessage[MD_BUFFER_SIZE];
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    int nErrorType;
    unsigned long  nBytesWrite;;
    int nReturn;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x03;		// Will follow 3 bytes more
    cMessage[2] = SHUTTER_CMD;
    cMessage[3] = OPEN_SHUTTER;
    cMessage[4] = checksum_MaxDomeII(cMessage, 4);
    
#ifdef MAXDOME_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage,cHexMessage,cMessage[1]+2);
    fprintf(Logfile, "[%s] CMaxDome::Open_Shutter_MaxDomeII sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Open_Shutter_MaxDomeII] cMessage = %s",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    nErrorType = pSerx->writeFile(cMessage, 5, nBytesWrite);
    pSerx->flushTx();

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Open_Shutter_MaxDomeII] response = %s\n",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    if (nReturn != 0)
        return nReturn;
    
    if (cMessage[2] == (unsigned char)(SHUTTER_CMD | TO_COMPUTER))
        return 0;
    
    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Opens the upper shutter only
 
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Open_Upper_Shutter_Only_MaxDomeII()
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    int nErrorType;
    unsigned long  nBytesWrite;;
    int nReturn;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x03;		// Will follow 3 bytes more
    cMessage[2] = SHUTTER_CMD;
    cMessage[3] = OPEN_UPPER_ONLY_SHUTTER;
    cMessage[4] = checksum_MaxDomeII(cMessage, 4);
    
#ifdef MAXDOME_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage,cHexMessage,cMessage[1]+2);
    fprintf(Logfile, "[%s] CMaxDome::Open_Upper_Shutter_Only_MaxDomeII sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Open_Upper_Shutter_Only_MaxDomeII] cMessage = %s",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    nErrorType = pSerx->writeFile(cMessage, 5, nBytesWrite);
    pSerx->flushTx();

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Open_Upper_Shutter_Only_MaxDomeII] response = %s\n",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    if (nReturn != 0)
        return nReturn;
    
    if (cMessage[2] == (unsigned char)(SHUTTER_CMD | TO_COMPUTER))
        return 0;
    
    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Close the shutter
 
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Close_Shutter_MaxDomeII()
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    int nErrorType;
    unsigned long  nBytesWrite;;
    int nReturn;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x03;		// Will follow 3 bytes more
    cMessage[2] = SHUTTER_CMD;
    cMessage[3] = CLOSE_SHUTTER;
    cMessage[4] = checksum_MaxDomeII(cMessage, 4);
    
#ifdef MAXDOME_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage,cHexMessage,cMessage[1]+2);
    fprintf(Logfile, "[%s] CMaxDome::Close_Shutter_MaxDomeII sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Close_Shutter_MaxDomeII] cMessage = %s",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    nErrorType = pSerx->writeFile(cMessage, 5, nBytesWrite);
    pSerx->flushTx();

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Close_Shutter_MaxDomeII] response = %s\n",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    if (nReturn != 0)
        return nReturn;
    
    if (cMessage[2] == (unsigned char)(SHUTTER_CMD | TO_COMPUTER))
        return 0;
    
    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Abort shutter movement
 
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Abort_Shutter_MaxDomeII()
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    int nErrorType;
    unsigned long  nBytesWrite;;
    int nReturn;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x03;		// Will follow 3 bytes more
    cMessage[2] = SHUTTER_CMD;
    cMessage[3] = ABORT_SHUTTER;
    cMessage[4] = checksum_MaxDomeII(cMessage, 4);
    
#ifdef MAXDOME_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage,cHexMessage,cMessage[1]+2);
    fprintf(Logfile, "[%s] CMaxDome::Abort_Shutter_MaxDomeII sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Abort_Shutter_MaxDomeII] cMessage = %s",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    nErrorType = pSerx->writeFile(cMessage, 5, nBytesWrite);
    pSerx->flushTx();

    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Abort_Shutter_MaxDomeII] response = %s\n",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    if (nReturn != 0)
        return nReturn;
    
    if (cMessage[2] == (unsigned char)(SHUTTER_CMD | TO_COMPUTER))
        return 0;
    
    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Exit shutter (?) Normally send at program exit
	
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Exit_Shutter_MaxDomeII()
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    int nErrorType;
    unsigned long  nBytesWrite;;
    int nReturn;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x03;		// Will follow 3 bytes more
    cMessage[2] = SHUTTER_CMD;
    cMessage[3] = EXIT_SHUTTER;
    cMessage[4] = checksum_MaxDomeII(cMessage, 4);
    
#ifdef MAXDOME_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage,cHexMessage,cMessage[1]+2);
    fprintf(Logfile, "[%s] CMaxDome::Exit_Shutter_MaxDomeII sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Exit_Shutter_MaxDomeII] cMessage = %s",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    nErrorType = pSerx->writeFile(cMessage, 5, nBytesWrite);
    pSerx->flushTx();
    
    if (nErrorType != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if(bDebugLog) {
        hexdump(cMessage,cHexMessage,cMessage[1]+2);
        snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::Exit_Shutter_MaxDomeII] response = %s\n",cHexMessage);
        mLogger->out(mLogBuffer);
    }
    if (nReturn != 0)
        return nReturn;
    
    if (cMessage[2] == (unsigned char)(SHUTTER_CMD | TO_COMPUTER))
        return 0;
    
    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Convert pdAz to number of ticks from home and direction.
 
 */
void CMaxDome::AzToTicks(double pdAz, unsigned &dir, int &ticks)
{
    dir = MAXDOMEII_EW_DIR;
    
    ticks = (int) floor(0.5 + (pdAz - mHomeAz) * mNbTicksPerRev / 360.0);
    while (ticks > mNbTicksPerRev) ticks -= mNbTicksPerRev;
    while (ticks < 0) ticks += mNbTicksPerRev;
    
    // find the dirrection with the shortest path
    if (pdAz > mCurrentAzPosition) {
        if (pdAz - mCurrentAzPosition > 180.0)
            dir = MAXDOMEII_WE_DIR;
        else
            dir = MAXDOMEII_WE_DIR;
    }
    else {
        if (mCurrentAzPosition - pdAz > 180.0)
            dir = MAXDOMEII_WE_DIR;
        else
            dir = MAXDOMEII_EW_DIR;
    }
}


/*
	Convert ticks from home to Az
 
 */

void CMaxDome::TicksToAz(int ticks, double &pdAz)
{
    
    pdAz = mHomeAz + (ticks * 360.0 / mNbTicksPerRev);
    while (pdAz < 0) pdAz += 360;
    while (pdAz >= 360) pdAz -= 360;
}


int CMaxDome::IsGoToComplete(bool &complete)
{
    int err = 0;
    int tmpAz;
    int tmpHomePosition;
    enum SH_Status tmpShutterStatus;
    enum AZ_Status tmpAzimuthStatus;
    
    err = Status_MaxDomeII(tmpShutterStatus, tmpAzimuthStatus, tmpAz, tmpHomePosition);
    if(err)
        return err;

    // need to check +/- 1 ticks there
    // if(((tmpAz <= mGotoTicks+1) && (tmpAz >= mGotoTicks-1)) && (tmpAzimuthStatus == As_IDLE || tmpAzimuthStatus == As_IDLE2))
    if(tmpAzimuthStatus == As_IDLE || tmpAzimuthStatus == As_IDLE2)
        complete = true;
    else
        complete = false;
    
    return err;
}

int CMaxDome::IsOpenComplete(bool &complete)
{
    int err=0;
    int tmpAz;
    int tmpHomePosition;
    enum SH_Status tmpShutterStatus;
    enum AZ_Status tmpAzimuthStatus;
    
    err = Status_MaxDomeII(tmpShutterStatus, tmpAzimuthStatus, tmpAz, tmpHomePosition);
    if(err)
        return err;
    
    if( tmpShutterStatus == Ss_OPEN) {
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
    int tmpAz;
    int tmpHomePosition;
    enum SH_Status tmpShutterStatus;
    enum AZ_Status tmpAzimuthStatus;
    
    err = Status_MaxDomeII(tmpShutterStatus, tmpAzimuthStatus, tmpAz, tmpHomePosition);
    if(err)
        return err;
    
    if( tmpShutterStatus == Ss_CLOSED) {
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
    int tmpAz;
    int tmpHomePosition;
    enum SH_Status tmpShutterStatus;
    enum AZ_Status tmpAzimuthStatus;
    
    err = Status_MaxDomeII(tmpShutterStatus, tmpAzimuthStatus, tmpAz, tmpHomePosition);
    if(err)
        return err;

    // if(((tmpAz <= mParkAzInTicks+1) && (tmpAz >= mParkAzInTicks-1)) && (tmpAzimuthStatus == As_IDLE || tmpAzimuthStatus == As_IDLE2)) {
    if(tmpAzimuthStatus == As_IDLE || tmpAzimuthStatus == As_IDLE2) {
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

    if(!bIsConnected)
        return MD2_NOT_CONNECTED;

    mParked = false;
    complete = true;

    return err;
}

int CMaxDome::IsFindHomeComplete(bool &complete)
{
    int err=0;
    int tmpAz;
    int tmpHomePosition;
    enum SH_Status tmpShutterStatus;
    enum AZ_Status tmpAzimuthStatus;
    
    err = Status_MaxDomeII(tmpShutterStatus, tmpAzimuthStatus, tmpAz, tmpHomePosition);
    if(err)
        return err;

#pragma mark TODO : Fix Home az check

    // need to check +/- 1 ticks as we know it pass home by at least 1 ticks.
    //if((tmpAz <= 2) && (tmpAz >= 0))
    //   tmpAz = 1;

    //if((tmpAz == 1) && (tmpAzimuthStatus == As_IDLE || tmpAzimuthStatus == As_IDLE2)) {
    if(tmpAzimuthStatus == As_IDLE || tmpAzimuthStatus == As_IDLE2) {
        if (mCalibrating) {
            setNbTicksPerRev(tmpHomePosition +1);
            SyncMode_MaxDomeII();
            SetPark_MaxDomeII_Ticks(mCloseShutterBeforePark, 32767);
            Goto_Azimuth_MaxDomeII(MAXDOMEII_WE_DIR, 1);
            TicksToAz(1, mCurrentAzPosition);
            mCalibrating = false;
        }
        complete = true;
        mHomed = true;
    }
    else {
        complete = false;
        mHomed = false;
    }
    
    return err;
    
}

#pragma mark - Getter / Setter

int CMaxDome::getNbTicksPerRev()
{
    return mNbTicksPerRev;
}

void CMaxDome::setNbTicksPerRev(unsigned nbTicksPerRev)
{
    int err = 0;
    mNbTicksPerRev = nbTicksPerRev;
    if(bIsConnected) {
        err = SetTicksPerCount_MaxDomeII(mNbTicksPerRev);
        if(err) {
            snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::setNbTicksPerRev -> SetTicksPerCount_MaxDomeII] err = %d",err);
            mLogger->out(mLogBuffer);
        }
    }
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

int CMaxDome::setParkAz(unsigned nParkOnShutter, double dAz)
{
    unsigned dir;
    int err = 0;

    mParkAz = dAz;

    if(bIsConnected) {
        mCloseShutterBeforePark = nParkOnShutter;
        AzToTicks(dAz, dir, mParkAzInTicks);
        err = SetPark_MaxDomeII_Ticks(mCloseShutterBeforePark, mParkAzInTicks);
        if(err) {
            snprintf(mLogBuffer,LOG_BUFFER_SIZE,"[CMaxDome::setParkAz -> SetPark_MaxDomeII] err = %d",err);
            mLogger->out(mLogBuffer);
            return err;
        }
    }
    return err;
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


void CMaxDome::setCalibrating(bool bCal)
{
    mCalibrating = bCal;
}

void CMaxDome::setDebugLog(bool enable)
{
    bDebugLog = enable;
}

void  CMaxDome::hexdump(unsigned char* inputData, unsigned char *outBuffer, int size)
{
    unsigned char *buf = outBuffer;
    int idx=0;
    for(idx=0; idx<size; idx++){
        
        snprintf((char *)buf,4,"%02X ", inputData[idx]);
        buf+=3;
    }
    *buf = 0;
}
