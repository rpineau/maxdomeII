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
    pSerx = NULL;

    bIsConnected = false;
    
    mNbTicksPerRev = 360;

    mCurrentAzPosition = 0;
    mCurrentAzPositionInTicks = 0;
    
    mHomeAz = 0;

    m_dSyncOffset = 0;

    mParkAzInTicks = 0;
    mParkAz = -1;

    mParkBeforeCloseShutter = true;
    mShutterOpened = false;
    
    mParked = true;
    mHomed = false;
    mCalibrating = false;

    m_nDebounceTime = 120;
    
    memset(m_szFirmwareVersion,0,LOG_BUFFER_SIZE);


#ifdef MAXDOME_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\MaxdomeLog.txt";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/MaxdomeLog.txt";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/MaxdomeLog.txt";
#endif
    Logfile = fopen(m_sLogfilePath.c_str(), "w");
#endif

#if defined MAXDOME_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CMaxDome New Constructor Called\n", timestamp);
    fflush(Logfile);
#endif
    
}

CMaxDome::~CMaxDome()
{
#if defined MAXDOME_DEBUG
    // Close LogFile
    if (Logfile) fclose(Logfile);
#endif

}

int CMaxDome::Connect(const char *pszPort)
{
    int nErr = MD2_OK;
    int tmpAz;
    int tmpHomePosition;
    enum SH_Status tmpShutterStatus;
    enum AZ_Status tmpAzimuthStatus;

    if(!pSerx)
        return ERR_COMMNOLINK;
    m_sPort.clear();

#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [Connect] Called %s\n", timestamp, pszPort);
    fflush(Logfile);
#endif

    // 19200 8N1
    nErr = pSerx->open(pszPort, 19200, SerXInterface::B_NOPARITY);
    if(nErr) {
        bIsConnected = false;
        return ERR_COMMNOLINK;
    }
    bIsConnected = true;
    m_sPort.assign(pszPort);

#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [Connect] connected to %s\n", timestamp, pszPort);
    fflush(Logfile);
#endif

    pSerx->purgeTxRx();

    // init the comms
    nErr = Init_Communication();
    if(nErr)
    {
        pSerx->close();
        bIsConnected = false;
        return ERR_NORESPONSE;
    }

    if(mNbTicksPerRev) {
        nErr = SetTicksPerCount_MaxDomeII(mNbTicksPerRev);
        if(nErr) {
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [setNbTicksPerRev] SetTicksPerCount_MaxDomeII] err = %d\n", timestamp, nErr);
            fflush(Logfile);
#endif
            bIsConnected = false;

            return false;
        }
    }

    if(mParkAz != -1) {
        nErr = setParkAz(mParkBeforeCloseShutter, mParkAz);
        if(nErr) {
            bIsConnected = false;
            return ERR_NORESPONSE;
        }
    }

    setDebounceTime(m_nDebounceTime);

    // get the device status to make sure we're properly connected.
    nErr = Status_MaxDomeII(tmpShutterStatus, tmpAzimuthStatus, tmpAz, tmpHomePosition);
    if(nErr)
    {
        bIsConnected = false;
        pSerx->close();
        return ERR_NORESPONSE;
    }

    return nErr;
}

