

#include "RedisBrokerMain.h"
#include "../RedisBrokerWorkThreads/RedisBrokerWorkThreads.h"
#include "../RedisBrokerServer/RedisBrokerServer.h"
#include "../RedisBrokerUpdate/RedisBrokerUpdate.h"
#include "../RedisBrokerCount/RedisBrokerCount.h"
#include "../RedisBrokerHiveLog/RedisBrokerHiveLog.h"

#include <time.h>

CLog *gp_log = NULL;
extern std::string strServerName;
extern std::string strConfigFileName;
extern pthread_rwlock_t p_rwlock;
extern  pthread_rwlockattr_t p_rwlock_attr;
extern pthread_mutex_t mutex;
std::string strSslKeyPath;
std::string strSslKeyPassWord;
int iAPIQpsLimit;

IPPORT_S m_stMonitorApi;
IPPORT_S m_stEmailServer;
string m_strUserName;
string m_strPassWord;
string m_strMailFrom;
string m_strRcptTo;
string m_strSubject;
string m_strErrorMsg;



//std::map<std::string,int> g_mapUserQueryLimit;
std::map<std::string,QUERYAPIINFO_S> g_vecUrlAPIS;
std::string g_remoteIp;				
uint16_t g_remotePort;
std::string g_serviceBrokerUser;	
std::string g_serviceBrokerPass;	





CUserQueryMain::CUserQueryMain(char* pchConfFile) {
	if(!BdxInitConfig(pchConfFile)) {
		throw std::runtime_error("BdxInitConfig error.");
	}

	gp_log = new CLog(const_cast<char*>(m_stLogPrms.m_strPath.c_str()), const_cast<char*>(m_stLogPrms.m_strFileName.c_str()), (int)m_stLogPrms.m_uiMaxLine, m_stLogPrms.m_Rank);
	if(!gp_log) {
		throw std::runtime_error("new log error.");
	}

}

CUserQueryMain::~CUserQueryMain() {
	// TODO Auto-generated destructor stub
}

void CUserQueryMain::UserQueryMainCore()
{
	BdxPrintConfig();
	
 // inint read write lock
	pthread_rwlockattr_init(&p_rwlock_attr);
	pthread_rwlockattr_setkind_np(&p_rwlock_attr,PTHREAD_RWLOCK_PREFER_WRITER_NP);
	if (pthread_rwlock_init(&p_rwlock, &p_rwlock_attr))	
	{
		throw std::runtime_error("pthread_rwlock_init error."); 
	}  

	pthread_mutex_init (&mutex,NULL);


	for(u_int i = 0; i < m_uiThreadsNum; ++i) {
		CUserQueryWorkThreads* pclWorkThread = new CUserQueryWorkThreads(m_stServerInfo.m_stRedisServer,m_stServerInfo.m_stGoodsServer);
		m_cThreads.SetRoutine(StartRoutine<CUserQueryWorkThreads>);
		m_cThreads.CreateThead(pclWorkThread);
		
	}
	LOG(DEBUG, "start work threads ok.");


	#if 0
	{
		CUserQueryCount* pCount = new CUserQueryCount(m_stStatisticsPrm);
		m_cThreads.SetRoutine(StartRoutine<CUserQueryCount>);
		m_cThreads.CreateThead(pCount);
	}
	{
		CUserQueryHiveLog* pHiveLog = new CUserQueryHiveLog(m_stHiveLogPrm);
		m_cThreads.SetRoutine(StartRoutine<CUserQueryHiveLog>);
		m_cThreads.CreateThead(pHiveLog);
	}
	{
		CUserQueryUpdate* pMonitorThread = new CUserQueryUpdate(m_stServerInfo.m_stTokenServer,m_stMysqlInfo);
		m_cThreads.SetRoutine(StartRoutine<CUserQueryUpdate>);
		m_cThreads.CreateThead(pMonitorThread);
	}
	#endif
	{
		CUserQueryServer* pServer = new CUserQueryServer(m_stServerInfo.m_stLocalServer, m_uiThreadsNum);
		m_cThreads.SetRoutine(StartRoutine<CUserQueryServer>);
		m_cThreads.CreateThead(pServer);

	}
	LOG(DEBUG, "start Adapter server Ok [port : %d]", m_stServerInfo.m_stLocalServer.m_uiPort);
	printf("File:%s,Line:%d,start Adapter server Ok [port : %d]",__FILE__,__LINE__,m_stServerInfo.m_stLocalServer.m_uiPort);
	
	m_cThreads.ThreadJionALL();

}

