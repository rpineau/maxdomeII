//
//  maxdomeII.cpp
//  MaxDome II X2 plugin
//
//  Created by Rodolphe Pineau on 4/9/2016.
//
// most of the code comes from the INDI driver.
// The following is the original header.
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
    pSerx = NULL;
    bIsConnected = false;
    
}

CMaxDome::~CMaxDome()
{

}


bool CMaxDome::Connect(const char *szPort)
{
    int err;
    unsigned tmpAz;
    unsigned tmpHomePosition;
    enum SH_Status tmpShutterStatus;
    enum AZ_Status tmpAzimuthStatus;

    // 19200 8N1
    if(pSerx->open(szPort,19200) == 0)
        bIsConnected = true;
    else
        bIsConnected = false;

    // Check to see if we can't even connect to the device
    if(!bIsConnected)
        return false;

    // bIsConnected = GetFirmware(szFirmware);
    pSerx->purgeTxRx();

    
    // get the device status to make sure we're properly connected.
    err = Status_MaxDomeII(tmpShutterStatus, tmpAzimuthStatus, tmpAz, tmpHomePosition);

    if(err)
    {
        pSerx->close();
        bIsConnected = false;
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
    // int nErrorType = SB_OK;
    char nChecksum;
    int nErrorType = SB_OK;
    *cMessage = 0x00;
    cMessage[13] = 0x00;

    // Look for a 0x01 starting character, until time out occurs or MAX_BUFFER characters was read
    while (*cMessage != 0x01 && nErrorType == SB_OK)
    {
        // nErrorType = tty_read(fd, cMessage, 1, MAXDOME_TIMEOUT, &nBytesRead);
        nErrorType = pSerx->readFile(cMessage, 1, nBytesRead, MAXDOME_TIMEOUT);
        //fprintf(stderr,"\nIn 1: %ld %02x\n", nBytesRead, (int)cMessage[0]);
    }

    if (nErrorType != SB_OK || *cMessage != 0x01)
        return -1;

    // Read message length
    // nErrorType = tty_read(fd, cMessage + 1, 1, MAXDOME_TIMEOUT, &nBytesRead);
    nErrorType = pSerx->readFile(cMessage + 1, 1, nBytesRead, MAXDOME_TIMEOUT);

    //fprintf(stderr,"\nInLen: %d\n",(int) cMessage[1]);
    if (nErrorType != SB_OK || cMessage[1] < 0x02 || cMessage[1] > 0x0E)
        return -2;

    nLen = cMessage[1];
    // Read the rest of the message
    //nErrorType = tty_read(fd, cMessage + 2, nLen, MAXDOME_TIMEOUT, &nBytesRead);
    nErrorType = pSerx->readFile(cMessage + 2, nLen, nBytesRead, MAXDOME_TIMEOUT);

    //fprintf(stderr,"\nIn: %s\n", cMessage);
    if (nErrorType != SB_OK || nBytesRead != nLen)
        return -3;

    nChecksum = checksum_MaxDomeII(cMessage, nLen + 2);
    if (nChecksum != 0x00)
        return -4;
    return 0;
}

/*
	Abort azimuth movement

	@return 0 command received by MAX DOME. -5 Couldn't send command. -6 Response don't match command. See ReadResponse() return
 */
int CMaxDome::Abort_Azimuth_MaxDomeII()
{
    char cMessage[MAX_BUFFER];
    // int nErrorType;
    unsigned long  nBytesWrite;
    int nReturn;
    int nErrorType = SB_OK;

    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = ABORT_CMD;
    cMessage[3] = checksum_MaxDomeII(cMessage, 3);
    // nErrorType = tty_write(fd, cMessage, 4, &nBytesWrite);

    nErrorType = pSerx->writeFile(cMessage, 4, nBytesWrite);

    if (nErrorType != SB_OK)
        return -5;
    

    if (nBytesWrite != 4)
        return -5;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (char)(ABORT_CMD | TO_COMPUTER))
        return 0;

    return -6;	// Response don't match command
}