int CMaxDome::reConnect()
{
    int nErr = MD2_OK;

#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [reConnect] Closing serial port connectionn\n", timestamp);
    fflush(Logfile);
#endif

    pSerx->purgeTxRx();
    pSerx->close();

#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [reConnect] Opening serial port connectionn\n", timestamp);
    fflush(Logfile);
#endif

    // 19200 8N1
    nErr = pSerx->open(m_sPort.c_str(), 19200, SerXInterface::B_NOPARITY);
    if(nErr) {
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [reConnect] Opening serial port ERROR : %d\n", timestamp, nErr);
    fflush(Logfile);
#endif
        bIsConnected = false;
        return ERR_COMMNOLINK;
    }
    pSerx->purgeTxRx();

    // init the comms
    nErr = Init_Communication();
    if(nErr) {
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [reConnect] Init_Communication ERROR : %d\n", timestamp, nErr);
    fflush(Logfile);
#endif
        pSerx->close();
        bIsConnected = false;
        nErr = ERR_CMDFAILED;
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

void CMaxDome::getFirmwareVersion(char *version, int strMaxLen)
{
    strncpy(version,m_szFirmwareVersion, strMaxLen);

}

// This actaully returns the firmware version
int CMaxDome::Init_Communication(void)
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    unsigned long  nBytesWrite;
    int nErr = MD2_OK;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = ACK_CMD;
    cMessage[3] = checksum_MaxDomeII(cMessage, 3);
    
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [Init_Communication] sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    nErr = pSerx->writeFile(cMessage, cMessage[1]+2, nBytesWrite);
    pSerx->flushTx();

    if (nErr != MD2_OK) {
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
        unsigned char cHexMessage[LOG_BUFFER_SIZE];
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
        fprintf(Logfile, "[%s] [Init_Communication] writeFile error :\n%d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return ERR_CMDFAILED;
    }
    
    if (nBytesWrite != 4) {
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [Init_Communication] writeFile only byte written :\n%lu\n", timestamp, nBytesWrite);
    fflush(Logfile);
#endif
        return ERR_CMDFAILED;
    }
    nErr = ReadResponse_MaxDomeII(cMessage);

    if (nErr != MD2_OK) {
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [Init_Communication] ReadResponse_MaxDomeII error :\n%d\n", timestamp, nErr);
    fflush(Logfile);
#endif
        return ERR_CMDFAILED;
    }
    
    if (cMessage[2] == (unsigned char)(ACK_CMD | TO_COMPUTER)) {
        m_nFirmwareVersion = (int)cMessage[3];
        snprintf(m_szFirmwareVersion, LOG_BUFFER_SIZE, "2.%1d", m_nFirmwareVersion);
        return MD2_OK;
    }

    return ERR_CMDFAILED;
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
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
#endif
    char nChecksum;
    int nErr = MD2_OK;
    
    memset(cMessage, 0, MD_BUFFER_SIZE);
    
    // Look for a 0x01 starting character, until time out occurs or MD_BUFFER_SIZE characters was read
    while (*cMessage != 0x01 && nErr == MD2_OK )
    {
        nErr = pSerx->readFile(cMessage, 1, nBytesRead, MAX_TIMEOUT);
        if (nBytesRead !=1) // timeout
            nErr = MD2_CANT_CONNECT;
    }
    
    if (nErr != MD2_OK || *cMessage != 0x01)
        return MD2_CANT_CONNECT;
    
    // Read message length
    nErr = pSerx->readFile(cMessage + 1, 1, nBytesRead, MAX_TIMEOUT);
    
    if (nErr != MD2_OK || nBytesRead!=1 || cMessage[1] < 0x02 || cMessage[1] > 0x0E)
        return -2;
    
    nLen = cMessage[1];
    // Read the rest of the message
    nErr = pSerx->readFile(cMessage + 2, nLen, nBytesRead, MAX_TIMEOUT);

#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [ReadResponse_MaxDomeII] got :\n%s\n\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    if (nErr != MD2_OK || nBytesRead != nLen) {
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [ReadResponse_MaxDomeII] error reading response\n", timestamp);
        fflush(Logfile);
#endif
        return -3;
    }

    nChecksum = checksum_MaxDomeII(cMessage, nLen + 2);
    if (nChecksum != 0x00) {
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [ReadResponse_MaxDomeII] checksum error in response\n", timestamp);
        fflush(Logfile);
#endif
        return -4;
    }
    return MD2_OK;
}

/*
	Abort azimuth movement
 
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Abort_Azimuth_MaxDomeII(void)
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    unsigned long  nBytesWrite;
    int nReturn;
    int nErr = MD2_OK;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = ABORT_CMD;
    cMessage[3] = checksum_MaxDomeII(cMessage, 3);

#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [Abort_Azimuth_MaxDomeII] sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    nErr = pSerx->writeFile(cMessage, cMessage[1]+2, nBytesWrite);
    pSerx->flushTx();

    if (nErr != MD2_OK)
        return MD2_CANT_CONNECT;
    
    
    if (nBytesWrite != 4)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != MD2_OK)
        return nReturn;
    
    if (cMessage[2] == (unsigned char)(ABORT_CMD | TO_COMPUTER)) {
        return MD2_OK;
    }

    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Move until 'home' position is detected
 
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Home_Azimuth_MaxDomeII(void)
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    int nErr = MD2_OK;
    unsigned long  nBytesWrite;;
    int nReturn;
    
    memset(cMessage, 0, MD_BUFFER_SIZE);
    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = HOME_CMD;
    cMessage[3] = checksum_MaxDomeII(cMessage, 3);
    
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [Home_Azimuth_MaxDomeII] sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    nErr = pSerx->writeFile(cMessage, cMessage[1]+2, nBytesWrite);
    pSerx->flushTx();

    if (nErr != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    
    if (nReturn != MD2_OK)
        return nReturn;
    
    if (cMessage[2] == (unsigned char)(HOME_CMD | TO_COMPUTER)) {
        m_dSyncOffset = 0;
        return MD2_OK;
    }
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
    int nErr=0;
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

#if defined MAXDOME_DEBUG  && MAXDOME_DEBUG >= 2
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [Goto_Azimuth_MaxDomeII] ticks : %d\n", timestamp, nTicks);
    fprintf(Logfile, "[%s] [Goto_Azimuth_MaxDomeII] dir : %d\n", timestamp, nDir);
#if MAXDOME_DEBUG >= 3
    fprintf(Logfile, "[%s] [Goto_Azimuth_MaxDomeII] sending :\n%s\n", timestamp, cHexMessage);
#endif
    fflush(Logfile);
#endif

    nErr = pSerx->writeFile(cMessage, cMessage[1]+2, nBytesWrite);
    pSerx->flushTx();

    if (nBytesWrite != 7)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != MD2_OK)
        return nReturn;

    if (cMessage[2] == (unsigned char)(GOTO_CMD | TO_COMPUTER))
    {
        mGotoTicks = nTicks;
        mHomed = false;
        mParked = false;
        return MD2_OK;
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

#if defined MAXDOME_DEBUG  && MAXDOME_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [Goto_Azimuth_MaxDomeII] newAz : %3.2f\n", timestamp, newAz);
    fprintf(Logfile, "[%s] [Goto_Azimuth_MaxDomeII] ticks : %d\n", timestamp, ticks);
    fprintf(Logfile, "[%s] [Goto_Azimuth_MaxDomeII] dir : %d\n", timestamp, dir);
    fflush(Logfile);
#endif

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
    int nErr = MD2_OK;
    unsigned long  nBytesWrite;;
    int nbRetry = 0;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = STATUS_CMD;
    cMessage[3] = checksum_MaxDomeII(cMessage, 3);
    
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [Status_MaxDomeII] sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif
    while(nbRetry < MAX_RETRY) {
        nErr = pSerx->writeFile(cMessage, cMessage[1]+2, nBytesWrite);
        pSerx->flushTx();

        if (nErr) {
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
            unsigned char cHexMessage[LOG_BUFFER_SIZE];
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
            fprintf(Logfile, "[%s] [Status_MaxDomeII] writeFile error :%d\n", timestamp, nErr);
            fflush(Logfile);
#endif
            nbRetry++;
            nErr = reConnect();
            if (nErr) {
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
            unsigned char cHexMessage[LOG_BUFFER_SIZE];
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
            fprintf(Logfile, "[%s] [Status_MaxDomeII] writeFile reConnect error :%d\n", timestamp, nErr);
            fflush(Logfile);
#endif
                bIsConnected = false;
                return ERR_CMDFAILED;
            }
            continue;
        }
        nErr = ReadResponse_MaxDomeII(cMessage);
        if (nErr) {
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
            unsigned char cHexMessage[LOG_BUFFER_SIZE];
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
            fprintf(Logfile, "[%s] [Status_MaxDomeII] ReadResponse_MaxDomeII error :%d\n", timestamp, nErr);
            fflush(Logfile);
#endif
            nbRetry++;
            nErr = reConnect();
            if (nErr) {
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
                unsigned char cHexMessage[LOG_BUFFER_SIZE];
                ltime = time(NULL);
                timestamp = asctime(localtime(&ltime));
                timestamp[strlen(timestamp) - 1] = 0;
                hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
                fprintf(Logfile, "[%s] [Status_MaxDomeII] ReadResponse_MaxDomeII reconnect error :%d\n", timestamp, nErr);
                fflush(Logfile);
#endif
                bIsConnected = false;
                return ERR_CMDFAILED;
            }
            continue;
        }
        break;
    }

    if(nbRetry== MAX_RETRY) {
        bIsConnected = false;
        return ERR_CMDFAILED;
    }

    if (cMessage[2] == (unsigned char)(STATUS_CMD | TO_COMPUTER))
    {
        nShutterStatus = (enum SH_Status)cMessage[3];
        nAzimuthStatus = (enum AZ_Status)cMessage[4];

        nAzimuthPosition = (unsigned)(((unsigned)cMessage[5]) * 256 + ((unsigned)cMessage[6]));
        mCurrentAzPositionInTicks = nAzimuthPosition;
        TicksToAz(mCurrentAzPositionInTicks, mCurrentAzPosition);
        nHomePosition = ((unsigned)cMessage[7]) * 256 + ((unsigned)cMessage[8]);
        return MD2_OK;
    }
    
    return ERR_CMDFAILED;
}

/*
    this switch the park command to a sync command
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::SyncMode_MaxDomeII(void)
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    int nErr = MD2_OK;
    unsigned long  nBytesWrite;;
    int nReturn;

    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = SYMC_CMD;
    cMessage[3] = checksum_MaxDomeII(cMessage, 3);

#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [SyncMode_MaxDomeII] sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    nErr = pSerx->writeFile(cMessage, cMessage[1]+2, nBytesWrite);
    pSerx->flushTx();

    if (nErr != MD2_OK)
        return MD2_CANT_CONNECT;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != MD2_OK)
        return nReturn;

    if (cMessage[2] == (unsigned char)(SYMC_CMD | TO_COMPUTER)) {
        return MD2_OK;
    }

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
    int nErr = MD2_OK;
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
    
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [SetPark_MaxDomeII_Ticks] sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif
    nErr = pSerx->writeFile(cMessage, cMessage[1]+2, nBytesWrite);
    pSerx->flushTx();

    if (nErr != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != MD2_OK)
        return nReturn;

    if (cMessage[2] == (unsigned char)(SETPARK_CMD | TO_COMPUTER))
    {
		mParkBeforeCloseShutter = nParkOnShutter;
		return MD2_OK;
	}

    return BAD_CMD_RESPONSE;	// Response don't match command
}

int CMaxDome::Sync_Dome(double dAz)
{
    int nErr = 0;
    int nTicks;
    double dTmpAz;

#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [Sync_Dome] dAz = %3.2f\n", timestamp, dAz);
    fflush(Logfile);
#endif

    // this switch the park command to a sync command
    nErr = SyncMode_MaxDomeII();
    if (nErr)
        return nErr;
    dTmpAz = dAz;
    // remove home offset
    dAz = dAz - mHomeAz;
    while (dAz < 0) dAz += 360;
    while (dAz >= 360) dAz -= 360;

    nTicks = int(round((360-dAz)/(360.0f/mNbTicksPerRev)));

#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [Sync_Dome] nTicks = %d ( %02X )\n", timestamp, nTicks, nTicks);
    fflush(Logfile);
#endif

    nErr = SetPark_MaxDomeII_Ticks(mParkBeforeCloseShutter, nTicks);
    if (nErr)
        return nErr;

    // sync reset the tick counter to 0.
    // so we need to compute the sync offset.
    m_dSyncOffset = dTmpAz - mHomeAz;
    while (m_dSyncOffset < 0) m_dSyncOffset += 360;
    while (m_dSyncOffset >= 360) m_dSyncOffset -= 360;

#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [Sync_Dome] m_dSyncOffset = %3.2f\n", timestamp, m_dSyncOffset);
    fflush(Logfile);
#endif

    return nErr;
}

//
// send the dome to its park position
//
int CMaxDome::Park_MaxDomeII(void)
{
    int nErr = MD2_OK;
    
    nErr = Goto_Azimuth_MaxDomeII(mParkAz);
    return nErr;
}

int CMaxDome::Unpark(void)
{
    mParked = false;
    return MD2_OK;
}

/*
 Set ticks per turn of the dome
 
 @param nTicks Ticks from home position in E to W direction.
 @return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::SetTicksPerCount_MaxDomeII(int nTicks)
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    int nErr = MD2_OK;
    unsigned long  nBytesWrite;;
    int nReturn;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x04;		// Will follow 4 bytes more
    cMessage[2] = TICKS_CMD;
    // Note: we do not use nTicks >> 8 in order to remain compatible with both little-endian and big-endian procesors
    cMessage[3] = (char)(nTicks / 256);
    cMessage[4] = (char)(nTicks % 256);
    cMessage[5] = checksum_MaxDomeII(cMessage, 5);
    
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [SetTicksPerCount_MaxDomeII] sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    nErr = pSerx->writeFile(cMessage, cMessage[1]+2, nBytesWrite);
    pSerx->flushTx();

    if (nErr != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != MD2_OK)
        return nReturn;
    
    if (cMessage[2] == (unsigned char)(TICKS_CMD | TO_COMPUTER))
    {
        mNbTicksPerRev = nTicks;
        return MD2_OK;
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
    int nErr = MD2_OK;
    unsigned long  nBytesWrite;;
    int nReturn;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x03;		// Will follow 3 bytes more
    cMessage[2] = SHUTTER_CMD;
    cMessage[3] = OPEN_SHUTTER;
    cMessage[4] = checksum_MaxDomeII(cMessage, 4);
    
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [Open_Shutter_MaxDomeII] sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif
    nErr = pSerx->writeFile(cMessage, cMessage[1]+2, nBytesWrite);
    pSerx->flushTx();

    if (nErr != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != MD2_OK)
        return nReturn;
    
    if (cMessage[2] == (unsigned char)(SHUTTER_CMD | TO_COMPUTER)) {
        return MD2_OK;
    }
    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Opens the upper shutter only
 
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Open_Upper_Shutter_Only_MaxDomeII()
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    int nErr = MD2_OK;
    unsigned long  nBytesWrite;;
    int nReturn;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x03;		// Will follow 3 bytes more
    cMessage[2] = SHUTTER_CMD;
    cMessage[3] = OPEN_UPPER_ONLY_SHUTTER;
    cMessage[4] = checksum_MaxDomeII(cMessage, 4);
    
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [Open_Upper_Shutter_Only_MaxDomeII] sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    nErr = pSerx->writeFile(cMessage, cMessage[1]+2, nBytesWrite);
    pSerx->flushTx();

    if (nErr != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != MD2_OK)
        return nReturn;
    
    if (cMessage[2] == (unsigned char)(SHUTTER_CMD | TO_COMPUTER)) {
        return MD2_OK;
    }

    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Close the shutter
 
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Close_Shutter_MaxDomeII()
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    int nErr = MD2_OK;
    unsigned long  nBytesWrite;;
    int nReturn;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x03;		// Will follow 3 bytes more
    cMessage[2] = SHUTTER_CMD;
    cMessage[3] = CLOSE_SHUTTER;
    cMessage[4] = checksum_MaxDomeII(cMessage, 4);
    
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [Close_Shutter_MaxDomeII] sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    nErr = pSerx->writeFile(cMessage, cMessage[1]+2, nBytesWrite);
    pSerx->flushTx();

    if (nErr != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != MD2_OK)
        return nReturn;
    
    if (cMessage[2] == (unsigned char)(SHUTTER_CMD | TO_COMPUTER)) {
        return MD2_OK;
    }

    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Abort shutter movement
 
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Abort_Shutter_MaxDomeII()
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    int nErr = MD2_OK;
    unsigned long  nBytesWrite;;
    int nReturn;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x03;		// Will follow 3 bytes more
    cMessage[2] = SHUTTER_CMD;
    cMessage[3] = ABORT_SHUTTER;
    cMessage[4] = checksum_MaxDomeII(cMessage, 4);
    
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [Abort_Shutter_MaxDomeII] sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif
    nErr = pSerx->writeFile(cMessage, cMessage[1]+2, nBytesWrite);
    pSerx->flushTx();

    if (nErr != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != MD2_OK)
        return nReturn;
    
    if (cMessage[2] == (unsigned char)(SHUTTER_CMD | TO_COMPUTER)) {
        return MD2_OK;
    }

    return BAD_CMD_RESPONSE;	// Response don't match command
}

/*
	Exit shutter (?) Normally send at program exit
	
	@return 0 command received by MAX DOME. MD2_CANT_CONNECT Couldn't send command. BAD_CMD_RESPONSE Response don't match command. See ReadResponse() return
 */
int CMaxDome::Exit_Shutter_MaxDomeII()
{
    unsigned char cMessage[MD_BUFFER_SIZE];
    int nErr = MD2_OK;
    unsigned long  nBytesWrite;;
    int nReturn;
    
    cMessage[0] = 0x01;
    cMessage[1] = 0x03;		// Will follow 3 bytes more
    cMessage[2] = SHUTTER_CMD;
    cMessage[3] = EXIT_SHUTTER;
    cMessage[4] = checksum_MaxDomeII(cMessage, 4);
    
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [Exit_Shutter_MaxDomeII] sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif

    nErr = pSerx->writeFile(cMessage, cMessage[1]+2, nBytesWrite);
    pSerx->flushTx();
    
    if (nErr != MD2_OK)
        return MD2_CANT_CONNECT;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != MD2_OK)
        return nReturn;
    
    if (cMessage[2] == (unsigned char)(SHUTTER_CMD | TO_COMPUTER)) {
        return MD2_OK;
    }

    return BAD_CMD_RESPONSE;	// Response don't match command
}

