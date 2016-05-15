#include "maxdomeii.h"
#include "../../licensedinterfaces/domedriverinterface.h"
#include "../../licensedinterfaces/serialportparams2interface.h"
#include "../../licensedinterfaces/modalsettingsdialoginterface.h"
#include "../../licensedinterfaces/x2guiinterface.h"


class SerXInterface;		
class TheSkyXFacadeForDriversInterface;
class SleeperInterface;
class BasicIniUtilInterface;
class LoggerInterface;
class MutexInterface;
class BasicIniUtilInterface;
class TickCountInterface;

#define DRIVER_VERSION      1.0

#define PARENT_KEY			"MadomeII"
#define CHILD_KEY_PORTNAME	"PortName"
#define CHILD_KEY_TICKS_PER_REV "NbTicksPerRev"
#define CHILD_KEY_HOME_AZ "HomeAzimuth"
#define CHILD_KEY_PARK_AZ "ParkAzimuth"
#define CHILD_KEY_SHUTTER_CONTROL "ShutterCtrl"
#define CHILD_KEY_ROOL_OFF_ROOF "RollOffRoof"
#define CHILD_KEY_SHUTTER_OPER_ANY_Az "ShutterOperAnyAz"
#define CHILD_KEY_
#define CHILD_KEY_
#define CHILD_KEY_

#if defined(SB_WIN_BUILD)
#define DEF_PORT_NAME					"COM1"
#elif defined(SB_MAC_BUILD)
#define DEF_PORT_NAME					"/dev/cu.KeySerial1"
#elif defined(SB_LINUX_BUILD)
#define DEF_PORT_NAME					"/dev/COM0"
#endif

enum lastCommand {AzGoto = 0, ShutterOpen, ShutterClose};

/*!
\brief The X2Dome example.

\ingroup Example

Use this example to write an X2Dome driver.
*/
class X2Dome: public DomeDriverInterface, public SerialPortParams2Interface, public ModalSettingsDialogInterface, public X2GUIEventInterface
{
public:

	/*!Standard X2 constructor*/
	X2Dome(	const char* pszSelectionString, 
					const int& nISIndex,
					SerXInterface*						pSerX,
					TheSkyXFacadeForDriversInterface* pTheSkyXForMounts,
					SleeperInterface*				pSleeper,
					BasicIniUtilInterface*			pIniUtil,
					LoggerInterface*					pLogger,
					MutexInterface*					pIOMutex,
					TickCountInterface*				pTickCount);
	virtual ~X2Dome();

	/*!\name DriverRootInterface Implementation
	See DriverRootInterface.*/
	//@{ 
	virtual DeviceType							deviceType(void)							  {return DriverRootInterface::DT_DOME;}
	virtual int									queryAbstraction(const char* pszName, void** ppVal);
	//@} 

	/*!\name LinkInterface Implementation
	See LinkInterface.*/
	//@{ 
	virtual int									establishLink(void)						;
	virtual int									terminateLink(void)						;
	virtual bool								isLinked(void) const					;
	//@} 

    virtual int initModalSettingsDialog(void){return 0;}
    virtual int execModalSettingsDialog(void);

	/*!\name HardwareInfoInterface Implementation
	See HardwareInfoInterface.*/
	//@{ 
	virtual void deviceInfoNameShort(BasicStringInterface& str) const					;
	virtual void deviceInfoNameLong(BasicStringInterface& str) const					;
	virtual void deviceInfoDetailedDescription(BasicStringInterface& str) const		;
	virtual void deviceInfoFirmwareVersion(BasicStringInterface& str)					;
	virtual void deviceInfoModel(BasicStringInterface& str)							;
	//@} 

	/*!\name DriverInfoInterface Implementation
	See DriverInfoInterface.*/
	//@{ 
	virtual void								driverInfoDetailedInfo(BasicStringInterface& str) const	;
	virtual double								driverInfoVersion(void) const								;
	//@} 

	//DomeDriverInterface
	virtual int dapiGetAzEl(double* pdAz, double* pdEl);
	virtual int dapiGotoAzEl(double dAz, double dEl);
	virtual int dapiAbort(void);
	virtual int dapiOpen(void);
	virtual int dapiClose(void);
	virtual int dapiPark(void);
	virtual int dapiUnpark(void);
	virtual int dapiFindHome(void);
	virtual int dapiIsGotoComplete(bool* pbComplete);
	virtual int dapiIsOpenComplete(bool* pbComplete);
	virtual int	dapiIsCloseComplete(bool* pbComplete);
	virtual int dapiIsParkComplete(bool* pbComplete);
	virtual int dapiIsUnparkComplete(bool* pbComplete);
	virtual int dapiIsFindHomeComplete(bool* pbComplete);
	virtual int dapiSync(double dAz, double dEl);

    //SerialPortParams2Interface
    virtual void			portName(BasicStringInterface& str) const			;
    virtual void			setPortName(const char* szPort)						;
    virtual unsigned int	baudRate() const			{return 9600;};
    virtual void			setBaudRate(unsigned int)	{};
    virtual bool			isBaudRateFixed() const		{return true;}

    virtual SerXInterface::Parity	parity() const				{return SerXInterface::B_NOPARITY;}
    virtual void					setParity(const SerXInterface::Parity& parity){parity;};
    virtual bool					isParityFixed() const		{return true;}



    virtual void uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent);

private:

	SerXInterface 									*	GetSerX() {return m_pSerX; }		
	TheSkyXFacadeForDriversInterface				*	GetTheSkyXFacadeForDrivers() {return m_pTheSkyXForMounts;}
	SleeperInterface								*	GetSleeper() {return m_pSleeper; }
	BasicIniUtilInterface							*	GetSimpleIniUtil() {return m_pIniUtil; }
	LoggerInterface									*	GetLogger() {return m_pLogger; }
	MutexInterface									*	GetMutex()  {return m_pIOMutex;}
	TickCountInterface								*	GetTickCountInterface() {return m_pTickCount;}

	SerXInterface									*	m_pSerX;		
	TheSkyXFacadeForDriversInterface				*	m_pTheSkyXForMounts;
	SleeperInterface								*	m_pSleeper;
	BasicIniUtilInterface							*	m_pIniUtil;
	LoggerInterface									*	m_pLogger;
	MutexInterface									*	m_pIOMutex;
	TickCountInterface								*	m_pTickCount;

    void portNameOnToCharPtr(char* pszPort, const int& nMaxSize) const;


	int         m_nPrivateISIndex;
	int         m_bLinked;
    CMaxDome    maxDome;
    int         mlastCommand;
    bool        mHasShutterControl;
    bool        mIsRollOffRoof;
};