bool CUserQueryMain::BdxInitConfig(const char *pszCfgFile)
{
	CConf CConf;
	char szLine[PACKET] = { 0 };//line buffer
	char szSection[PACKET] = { 0 };//current section

	//open configure file
	if (SUCCESS != CConf.ConfOpen(const_cast<char *>(pszCfgFile))) {
		fprintf(stderr, "open config File %s error.\n", pszCfgFile);		
		return false;
	}
	//read configure file
	do {
		memset(szLine, 0, PACKET);
		if (SUCCESS != CConf.ConfGetLine(szLine, PACKET)) {
			return false;
		}
		CConf.ConfTrim(szLine);
	} while (!CConf.ConfIsValid(szLine) || !CConf.ConfIsSection(szLine));

	strcpy(szSection, szLine);// get section ,stop when read the first [
	//get all parameters
	while (strlen(szSection) != 0) {
		if (strcmp(szSection, "[LOG]") == 0) { //continue when read [ every time
			BdxGetLogPrms(&CConf, szSection);
		} /*else if(strcmp(szSection, "[TRAFFICHIVELOG]") == 0) {
			BdxGetHiveLogPrms(&CConf, szSection);
		} else if(strcmp(szSection, "[TRAFFICSTATISTICS]") == 0) {
			BdxGetStatisticPrms(&CConf, szSection);
		} else if(strcmp(szSection, "[REDISSERVER]") == 0) {
			BdxGetServerPrms(&CConf, szSection, m_stServerInfo.m_stRedisServer);
		} else if(strcmp(szSection, "[GOODS]") == 0) {
			BdxGetServerPrms(&CConf, szSection, m_stServerInfo.m_stGoodsServer);
		}else if(strcmp(szSection, "[TOKENSERVER]") == 0) {
			BdxGetServerPrms(&CConf, szSection, m_stServerInfo.m_stTokenServer);
		}else if(strcmp(szSection, "[MYSQLSERVER]") == 0) {
			BdxGetMysqlServerPrms(&CConf, szSection, m_stMysqlInfo);
		}*/else if(strcmp(szSection, "[LOCALSERVER]") == 0) {
			BdxGetServerPrms(&CConf, szSection, m_stServerInfo.m_stLocalServer);
	  }  else if(strcmp(szSection, "[WORKTHREADS]") == 0) {
			BdxGetThreadsPrm(&CConf, szSection);
		}  else if(strcmp(szSection, "[ETCDAPIS]") == 0) {
			BdxGetServerUrlAPI(&CConf, szSection);
		} /*else if(strcmp(szSection, "[MONITORAPIINFO]") == 0) {
			BdxGetMonitorApiPrms(&CConf, szSection);
		}*/

	}

	CConf.ConfClose();
	return true;
}

