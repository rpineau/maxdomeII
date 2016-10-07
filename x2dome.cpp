#include <stdio.h>
#include <string.h>
#include "x2dome.h"
#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/basicstringinterface.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/basiciniutilinterface.h"
#include "../../licensedinterfaces/theskyxfacadefordriversinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/basiciniutilinterface.h"
#include "../../licensedinterfaces/mutexinterface.h"
#include "../../licensedinterfaces/tickcountinterface.h"
#include "../../licensedinterfaces/serialportparams2interface.h"


X2Dome::X2Dome(const char* pszSelection, 
							 const int& nISIndex,
					SerXInterface*						pSerX,
					TheSkyXFacadeForDriversInterface*	pTheSkyXForMounts,
					SleeperInterface*					pSleeper,
					BasicIniUtilInterface*			pIniUtil,
					LoggerInterface*					pLogger,
					MutexInterface*						pIOMutex,
					TickCountInterface*					pTickCount)
{

    m_nPrivateISIndex				= nISIndex;
	m_pSerX							= pSerX;
	m_pTheSkyXForMounts				= pTheSkyXForMounts;
	m_pSleeper						= pSleeper;
	m_pIniUtil						= pIniUtil;
	m_pLogger						= pLogger;	
	m_pIOMutex						= pIOMutex;
	m_pTickCount					= pTickCount;

	m_bLinked = false;
    maxDome.SetSerxPointer(pSerX);
    maxDome.setLogger(pLogger);


    if (m_pIniUtil)
    {   
        maxDome.setNbTicksPerRev( m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_TICKS_PER_REV, maxDome.getNbTicksPerRev()) );
        maxDome.setHomeAz( m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_HOME_AZ, maxDome.getHomeAz()) );
        maxDome.setParkAz( m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_PARK_AZ, maxDome.getParkAz()) );
        mHasShutterControl = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_SHUTTER_CONTROL, true);
        mOpenUpperShutterOnly = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_SHUTTER_OPEN_UPPER_ONLY, false);
        mIsRollOffRoof = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_ROOL_OFF_ROOF, mIsRollOffRoof);
        maxDome.setCloseShutterBeforePark( ! m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_SHUTTER_OPER_ANY_Az, false)); // if we can operate at any Az then CloseShutterBeforePark is false
    }
}


X2Dome::~X2Dome()
{
	if (m_pSerX)
		delete m_pSerX;
	if (m_pTheSkyXForMounts)
		delete m_pTheSkyXForMounts;
	if (m_pSleeper)
		delete m_pSleeper;
	if (m_pIniUtil)
		delete m_pIniUtil;
	if (m_pLogger)
		delete m_pLogger;
	if (m_pIOMutex)
		delete m_pIOMutex;
	if (m_pTickCount)
		delete m_pTickCount;

}


int X2Dome::establishLink(void)					
{
    bool connected;
    char szPort[DRIVER_MAX_STRING];

    X2MutexLocker ml(GetMutex());
    // get serial port device name
    portNameOnToCharPtr(szPort,DRIVER_MAX_STRING);
    connected = maxDome.Connect(szPort);
    if(!connected)
        return ERR_COMMNOLINK;

    m_bLinked = true;
	return SB_OK;
}

int X2Dome::terminateLink(void)					
{
    X2MutexLocker ml(GetMutex());
	m_bLinked = false;
	return SB_OK;
}

 bool X2Dome::isLinked(void) const				
{
	return m_bLinked;
}


int X2Dome::queryAbstraction(const char* pszName, void** ppVal)
{
    *ppVal = NULL;

    if (!strcmp(pszName, LoggerInterface_Name))
        *ppVal = GetLogger();
    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);
    else if (!strcmp(pszName, X2GUIEventInterface_Name))
        *ppVal = dynamic_cast<X2GUIEventInterface*>(this);
    else if (!strcmp(pszName, SerialPortParams2Interface_Name))
        *ppVal = dynamic_cast<SerialPortParams2Interface*>(this);
    
    return SB_OK;
}

