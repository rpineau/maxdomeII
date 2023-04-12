//
//  maxdomeII.h
//  MaxDome II
//
//  Created by Rodolphe Pineau on 4/9/2016.
//  MaxDome II X2 plugin
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

#ifndef __MAXDOMEII__
#define __MAXDOMEII__
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif

// C++ includes
#include <string>

#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"

#define PLUGIN_VERSION      1.35

#define MAXDOME_DEBUG 3

#define LOG_BUFFER_SIZE 256

#define MD_BUFFER_SIZE 15			// Message length can be up to 12 bytes.
#define MAX_TIMEOUT 5000        // timeout after 5 seonds. (value is in ms).
// Start byte
#define START 0x01

// Message Destination
#define TO_MAXDOME  0x00
#define TO_COMPUTER 0x80

// Commands available
#define ABORT_CMD   0x03		// Abort azimuth movement
#define HOME_CMD    0x04		// Move until 'home' position is detected
#define GOTO_CMD    0x05		// Go to azimuth position
#define SHUTTER_CMD 0x06		// Send a command to Shutter
#define STATUS_CMD  0x07		// Retrieve status
#define SYMC_CMD    0x08        // swicth patk to sync mode
#define TICKS_CMD   0x09		// Set the number of tick per revolution of the dome
#define ACK_CMD     0x0A		// ACK (?)
#define SETPARK_CMD 0x0B		// Set park coordinates and if need to park before to operating shutter

#define SETDEBOUNCE_CMD 0x0C		// Set park coordinates and if need to park before to operating shutter

// Shutter commands
#define OPEN_SHUTTER            0x01
#define OPEN_UPPER_ONLY_SHUTTER 0x02
#define CLOSE_SHUTTER           0x03
#define EXIT_SHUTTER            0x04  // Command send to shutter on program exit
#define ABORT_SHUTTER           0x07

// Direction fo azimuth movement
#define MAXDOMEII_EW_DIR 0x01
#define MAXDOMEII_WE_DIR 0x02

// Azimuth motor status. When motor is idle, sometimes returns 0, sometimes 4. After connect, it returns 5
// 5 might not be an error at all !!!
enum AZ_Status {As_IDLE = 1, As_MOVING_WE, As_MOVING_EW, As_IDLE2, As_ERROR};

// Shutter status
enum SH_Status {Ss_CLOSED = 0, Ss_OPENING, Ss_OPEN, Ss_CLOSING, Ss_ABORTED, Ss_ERROR};

// Error code
enum MD2_Errors {MD2_OK=0, MD2_CANT_CONNECT, BAD_CMD_RESPONSE, MD2_NOT_CONNECTED};

class CMaxDome
{
public:
    CMaxDome();
    ~CMaxDome();
    
    int         Connect(const char *szPort);
    void        Disconnect(void);
    bool        IsConnected(void) { return bIsConnected; }
    
    void        SetSerxPointer(SerXInterface *p) { pSerx = p; }

    // controller info
    void getFirmwareVersion(char *version, int strMaxLen);

    // Dome commands
    int Init_Communication(void);
    int Abort_Azimuth_MaxDomeII(void);
    int Home_Azimuth_MaxDomeII(void);
    int Goto_Azimuth_MaxDomeII(int nDir, int nTicks);
    int Status_MaxDomeII(enum SH_Status &nShutterStatus, enum AZ_Status &nAzimuthStatus, int &nAzimuthPosition, int &nHomePosition);
    int Goto_Azimuth_MaxDomeII(double newAz);
    int SyncMode_MaxDomeII(void);
    int SetPark_MaxDomeII_Ticks(unsigned nParkOnShutter, int nTicks);
    int SetTicksPerCount_MaxDomeII(int nTicks);
    int Park_MaxDomeII(void);
    int Unpark(void);
    int Sync_Dome(double dAz);
    
    //  Shutter commands
    int Open_Shutter_MaxDomeII(void);
    int Open_Upper_Shutter_Only_MaxDomeII(void);
    int Close_Shutter_MaxDomeII(void);
    int Abort_Shutter_MaxDomeII(void);
    int Exit_Shutter_MaxDomeII(void);
    
    // convertion functions
    void AzToTicks(double dAz, unsigned &dir, int &ticks);
    void TicksToAz(int ticks, double &pdAz);
    
    // command complete functions
    int IsGoToComplete(bool &complete);
    int IsOpenComplete(bool &complete);
    int IsCloseComplete(bool &complete);
    int IsParkComplete(bool &complete);
    int IsUnparkComplete(bool &complete);
    int IsFindHomeComplete(bool &complete);
    
    // getter/setter
    int getFirmwareIntValue();

    int getNbTicksPerRev();
    void setNbTicksPerRev(unsigned nbTicksPerRev);
    
    double getHomeAz();
    void setHomeAz(double dAz);
    
    double getParkAz();
    int setParkAz(unsigned nParkOnShutter, double dAz);
    
    bool getCloseShutterBeforePark();
    void setParkBeforeCloseShutter(bool close);
    
    double getCurrentAz();
    void setCurrentAz(double dAz);
    void setCalibrating(bool bCal);

    int setDebounceTime(int nDebounceTime);
    int getDebounceTime();

protected:
    
    signed char     checksum_MaxDomeII(unsigned char *cMessage, int nLen);
    int             ReadResponse_MaxDomeII(unsigned char *cMessage);
    bool            bIsConnected;

    char            m_szFirmwareVersion[LOG_BUFFER_SIZE];
    int             m_nFirmwareVersion;

    bool            mHomed;
    bool            mParked;
    bool            mParkBeforeCloseShutter;
    bool            mShutterOpened;
    bool            mCalibrating;
	
    int             mNbTicksPerRev;
    int             m_nDebounceTime;

    double          mHomeAz;

    double          m_dSyncOffset;

    int             mParkAzInTicks;
    double          mParkAz;
    
    int             mCurrentAzPositionInTicks;
    double          mCurrentAzPosition;
    
    int             mGotoTicks;
    SerXInterface   *pSerx;

    void            hexdump(unsigned char* pszInputBuffer, unsigned char *pszOutputBuffer, int nInputBufferSize, int nOutpuBufferSize);
    
#ifdef MAXDOME_DEBUG
    std::string m_sLogfilePath;
    // timestamp for logs
    char *timestamp;
    time_t ltime;
    FILE *Logfile;	  // LogFile
#endif

};

#endif