bool CUserQueryMain::BdxGetLogPrms(CConf *pCConf, char *pszSection)
{
	char* pszKey = NULL;
	char* pszVal = NULL;
	char szLine[PACKET] = { 0 };//line buffer

	while (SUCCESS == pCConf->ConfGetLine(szLine, PACKET)) {
		if (!pCConf->ConfIsValid(szLine)) {
			continue;
		}
		//printf("szLine=%s\n",szLine);
		pCConf->ConfTrim(szLine);
		if (pCConf->ConfIsSection(szLine)) {
			memset(pszSection, 0, PACKET);
			strcpy(pszSection, szLine);//get new section
			return true;
		} else {
			if (SUCCESS == pCConf->ConfKeyValue(szLine, &pszKey, &pszVal)) {
				if (strcmp(pszKey, "RANK") == 0) {
					if(strcmp(pszVal, "DEBUG") == 0) {
						m_stLogPrms.m_Rank = DEBUG;
					} else if(strcmp(pszVal, "INFO") == 0) {
						m_stLogPrms.m_Rank = INFO;
					} else if(strcmp(pszVal, "WARNING") == 0) {
						m_stLogPrms.m_Rank = WARNING;
					} else if(strcmp(pszVal, "ERROR") == 0) {
						m_stLogPrms.m_Rank = ERROR;
					} else if(strcmp(pszVal, "REQUIRED") == 0) {
						m_stLogPrms.m_Rank = REQUIRED;
					} else {
						m_stLogPrms.m_Rank = DEBUG;
					}
				} else if (strcmp(pszKey, "FILE") == 0) {
					m_stLogPrms.m_strFileName = pszVal;
				} else if(strcmp(pszKey, "PATH") == 0) {
					m_stLogPrms.m_strPath = pszVal;
				} else if(strcmp(pszKey, "MAXLINE") == 0) {
					m_stLogPrms.m_uiMaxLine = atoi(pszVal);
				}
				
			}
		}
	}
	pszSection[0] = 0;//set section
	return true;
}
bool CUserQueryMain::BdxGetHiveLogPrms(CConf *pCConf, char *pszSection)
{
	char* pszKey = NULL;
	char* pszVal = NULL;
	char szLine[PACKET] = { 0 };//line buffer

	while (SUCCESS == pCConf->ConfGetLine(szLine, PACKET)) {
		if (!pCConf->ConfIsValid(szLine)) {
			continue;
		}
		//printf("szLine=%s\n",szLine);
		pCConf->ConfTrim(szLine);
		if (pCConf->ConfIsSection(szLine)) {
			memset(pszSection, 0, PACKET);
			strcpy(pszSection, szLine);//get new section
			return true;
		} else {
			if (SUCCESS == pCConf->ConfKeyValue(szLine, &pszKey, &pszVal)) {
				if (strcmp(pszKey, "PATH") == 0) {
					m_stHiveLogPrm.m_strStatisticsPath= pszVal;
				} else if(strcmp(pszKey, "FILE") == 0) {
					m_stHiveLogPrm.m_strStatisticsFileName= pszVal;
				} else if(strcmp(pszKey, "TIME") == 0) {
					m_stHiveLogPrm.m_uiStatisticsTime= atoi(pszVal);
				}
				
			}
		}
	}
	pszSection[0] = 0;//set section
	return true;
}
bool CUserQueryMain::BdxGetStatisticPrms(CConf *pCConf, char *pszSection)

{
	char* pszKey = NULL;
	char* pszVal = NULL;
	char szLine[PACKET] = { 0 };//line buffer

	while (SUCCESS == pCConf->ConfGetLine(szLine, PACKET)) {
		if (!pCConf->ConfIsValid(szLine)) {
			continue;
		}
		//printf("szLine=%s\n",szLine);
		pCConf->ConfTrim(szLine);
		if (pCConf->ConfIsSection(szLine)) {
			memset(pszSection, 0, PACKET);
			strcpy(pszSection, szLine);//get new section
			return true;
		} else {
			if (SUCCESS == pCConf->ConfKeyValue(szLine, &pszKey, &pszVal)) {
				if (strcmp(pszKey, "PATH") == 0) {
					m_stStatisticsPrm.m_strStatisticsPath= pszVal;
				} else if(strcmp(pszKey, "FILE") == 0) {
					m_stStatisticsPrm.m_strStatisticsFileName= pszVal;
				} else if(strcmp(pszKey, "TIME") == 0) {
					m_stStatisticsPrm.m_uiStatisticsTime= atoi(pszVal);
				}else if(strcmp(pszKey, "SSLPATH") == 0) {
					strSslKeyPath= pszVal;
					printf("strSslKeyPath=%s\n",strSslKeyPath.c_str());
				}else if(strcmp(pszKey, "SSLPASSWORD") == 0) {
					strSslKeyPassWord= pszVal;
					printf("strSslKeyPassWord=%s\n",strSslKeyPassWord.c_str());
				}else if(strcmp(pszKey, "APIQPSLIMIT") == 0) {
					iAPIQpsLimit= atoi(pszVal);
					printf("iAPIQpsLimit=%d\n",iAPIQpsLimit);
				}						
			}
		}
	}
	pszSection[0] = 0;//set section
	return true;
}