#pragma mark - UI binding

int X2Dome::execModalSettingsDialog()
{
    int nErr = SB_OK;
    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*					ui = uiutil.X2UI();
    X2GUIExchangeInterface*			dx = NULL;//Comes after ui is loaded
    bool bPressedOK = false;

    double dHomeAz;
    double dParkAz;
    bool operateAnyAz;
    int nTicksPerRev;

    if (NULL == ui)
        return ERR_POINTER;

    if ((nErr = ui->loadUserInterface("maxdomeII.ui", deviceType(), m_nPrivateISIndex)))
        return nErr;

    if (NULL == (dx = uiutil.X2DX()))
        return ERR_POINTER;

    // set controls state depending on the connection state
    if(mHasShutterControl)
    {
        dx->setChecked("hasShutterCtrl",true);
        dx->setEnabled("radioButtonShutterAnyAz", true);
        dx->setEnabled("isRoolOffRoof", true);
        dx->setEnabled("radioButtonShutterAnyAz", true);
        dx->setEnabled("groupBoxShutter", true);

        if(mOpenUpperShutterOnly)
            dx->setChecked("openUpperShutterOnly", true);
        else
            dx->setChecked("openUpperShutterOnly", false);

        if(maxDome.getCloseShutterBeforePark())
            dx->setChecked("radioButtonShutterPark", true);
        else
            dx->setChecked("radioButtonShutterAnyAz", true);

        if (mIsRollOffRoof)
            dx->setChecked("isRoolOffRoof",true);
        else
            dx->setChecked("isRoolOffRoof",false);
    }
    else
    {
        dx->setChecked("hasShutterCtrl",false);
        dx->setChecked("radioButtonShutterAnyAz",false);
        dx->setChecked("openUpperShutterOnly",false);
        dx->setChecked("isRoolOffRoof",false);

        dx->setEnabled("openUpperShutterOnly", false);
        dx->setEnabled("isRoolOffRoof", false);
        dx->setEnabled("groupBoxShutter", false);
        dx->setEnabled("radioButtonShutterAnyAz", false);
        
    }
    
    // disable Auto Calibrate for now
    dx->setEnabled("pushButton",true);

    dx->setPropertyInt("ticksPerRev","value", maxDome.getNbTicksPerRev());
    dx->setPropertyDouble("homePosition","value", maxDome.getHomeAz());
    dx->setPropertyDouble("parkPosition","value", maxDome.getParkAz());

    X2MutexLocker ml(GetMutex());

    //Display the user interface
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;

    //Retreive values from the user interface
    if (bPressedOK)
    {
        dx->propertyDouble("homePosition", "value", dHomeAz);
        dx->propertyDouble("parkPosition", "value", dParkAz);
        operateAnyAz = dx->isChecked("radioButtonShutterAnyAz");
        dx->propertyInt("ticksPerRev", "value", nTicksPerRev);
        mHasShutterControl = dx->isChecked("hasShutterCtrl");
        if(mHasShutterControl)
        {
            mOpenUpperShutterOnly = dx->isChecked("openUpperShutterOnly");
            mIsRollOffRoof = dx->isChecked("isRoolOffRoof");
        }
        else
        {
            mOpenUpperShutterOnly = false;
            mIsRollOffRoof = false;
        }

        if(m_bLinked)
        {
            maxDome.setHomeAz(dHomeAz);
            maxDome.setParkAz(dHomeAz);
            maxDome.SetPark_MaxDomeII(!operateAnyAz, dParkAz);
            maxDome.setNbTicksPerRev(nTicksPerRev);
        }

        // save the values to persistent storage
        nErr |= m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_TICKS_PER_REV, nTicksPerRev);
        nErr |= m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_HOME_AZ, dHomeAz);
        nErr |= m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_PARK_AZ, dParkAz);
        nErr |= m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_SHUTTER_CONTROL, mHasShutterControl);
        nErr |= m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_SHUTTER_OPEN_UPPER_ONLY, mOpenUpperShutterOnly);
        nErr |= m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_ROOL_OFF_ROOF, mIsRollOffRoof);
        nErr |= m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_SHUTTER_OPER_ANY_Az, operateAnyAz);
        
    }
    return nErr;

}

