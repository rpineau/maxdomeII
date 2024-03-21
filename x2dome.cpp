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


    if (m_pIniUtil)
    {
        maxDome.setNbTicksPerRev( m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_TICKS_PER_REV, 360) );
        maxDome.setHomeAz( m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_HOME_AZ, 0) );
        mOpenUpperShutterOnly = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_SHUTTER_OPEN_UPPER_ONLY, false);
        maxDome.setParkAz( mOpenUpperShutterOnly, m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_PARK_AZ, 0) );
        mHasShutterControl = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_SHUTTER_CONTROL, true);
        mIsRollOffRoof = m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_ROOL_OFF_ROOF, false);
        maxDome.setParkBeforeCloseShutter( ! m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_SHUTTER_OPER_ANY_Az, false)); // if we can operate at any Az then CloseShutterBeforePark is false
        maxDome.setDebounceTime(m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_DEBOUNCE_TIME, 120));

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

    int nErr = SB_OK;
    char szPort[DRIVER_MAX_STRING];

    X2MutexLocker ml(GetMutex());
    // get serial port device name
    portNameOnToCharPtr(szPort,DRIVER_MAX_STRING);
    nErr = maxDome.Connect(szPort);
    if(nErr)
        return nErr;

    m_bLinked = true;
	return SB_OK;
}

int X2Dome::terminateLink(void)					
{
    X2MutexLocker ml(GetMutex());
    maxDome.Disconnect();
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
    int nDebouceIndex;

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

    nDebouceIndex = (maxDome.getDebounceTime() - 20)/10;

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

    if(m_bLinked) {
        dx->setEnabled("pushButton", true);
        dx->setEnabled("comboBox", true);
        dx->setEnabled("pushButton_2", true);
    }
    else {
        dx->setEnabled("pushButton", false);
        dx->setEnabled("comboBox", false);
        dx->setEnabled("pushButton_2", false);
    }

    dx->setCurrentIndex("comboBox", nDebouceIndex);
    dx->setPropertyInt("ticksPerRev","value", maxDome.getNbTicksPerRev());
    dx->setPropertyDouble("homePosition","value", maxDome.getHomeAz());
    dx->setPropertyDouble("parkPosition","value", maxDome.getParkAz());

    X2MutexLocker ml(GetMutex());

    mHomingDome = false;
    mInitCalibration = false;
    mCalibratingDome = false;
    maxDome.setCalibrating(mCalibratingDome);

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

        maxDome.setHomeAz(dHomeAz);
        maxDome.setParkAz(!operateAnyAz, dParkAz);
        maxDome.setNbTicksPerRev(nTicksPerRev);

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
    int nErr = SB_OK;
    char errorMessage[LOG_BUFFER_SIZE];
    double  dHomeAz;
    int nDebounceTime;

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
                nErr = maxDome.IsFindHomeComplete(complete);
                if (nErr) {
                    uiex->setEnabled("pushButton",true);
                    uiex->setEnabled("pushButtonOK",true);
                    snprintf(errorMessage, LOG_BUFFER_SIZE, "Error homing dome while calibrating : Error %d", nErr);
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
                    maxDome.SyncMode_MaxDomeII();
                    maxDome.SetPark_MaxDomeII_Ticks(mOpenUpperShutterOnly, 32767);
                    // move 10 ticks forward to the right from inside the dome to clear the home sensor.
                    maxDome.Goto_Azimuth_MaxDomeII(MAXDOMEII_EW_DIR, 10);
                    return;
                }
            }

            else if(mInitCalibration) {
                nErr = maxDome.IsGoToComplete(complete);
                if (nErr) {
                    uiex->setEnabled("pushButton",true);
                    uiex->setEnabled("pushButtonOK",true);
                    snprintf(errorMessage, LOG_BUFFER_SIZE, "Error moving dome while calibrating : Error %d", nErr);
                    uiex->messageBox("MaxDome II Calibrate", errorMessage);
                    mHomingDome = false;
                    mInitCalibration = false;
                    mCalibratingDome = false;
                    return;
                }

                if(complete) {
                    mCalibratingDome = true;
                    maxDome.setCalibrating(mCalibratingDome);
                    mInitCalibration = false;
                    maxDome.Home_Azimuth_MaxDomeII();
                }
                else {
                    return;
                }

            }

            else if(mCalibratingDome) {
                nErr = maxDome.IsFindHomeComplete(complete);
                if (nErr) {
                    uiex->setEnabled("pushButton",true);
                    uiex->setEnabled("pushButtonOK",true);
                    snprintf(errorMessage, LOG_BUFFER_SIZE, "Error on 2nd homing dome while calibrating : Error %d", nErr);
                    uiex->messageBox("MaxDome II Calibrate", errorMessage);
                    mHomingDome = false;
                    mInitCalibration = false;
                    mCalibratingDome = false;
                    return;
                }
                if(complete) {
                    mHomingDome = false;
                    mCalibratingDome = false;
                    maxDome.setCalibrating(mCalibratingDome);
                    mInitCalibration = false;
                    // enable "ok" and "calibrate"
                    uiex->setEnabled("pushButton",true);
                    uiex->setEnabled("pushButtonOK",true);
                    // read step per rev from dome
                    uiex->setPropertyInt("ticksPerRev","value", maxDome.getNbTicksPerRev());
                    uiex->propertyDouble("homePosition", "value", dHomeAz);
                    maxDome.Sync_Dome(dHomeAz);
                    return;
                }
            }
            else {
                mHomingDome = false;
                mInitCalibration = false;
                mCalibratingDome = false;
            }

        }

    }

    if (!strcmp(pszEvent, "on_pushButton_clicked")) {
        if(m_bLinked) {
            // disable "ok" and "calibrate"
            uiex->setEnabled("pushButton",false);
            uiex->setEnabled("pushButtonOK",false);
            maxDome.setNbTicksPerRev(32767);
            maxDome.Home_Azimuth_MaxDomeII();
            mHomingDome = true;
        }
    }

    if (!strcmp(pszEvent, "on_pushButton_2_clicked")) {
        if(m_bLinked) {
            nDebounceTime = (uiex->currentIndex("comboBox") * 10) + 20;
            nErr = maxDome.setDebounceTime(nDebounceTime);
            if(nErr == FIRMWARE_NOT_SUPPORTED) {
                snprintf(errorMessage, LOG_BUFFER_SIZE, "This is not supported by this version of the firmware");
                uiex->messageBox("MaxDome II debounce time change", errorMessage);

            }
            else if (nErr) {
                snprintf(errorMessage, LOG_BUFFER_SIZE, "Error setting the new debounce time : %d", nErr);
                uiex->messageBox("MaxDome II debounce time change", errorMessage);
            }
            else {
                m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_DEBOUNCE_TIME, nDebounceTime);
            }
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
    if(!m_bLinked) {
        str = "Not available.";
    }
    else {
        char cFirmware[LOG_BUFFER_SIZE];
        maxDome.getFirmwareVersion(cFirmware, LOG_BUFFER_SIZE);
        str = cFirmware;
    }
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
    str = "MaxDome II plugin";
}