bool CUserQueryMain::BdxGetServerPrms(CConf *pCConf, char *pszSection, IPPORT_S& stIpPort)
{
	char* pszKey = NULL;
	char* pszVal = NULL;
	char szLine[PACKET] = { 0 };//line buffer

	while (SUCCESS == pCConf->ConfGetLine(szLine, PACKET)) {
		if (!pCConf->ConfIsValid(szLine)) {
			continue;
		}
		pCConf->ConfTrim(szLine);
		if (pCConf->ConfIsSection(szLine)) {
			memset(pszSection, 0, PACKET);
			strcpy(pszSection, szLine);//get new section
			return true;
		} else {
			if (SUCCESS == pCConf->ConfKeyValue(szLine, &pszKey, &pszVal)) {
				if (strcmp(pszKey, "IP") == 0) {
					memset(stIpPort.m_pchIp, 0, _128BYTES);
					memcpy(stIpPort.m_pchIp, pszVal, _128BYTES);
				} else if (strcmp(pszKey, "PORT") == 0) {
					stIpPort.m_uiPort = atoi(pszVal);
				}else if (strcmp(pszKey, "IDX") == 0) {
					stIpPort.m_idx= atoi(pszVal);
				}else if (strcmp(pszKey, "NUMS") == 0) {
					stIpPort.m_count= atoi(pszVal);
				}
				
			}
		}
	}
	pszSection[0] = 0;//set section
	return true;
}


bool CUserQueryMain::BdxGetMonitorApiPrms(CConf *pCConf, char *pszSection)
{
	char* pszKey = NULL;
	char* pszVal = NULL;
	char szLine[PACKET] = { 0 };//line buffer

	while (SUCCESS == pCConf->ConfGetLine(szLine, PACKET)) {
		if (!pCConf->ConfIsValid(szLine)) {
			continue;
		}
		pCConf->ConfTrim(szLine);
		if (pCConf->ConfIsSection(szLine)) {
			memset(pszSection, 0, PACKET);
			strcpy(pszSection, szLine);//get new section
			return true;
		} else {
			if (SUCCESS == pCConf->ConfKeyValue(szLine, &pszKey, &pszVal)) {
				if (strcmp(pszKey, "APIIP") == 0) {
					memset(m_stMonitorApi.m_pchIp, 0, _128BYTES);
					memcpy(m_stMonitorApi.m_pchIp, pszVal, _128BYTES);
				} else if (strcmp(pszKey, "APIPORT") == 0) {
					m_stMonitorApi.m_uiPort = atoi(pszVal);
				}else if (strcmp(pszKey, "EMAILIP") == 0) {
					memset(m_stEmailServer.m_pchIp, 0, _128BYTES);
					memcpy(m_stEmailServer.m_pchIp, pszVal, _128BYTES);
				}else if (strcmp(pszKey, "EMAILPORT") == 0) {
					m_stEmailServer.m_uiPort = atoi(pszVal);
				}else if (strcmp(pszKey, "USERNAME") == 0) {
					m_strUserName = pszVal;
				}else if (strcmp(pszKey, "PASSWORD") == 0) {
					m_strPassWord = pszVal;
				}else if (strcmp(pszKey, "FROM") == 0) {
					m_strMailFrom = pszVal;
				}else if (strcmp(pszKey, "TO") == 0) {
					m_strRcptTo = pszVal;
				}else if (strcmp(pszKey, "SUBJECT") == 0) {
					m_strSubject = pszVal;
				}else if (strcmp(pszKey, "ERRORMSG") == 0) {
					m_strErrorMsg = pszVal;
				}
				
				
				
			}
		}
	}
	pszSection[0] = 0;//set section
	return true;
}