void X2Dome::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    bool complete = false;
    int err;
    char tmpBuf[20];
    char errorMessage[LOG_BUFFER_SIZE];

    if (!strcmp(pszEvent, "on_pushButtonCancel_clicked")) {
        maxDome.Abort_Azimuth_MaxDomeII();
        maxDome.Abort_Shutter_MaxDomeII();
    }

    if (!strcmp(pszEvent, "on_timer")) {
        if(uiex->isChecked("hasShutterCtrl")) {
            uiex->setEnabled("openUpperShutterOnly", true);
            uiex->setEnabled("isRoolOffRoof", true);
            uiex->setEnabled("groupBoxShutter", true);
            uiex->setEnabled("radioButtonShutterAnyAz", true);
            mHasShutterControl = true;
        }
        else {
            uiex->setEnabled("openUpperShutterOnly", false);
            uiex->setEnabled("isRoolOffRoof", false);
            uiex->setEnabled("groupBoxShutter", false);
            uiex->setEnabled("radioButtonShutterAnyAz", false);
            mHasShutterControl = false;
        }
        // deal with auto calibrate here.
        if(m_bLinked) {
            // are we going to Home position to calibrate ?
            if(mHomingDome) {
                // are we home ?
                complete = false;
                err = maxDome.IsFindHomeComplete(complete);
                if (err) {
                    uiex->setEnabled("pushButton",true);
                    uiex->setEnabled("pushButtonOK",true);
                    snprintf(errorMessage, LOG_BUFFER_SIZE, "Error homing dome while calibrating : Error %d", err);
                    uiex->messageBox("MaxDome II Calibrate", errorMessage);
                    mHomingDome = false;
                    mInitCalibration = false;
                    mCalibratingDome = false;
                    return;
                }
                if(complete) {
                    mHomingDome = false;
                    mCalibratingDome = false;
                    mInitCalibration = true;
                    // move 10 ticks forward to the right from inside the dome (hopefully) :)
                    maxDome.Goto_Azimuth_MaxDomeII(MAXDOMEII_EW_DIR, 10);
                    return;
                }
            }

            if(mInitCalibration) {
                err = maxDome.IsGoToComplete(complete);
                if (err) {
                    uiex->setEnabled("pushButton",true);
                    uiex->setEnabled("pushButtonOK",true);
                    snprintf(errorMessage, LOG_BUFFER_SIZE, "Error moving dome while calibrating : Error %d", err);
                    uiex->messageBox("MaxDome II Calibrate", errorMessage);
                    mHomingDome = false;
                    mInitCalibration = false;
                    mCalibratingDome = false;
                    return;
                }

                if(complete) {
                    mCalibratingDome = true;
                    mInitCalibration = false;
                    maxDome.Home_Azimuth_MaxDomeII();
                }
                else {
                    return;
                }

            }

            if(mCalibratingDome) {
                err = maxDome.IsFindHomeComplete(complete);
                if (err) {
                    uiex->setEnabled("pushButton",true);
                    uiex->setEnabled("pushButtonOK",true);
                    snprintf(errorMessage, LOG_BUFFER_SIZE, "Error on 2nd homing dome while calibrating : Error %d", err);
                    uiex->messageBox("MaxDome II Calibrate", errorMessage);
                    mHomingDome = false;
                    mInitCalibration = false;
                    mCalibratingDome = false;
                    return;
                }
                if(complete) {
                    mHomingDome = false;
                    mCalibratingDome = false;
                    mInitCalibration = false;
                    // enable "ok" and "calibrate"
                    uiex->setEnabled("pushButton",true);
                    uiex->setEnabled("pushButtonOK",true);
                    // read step per rev from dome
                    snprintf(tmpBuf,16,"%d",maxDome.getNbTicksPerRev());
                    uiex->setPropertyString("ticksPerRev","text", tmpBuf);
                    return;
                }
            }

        }

    }

    if (!strcmp(pszEvent, "on_pushButton_clicked")) {
        if(m_bLinked) {
            // disable "ok" and "calibrate"
            uiex->setEnabled("pushButton",false);
            uiex->setEnabled("pushButtonOK",false);
            maxDome.Home_Azimuth_MaxDomeII();
            mHomingDome = true;
        }
    }

}

