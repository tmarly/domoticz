#include "stdafx.h"
#include "NestApi.h"
#include "../main/Helper.h"
#include "../main/Logger.h"
#include "hardwaretypes.h"
#include "../main/localtime_r.h"
#include "../json/json.h"
#include "../main/RFXtrx.h"
#include "../main/SQLHelper.h"
#include "../httpclient/HTTPClient.h"
#include "../main/mainworker.h"
#include "../json/json.h"

#define round(a) ( int ) ( a + .5 )

const std::string NEST_API_PATH = "https://developer-api.nest.com/";
const std::string NEST_API_GET_STATUS = "/v2/mobile/user.";
const std::string NEST_API_SET_SHARED = "/v2/put/shared.";
const std::string NEST_API_SET_STRUCTURE = "/v2/put/structure.";


CNestApi::CNestApi(const int ID, const std::string &AccessToken) 
{
	m_HwdID=ID;
	m_AccessToken = AccessToken;
	Init();
}

CNestApi::~CNestApi(void)
{
}

void CNestApi::Init()
{
	m_stoprequested = false;
}

bool CNestApi::StartHardware()
{
	Init();
	//Start worker thread
	m_thread = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&CNestApi::Do_Work, this)));
	m_bIsStarted=true;
	sOnConnected(this);
	return (m_thread!=NULL);
}

bool CNestApi::StopHardware()
{
	if (m_thread!=NULL)
	{
		assert(m_thread);
		m_stoprequested = true;
		m_thread->join();
	}
    m_bIsStarted=false;
    return true;
}

#define NEST_API_POLL_INTERVAL 60

void CNestApi::Do_Work()
{
	_log.Log(LOG_STATUS,"NestApi: Worker started...");
	int sec_counter = NEST_API_POLL_INTERVAL-5;
	while (!m_stoprequested)
	{
		sleep_seconds(1);
		sec_counter++;
		if (sec_counter % 12 == 0)
		{
			m_LastHeartbeat = mytime(NULL);
		}

		if (sec_counter % NEST_API_POLL_INTERVAL == 0)
		{
			GetMeterDetails();
		}

	}
	_log.Log(LOG_STATUS,"NestApi: Worker stopped...");
}