bool CUserQueryMain::BdxGetMysqlServerPrms(CConf *pCConf, char *pszSection, MYSQL_SERVERINFO_S& stMysqlServer)
{
	char* pszKey = NULL;
	char* pszVal = NULL;
	char szLine[PACKET] = { 0 };//line buffer

	while (SUCCESS == pCConf->ConfGetLine(szLine, PACKET)) {
		if (!pCConf->ConfIsValid(szLine)) {
			continue;
		}
		pCConf->ConfTrim(szLine);
		if (pCConf->ConfIsSection(szLine)) {
			memset(pszSection, 0, PACKET);
			strcpy(pszSection, szLine);//get new section
			return true;
		} else {
			if (SUCCESS == pCConf->ConfKeyValue(szLine, &pszKey, &pszVal)) {
				if (strcmp(pszKey, "IP") == 0) {
					memset(stMysqlServer.m_stMysqlLinkInfo.m_pchIp, 0, _128BYTES);
					memcpy(stMysqlServer.m_stMysqlLinkInfo.m_pchIp, pszVal, _128BYTES);
				} else if (strcmp(pszKey, "PORT") == 0) {
					stMysqlServer.m_stMysqlLinkInfo.m_uiPort = atoi(pszVal);
				}else if (strcmp(pszKey, "USERNAME") == 0) {
					memset(stMysqlServer.pchUserName, 0, _128BYTES);
					memcpy(stMysqlServer.pchUserName, pszVal, _128BYTES);
					
				}else if (strcmp(pszKey, "PASSWORD") == 0) {
					memset(stMysqlServer.pchPassWord, 0, _128BYTES);
					memcpy(stMysqlServer.pchPassWord, pszVal, _128BYTES);
					
				}else if (strcmp(pszKey, "DBNAME") == 0) {
					memset(stMysqlServer.pchDbName, 0, _128BYTES);
					memcpy(stMysqlServer.pchDbName, pszVal, _128BYTES);
					
				}
				
			}
		}
	}
	pszSection[0] = 0;//set section
	return true;
}