//
//HardwareInfoInterface
//
#pragma mark - HardwareInfoInterface

void X2Dome::deviceInfoNameShort(BasicStringInterface& str) const					
{
	str = "MaxDome II";
}
void X2Dome::deviceInfoNameLong(BasicStringInterface& str) const					
{
    str = "MaxDome II Dome Control System";
}
void X2Dome::deviceInfoDetailedDescription(BasicStringInterface& str) const		
{
    str = "MaxDome II Dome Control System by Rodolphe Pineau";
}
 void X2Dome::deviceInfoFirmwareVersion(BasicStringInterface& str)					
{
    str = "Not available.";
}
void X2Dome::deviceInfoModel(BasicStringInterface& str)
{
    str = "MaxDome II";
}

//
//DriverInfoInterface
//
#pragma mark - DriverInfoInterface

 void	X2Dome::driverInfoDetailedInfo(BasicStringInterface& str) const	
{
    str = "MaxDome II plugin v1.0 beta";
}

double	X2Dome::driverInfoVersion(void) const
{
	return DRIVER_VERSION;
}

//
//DomeDriverInterface
//
#pragma mark - DomeDriverInterface

int X2Dome::dapiGetAzEl(double* pdAz, double* pdEl)
{
    int err;

    unsigned tmpAzInTicks;
    double tmpAz;
    unsigned tmpHomePosition;
    enum SH_Status tmpShutterStatus;
    enum AZ_Status tmpAzimuthStatus;

    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(mIsRollOffRoof)
    {
        *pdAz = maxDome.getCurrentAz();
        *pdEl = 0.0f;
        return SB_OK;
    }

    *pdEl=0.0f;
    // returns number of ticks from home position for tmpAzInTicks
    err = maxDome.Status_MaxDomeII(tmpShutterStatus, tmpAzimuthStatus, tmpAzInTicks, tmpHomePosition);
    if(err)
        return ERR_CMDFAILED;
    
    maxDome.TicksToAz(tmpAzInTicks, tmpAz);
    *pdAz = tmpAz;
    return SB_OK;
}

int X2Dome::dapiGotoAzEl(double dAz, double dEl)
{
    int err;

    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(mIsRollOffRoof)
    {
        maxDome.setCurrentAz(dAz);
        return SB_OK;
    }

    err = maxDome.Goto_Azimuth_MaxDomeII(dAz);

    if(err)
        return ERR_CMDFAILED;
    else
    {
        mlastCommand = AzGoto;
        return SB_OK;
    }
}

int X2Dome::dapiAbort(void)
{

    if(!m_bLinked)
        return ERR_NOLINK;


    switch(mlastCommand)
    {
        case AzGoto:
            if(mIsRollOffRoof)
                break;
            maxDome.Abort_Azimuth_MaxDomeII();
            break;
        
        case ShutterOpen:
        case ShutterClose:
            maxDome.Abort_Shutter_MaxDomeII();
            break;
    }
	return SB_OK;
}