#pragma mark - convertions
//Convert dAz to number of ticks from home (home = 0 for ticks) and direction.
void CMaxDome::AzToTicks(double dAz, unsigned &dir, int &ticks)
{
    dir = MAXDOMEII_EW_DIR;
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [AzToTicks] pdAz : %3.2f\n", timestamp, dAz);
    fprintf(Logfile, "[%s] [AzToTicks] mCurrentAzPosition : %3.2f\n", timestamp, mCurrentAzPosition);
#endif

    ticks = (int) floor(0.5 + (dAz - mHomeAz - m_dSyncOffset) * mNbTicksPerRev / 360.0);
#if defined MAXDOME_DEBUG  && MAXDOME_DEBUG >= 2
    fprintf(Logfile, "[%s] [AzToTicks] step 1 ticks : %d\n", timestamp, ticks);
#endif
    while (ticks > mNbTicksPerRev) ticks -= mNbTicksPerRev;
#if defined MAXDOME_DEBUG  && MAXDOME_DEBUG >= 2
    fprintf(Logfile, "[%s] [AzToTicks] step 2 ticks : %d\n", timestamp, ticks);
#endif
    while (ticks < 0) ticks += mNbTicksPerRev;
#if defined MAXDOME_DEBUG  && MAXDOME_DEBUG >= 2
    fprintf(Logfile, "[%s] [AzToTicks] step 3 ticks : %d\n", timestamp, ticks);
#endif
    // find the direction with the shortest path
    if (dAz > mCurrentAzPosition) {
        if (dAz - mCurrentAzPosition > 180.0)
            dir = MAXDOMEII_WE_DIR;
        else
            dir = MAXDOMEII_EW_DIR;
    }
    else {
        if (mCurrentAzPosition - dAz > 180.0)
            dir = MAXDOMEII_EW_DIR;
        else
            dir = MAXDOMEII_WE_DIR;
    }
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
    fprintf(Logfile, "[%s] [AzToTicks] dir : %s\n", timestamp, dir == MAXDOMEII_WE_DIR?"MAXDOMEII_WE_DIR":"MAXDOMEII_EW_DIR");
    fflush(Logfile);
#endif

}

