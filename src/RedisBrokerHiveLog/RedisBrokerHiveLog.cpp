
#include "RedisBrokerHiveLog.h"
#include <dirent.h>
CUserQueryHiveLog::CUserQueryHiveLog(const STATISTICSPRM_S& stHiveLogPrm)
 :m_stHiveLogPrm(stHiveLogPrm)
{	
	struct stat statbuf;
	std::string strHiveLogDir;
	srand((int)time(0));
	int randomInt = rand()%100000;
	m_RandomInt = randomInt;
	currentHour = BdxUserQueryHiveLogGetHour();
	
	char randomchar[5];
	memset(randomchar,0,5);
	sprintf(randomchar,"%d",m_RandomInt);
	strHiveLogDir = m_stHiveLogPrm.m_strStatisticsPath + "/" +	BdxUserQueryHiveLogGetDate();
	if(!m_clFile.FileBeExists(strHiveLogDir.c_str())) {
		m_clFile.FileCreatDir(strHiveLogDir.c_str());
	}
	std::string strFileName = strHiveLogDir + "/"+m_stHiveLogPrm.m_strStatisticsFileName+"_"+ BdxUserQueryHiveLogGetDate()+"_"+BdxUserQueryHiveLogGetHour()+"_"+std::string(randomchar)+ ".txt";

	m_pFile = fopen(strFileName.c_str(), "a");
	if (!stat(strFileName.c_str(),&statbuf))
	{
		if( statbuf.st_size == 0 )
		{
			BdxQueryHiveLogWriteTitle();
		}
	}
}

CUserQueryHiveLog::~CUserQueryHiveLog() {
	// TODO Auto-generated destructor stub
}
void CUserQueryHiveLog::Core()
{
	while(true) {
		std::vector<HIVELOCALLOG_S>::iterator itr;
		BdxQueryHiveLogOpenFile();
		//BdxQueryHiveLogGetReport();
        time_t timep;
        time(&timep);
        struct tm* timeinfo = localtime(&timep);
		for(u_int i = 0; i < CUserQueryWorkThreads::m_vecHiveLog.size(); ++i) {
			while(!CUserQueryWorkThreads::m_vecHiveLog[i].empty())
			{
				m_stHiveLog = CUserQueryWorkThreads::m_vecHiveLog[i].front();
							  CUserQueryWorkThreads::m_vecHiveLog[i].pop();				
				fprintf(m_pFile, "%04d-%02d-%02d %02d:%02d\t", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min);
				fprintf(m_pFile,"%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",m_stHiveLog.strLogKey.c_str(),m_stHiveLog.strLogKeyType.c_str(),m_stHiveLog.strLogValue.c_str(),m_stHiveLog.strCreateTime.c_str(),m_stHiveLog.strLastDataTime.c_str(),m_stHiveLog.strQueryTime.c_str(),m_stHiveLog.strDspName.c_str(),m_stHiveLog.strProvider.c_str(),m_stHiveLog.strProvince.c_str(),m_stHiveLog.strReqParams.c_str(), m_stHiveLog.strDayId.c_str(),m_stHiveLog.strHourId.c_str());
			}		
		}
		fflush(m_pFile);
        //sleep(m_stHiveLogPrm.m_uiStatisticsTime);
        sleep(60);
	}

}

std::string CUserQueryHiveLog::BdxUserQueryHiveLogGetDate(const time_t ttime)
{
	time_t tmpTime;
	if(ttime == 0)
		tmpTime = time(0);
	else
		tmpTime = ttime;
	struct tm* timeinfo = localtime(&tmpTime);
	char dt[20];
	memset(dt, 0, 20);
	sprintf(dt, "%4d%02d%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday);
	return std::string(dt);
}

std::string CUserQueryHiveLog::BdxUserQueryHiveLogGetHour(const time_t ttime)
{
	time_t tmpTime;
	if(ttime == 0)
		tmpTime = time(0);
	else
		tmpTime = ttime;
	struct tm* timeinfo = localtime(&tmpTime);
	char dt[20];
	memset(dt, 0, 20);
	sprintf(dt, "%02d",timeinfo->tm_hour);
	return std::string(dt);
}

std::string CUserQueryHiveLog::BdxUserQueryHiveLogGetLastHour(const time_t ttime)
{
	time_t tmpTime;
	if(ttime == 0)
		tmpTime = time(0);
	else
		tmpTime = ttime;
	tmpTime-=3600;
	struct tm* timeinfo = localtime(&tmpTime);
	char dt[20];
	memset(dt, 0, 20);
	sprintf(dt, "%02d",timeinfo->tm_hour);
	return std::string(dt);
}
void CUserQueryHiveLog::BdxQueryHiveLogOpenFile()
{
	struct stat statbuf;
    std::string strHiveLogDir;
	srand((int)time(0));
	int randomInt ;
	char randomchar[5];
	memset(randomchar,0,5);
	if( currentHour != BdxUserQueryHiveLogGetHour() )
	{
		randomInt = rand()%100000;
		currentHour = BdxUserQueryHiveLogGetHour();
		m_RandomInt = randomInt;
	}
    sprintf(randomchar,"%d",m_RandomInt);
	
	strHiveLogDir = m_stHiveLogPrm.m_strStatisticsPath + "/" +BdxUserQueryHiveLogGetDate();
	std::string strFileName = strHiveLogDir + "/"+m_stHiveLogPrm.m_strStatisticsFileName+"_"+ BdxUserQueryHiveLogGetDate()+"_"+BdxUserQueryHiveLogGetHour()+"_"+std::string(randomchar)+ ".txt";
	printf("Hive Log strFileName=%s\n",strFileName.c_str());

	if(!m_clFile.FileBeExists(strFileName.c_str())) 
	{
		if(m_pFile){
			fclose(m_pFile);
			m_pFile = NULL;
		}
		m_pFile = fopen(strFileName.c_str(), "a");
	}
	if (!stat(strFileName.c_str(),&statbuf))
	{
		if( statbuf.st_size == 0 )
		{
			BdxQueryHiveLogWriteTitle();
		}		
	}
}

void CUserQueryHiveLog::BdxQueryHiveLogWriteTitle()
{
	//printf("BdxQueryHiveLogWriteTitle.....\n");
}

void CUserQueryHiveLog::BdxQueryHiveLogGetReport()
{
	//memset(&m_stReport.m_strUserInfo,0,sizeof(std::map<std::string,USERINFO_S>));
	//memset(&m_stReport, 0, sizeof(UESRQUERYRPORT_S)); 结构体中有map 
	// 不需要初始化，否则会出错
	//for(u_int i = 0; i < CUserQueryWorkThreads::m_vecHiveLog.size(); ++i) {
		//m_stHiveLog += CUserQueryWorkThreads::m_vecHiveLog[i];
		//CUserQueryWorkThreads::m_vecHiveLog[i] = 0;
	//}
}