/*
	Move until 'home' position is detected

	@return 0 command received by MAX DOME. -5 Couldn't send command. -6 Response don't match command. See ReadResponse() return
 */
int CMaxDome::Home_Azimuth_MaxDomeII()
{
    char cMessage[MAX_BUFFER];
    int nErrorType;
    unsigned long  nBytesWrite;;
    int nReturn;

    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = HOME_CMD;
    cMessage[3] = checksum_MaxDomeII(cMessage, 3);
    //nErrorType = tty_write(fd, cMessage, 4, &nBytesWrite);
    nErrorType = pSerx->writeFile(cMessage, 4, nBytesWrite);

    if (nErrorType != SB_OK)
        return -5;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (char)(HOME_CMD | TO_COMPUTER))
        return 0;

    return -6;	// Response don't match command
}

/*
	Go to a new azimuth position

	@param nDir Direcction of the movement. 0 E to W. 1 W to E
	@param nTicks Ticks from home position in E to W direction.
	@return 0 command received by MAX DOME. -5 Couldn't send command. -6 Response don't match command. See ReadResponse() return
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
    // nErrorType = tty_write(fd, cMessage, 7, &nBytesWrite);
    nErrorType = pSerx->writeFile(cMessage, 7, nBytesWrite);

    //fprintf(stderr,"\nOut: %ld %02x %02x %02x %02x %02x %02x %02x\n", nBytesWrite, (int)cMessage[0], (int)cMessage[1], (int)cMessage[2], (int)cMessage[3], (int)cMessage[4], (int)cMessage[5], (int)cMessage[6]);
    //if (nErrorType != SB_OK)
    if (nBytesWrite != 7)
        return -5;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (char)(GOTO_CMD | TO_COMPUTER))
    {
        mGotoTicks = nTicks;
        return 0;
    }

    return -6;	// Response don't match command
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
	@return 0 command received by MAX DOME. -5 Couldn't send command. -6 Response don't match command. See ReadResponse() return
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
    // nErrorType = tty_write(fd, cMessage, 4, &nBytesWrite);
    nErrorType = pSerx->writeFile(cMessage, 4, nBytesWrite);

    if (nErrorType != SB_OK)
        return -5;

    nReturn = ReadResponse_MaxDomeII((char*)cMessage);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (unsigned char)(STATUS_CMD | TO_COMPUTER))
    {
        nShutterStatus = (enum SH_Status)cMessage[3];
        nAzimuthStatus = (enum AZ_Status)cMessage[4];
        nAzimuthPosition = (unsigned)(((unsigned)cMessage[5]) * 256 + ((unsigned)cMessage[6]));
        nHomePosition = ((unsigned)cMessage[7]) * 256 + ((unsigned)cMessage[8]);
        mAzimuthPositionInTicks = nAzimuthPosition;
        TicksToAz(mAzimuthPositionInTicks, mAzimuthPosition);
        mHomePositionInTicks = nHomePosition;
        TicksToAz(mHomePositionInTicks, mHomePosition);
        return 0;
    }

    return -6;	// Response don't match command
}

/*
	Ack comunication

	@return 0 command received by MAX DOME. -5 Couldn't send command. -6 Response don't match command. See ReadResponse() return
 */
int CMaxDome::Ack_MaxDomeII()
{
    char cMessage[MAX_BUFFER];
    int nErrorType = SB_OK;
    unsigned long  nBytesWrite;;
    int nReturn;

    cMessage[0] = 0x01;
    cMessage[1] = 0x02;		// Will follow 2 bytes more
    cMessage[2] = ACK_CMD;
    cMessage[3] = checksum_MaxDomeII(cMessage, 3);
    // nErrorType = tty_write(fd, cMessage, 4, &nBytesWrite);
    nErrorType = pSerx->writeFile(cMessage, 4, nBytesWrite);

    //nBytesWrite = write(fd, cMessage, 4);
    //if (nBytesWrite != 4)
    //	nErrorType = TTY_WRITE_ERROR;
    //fprintf(stderr,"\nOut: %ld %02x %02x %02x %02x\n", nBytesWrite, (int)cMessage[0], (int)cMessage[1], (int)cMessage[2], (int)cMessage[3]);
    if (nErrorType != SB_OK)
        return -5;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    //fprintf(stderr,"\nIn: %02x %02x %02x %02x %02x\n", cMessage[0], cMessage[1], cMessage[2], cMessage[3], cMessage[4]);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (char)(ACK_CMD | TO_COMPUTER))
        return 0;

    //fprintf(stderr,"\nIn: %02x %02x", (unsigned char)(ACK_CMD | TO_COMPUTER), cMessage[2]);
    return -6;	// Response don't match command
}