// Convert ticks from home to Az
void CMaxDome::TicksToAz(int ticks, double &pdAz)
{
    
    pdAz = mHomeAz + (ticks * 360.0 / mNbTicksPerRev) + m_dSyncOffset;
    while (pdAz < 0) pdAz += 360;
    while (pdAz >= 360) pdAz -= 360;
}


#pragma mark - Operation Completion checks

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

    if(tmpAzimuthStatus == As_IDLE || tmpAzimuthStatus == As_IDLE2) {
        complete = true;
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [IsGoToComplete] mCurrentAzPosition : %3.2f\n", timestamp, mCurrentAzPosition);
        fflush(Logfile);
#endif
    }
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

    if(!mParked)
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


#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [IsFindHomeComplete] tmpAzimuthStatus = %d\ntmpAz = %d\ntmpHomePosition = %d\n", timestamp, tmpAzimuthStatus, tmpAz, tmpHomePosition);
    fflush(Logfile);
#endif

    if(tmpAzimuthStatus == As_IDLE || tmpAzimuthStatus == As_IDLE2) {
        if (mCalibrating) {
            setNbTicksPerRev(tmpHomePosition);
            SyncMode_MaxDomeII();
            SetPark_MaxDomeII_Ticks(mParkBeforeCloseShutter, 32767);
            Goto_Azimuth_MaxDomeII(MAXDOMEII_WE_DIR, 1);
            TicksToAz(1, mCurrentAzPosition);
            mCalibrating = false;
        }
        complete = true;
        mHomed = true;
    }
	else if (tmpAzimuthStatus == As_ERROR) {
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [IsFindHomeComplete] tmpAzimuthStatus reported As_ERROR\n", timestamp);
        fflush(Logfile);
#endif
		complete = false;
		mHomed = false;
		return ERR_CMDFAILED;
	}
    else {
        complete = false;
        mHomed = false;
    }
    
    return err;
    
}