void CNestApi::GetMeterDetails()
{
	std::string sResult;
	if (m_AccessToken.size()==0)
		return;

	std::vector<std::string> ExtraHeaders;

	ExtraHeaders.push_back("Authorization: Bearer " + m_AccessToken);

	//Get Data
	std::string sURL = NEST_API_PATH;
	if (!HTTPClient::GET(sURL, ExtraHeaders, sResult))
	{
		_log.Log(LOG_ERROR, "NestApi: Error getting current state!");
		return;
	}

	Json::Value root;
	Json::Reader jReader;
	if (!jReader.parse(sResult, root))
	{
		_log.Log(LOG_ERROR, "NestApi: Invalid data received!");
		return;
	}

	// Json should look like this:
	// {
	// "devices":{
	// 	"thermostats":{
	// 		"ROBZRW0_j5odsEEefHRpBB":{
	// 			"humidity":50,
	//			"locale":"fr-FR",
	//			"temperature_scale":"C",
	//			"is_using_emergency_heat":false,
	//			"has_fan":false,
	//			"software_version":"5.1.5rc7",
	//			"has_leaf":false,
	//			"where_id":"Oxuw5_UD2xgvJ12R9pCoxWAby5mIDx5Es-cRlxsVxQ",
	//			"device_id":"ROBZRW0_j5odsEEefHRpBB",
	//			"name":"Living Room",
	//			"can_heat":true,
	//			"can_cool":false,
	//			"hvac_mode":"heat",
	//			"target_temperature_c":20.0,
	//			"target_temperature_f":68,
	//			"target_temperature_high_c":24.0,
	//			"target_temperature_high_f":75,
	//			"target_temperature_low_c":20.0,
	//			"target_temperature_low_f":68,
	//			"ambient_temperature_c":20.0,
	//			"ambient_temperature_f":69,
	//			"away_temperature_high_c":24.0,
	//			"away_temperature_high_f":76,
	//			"away_temperature_low_c":16.0,
	//			"away_temperature_low_f":61,
	//			"structure_id":"jaiWql84AYUC2w4sDutb5Dc8CKQ_Y2",
	//			"fan_timer_active":false,
	//			"name_long":"Living Room Thermostat",
	//			"is_online":true,
	//			"last_connection":"2016-01-10T14:04:54.358Z",
	//			"hvac_state":"off"
	//		}
	//	}
	//},
	//"structures":{
	//	"jaiWql84AYUC2w4sDutb5Dc8CKQ_Y2":{
	//		"name":"Domicile",
	//		"country_code":"FR",
	//		"time_zone":"Europe/Paris",
	//		"away":"home",
	//		"thermostats":[
	//			"ROBZRW0_j5odsEEefHRpBB"
	//		],
	//		"structure_id":"jaiWql84AYUC2w4sDutb5Dc8CKQ_Y2",
	//		"wheres":{
	//			"Oxuw5_UD2xgvJ12R9pCoxWAby5mIDx4KfgzbW2DDHg":{
	//				"where_id":"Oxuw5_UD2xgvJ12R9pCoxWAby5mIDx4KfgzbW2DDHg",
	//				"name":"Basement"
	//			},
	//			"Oxuw5_UD2xgvJ12R9pCoxWAby5mIDx4KAdbqighcAg":{
	//				"where_id":"Oxuw5_UD2xgvJ12R9pCoxWAby5mIDx4KAdbqighcAg",
	//				"name":"Bedroom"
	//			},
	//			...
	//		}
	//	}
	//},
	//"metadata":{
	//	"access_token":"c.xxxxx",
	//	"client_version":1
	//	}
	//}

	if (!root["error"].empty()) {
		_log.Log(LOG_ERROR, ("NestApi: Error: " + root["error"].asString()).c_str());
		return;
	}

	if (root["structures"].empty()) {
		_log.Log(LOG_ERROR, "NestApi: Structures not found");
		return;
	}

	Json::Value::Members members;

	size_t iThermostat = 0;

	// iterator over thermostats
	for (Json::Value::iterator itThermostat = root["devices"]["thermostats"].begin(); itThermostat != root["devices"]["thermostats"].end(); ++itThermostat)
	{
		// the key (key => value) is the thermostat id
		std::string thermostatId = itThermostat.key().asString();

		// The values
		Json::Value thermostatData = *itThermostat;
		
		std::string structureId = thermostatData["structure_id"].asString();
		std::string name = thermostatData["name_long"].asString();// living room thermostat

		_tNestApiThermostat ntherm;
		ntherm.Serial = thermostatId;
		ntherm.StructureID = structureId;
		ntherm.Name = name;

		// save it in the "all thermostats" container
		m_thermostats[iThermostat] = ntherm;

		// Target temperature - saved in "temperature" tab
		float targetTemp = thermostatData["target_temperature_c"].asFloat();// 20.0
		SendTempSensor((const unsigned char)(iThermostat * 4) + 1/* unique id */, 255 /* battery */, targetTemp, Name + " target temperature");

		// Ambient temperature & humidity - saved in "temparture" tab
		float ambientTemp = thermostatData["ambient_temperature_c"].asFloat();// 50
		int humidity = thermostatData["humidity"].asInt();// 50
		SendTempHumSensor((iThermostat * 4) + 2/* unique id */, 255 /* battery */, ambientTemp, humidity, Name + " current temperature");

		// Away - saved in "switch" tab
		std::string away = root["structures"][structureId]["away"].asString(); // home | away
		SendSwitch((iThermostat * 4) + 3/* unique id */, 1, 255 /* battery */, away != "away", 0, Name + " presence detected");

		// heating switch - saved in "switch" tab
		std::string hvacState = thermostatData["hvac_state"].asString();// off | heating
		SendSwitch((iThermostat * 4) + 4/* unique id */, 1, 255/* battery */, hvacState != "away", 0, Name + " heat switch");

		iThermostat++;
	}
}

bool CNestApi::WriteToHardware(const char *pdata, const unsigned char length)
{
	/*
	if (m_UserName.size() == 0)
		return false;
	if (m_Password.size() == 0)
		return false;

	tRBUF *pCmd = (tRBUF *)pdata;
	if (pCmd->LIGHTING2.packettype != pTypeLighting2)
		return false; //later add RGB support, if someone can provide access

	int node_id = pCmd->LIGHTING2.id4;

	bool bIsOn = (pCmd->LIGHTING2.cmnd == light2_sOn);

	if (node_id % 3 == 0)
	{
		//Away
		return SetAway(node_id, bIsOn);
	}
	*/

	return false;
}

void CNestApi::SetProgramState(const int newState)
{
	/*
	if (m_UserName.size() == 0)
		return;
	if (m_Password.size() == 0)
		return;

	if (m_bDoLogin)
	{
		if (!Login())
			return;
	}
	*/
}