/*
	Set park coordinates and if need to park before to operating shutter

	@param nParkOnShutter 0 operate shutter at any azimuth. 1 go to park position before operating shutter
	@param nTicks Ticks from home position in E to W direction.
	@return 0 command received by MAX DOME. -5 Couldn't send command. -6 Response don't match command. See ReadResponse() return
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
    //nErrorType = tty_write(fd, cMessage, 7, &nBytesWrite);
    nErrorType = pSerx->writeFile(cMessage, 7, nBytesWrite);

    if (nErrorType != SB_OK)
        return -5;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (char)(SETPARK_CMD | TO_COMPUTER))
    {
        mParkPositionInTicks = mHomePositionInTicks + nTicks;

        return 0;
    }
    return -6;	// Response don't match command
}

//
// send the dome to its park position
//
int CMaxDome::Park_MaxDomeII()
{
    int nErrorType;
    int dir = 0;
    int ticks;
    unsigned tmpAz;
    unsigned tmpHomePosition;
    enum SH_Status tmpShutterStatus;
    enum AZ_Status tmpAzimuthStatus;

    // where are we ?
    nErrorType = Status_MaxDomeII(tmpShutterStatus, tmpAzimuthStatus, tmpAz, tmpHomePosition);
    if(nErrorType)
        return nErrorType;
    // how far do we need to move and in which direction.
    // nbticks to move = current position - (park - home)

    // this doesn't look right,tmpAzimuthStatus is an enum, tmpAzimuthStatus should probably be tmpAz
    // ticks = tmpAzimuthStatus - (mParkPositionInTicks - tmpHomePosition);
    ticks = tmpAz - (mParkPositionInTicks - tmpHomePosition);
    if (ticks <0)
    {
        dir = 1;
        ticks = -ticks;
    }

    // this doesn't look right,tmpAzimuthStatus is an enum, tmpAzimuthStatus should probably be tmpAz
    // if (ticks == tmpAzimuthStatus)
    if (ticks == tmpAz)
    {
        // park = home
        nErrorType = Home_Azimuth_MaxDomeII();
    }
    else
        nErrorType = Goto_Azimuth_MaxDomeII(dir, ticks);
    return nErrorType;
}


/*
 Set ticks per turn of the dome

 @param nTicks Ticks from home position in E to W direction.
 @return 0 command received by MAX DOME. -5 Couldn't send command. -6 Response don't match command. See ReadResponse() return
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
    // nErrorType = tty_write(fd, cMessage, 6, &nBytesWrite);
    nErrorType = pSerx->writeFile(cMessage, 6, nBytesWrite);
    if (nErrorType != SB_OK)
        return -5;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (char)(TICKS_CMD | TO_COMPUTER))
    {
        mNbTicksPerRev = nTicks;
        return 0;
    }
    return -6;	// Response don't match command
}

///////////////////////////////////////////////////////////////////////////
//
//  Shutter commands
//
///////////////////////////////////////////////////////////////////////////

/*
	Opens the shutter fully

	@return 0 command received by MAX DOME. -5 Couldn't send command. -6 Response don't match command. See ReadResponse() return
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
    // nErrorType = tty_write(fd, cMessage, 5, &nBytesWrite);
    nErrorType = pSerx->writeFile(cMessage, 5, nBytesWrite);

    if (nErrorType != SB_OK)
        return -5;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (char)(SHUTTER_CMD | TO_COMPUTER))
        return 0;

    return -6;	// Response don't match command
}

/*
	Opens the upper shutter only

	@return 0 command received by MAX DOME. -5 Couldn't send command. -6 Response don't match command. See ReadResponse() return
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
    // nErrorType = tty_write(fd, cMessage, 5, &nBytesWrite);
    nErrorType = pSerx->writeFile(cMessage, 5, nBytesWrite);

    if (nErrorType != SB_OK)
        return -5;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (char)(SHUTTER_CMD | TO_COMPUTER))
        return 0;

    return -6;	// Response don't match command
}

/*
	Close the shutter

	@return 0 command received by MAX DOME. -5 Couldn't send command. -6 Response don't match command. See ReadResponse() return
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
    // nErrorType = tty_write(fd, cMessage, 5, &nBytesWrite);
    nErrorType = pSerx->writeFile(cMessage, 5, nBytesWrite);

    if (nErrorType != SB_OK)
        return -5;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;

    if (cMessage[2] == (char)(SHUTTER_CMD | TO_COMPUTER))
        return 0;

    return -6;	// Response don't match command
}

/*
	Abort shutter movement

	@return 0 command received by MAX DOME. -5 Couldn't send command. -6 Response don't match command. See ReadResponse() return
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
    // nErrorType = tty_write(fd, cMessage, 5, &nBytesWrite);
    nErrorType = pSerx->writeFile(cMessage, 5, nBytesWrite);

    if (nErrorType != SB_OK)
        return -5;

    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;
    
    if (cMessage[2] == (char)(SHUTTER_CMD | TO_COMPUTER))
        return 0;

    return -6;	// Response don't match command
}

/*
	Exit shutter (?) Normally send at program exit
	
	@return 0 command received by MAX DOME. -5 Couldn't send command. -6 Response don't match command. See ReadResponse() return
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
    // nErrorType = tty_write(fd, cMessage, 5, &nBytesWrite);
    nErrorType = pSerx->writeFile(cMessage, 5, nBytesWrite);

    if (nErrorType != SB_OK)
        return -5;
    
    nReturn = ReadResponse_MaxDomeII(cMessage);
    if (nReturn != 0)
        return nReturn;
    
    if (cMessage[2] == (char)(SHUTTER_CMD | TO_COMPUTER))
        return 0;

    return -6;	// Response don't match command
}

/*
	Convert pdAz to number of ticks from home and direction.

 */