double	X2Dome::driverInfoVersion(void) const
{
	return PLUGIN_VERSION;
}

//
//DomeDriverInterface
//
#pragma mark - DomeDriverInterface

int X2Dome::dapiGetAzEl(double* pdAz, double* pdEl)
{
    int nErr = SB_OK;

    int tmpAzInTicks;
    double tmpAz;
    int tmpHomePosition;
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
    nErr = maxDome.Status_MaxDomeII(tmpShutterStatus, tmpAzimuthStatus, tmpAzInTicks, tmpHomePosition);
    if(nErr)
        return ERR_CMDFAILED;
    
    maxDome.TicksToAz(tmpAzInTicks, tmpAz);
    *pdAz = tmpAz;
    return SB_OK;
}

int X2Dome::dapiGotoAzEl(double dAz, double dEl)
{
    int nErr = SB_OK;

    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(mIsRollOffRoof)
    {
        maxDome.setCurrentAz(dAz);
        return SB_OK;
    }

    nErr = maxDome.Goto_Azimuth_MaxDomeII(dAz);

    if(nErr)
        return ERR_CMDFAILED;
    else
    {
        mlastCommand = AzGoto;
    }
    return SB_OK;
}

int X2Dome::dapiAbort(void)
{

    X2MutexLocker ml(GetMutex());
    if(!m_bLinked)
        return ERR_NOLINK;

    maxDome.Abort_Azimuth_MaxDomeII();
    maxDome.Abort_Shutter_MaxDomeII();
	return SB_OK;
}

