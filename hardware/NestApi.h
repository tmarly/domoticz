#pragma once

#include "DomoticzHardware.h"
#include <iostream>
#include "hardwaretypes.h"
#include <map>

class CNestApi : public CDomoticzHardwareBase
{
	struct _tNestApiThermostat
	{
		std::string Serial;
		std::string StructureID;
		std::string Name;
	};
public:
	CNestApi(const int ID, const std::string &AccessToken);
	~CNestApi(void);
	bool WriteToHardware(const char *pdata, const unsigned char length);
	void SetProgramState(const int newState);
	
private:

	volatile bool m_stoprequested;
	std::string m_AccessToken;
	boost::shared_ptr<boost::thread> m_thread;
	std::map<int, _tNestApiThermostat> m_thermostats;

	void Init();
	bool StartHardware();
	bool StopHardware();
	void Do_Work();
	void GetMeterDetails();
};