#pragma mark - Getter / Setter

int CMaxDome::getFirmwareIntValue()
{
    return m_nFirmwareVersion;
}

int CMaxDome::getNbTicksPerRev()
{
    return mNbTicksPerRev;
}

void CMaxDome::setNbTicksPerRev(unsigned nbTicksPerRev)
{
    int nErr = 0;

    if(nbTicksPerRev == 0)
        nbTicksPerRev = 360;
    
    mNbTicksPerRev = nbTicksPerRev;
    if(bIsConnected) {
        nErr = SetTicksPerCount_MaxDomeII(mNbTicksPerRev);
        if(nErr) {
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [setNbTicksPerRev] SetTicksPerCount_MaxDomeII] err = %d\n", timestamp, nErr);
            fflush(Logfile);
#endif
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
    int nErr = 0;

    mParkAz = dAz;

    if(bIsConnected) {
        mParkBeforeCloseShutter = nParkOnShutter;
        AzToTicks(dAz, dir, mParkAzInTicks);
        nErr = SetPark_MaxDomeII_Ticks(mParkBeforeCloseShutter, mParkAzInTicks);
        if(nErr) {
#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [setParkAz] SetPark_MaxDomeII] err = %d\n", timestamp, nErr);
            fflush(Logfile);
#endif
            return nErr;
        }
    }
    return nErr;
}

bool CMaxDome::getCloseShutterBeforePark()
{
    return mParkBeforeCloseShutter;
}

void CMaxDome::setParkBeforeCloseShutter(bool close)
{
    mParkBeforeCloseShutter = close;
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

void CMaxDome::hexdump(unsigned char* pszInputBuffer, unsigned char *pszOutputBuffer, int nInputBufferSize, int nOutpuBufferSize)
{
    unsigned char *pszBuf = pszOutputBuffer;
    int nIdx=0;
    
    memset(pszOutputBuffer, 0, nOutpuBufferSize);
    for(nIdx=0; nIdx < nInputBufferSize && pszBuf < (pszOutputBuffer + nOutpuBufferSize -3); nIdx++){
        snprintf((char *)pszBuf,4,"%02X ", pszInputBuffer[nIdx]);
        pszBuf+=3;
    }
}


int CMaxDome::setDebounceTime(int nDebounceTime)
{
    int nErr = MD2_OK;
    unsigned char cMessage[MD_BUFFER_SIZE];
    unsigned long  nBytesWrite;;

    if(!bIsConnected) {
        m_nDebounceTime = nDebounceTime;
        return nErr;
    }

    if(m_nFirmwareVersion<4) {
        return FIRMWARE_NOT_SUPPORTED;
    }


    cMessage[0] = 0x01;
    cMessage[1] = 0x03;		// Will follow 4 bytes more
    cMessage[2] = SETDEBOUNCE_CMD;
    // Note: we do not use nTicks >> 8 in order to remain compatible with both little-endian and big-endian procesors
    cMessage[3] = (char)(nDebounceTime / 10);
    cMessage[4] = checksum_MaxDomeII(cMessage, 4);

#if defined MAXDOME_DEBUG && MAXDOME_DEBUG >= 3
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump(cMessage, cHexMessage, cMessage[1]+2, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [setDebounceTime] sending :\n%s\n", timestamp, cHexMessage);
    fflush(Logfile);
#endif
    nErr = pSerx->writeFile(cMessage, cMessage[1]+2, nBytesWrite);
    pSerx->flushTx();

    if (nErr != MD2_OK)
        return ERR_CMDFAILED;

    nErr = ReadResponse_MaxDomeII(cMessage);
    if (nErr != MD2_OK)
        return nErr;

    if (cMessage[2] == (unsigned char)(SETDEBOUNCE_CMD | TO_COMPUTER))
    {
        m_nDebounceTime = nDebounceTime;
        return MD2_OK;
    }

    return BAD_CMD_RESPONSE;	// Response don't match command
}

int CMaxDome::getDebounceTime()
{
    return m_nDebounceTime;
}