void CMaxDome::AzToTicks(double pdAz, int &dir, int &ticks)
{
    double nbDeg;
    
    // delta between home position and pdAz
    nbDeg = pdAz - mHomePosition;
    
    // 0 E to W. 1 W to E
    if (nbDeg<0)
    {
        nbDeg = -nbDeg;
        dir = 1;
    }
    else{
        dir =0;
    }
    ticks = mHomePositionInTicks + (int) (((double)mNbTicksPerRev/360.0f) * nbDeg);
}

/*
	Convert ticks from home to Az
 
*/

void CMaxDome::TicksToAz(int ticks, double &pdAz)
{
    pdAz = ((mHomePositionInTicks + ticks)*360.0f) / (double)mNbTicksPerRev;
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
    {
        complete = true;
    }
    else
    {
        complete = false;
    }

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
    }
    else
    {
        complete = false;
    }

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
    }
    else
    {
        complete = false;
    }

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

    if((mParkPositionInTicks == tmpAz) && (tmpAzimuthStatus == As_IDLE || tmpAzimuthStatus == As_IDLE2))

    {
        complete = true;
    }
    else
    {
        complete = false;
    }

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
    }
    else
    {
        complete = false;
    }

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

     if((mHomePositionInTicks == tmpAz) && (tmpAzimuthStatus == As_IDLE || tmpAzimuthStatus == As_IDLE2))
    {
        complete = true;
    }
    else
    {
        complete = false;
    }

    return err;

}