int X2Dome::dapiOpen(void)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(!mHasShutterControl)
        return SB_OK;

    if(mOpenUpperShutterOnly)
        err = maxDome.Open_Upper_Shutter_Only_MaxDomeII();
    else
        err = maxDome.Open_Shutter_MaxDomeII();
    if(err)
        return ERR_CMDFAILED;

    mlastCommand = ShutterOpen;
	return SB_OK;
}

int X2Dome::dapiClose(void)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(!mHasShutterControl)
        return SB_OK;

    err = maxDome.Close_Shutter_MaxDomeII();
    if(err)
        return ERR_CMDFAILED;

    mlastCommand = ShutterClose;
	return SB_OK;
}

int X2Dome::dapiPark(void)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(mIsRollOffRoof)
    {
        err = maxDome.Close_Shutter_MaxDomeII();
        if(err)
            return ERR_CMDFAILED;

        return SB_OK;
    }

    err = maxDome.Park_MaxDomeII();
    if(err)
        return ERR_CMDFAILED;

	return SB_OK;
}

int X2Dome::dapiUnpark(void)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    err = maxDome.Unpark();
    if(err)
        return ERR_CMDFAILED;

	return SB_OK;
}

int X2Dome::dapiFindHome(void)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(mIsRollOffRoof)
        return SB_OK;

    err = maxDome.Home_Azimuth_MaxDomeII();
    if(err)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiIsGotoComplete(bool* pbComplete)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(mIsRollOffRoof)
    {
        *pbComplete = true;
        return SB_OK;

    }

    err = maxDome.IsGoToComplete(*pbComplete);
    if(err)
        return ERR_CMDFAILED;
    return SB_OK;
}

int X2Dome::dapiIsOpenComplete(bool* pbComplete)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;
    
    if(!mHasShutterControl)
    {
        *pbComplete = true;
        return SB_OK;
    }

    err = maxDome.IsOpenComplete(*pbComplete);
    if(err)
        return ERR_CMDFAILED;

    return SB_OK;
}

int	X2Dome::dapiIsCloseComplete(bool* pbComplete)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(!mHasShutterControl)
    {
        *pbComplete = true;
        return SB_OK;
    }

    err = maxDome.IsCloseComplete(*pbComplete);
    if(err)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiIsParkComplete(bool* pbComplete)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(mIsRollOffRoof)
    {
        *pbComplete = true;
        return SB_OK;
    }

    err = maxDome.IsParkComplete(*pbComplete);
    if(err)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiIsUnparkComplete(bool* pbComplete)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(mIsRollOffRoof)
    {
        *pbComplete = true;
        return SB_OK;
    }

    err = maxDome.IsUnparkComplete(*pbComplete);
    if(err)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiIsFindHomeComplete(bool* pbComplete)
{
    int err;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(mIsRollOffRoof)
    {
        *pbComplete = true;
        return SB_OK;
    }

    err = maxDome.IsFindHomeComplete(*pbComplete);
    if(err)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiSync(double dAz, double dEl)
{
    int err;

    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(mIsRollOffRoof)
        return SB_OK;

    err = maxDome.Sync_Dome(dAz);
    if (err)
        return ERR_CMDFAILED;
	return SB_OK;
}

//
// SerialPortParams2Interface
//
#pragma mark - SerialPortParams2Interface

void X2Dome::portName(BasicStringInterface& str) const
{
    char szPortName[DRIVER_MAX_STRING];

    portNameOnToCharPtr(szPortName, DRIVER_MAX_STRING);

    str = szPortName;

}

void X2Dome::setPortName(const char* szPort)
{
    if (m_pIniUtil)
        m_pIniUtil->writeString(PARENT_KEY, CHILD_KEY_PORTNAME, szPort);
    
}


void X2Dome::portNameOnToCharPtr(char* pszPort, const int& nMaxSize) const
{
    if (NULL == pszPort)
        return;

    sprintf(pszPort, DEF_PORT_NAME);

    if (m_pIniUtil)
        m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort, pszPort, nMaxSize);
    
}



