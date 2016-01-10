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
	void SetSetpoint(const int idx, const float temp);
	void SetProgramState(const int newState);
private:
	void SendSetPointSensor(const unsigned char Idx, const float Temp, const std::string &defaultname);
	void UpdateSmokeSensor(const unsigned char Idx, const bool bOn, const std::string &defaultname);
	bool SetAway(const unsigned char Idx, const bool bIsAway);
	bool Login();
	void Logout();

	std::string m_TransportURL;
	std::string m_StructureId;
	std::string m_AccessToken;
//	std::string m_Serial;
	//std::string m_StructureID;
	volatile bool m_stoprequested;
	boost::shared_ptr<boost::thread> m_thread;
	std::map<int, _tNestApiThermostat> m_thermostats;

	void Init();
	bool StartHardware();
	bool StopHardware();
	void Do_Work();
	void GetMeterDetails();
};