int X2Dome::dapiOpen(void)
{
    int nErr = SB_OK;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(!mHasShutterControl)
        return SB_OK;

    if(mOpenUpperShutterOnly)
        nErr = maxDome.Open_Upper_Shutter_Only_MaxDomeII();
    else
        nErr = maxDome.Open_Shutter_MaxDomeII();
    if(nErr)
        return ERR_CMDFAILED;

    mlastCommand = ShutterOpen;
	return SB_OK;
}

int X2Dome::dapiClose(void)
{
    int nErr = SB_OK;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(!mHasShutterControl)
        return SB_OK;

    nErr = maxDome.Close_Shutter_MaxDomeII();
    if(nErr)
        return ERR_CMDFAILED;

    mlastCommand = ShutterClose;
	return SB_OK;
}

int X2Dome::dapiPark(void)
{
    int nErr = SB_OK;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(mIsRollOffRoof)
    {
        nErr = maxDome.Close_Shutter_MaxDomeII();
        if(nErr)
            return ERR_CMDFAILED;

        return SB_OK;
    }

    nErr = maxDome.Park_MaxDomeII();
    if(nErr)
        return ERR_CMDFAILED;

	return SB_OK;
}

int X2Dome::dapiUnpark(void)
{
    int nErr = SB_OK;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    nErr = maxDome.Unpark();
    if(nErr)
        return ERR_CMDFAILED;

	return SB_OK;
}

int X2Dome::dapiFindHome(void)
{
    int nErr = SB_OK;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(mIsRollOffRoof)
        return SB_OK;

    nErr = maxDome.Home_Azimuth_MaxDomeII();
    if(nErr)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiIsGotoComplete(bool* pbComplete)
{
    int nErr = SB_OK;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(mIsRollOffRoof)
    {
        *pbComplete = true;
        return SB_OK;

    }

    nErr = maxDome.IsGoToComplete(*pbComplete);
    if(nErr)
        return ERR_CMDFAILED;
    return SB_OK;
}

int X2Dome::dapiIsOpenComplete(bool* pbComplete)
{
    int nErr = SB_OK;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;
    
    if(!mHasShutterControl)
    {
        *pbComplete = true;
        return SB_OK;
    }

    nErr = maxDome.IsOpenComplete(*pbComplete);
    if(nErr)
        return ERR_CMDFAILED;

    return SB_OK;
}

int	X2Dome::dapiIsCloseComplete(bool* pbComplete)
{
    int nErr = SB_OK;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(!mHasShutterControl)
    {
        *pbComplete = true;
        return SB_OK;
    }

    nErr = maxDome.IsCloseComplete(*pbComplete);
    if(nErr)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiIsParkComplete(bool* pbComplete)
{
    int nErr = SB_OK;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(mIsRollOffRoof)
    {
        *pbComplete = true;
        return SB_OK;
    }

    nErr = maxDome.IsParkComplete(*pbComplete);
    if(nErr)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiIsUnparkComplete(bool* pbComplete)
{
    int nErr = SB_OK;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(mIsRollOffRoof)
    {
        *pbComplete = true;
        return SB_OK;
    }

    nErr = maxDome.IsUnparkComplete(*pbComplete);
    if(nErr)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiIsFindHomeComplete(bool* pbComplete)
{
    int nErr = SB_OK;
    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(mIsRollOffRoof)
    {
        *pbComplete = true;
        return SB_OK;
    }

    nErr = maxDome.IsFindHomeComplete(*pbComplete);
    if(nErr)
        return ERR_CMDFAILED;

    return SB_OK;
}

int X2Dome::dapiSync(double dAz, double dEl)
{
    int nErr = SB_OK;

    X2MutexLocker ml(GetMutex());

    if(!m_bLinked)
        return ERR_NOLINK;

    if(mIsRollOffRoof)
        return SB_OK;

    nErr = maxDome.Sync_Dome(dAz);
    if (nErr)
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

    snprintf(pszPort, nMaxSize,  DEF_PORT_NAME);

    if (m_pIniUtil)
        m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort, pszPort, nMaxSize);
    
}



