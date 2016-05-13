//
//  maxdomeII.h
//  MaxDome II
//
//  Created by Rodolphe Pineau on 4/9/2016.
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

#ifndef __MAXDOMEII__
#define __MAXDOMEII__

#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"

#define MAXDOME_TIMEOUT	5		/* FD timeout in seconds */
#define MAX_BUFFER 15			// Message length can be up to 12 bytes.

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
#define TICKS_CMD   0x09		// Set the number of tick per revolution of the dome
#define ACK_CMD     0x0A		// ACK (?)
#define SETPARK_CMD 0x0B		// Set park coordinates and if need to park before to operating shutter

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
enum AZ_Status {As_IDLE = 1, As_MOVING_WE, As_MOVING_EW, As_IDLE2, As_ERROR};

// Shutter status
enum SH_Status {Ss_CLOSED = 0, Ss_OPENING, Ss_OPEN, Ss_CLOSING, Ss_ABORTED, Ss_ERROR};

class CMaxDome
{
public:
    CMaxDome();
    ~CMaxDome();

    bool        Connect(const char *szPort);
    void        Disconnect(void);
    bool        IsConnected(void) { return bIsConnected; }

    void        SetSerxPointer(SerXInterface *p) { pSerx = p; }

    // Dome commands
    int Init_Communication();
    int Abort_Azimuth_MaxDomeII();
    int Home_Azimuth_MaxDomeII();
    int Goto_Azimuth_MaxDomeII(int nDir, int nTicks);
    int Status_MaxDomeII(enum SH_Status &nShutterStatus, enum AZ_Status &nAzimuthStatus, unsigned &nAzimuthPosition, unsigned &nHomePosition);
    int Goto_Azimuth_MaxDomeII(double newAz);
    int Ack_MaxDomeII();
    int SetPark_MaxDomeII(int nParkOnShutter, int nTicks);
    int SetTicksPerCount_MaxDomeII(int nTicks);
    int Park_MaxDomeII();

    //  Shutter commands
    int Open_Shutter_MaxDomeII();
    int Open_Upper_Shutter_Only_MaxDomeII();
    int Close_Shutter_MaxDomeII();
    int Abort_Shutter_MaxDomeII();
    int Exit_Shutter_MaxDomeII();

    // convertion functions
    void AzToTicks(double pdAz, int &dir, int &ticks);
    void TicksToAz(int ticks, double &pdAz);

    // command complete functions
    int IsGoToComplete(bool &complete);
    int IsOpenComplete(bool &complete);
    int IsCloseComplete(bool &complete);
    int IsParkComplete(bool &complete);
    int IsUnparkComplete(bool &complete);
    int IsFindHomeComplete(bool &complete);
    
protected:

    signed char     checksum_MaxDomeII(char *cMessage, int nLen);
    int             ReadResponse_MaxDomeII(char *cMessage);
    bool            bIsConnected;
    int             mNbTicksPerRev;

    unsigned        mAzimuthPositionInTicks;
    double          mAzimuthPosition;

    unsigned        mHomePositionInTicks;
    double          mHomePosition;
    
    unsigned        mParkPositionInTicks;
    double          mParkPosition;

    unsigned        mCurrentAzPositionInTicks;
    double          mCurrentAzPosition;

    unsigned        mGotoTicks;
    SerXInterface   *pSerx;
};

#endif
