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
enum AZ_Status {As_IDLE = 1, As_MOVING_WE, As_MOVING_EW, As_IDEL2, As_ERROR};

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

    // Dome commands
    int Abort_Azimuth_MaxDomeII(int fd);
    int Home_Azimuth_MaxDomeII(int fd);
    int Goto_Azimuth_MaxDomeII(int fd, int nDir, int nTicks);
    int Status_MaxDomeII(int fd, enum SH_Status *nShutterStatus, enum AZ_Status *nAzimuthStatus, unsigned *nAzimuthPosition, unsigned *nHomePosition);
    int Ack_MaxDomeII(int fd);
    int SetPark_MaxDomeII(int fd, int nParkOnShutter, int nTicks);
    int SetTicksPerCount_MaxDomeII(int fd, int nTicks);
    int Park_MaxDomeII(int fd);

    //  Shutter commands
    int Open_Shutter_MaxDomeII(int fd);
    int Open_Upper_Shutter_Only_MaxDomeII(int fd);
    int Close_Shutter_MaxDomeII(int fd);
    int Abort_Shutter_MaxDomeII(int fd);
    int Exit_Shutter_MaxDomeII(int fd);

protected:

    signed char checksum_MaxDomeII(char *cMessage, int nLen);
    int         ReadResponse_MaxDomeII(int fd, char *cMessage);
    bool        bIsConnected;

    SerXInterface        *pSerx;
    int fd;
};

#endif