bool CUserQueryMain::BdxGetServerUrlAPI(CConf *pCConf, char *pszSection)
{
	char* pszKey = NULL;
	char* pszVal = NULL;
	//char* ptr;
	char szLine[PACKET] = { 0 };//line buffer

	char *buf;
	int  index = 0;
	char bufTemp[PACKET];
	char *outer_ptr = NULL;  
	char *inner_ptr = NULL;  
	char *temp[PACKET]; 

	QUERYAPIINFO_S sQueryInfo;
	memset(bufTemp, 0, PACKET);
	memset(temp, 0, PACKET);
	
	while (SUCCESS == pCConf->ConfGetLine(szLine, PACKET)) {
		if (!pCConf->ConfIsValid(szLine)) {
			continue;
		}
		pCConf->ConfTrim(szLine);
		if (pCConf->ConfIsSection(szLine)) {
			memset(pszSection, 0, PACKET);
			strcpy(pszSection, szLine);//get new section
			return true;
		} else {
			if (SUCCESS == pCConf->ConfKeyValue(szLine, &pszKey, &pszVal)) {
				if (strcmp(pszKey, "URLS") == 0) {
					
					buf=pszVal;
					printf("QUERY APIS=%s\n",buf);
					/*
					ptr = strtok(pszVal,";");
					while( NULL!= ptr )
					{	
						g_vecUrlAPIS.push_back(string(ptr));					
						ptr=strtok(NULL,";");

					}
					*/
					while((temp[index] = strtok_r(buf,";", &outer_ptr))!=NULL)   
					{  	
						
						memset(sQueryInfo.mParam, 0, _128BYTES);
						memset(sQueryInfo.mCarrierOperator, 0, BUF_SIZE_8BYTE);
					    buf=temp[index];
					    while((temp[index]=strtok_r(buf,",", &inner_ptr))!=NULL)   
					    {   
					    	//if( index%4 == 0 && index != 0 )
					    	if( index%5 == 4 )
					        {
					        	memcpy(sQueryInfo.mParam, temp[index-3],_128BYTES);
					        	//sQueryInfo.mProvince = atol(temp[index-2]);
					        	printf("sQueryInfo.mParam1=%s\n",string(temp[index-2]).c_str());
					        	sQueryInfo.mProvince = string(temp[index-2]);
					        	memcpy(sQueryInfo.mCarrierOperator, temp[index-1], BUF_SIZE_8BYTE);
					        	sQueryInfo.mQueryLimits = atol(temp[index]);
					        	//printf("sQueryInfo.mParam2=%s\n",sQueryInfo.mParam);
					        	//printf("sQueryInfo.mParam3=%s\n",sQueryInfo.mProvince.c_str());
					        	//printf("sQueryInfo.mParam4=%s\n",sQueryInfo.mCarrierOperator);
					        	//printf("sQueryInfo.mParam5=%ld\n",sQueryInfo.mQueryLimits);
					            g_vecUrlAPIS[temp[index-4]]=sQueryInfo;
					        }
					        index++;
					        buf=NULL;  
					    }  
					    buf=NULL;  
					}  


					
				} 		
			}
		}
	}
	pszSection[0] = 0;//set section

	std::map<std::string,QUERYAPIINFO_S>::iterator it=g_vecUrlAPIS.begin();

	g_remoteIp.assign(it->first,0,it->first.find(":",0)); 
	g_remotePort = atoi(it->first.substr(it->first.find(":",0)+1).c_str());
	g_serviceBrokerUser = std::string(it->second.mParam);
	g_serviceBrokerPass = it->second.mProvince;
	
	printf("g_serviceBrokerUser=%s,g_serviceBrokerPass=%s\n",g_serviceBrokerUser.c_str(),g_serviceBrokerPass.c_str());

	
	return true;
}

bool CUserQueryMain::BdxGetThreadsPrm(CConf *pCConf, char *pszSection)
{
	char* pszKey = NULL;
	char* pszVal = NULL;
	char szLine[PACKET] = { 0 };//line buffer

	while (SUCCESS == pCConf->ConfGetLine(szLine, PACKET)) {
		if (!pCConf->ConfIsValid(szLine)) {
			continue;
		}
		pCConf->ConfTrim(szLine);
		if (pCConf->ConfIsSection(szLine)) {
			memset(pszSection, 0, PACKET);
			strcpy(pszSection, szLine);//get new section
			return true;
		} else {
			if (SUCCESS == pCConf->ConfKeyValue(szLine, &pszKey, &pszVal)) {
				if (strcmp(pszKey, "NUM") == 0) {
					m_uiThreadsNum = atoi(pszVal);
				}
			}
		}
	}
	pszSection[0] = 0;//set section
	return true;
}


void CUserQueryMain::BdxPrintConfig()
{
	LOG(DEBUG, "LOG Parameter: [Path: %s] [File: %s] [Rank: %d] [MAXLINE: %d]", m_stLogPrms.m_strPath.c_str(),m_stLogPrms.m_strFileName.c_str(), m_stLogPrms.m_Rank, m_stLogPrms.m_uiMaxLine);
	LOG(DEBUG, "Server Info [Bjob server: %s:%d] .",m_stServerInfo.m_stRedisServer.m_pchIp, m_stServerInfo.m_stRedisServer.m_uiPort);
	LOG(DEBUG, "Work threads number: %d.", m_uiThreadsNum);
	
}


