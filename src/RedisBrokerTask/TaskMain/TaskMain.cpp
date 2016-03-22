/*
 * TaskMain.cpp
 */

#include "TaskMain.h"
#include "../../CommonTools/UrlEncode/UrlEncode.h"
#include "../../RedisBrokerWorkThreads/RedisBrokerWorkThreads.h"

#include "../../CommonTools/Base64Encode/Base64.h"
#include "../../CommonTools/Base64Encode/Base64_2.h"
#include "../../../include/json/json.h"
#include "../../../include/etcdcpp/rapid_reply.hpp"
//#include "../../../include/etcdcpp/etcd.hpp"


#include <arpa/inet.h>
#include <stdlib.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/err.h>
//#include <memory>

#define __CONTAINER__

extern CLog *gp_log;
const char* CTaskMain::m_pszHttpHeaderEnd = "\r\n\r\n";
const char* CTaskMain::m_pszHttpLineEnd = "\r\n";
const std::string CTaskMain::keyEdcpMd5Sign="edc_543_key_155&";
extern std::map<std::string,BDXPERMISSSION_S> g_mapUserInfo;
extern std::map<std::string,int> g_mapUserQueryLimit;
extern std::map<std::string,QUERYAPIINFO_S> g_vecUrlAPIS;


extern pthread_rwlock_t p_rwlock;
extern pthread_rwlockattr_t p_rwlock_attr;
extern pthread_mutex_t mutex;
extern std::string g_strTokenString ;
extern std::string ssToken;
extern u_int  g_iNeedUpdateToken ;
extern int iAPIQpsLimit;

int InitSSLFlag = 0;


std::string etcdRedisValue = "{\
	\"kind\":\"DeploymentConfig\",\
	\"apiVersion\":\"v1\",\
  \"metadata\": {\
      \"name\": \"apidocker\",\
      \"namespace\": \"chenygtest\",\
      \"selfLink\": \"/oapi/v1/namespaces/chenygtest/deploymentconfigs/apidocker\"\
  },\
	\"spec\": {\
        \"strategy\": {\
            \"type\": \"Rolling\",\
            \"rollingParams\": {\
                \"updatePeriodSeconds\": 1,\
                \"intervalSeconds\": 1,\
                \"timeoutSeconds\": 600,\
                \"maxUnavailable\": \"25%\",\
                \"maxSurge\": \"25%\"\
            },\
            \"resources\": {}\
        },\
        \"triggers\": [\
            {\
                \"type\": \"ConfigChange\"\
            }\
        ],\
        \"replicas\": 1,\
        \"selector\": {\
            \"run\": \"apidocker\"\
        },\
        \"template\": {\
            \"metadata\": {\
                \"labels\": {\
                    \"run\": \"apidocker\"\
                }\
            },\
            \"spec\": {\
                \"containers\": [\
                    {\
                        \"name\": \"apidocker\",\
                        \"image\": \"172.30.32.106:5000/chenygtest/testdocker\",\
                        \"env\": [\
                            {\
                                \"name\": \"MYSQL_PORT_3306_TCP_PORT\",\
                                \"value\": \"3306\"\
                            }\
                        ]\
                    }\
                ],\
                \"restartPolicy\": \"Always\",\
                \"terminationGracePeriodSeconds\": 30,\
                \"dnsPolicy\": \"ClusterFirst\",\
                \"securityContext\": {}\
            }\
        }\
    }\
}";


static const string http=" HTTP/1.1";

static const char http200ok[] = "HTTP/1.1 200 OK\r\nServer: Bdx LDP/0.1.0\r\nCache-Control: must-revalidate\r\nExpires: Thu, 01 Jan 1970 00:00:00 GMT\r\nPragma: no-cache\r\nConnection: Keep-Alive\r\nContent-Type: application/json;charset=UTF-8\r\nDate: ";
//static const char http200ok[] = "";
static const char httpReq[]="GET %s HTTP/1.1\r\nHost: %s\r\nAccept-Encoding: identity\r\n\r\n";

static const char redisTemplateValue[] = "daemonize yes\npidfile ./redis.%s.pid\nport %s\ntimeout 0\ntcp-keepalive 0\nloglevel notice\nlogfile stdout\ndatabases 16\nsave 900 1\nsave 300 10\nsave 60 10000\ndbfilename dump_%s.rdb\ndir ./redis/\nmaxmemory %ld\nrequirepass %s\n";


//std::string etcdIP="54.222.135.148";
//uint16_t etctPort = 2379;

extern std::string g_remoteIp;				
extern uint16_t g_remotePort;
extern std::string g_serviceBrokerUser;	
extern std::string g_serviceBrokerPass;

etcd::Client<example::RapidReply>etcd_client(getenv("ETCD_IP"),atoi(getenv("ETCD_PORT")));
std::string etcdAuthentication = getenv("ETCD_AUTH");

CTaskMain::CTaskMain(CTcpSocket* pclSock):CUserQueryTask(pclSock)
{
	// TODO Auto-generated constructor stub
	m_piKLVLen = (int*)m_pszKLVBuf;
	m_piKLVContent = m_pszKLVBuf + sizeof(int);
	*m_piKLVLen = 0;
}

CTaskMain::CTaskMain()
{

}

CTaskMain::~CTaskMain() {
	// TODO Auto-generated destructor stub

}


int CTaskMain::BdxRunTask(BDXREQUEST_S& stRequestInfo, BDXRESPONSE_S& stResponseInfo)
{
	string strErrorMsg;
	string retKey,retKeyType,retUser,retParams;
    HIVELOCALLOG_S stHiveEmptyLog;
	int iRes = 0;
	if(!m_pclSock) {
		LOG(ERROR, "[thread: %d]m_pclSock is NULL.", m_uiThreadId);
		return LINKERROR;
	}
	iRes = 	BdxGetHttpPacket(stRequestInfo,stResponseInfo);	
	if(iRes == SUCCESS )//&& !stRequestInfo.m_strUserID.empty() /*&& m_bSend*/) 
	{
		return BdxSendRespones( stRequestInfo, stResponseInfo);
	}
	else
	{
		return BdxSendEmpyRespones(stResponseInfo.ssErrorMsg);
	}
	return iRes;
}


int CTaskMain::BdxGetHttpPacket(BDXREQUEST_S& stRequestInfo,BDXRESPONSE_S &stResponseInfo)
{
	int iRes = 0;
	std::string keyCatalog = "/servicebroker/catalog/redisBroker/service_redis_broker_here";
	std::string keyLastOperation = "/servicebroker/catalog/redisBroker/instance/last_operation/";
	std::string keyProvision = "/servicebroker/catalog/redisBroker/instance/";
	std::string keyBind = "/servicebroker/catalog/redisBroker/instance/service_bindings/";
	std::string keyBroker = "/servicebroker/catalog/redisBroker/instance/service_bindings/redisbroker_info/";
	char bufTemp[PACKET];
	
	memset(bufTemp, 0, PACKET);
	memset(m_pszAdxBuf, 0, _8KBLEN);
	printf("etcdAuthentication=%s\n",etcdAuthentication.c_str());
	iRes = m_pclSock->TcpRead(m_pszAdxBuf, _8KBLEN);
	if(iRes <= (int)http.length()) 
	{		
		LOG(DEBUG, "[thread: %d]Read Socket Error [%d].", m_uiThreadId, iRes);
		return LINKERROR;
	}
	std::string ssContent = std::string(m_pszAdxBuf);
	printf("File:%s,Line:%d,ssContent=%s\n",__FILE__,__LINE__,ssContent.c_str());
	if(BdxCheckPasswordAndUsername(ssContent)!=SUCCESS)
	{
		stResponseInfo.ssErrorMsg="Authorized Failed!";
		return LINKERROR;
	}
	
	m_httpType = BdxGetRequestMethod(ssContent);
	stResponseInfo.keyCatalog = keyCatalog ; 
	stResponseInfo.keyLastOperation = keyLastOperation;
	stResponseInfo.keyProvision = keyProvision;
	stResponseInfo.keyBind = keyBind;
	stResponseInfo.keyBroker = keyBroker;
	
	printf("File:%s,Line:%d,m_httpType=%d\n",__FILE__,__LINE__,m_httpType);

	switch(m_httpType)
	{
		case CATALOG: 
				iRes = BdxCatalog(stRequestInfo,stResponseInfo);
				break;
		case PROVISION:
				iRes = BdxProvision(stRequestInfo,stResponseInfo,ssContent);
				break;
		case DEPROVISION:
				iRes = BdxDeProvision(stRequestInfo,stResponseInfo,ssContent);
				break;
		case LASTOPERATION:
				iRes = BdxLastOperation(stRequestInfo,stResponseInfo,ssContent);
				break;
		case PATCH:
				iRes = BdxUpdate(stRequestInfo,stResponseInfo,ssContent);
				break;
		case BIND:
				iRes = BdxBind(stRequestInfo,stResponseInfo,ssContent);
				break;
		case UNBIND:
				iRes = BdxUnbind(stRequestInfo,stResponseInfo,ssContent);
				break;
		default:
				printf("no match mothod.....\n");
				stRequestInfo.m_strReqContent="no match mothod.....";
				return OTHERERROR;
				break;

	}
	return iRes;
	
	}

int CTaskMain::BdxParseHttpPacket(char*& pszBody, u_int& uiBodyLen, const u_int uiParseLen)
{
	u_int uiHeadLen = 0;
	char* pszTmp = NULL;
	char* pszPacket = m_pszAdxBuf;
	if(strncmp(m_pszAdxBuf, "GET", strlen("GET"))) {
		//LOG(ERROR, "[thread: %d]It is not POST request.", m_uiThreadId);
		return PROTOERROR;
	}
	//find body
	pszTmp = strstr(pszPacket, m_pszHttpHeaderEnd);
	if(pszTmp == NULL) {
		LOG(ERROR, "[thread: %d]can not find Header End.", m_uiThreadId);
		return PROTOERROR;
	}
	pszBody = pszTmp + strlen(m_pszHttpHeaderEnd);
	uiHeadLen = pszBody - m_pszAdxBuf;

	return SUCCESS;
}

int CTaskMain::BdxParseBody(char *pszBody, u_int uiBodyLen, BDXREQUEST_S& stRequestInfo)
{

    LOG(DEBUG,"SUCCESS");
	return SUCCESS;
}



int CTaskMain::BdxSendEmpyRespones(std::string &errorMsg)
{
	m_clEmTime.TimeOff();
	std::string strOutput=errorMsg;
	char pszDataBuf[_8KBLEN];
	memset(pszDataBuf, 0, _8KBLEN);
	sprintf((char *)pszDataBuf, "%s%sContent-Length: %d\r\n\r\n", http200ok,BdxGetHttpDate().c_str(),(int)strOutput.length());
	int iHeadLen = strlen(pszDataBuf);
	
	memcpy(pszDataBuf + iHeadLen, strOutput.c_str(), strOutput.length());
	LOG(DEBUG,"Thread : %d ,AdAdxSendEmpyRespones=%s\n",m_uiThreadId,pszDataBuf);
	if(!m_pclSock->TcpWrite(pszDataBuf, iHeadLen + strOutput.length())) {
		LOG(ERROR, "[tread: %d]write empty response data error.", m_uiThreadId);
		return LINKERROR;
	}
	return SUCCESS;
}

int CTaskMain::BdxSendRespones(BDXREQUEST_S& stRequestInfo, BDXRESPONSE_S& stAdxRes)
{
	memset(m_pszAdxResponse, 0, _64KBLEN);
	if( !stAdxRes.ssErrorMsg.empty())
	{		
		std::string strOutput=stAdxRes.ssErrorMsg;
	}
	if(m_httpType)
	{
		sprintf((char *)m_pszAdxResponse, "%s%sContent-Length: %d\r\n\r\n", http200ok,BdxGetHttpDate().c_str(),(int)stRequestInfo.m_strReqContent.length());
		int iHeadLen = strlen(m_pszAdxResponse);
		memcpy(m_pszAdxResponse + iHeadLen, stRequestInfo.m_strReqContent.c_str(),stRequestInfo.m_strReqContent.length());
	}
	else
	{
		sprintf((char *)m_pszAdxResponse,"%s",stRequestInfo.m_strReqContent.c_str());
	}
	
	int iBodyLength = strlen(m_pszAdxResponse);
	iBodyLength=strlen(m_pszAdxResponse);

	if(!m_pclSock->TcpWrite(m_pszAdxResponse, iBodyLength)) 
	{

		LOG(ERROR, "[thread: %d]write  response error.", m_uiThreadId);
		return LINKERROR;
	}
	
	LOG(DEBUG, "[thread: %d]write response iBodyLength=%d.",m_uiThreadId,iBodyLength);
	
    return SUCCESS;
}


std::string CTaskMain::BdxTaskMainGetTime(const time_t ttime)
{
    time_t tmpTime1=0;
    if(ttime == 0)
    {
    	tmpTime1 = time(0);
    }
    else
    {
    	tmpTime1 = ttime;
    }
    struct tm* timeinfo1 = localtime(&tmpTime1);
    char dt1[20];
    memset(dt1, 0, 20);
    string tempDate;
    long int tmp=0;
    strftime(dt1,20,"%Y%m%d%H",timeinfo1);
    tmp=(timeinfo1->tm_year + 1900)*1000000+(timeinfo1->tm_mon+1)*10000+(timeinfo1->tm_mday)*100+(timeinfo1->tm_hour);
    sprintf(dt1,"%ld",tmp);
    return std::string(dt1);
        
}

std::string CTaskMain::BdxTaskMainGetMinute(const time_t ttime)
{

	time_t tmpTime;
	if(ttime == 0)
		tmpTime = time(0);
	else
		tmpTime = ttime;
	struct tm* timeinfo = localtime(&tmpTime);
	char dt[20];
	memset(dt, 0, 20);
	sprintf(dt, "%4d%02d%02d%02d%02d", timeinfo->tm_year + 1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min);
	return std::string(dt);
}

std::string CTaskMain::BdxTaskMainGetFullTime(const time_t ttime)
{

	time_t tmpTime;
	if(ttime == 0)
		tmpTime = time(0);
	else
		tmpTime = ttime;
	struct tm* timeinfo = localtime(&tmpTime);
	char dt[20];
	memset(dt, 0, 20);
	sprintf(dt, "%4d%02d%02d%02d%02d%02d", timeinfo->tm_year + 1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	return std::string(dt);
}
std::string CTaskMain::BdxTaskMainGetUCTime(const time_t ttime)
{

	time_t tmpTime;
	if(ttime == 0)
	{
		tmpTime = time(0);
	}
	else
	{
		tmpTime = ttime;
	}
	tmpTime -= 8*3600;
	struct tm* timeinfo = localtime(&tmpTime);
	char dt[20];
	memset(dt, 0, 20);
	sprintf(dt, "%4d-%02d-%02dT%02d:%02d:%02dZ", timeinfo->tm_year + 1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	return std::string(dt);
}

std::string CTaskMain::BdxTaskMainGetDate(const time_t ttime)
{

	time_t tmpTime;
	if(ttime == 0)
		tmpTime = time(0);
	else
		tmpTime = ttime;
	struct tm* timeinfo = localtime(&tmpTime);
	char dt[20];
	memset(dt, 0, 20);
	sprintf(dt, "%4d%02d%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon+1,timeinfo->tm_mday);
	return std::string(dt);
}

std::string CTaskMain::BdxTaskMainGetNextDate(const time_t ttime)
{

	time_t tmpTime;
	if(ttime == 0)
		tmpTime = time(0);
	else
		tmpTime = ttime;
	tmpTime+=86400;
	struct tm* timeinfo = localtime(&tmpTime);
	char dt[20];
	memset(dt, 0, 20);
	sprintf(dt, "%4d%02d%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon+1,timeinfo->tm_mday);
	return std::string(dt);
}

std::string CTaskMain::BdxTaskMainGetLastDate(const time_t ttime)
{

	time_t tmpTime;
	if(ttime == 0)
		tmpTime = time(0);
	else
		tmpTime = ttime;
	tmpTime-=86400;
	struct tm* timeinfo = localtime(&tmpTime);
	char dt[20];
	memset(dt, 0, 20);
	sprintf(dt, "%4d%02d%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon+1,timeinfo->tm_mday);
	return std::string(dt);
}

std::string CTaskMain::BdxTaskMainGetMonth(const time_t ttime)
{

	time_t tmpTime;
	if(ttime == 0)
		tmpTime = time(0);
	else
		tmpTime = ttime;
	struct tm* timeinfo = localtime(&tmpTime);
	char dt[20];
	memset(dt, 0, 20);
	sprintf(dt, "%4d%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon+1);
	return std::string(dt);
}

std::string CTaskMain::BdxGenNonce(int length) 
{
        char CHAR_ARRAY[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b','c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x','y', 'z', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H','I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T','U', 'V', 'W', 'X', 'Y', 'Z'};
        srand((int)time(0));
         
        std::string strBuffer ;
        //int nextPos = strlen(CHAR_ARRAY);
        int nextPos = sizeof(CHAR_ARRAY);
        //printf("nextPos=%d\n",nextPos);
        int tmp = 0;
        for (int i = 0; i < length; ++i) 
        { 
            tmp = rand()%nextPos;
            
            strBuffer.append(std::string(1,CHAR_ARRAY[tmp]));
        }
        return strBuffer;
}

std::string CTaskMain::GenPasswordDigest(std::string utcTime, std::string nonce, std::string appSecret)
{
		std::string strDigest;

		std::string strValue = nonce + utcTime + appSecret;

        unsigned char *dmg = mdSHA1.SHA1_Encode(strValue.c_str());
        const  char *pchTemp = (const  char *)(char*)dmg;
        //std::string strDmg = base64_encode((const unsigned char*)pchTemp,strlen(pchTemp));
        std::string strDmg = base64_encode((const unsigned char*)pchTemp,SHA_DIGEST_LENGTH);
		//std::string strDmg = base64_encode(reinterpret_cast<const char *>(static_cast<void*>(dmg)),strlen(dmg));
		free(dmg);
        return strDmg;
}

string   CTaskMain::BdxTaskMainReplace_All(string    str,   string   old_value,   string   new_value)   
{   
    while(true)   {   
        string::size_type   pos(0);   
        if(   (pos=str.find(old_value))!=string::npos )  
        	{	
        		//printf("Line:%d,str=%s\n",__LINE__,str.c_str());
        		str.replace(pos,old_value.length(),new_value);   
            }
        else   break;   
    }   
    return   str;   
}   

std::string CTaskMain::BdxGetParamSign(const std::string& strParam, const std::string& strSign)
{
	char pszMd5Hex[33];
	std::string strParamKey = strParam + strSign;
	printf("Line:%d,strParamKey=%s\n",__LINE__,strParamKey.c_str());

    //计算参数串的128位MD5
    m_clMd5.Md5Init();
    m_clMd5.Md5Update((u_char*)strParamKey.c_str(), strParamKey.length());

    u_char pszParamSign[16];
    m_clMd5.Md5Final(pszParamSign);

    //以16进制数表示
    for (unsigned char i = 0; i < sizeof(pszParamSign); i++) {
    	sprintf(&pszMd5Hex[i * 2], "%c", to_hex(pszParamSign[i] >> 4));
    	sprintf(&pszMd5Hex[i * 2 + 1], "%c", to_hex((pszParamSign[i] << 4) >> 4));
    	//sprintf(&pszMd5Hex[i * 2], "%c", to_hex(pszParamSign[i]));
    	//sprintf(&pszMd5Hex[i * 2 + 1], "%c", to_hex((pszParamSign[i] << 4)));
    }
    pszMd5Hex[32] = '\0';
    return std::string(pszMd5Hex);
}


int CTaskMain::BdxCheckRemoteServer(std::string serverIP,uint16_t serverPORT)
{

	CTcpSocket* remoteSocket;				
	int iResult = SUCCESS;

	remoteSocket=new CTcpSocket(serverPORT,serverIP);
	if(remoteSocket->TcpConnect()!=0)
	{
		remoteSocket->TcpClose();
		iResult =  LINKERROR;
	}
	remoteSocket->TcpClose();
	delete remoteSocket;
	return iResult;

}

int CTaskMain::BdxCheckPasswordAndUsername(std::string strContent)
{
	std::string strAuthString;
	strAuthString = BdxGetAuthorization(strContent);
	strAuthString  = base64_decode(strAuthString);
	printf("strAuthString=%s\n",strAuthString.c_str());
	return strAuthString.compare(g_serviceBrokerUser+":"+g_serviceBrokerPass);

}
std::string CTaskMain::BdxGetAuthorization(std::string strContent)
{
	std::string strAuthString = "Authorization";
	std::string strBasic = "Basic";
	
	unsigned int iPos = strContent.find(strAuthString,0);
	unsigned int jPos = strContent.find(strBasic,iPos);
	unsigned int kPos = strContent.find(CTRL_N,jPos);

	if ((iPos == std::string::npos)||(jPos == std::string::npos)||(kPos == std::string::npos))
	{
		strAuthString = "";
		return strAuthString;
	}
	strAuthString = strContent.substr(jPos + strBasic.length()+1,kPos - (jPos + strBasic.length()) );
	return strAuthString;
}
int CTaskMain::BdxCheckEtcdKeyIsExists(BDXRESPONSE_S& stResponseInfo,std::string serverIP,uint16_t serverPORT,std::string etcdKey)
{
	CTcpSocket* remoteSocket;	
	int iResult = EXISTS;
	char m_httpReq[_8KBLEN];
	char remoteBuffer[_8KBLEN];
	std::string hostInfo;
	char chPort[5];
	
	memset(chPort,0,5);
	sprintf(chPort,"%d",g_remotePort);
	hostInfo = g_remoteIp + ":" + std::string(chPort);
	etcdKey = "/v2/keys" + etcdKey ;
	printf("hostInfo=%s\n",hostInfo.c_str());
	if( BdxCheckRemoteServer(g_remoteIp,g_remotePort)!=SUCCESS )
	{
		stResponseInfo.ssErrorMsg = E422;
		return LINKERROR;
	}

	memset(m_httpReq, 0, _8KBLEN);
	sprintf(m_httpReq,"GET %s HTTP/1.1\r\nHost: %s\r\nAccept-Encoding: identity\r\n\r\n",etcdKey.c_str(),hostInfo.c_str());										
	printf("%s\n",m_httpReq);
	remoteSocket=new CTcpSocket(serverPORT,serverIP);
	if(remoteSocket->TcpConnect()!=0)
	{
		remoteSocket->TcpClose();
		iResult =  LINKERROR;
	}
	else
	{
		if(remoteSocket->TcpWrite(m_httpReq,strlen(m_httpReq))!=0)	
		{	
			memset(remoteBuffer,0,_8KBLEN); 	
			//remoteSocket->TcpReadAll(remoteBuffer,_8KBLEN);
			remoteSocket->TcpRead(remoteBuffer,_8KBLEN);
			if( strlen(remoteBuffer) > 0 )			
			{					
				stResponseInfo.mResValue = std::string(remoteBuffer);
				if (stResponseInfo.mResValue.find("Key not found")!=std::string::npos)
				{
					stResponseInfo.ssErrorMsg = "Key not found";
					iResult = NOTEXISTS;
				}
				printf("stResponseInfo.mResValue=%s\n",stResponseInfo.mResValue.c_str());
			}							
		 }
		 else
		 {
			iResult =  LINKERROR;
		 }
	}
	remoteSocket->TcpClose();
	delete remoteSocket;
	return iResult;

}

int CTaskMain::BdxCatalog(BDXREQUEST_S& stRequestInfo,BDXRESPONSE_S& stResponseInfo)
{
	printf("g_remotePort=%d,g_remotePort=%s\n",g_remotePort,g_remoteIp.c_str());
	if( BdxCheckRemoteServer(g_remoteIp,g_remotePort)!=SUCCESS )
	{
		stResponseInfo.ssErrorMsg = E422;
		return LINKERROR;
	}
	//etcd::Client<example::RapidReply>etcd_client(g_remoteIp, g_remotePort);
	Json::Reader *jReader= new Json::Reader(Json::Features::strictMode());
	Json::Value jValue;
	example::RapidReply reply = etcd_client.Get(stResponseInfo.keyCatalog);
	stRequestInfo.m_strReqContent = reply.ReplyToString();

	if(jReader->parse(reply.ReplyToString(), jValue))
	{
		if(jReader->parse(jValue["node"].toStyledString(), jValue))
		{	 
			#if 0
			if( jValue["value"].toStyledString() == "\"success\"\n" )//success is  provisioned,doing is not  provision
			{
				stRequestInfo.m_strReqContent = E200;
				delete jReader;
				return SUCCESS;
			}
			else
			{
				stResponseInfo.ssErrorMsg = E201;
				delete jReader;
				return OTHERERROR;
			}
			#endif
			stRequestInfo.m_strReqContent = jValue["value"].toStyledString();
			printf("Line:%d,=========================\n",__LINE__);
			stRequestInfo.m_strReqContent = BdxTaskMainReplace_All(stRequestInfo.m_strReqContent,std::string("\\"),std::string(""));

			
		}
		else
		{
			printf("File:%s,Line:%d,parse json error\n",__FILE__,__LINE__);
			stResponseInfo.ssErrorMsg = E422;
			delete jReader;
			return OTHERERROR;
		}

	}
	else
	{
		printf("File:%s,Line:%d,parse json error\n",__FILE__,__LINE__);
		stResponseInfo.ssErrorMsg = E422;
		delete jReader;
		return OTHERERROR;
	}

#if 0
	stRequestInfo.m_strReqContent="{\
\"services\": [{\
\"id\": \"service-guid-redis\",\
\"name\": \"myredis\",\
\"description\": \"A MySQL-compatible relational database\",\
\"bindable\": true,\
\"plans\": [{\
\"id\": \"plan1-free-5G\",\
\"name\": \"small\",\
\"description\": \"A small shared database with 5000mb storage quota\",\
\"free\":true\
},{\
\"id\": \"plan2-charge-20G\",\
\"name\": \"large\",\
\"description\": \"A large dedicated database with 20GB storage quota\",\
\"free\": false\
}],\
\"dashboard_client\": {\
\"id\": \"client-id-1\",\
\"secret\": \"secret-1\",\
\"redirect_uri\": \"https://myredis:port\"\
}\
}]\
}";	
#endif
	printf("File:%s,Line:%d,BdxCatalog...\n",__FILE__,__LINE__);
	return SUCCESS;
}

int CTaskMain::BdxProvision(BDXREQUEST_S& stRequestInfo,BDXRESPONSE_S& stResponseInfo,std::string &reqParams)
{
	std::string strInstanceId,strServiceId,strPlanId;
	BDXSERVICEPARAM_S sProvisionValue;
	BDXSERVICEPARAM_S sCatlogValue;
	int iPos,gStart,gEnd;

	BDXREQUESTURLINFO_S reqUrlResult = BdxGetReqUrlAndContent(reqParams);
	//etcd::Client<example::RapidReply>etcd_client(g_remoteIp, g_remotePort);
	iPos = reqUrlResult.m_ReqUrl.rfind(SLASH,reqUrlResult.m_ReqUrl.length());
	//check service id and plan id

	sProvisionValue = BdxGetProvisionParamValue(reqUrlResult.m_ReqContent);
	sCatlogValue	= BdxGetCatlogParamValue(stResponseInfo);
	if(sProvisionValue.mServiceId.empty()||sProvisionValue.mPlanId.empty()||sCatlogValue.mPlanId.empty()||sCatlogValue.mServiceId.empty())
	{
		stResponseInfo.ssErrorMsg = E422; // etcd is someproblem
		printf("Line:%d,catlog plan is not null \n",__LINE__);
		return LINKERROR;
	}

	if((sCatlogValue.mPlanId.find(sProvisionValue.mPlanId)==std::string::npos)
	||(sCatlogValue.mPlanId.find(sProvisionValue.mPlanId)==std::string::npos))	
	{
		stResponseInfo.ssErrorMsg = E422; // etcd is someproblem
		printf("Line:%d,catlog plan is not match with provision plan\n",__LINE__);
		return LINKERROR;		
	}

	//get redis memory
	gStart   = sProvisionValue.mPlanId.rfind('-',sProvisionValue.mPlanId.length());
	gEnd = sProvisionValue.mPlanId.rfind('G',sProvisionValue.mPlanId.length());
	stRequestInfo.m_strReqContent= sProvisionValue.mPlanId.substr(gStart + 1,gEnd - gStart + 1);

	printf("Line:%d,Redis Memory stRequestInfo.m_strReqContent=%s\n",__LINE__,stRequestInfo.m_strReqContent.c_str());
	
	strInstanceId = reqUrlResult.m_ReqUrl.substr(iPos + 1);
	stResponseInfo.keyLastOperation = stResponseInfo.keyLastOperation + strInstanceId;
	if( BdxCheckRemoteServer(g_remoteIp,g_remotePort)!=SUCCESS )
	{
		stResponseInfo.ssErrorMsg = E422; // etcd is someproblem
		return LINKERROR;
	}
	if (BdxCheckEtcdKeyIsExists(stResponseInfo,g_remoteIp,g_remotePort,stResponseInfo.keyLastOperation) == OTHERERROR )
	{	
		stResponseInfo.ssErrorMsg = E422; // etcd is someproblem
		return LINKERROR;
	}
	if (BdxCheckEtcdKeyIsExists(stResponseInfo,g_remoteIp,g_remotePort,stResponseInfo.keyLastOperation) == NOTEXISTS  )
	{
		etcd_client.Set(stResponseInfo.keyLastOperation,"doing");
		#ifndef __CONTAINER__
			BdxGenRedisTemplate(stRequestInfo,stResponseInfo,strInstanceId);
			stResponseInfo.keyProvision = stResponseInfo.keyProvision + strInstanceId;
			etcd_client.Set(stResponseInfo.keyProvision,reqUrlResult.m_ReqContent);
			etcd_client.Set(stResponseInfo.keyLastOperation,"success");
			stRequestInfo.m_strReqContent = E200;
		#else
			BdxGenRedisTemplateContainer(stRequestInfo,stResponseInfo,strInstanceId);
			stResponseInfo.keyProvision = stResponseInfo.keyProvision + strInstanceId;
			etcd_client.Set(stResponseInfo.keyProvision,reqUrlResult.m_ReqContent);
			etcd_client.Set(stResponseInfo.keyLastOperation,"success");
			stRequestInfo.m_strReqContent = E200;
		#endif
	}
	else
	{
		stResponseInfo.ssErrorMsg = E409;
		return LINKERROR;
	}
	printf("File:%s,Line:%d,BdxProvision...\n",__FILE__,__LINE__);
	//delete jReader;
	return SUCCESS;
}
int CTaskMain::BdxDeProvision(BDXREQUEST_S& stRequestInfo,BDXRESPONSE_S& stResponseInfo,std::string &reqParams)
{
	std::string strInstanceId,strKeyProvision,strKeyRedisTemplate;
	int iPos;
	//Json::Reader *jReader= new Json::Reader(Json::Features::strictMode());
	Json::Value jValue;
	BDXREQUESTURLINFO_S reqUrlResult = BdxGetReqUrlAndContent(reqParams);
	//etcd::Client<example::RapidReply>etcd_client(g_remoteIp, g_remotePort);
	iPos = reqUrlResult.m_ReqUrl.rfind(SLASH,reqUrlResult.m_ReqUrl.length());
	strInstanceId = reqUrlResult.m_ReqUrl.substr(iPos + 1);
	stResponseInfo.keyLastOperation = stResponseInfo.keyLastOperation + strInstanceId;


	if( BdxCheckRemoteServer(g_remoteIp,g_remotePort)!=SUCCESS )
	{
		stResponseInfo.ssErrorMsg = E422; // etcd is someproblem
		return LINKERROR;
	}
	if (BdxCheckEtcdKeyIsExists(stResponseInfo,g_remoteIp,g_remotePort,stResponseInfo.keyLastOperation) == OTHERERROR )
	{	
		stResponseInfo.ssErrorMsg = E422; // etcd is someproblem
		return LINKERROR;
	}
	if (BdxCheckEtcdKeyIsExists(stResponseInfo,g_remoteIp,g_remotePort,stResponseInfo.keyLastOperation) == EXISTS  )
	{
		strKeyRedisTemplate = stResponseInfo.keyProvision +"redisTemplate/" + strInstanceId;
		strKeyProvision = stResponseInfo.keyProvision + strInstanceId;
		etcd_client.Delete(strKeyRedisTemplate);
		etcd_client.Delete(strKeyProvision);
		etcd_client.Delete(stResponseInfo.keyLastOperation);
		stRequestInfo.m_strReqContent = E200;
	}
	else
	{
		stResponseInfo.ssErrorMsg = E422; // etcd is someproblem  or key is not exists
		return LINKERROR;
	}

	// ɾ������,ɱ������
	printf("File:%s,Line:%d,BdxDeProvision...\n",__FILE__,__LINE__);
	//delete jReader;
	return SUCCESS;
}
int CTaskMain::BdxLastOperation(BDXREQUEST_S& stRequestInfo,BDXRESPONSE_S& stResponseInfo,std::string &reqParams)
{
	//printf("reqParams=%s\n",reqParams.c_str());
	std::string strInstanceId;
	int iPos,jPos;
	Json::Reader *jReader= new Json::Reader(Json::Features::strictMode());
	Json::Value jValue;
	BDXREQUESTURLINFO_S reqUrlResult = BdxGetReqUrlAndContent(reqParams);
	//etcd::Client<example::RapidReply>etcd_client(g_remoteIp, g_remotePort);
	iPos = reqUrlResult.m_ReqUrl.rfind(SLASH,reqUrlResult.m_ReqUrl.length());
	jPos = reqUrlResult.m_ReqUrl.rfind(SLASH,iPos - 1);
	strInstanceId = reqUrlResult.m_ReqUrl.substr(jPos + 1,iPos - jPos -1);
	stResponseInfo.keyLastOperation = stResponseInfo.keyLastOperation + strInstanceId;
	stResponseInfo.keyProvision = stResponseInfo.keyProvision + strInstanceId;

	if( BdxCheckRemoteServer(g_remoteIp,g_remotePort)!=SUCCESS )
	{
		stResponseInfo.ssErrorMsg = E422; // etcd is someproblem
		delete jReader;
		return LINKERROR;
	}
	if (BdxCheckEtcdKeyIsExists(stResponseInfo,g_remoteIp,g_remotePort,stResponseInfo.keyLastOperation) == OTHERERROR )
	{	
		stResponseInfo.ssErrorMsg = E422; // etcd is someproblem
		delete jReader;
		return LINKERROR;
	}
	if (BdxCheckEtcdKeyIsExists(stResponseInfo,g_remoteIp,g_remotePort,stResponseInfo.keyLastOperation) == EXISTS  )
	{
		example::RapidReply replyGetLastOperation= etcd_client.Get(stResponseInfo.keyLastOperation);
		//stRequestInfo.m_strReqContent = replyGetLastOperation.ReplyToString();
		replyGetLastOperation.Print();
		if(jReader->parse(replyGetLastOperation.ReplyToString(), jValue))
		{
			if(jReader->parse(jValue["node"].toStyledString(), jValue))
			{	 
				if( jValue["value"].toStyledString() == "\"success\"\n" )//success is  provisioned,doing is not  provision
				{
					stRequestInfo.m_strReqContent = E200;
					delete jReader;
					return SUCCESS;
				}
				else
				{
					stResponseInfo.ssErrorMsg = E201;
					delete jReader;
					return OTHERERROR;
				}
			}
			else
			{
				printf("File:%s,Line:%d,parse json error\n",__FILE__,__LINE__);
				stResponseInfo.ssErrorMsg = E422;
				delete jReader;
				return OTHERERROR;
			}
		
		}
		else
		{
			printf("File:%s,Line:%d,parse json error\n",__FILE__,__LINE__);
			stResponseInfo.ssErrorMsg = E422;
			delete jReader;
			return OTHERERROR;
		}
	
   	 }
	printf("File:%s,Line:%d,BdxLastOperation...\n",__FILE__,__LINE__);
	delete jReader;
	return SUCCESS;
}
int CTaskMain::BdxUpdate(BDXREQUEST_S& stRequestInfo,BDXRESPONSE_S& stResponseInfo,std::string &reqParams)
{
	printf("File:%s,Line:%d,BdxUpdate...\n",__FILE__,__LINE__);
	return SUCCESS;
}
int CTaskMain::BdxBind(BDXREQUEST_S& stRequestInfo,BDXRESPONSE_S& stResponseInfo,std::string &reqParams)
{
	std::string strInstanceId,strBindId,strBindInfoId,strRedisTemplate,strBindInfo;
	int iPos,jPos,kPos;
	BDXREDISHOSTINFO_S redisHostInfo;
	//Json::Reader *jReader= new Json::Reader(Json::Features::strictMode());
	Json::Value jValue;
	BDXREQUESTURLINFO_S reqUrlResult = BdxGetReqUrlAndContent(reqParams);
	//etcd::Client<example::RapidReply>etcd_client(g_remoteIp, g_remotePort);
	
	iPos = reqUrlResult.m_ReqUrl.rfind(SLASH,reqUrlResult.m_ReqUrl.length());
	strBindId = reqUrlResult.m_ReqUrl.substr(iPos + 1);

	jPos = reqUrlResult.m_ReqUrl.rfind(SLASH,iPos - 17);//service_bindings length is 17
	kPos = reqUrlResult.m_ReqUrl.rfind(SLASH,jPos - 1);
	strInstanceId = reqUrlResult.m_ReqUrl.substr(kPos+1,jPos-kPos-1);
	
	printf("Line:%d,jPos=%d,kPos=%d\n",__LINE__,jPos,kPos);
	printf("Line:%d,strInstanceId=%s\n",__LINE__,strInstanceId.c_str());
	
	stResponseInfo.keyBind = stResponseInfo.keyBind + strBindId;
	//stResponseInfo.keyProvision = stResponseInfo.keyProvision + strInstanceId;

	strRedisTemplate = stResponseInfo.keyProvision + "redisTemplate/" + strInstanceId;
	strBindInfoId = stResponseInfo.keyBroker + strBindId;
	strInstanceId = stResponseInfo.keyProvision + strInstanceId;
	printf("Line:%d,strInstanceId=%s\n",__LINE__,strInstanceId.c_str());
	
	if( BdxCheckRemoteServer(g_remoteIp,g_remotePort)!=SUCCESS )
	{
		stResponseInfo.ssErrorMsg = E422; // etcd is someproblem
		return LINKERROR;
	}
	if (BdxCheckEtcdKeyIsExists(stResponseInfo,g_remoteIp,g_remotePort,strInstanceId) == OTHERERROR )
	{	
		stResponseInfo.ssErrorMsg = E422; // etcd is someproblem
		return LINKERROR;
	}

	if (BdxCheckEtcdKeyIsExists(stResponseInfo,g_remoteIp,g_remotePort,strInstanceId) == NOTEXISTS )
	{	
		stResponseInfo.ssErrorMsg = E422; // etcd is someproblem
		return LINKERROR;
	}

	if (BdxCheckEtcdKeyIsExists(stResponseInfo,g_remoteIp,g_remotePort,stResponseInfo.keyBind) == OTHERERROR )
	{	
		stResponseInfo.ssErrorMsg = E422; // etcd is someproblem
		return LINKERROR;
	}
	if (BdxCheckEtcdKeyIsExists(stResponseInfo,g_remoteIp,g_remotePort,stResponseInfo.keyBind) == NOTEXISTS  )
	{
		example::RapidReply replySetBind  = etcd_client.Set(stResponseInfo.keyBind,reqUrlResult.m_ReqContent);
		example::RapidReply replyGetRedisInstanceInfo = etcd_client.Get(strRedisTemplate);
		stRequestInfo.m_strReqContent = replyGetRedisInstanceInfo.ReplyToString();
		redisHostInfo = BdxGetHostInfo(stRequestInfo.m_strReqContent);
		strBindInfo = "{\"credentials\":{\"uri\":\"\",\"username\":\"\",\"password\":\"" + redisHostInfo.mPassWord + "\",\"host\":\"" + redisHostInfo.mHostInfo +"\",\"port\":\"" + redisHostInfo.mPort +"\",\"database\":\"\"}}";			
		example::RapidReply replySetBindInfo  = etcd_client.Set(strBindInfoId,strBindInfo);
		std::string strCmd ="./redis-server " +  redisHostInfo.mFileName;
		system(strCmd.c_str());
		stRequestInfo.m_strReqContent = strBindInfo;
		//stRequestInfo.m_strReqContent = E200;  //replyGetRedisBrokerInfo.ReplyToString();
		//example::RapidReply replyGetRedisBrokerInfo = etcd_client.Get(stResponseInfo.keyBroker);
		//stRequestInfo.m_strReqContent = replyGetRedisBrokerInfo.ReplyToString();		
	}
	else
	{
		stResponseInfo.ssErrorMsg = E409; // etcd is someproblem
		return LINKERROR;
	}
	printf("File:%s,Line:%d,BdxBind...\n",__FILE__,__LINE__);
	//delete jReader;
	return SUCCESS;
}
int CTaskMain::BdxUnbind(BDXREQUEST_S& stRequestInfo,BDXRESPONSE_S& stResponseInfo,std::string &reqParams)
{
	std::string strInstanceId,strBindId,strBindInfoId,strRedisTemplate,strBindInfo;
	int iPos,jPos,kPos;
	BDXREDISHOSTINFO_S redisHostInfo;
	//Json::Reader *jReader= new Json::Reader(Json::Features::strictMode());
	Json::Value jValue;
	BDXREQUESTURLINFO_S reqUrlResult = BdxGetReqUrlAndContent(reqParams);
	//etcd::Client<example::RapidReply>etcd_client(g_remoteIp, g_remotePort);
	
	iPos = reqUrlResult.m_ReqUrl.rfind(SLASH,reqUrlResult.m_ReqUrl.length());
	strBindId = reqUrlResult.m_ReqUrl.substr(iPos + 1);

	jPos = reqUrlResult.m_ReqUrl.rfind(SLASH,iPos - 17);//service_bindings length is 17
	kPos = reqUrlResult.m_ReqUrl.rfind(SLASH,jPos - 1);
	strInstanceId = reqUrlResult.m_ReqUrl.substr(kPos+1,jPos-kPos-1);
	
	printf("Line:%d,jPos=%d,kPos=%d\n",__LINE__,jPos,kPos);
	printf("Line:%d,strInstanceId=%s\n",__LINE__,strInstanceId.c_str());
	
	stResponseInfo.keyBind = stResponseInfo.keyBind + strBindId;
	//stResponseInfo.keyProvision = stResponseInfo.keyProvision + strInstanceId;

	strRedisTemplate = stResponseInfo.keyProvision + "redisTemplate/" + strInstanceId;
	strBindInfoId = stResponseInfo.keyBroker + strBindId;
	strInstanceId = stResponseInfo.keyProvision + strInstanceId;
	printf("Line:%d,strInstanceId=%s\n",__LINE__,strInstanceId.c_str());
	
	if( BdxCheckRemoteServer(g_remoteIp,g_remotePort)!=SUCCESS )
	{
		stResponseInfo.ssErrorMsg = E422; // etcd is someproblem
		return LINKERROR;
	}
	if (BdxCheckEtcdKeyIsExists(stResponseInfo,g_remoteIp,g_remotePort,strInstanceId) == OTHERERROR )
	{	
		stResponseInfo.ssErrorMsg = E422; // etcd is someproblem
		return LINKERROR;
	}

	if (BdxCheckEtcdKeyIsExists(stResponseInfo,g_remoteIp,g_remotePort,strInstanceId) == NOTEXISTS )
	{	
		stResponseInfo.ssErrorMsg = E422; // etcd is someproblem
		return LINKERROR;
	}

	if (BdxCheckEtcdKeyIsExists(stResponseInfo,g_remoteIp,g_remotePort,stResponseInfo.keyBind) == OTHERERROR )
	{	
		stResponseInfo.ssErrorMsg = E422; // etcd is someproblem
		return LINKERROR;
	}
	if (BdxCheckEtcdKeyIsExists(stResponseInfo,g_remoteIp,g_remotePort,stResponseInfo.keyBind) == EXISTS  )
	{
		example::RapidReply replyDeleteBind  = etcd_client.Delete(stResponseInfo.keyBind);
		example::RapidReply replyGetRedisInstanceInfo = etcd_client.Get(strRedisTemplate);
		stRequestInfo.m_strReqContent = replyGetRedisInstanceInfo.ReplyToString();
		redisHostInfo = BdxGetHostInfo(stRequestInfo.m_strReqContent);
		//strBindInfo = "{\"credentials\":{\"uri\":\"\",\"username\":\"\",\"password\":\"" + redisHostInfo.mPassWord + "\",\"host\":\"" + redisHostInfo.mHostInfo +"\",\"port\":\"" + redisHostInfo.mPort +"\",\"database\":\"\"}}";			
		example::RapidReply replyDeleteBindInfo  = etcd_client.Delete(strBindInfoId);
		std::string strCmd ="./unBindServiceBroker.sh " +  redisHostInfo.mPort;
		system(strCmd.c_str());
		stRequestInfo.m_strReqContent = E200;  //replyGetRedisBrokerInfo.ReplyToString();
		//example::RapidReply replyGetRedisBrokerInfo = etcd_client.Get(stResponseInfo.keyBroker);
		//stRequestInfo.m_strReqContent = replyGetRedisBrokerInfo.ReplyToString();		
	}
	else
	{
		stResponseInfo.ssErrorMsg = E409; // etcd is someproblem
		return LINKERROR;
	}
	printf("File:%s,Line:%d,BdxUnBind...\n",__FILE__,__LINE__);
	//delete jReader;
	return SUCCESS;
}


int CTaskMain::BdxGetRequestMethod(std::string &reqParams)
{
	m_httpUri = BdxGetRequestURI(reqParams);
	if (strcasecmp(reqParams.substr(0,reqParams.find(BLANK,0)).c_str(),REQ_TYPE_GET)== 0)
	{	
		if( m_httpUri == 1 )
			return CATALOG;
		if( m_httpUri == 2 )
			return LASTOPERATION;	
	}
	if (strcasecmp(reqParams.substr(0,reqParams.find(BLANK,0)).c_str(),REQ_TYPE_PUT)== 0)
	{
		if( m_httpUri == 4 )
			return PROVISION;
		if( m_httpUri == 3 )
			return BIND;	
	}
	if (strcasecmp(reqParams.substr(0,reqParams.find(BLANK,0)).c_str(),REQ_TYPE_DELETE)== 0)
	{
		if( m_httpUri == 4 )
			return DEPROVISION;
		if( m_httpUri == 3 )
			return UNBIND;	

	}
	if (strcasecmp(reqParams.substr(0,reqParams.find(BLANK,0)).c_str(),REQ_TYPE_PATCH)== 0)
	{
		if( m_httpUri == 4 )
			return PATCH;
	}
	return OTHERERROR;
	
}

int CTaskMain::BdxGetRequestURI(std::string &reqParams)
{	

	/*
	if (reqParams.substr(reqParams.find(BLANK,0),reqParams.find(CTRL_N,0)).find("/v2/catalog")!= std::string::npos)
	{
		return TYPE_GET; // 1 is stand for catalog
	}
	if (reqParams.substr(reqParams.find(BLANK,0),reqParams.find(CTRL_N,0)).find("/last_operation")!= std::string::npos)
	{
		return TYPE_PUT; // 2 is stand for lastOperation
	}
	if (reqParams.substr(reqParams.find(BLANK,0),reqParams.find(CTRL_N,0)).find("/service_bindings")!= std::string::npos)
	{
		return TYPE_DELETE; // 3 is stand for bind or unbind
	}
	if (reqParams.substr(reqParams.find(BLANK,0),reqParams.find(CTRL_N,0)).find("/v2/service_instances")!= std::string::npos)
	{
		return TYPE_PATCH; // 4 is stand for provision or deprovision or patch
	}
	*/
	if (reqParams.substr(reqParams.find(BLANK,0),reqParams.find(CTRL_N,0)).find("/v2/catalog")!= std::string::npos)
	{
		return TYPE_GET; // 1 is stand for catalog
	}
	if (reqParams.substr(reqParams.find(BLANK,0),reqParams.find(CTRL_N,0)).find("/last_operation")!= std::string::npos)
	{
		return TYPE_PUT; // 2 is stand for lastOperation
	}
	if (reqParams.substr(reqParams.find(BLANK,0),reqParams.find(CTRL_N,0)).find("/service_bindings")!= std::string::npos)
	{
		return TYPE_DELETE; // 3 is stand for bind or unbind
	}
	if (reqParams.substr(reqParams.find(BLANK,0),reqParams.find(CTRL_N,0)).find("/v2/service_instances")!= std::string::npos)
	{
		return TYPE_PATCH; // 4 is stand for provision or deprovision or patch
	}

	return OTHERERROR;
	
}

BDXREQUESTURLINFO_S CTaskMain::BdxGetReqUrlAndContent(std::string &reqParams)
{
	BDXREQUESTURLINFO_S reqResult;
	int iFirstBlank = reqParams.find(BLANK,0);
	int jSecondBlank = reqParams.find(BLANK,iFirstBlank + 1);
	int mFirst2Return = reqParams.find(CTRL_N_N,0);// find /n/n
	
	reqResult.m_ReqUrl = reqParams.substr(iFirstBlank + 1,jSecondBlank - iFirstBlank -1);
	reqResult.m_ReqContent = reqParams.substr(mFirst2Return + 4);
	
	return reqResult;
}




int CTaskMain::BdxGenRedisTemplate(BDXREQUEST_S stRequestInfo,BDXRESPONSE_S stResponseInfo,std::string reqParams)
{
	std::string strInstanceId;
	//int iPos;
	//Json::Reader *jReader= new Json::Reader(Json::Features::strictMode());
	Json::Value jValue;
	//etcd::Client<example::RapidReply>etcd_client(g_remoteIp, g_remotePort);
	char m_httpReq[_8KBLEN];
	
	srand((int)time(0)+123456789);
	int randomInt = (rand()%(5000-3000))+3000;	
	//(rand() % (b-a))+ a
	printf("randomInt=%d\n",randomInt);
	char randomchar[5];
	memset(randomchar,0,5);
	sprintf(randomchar,"%d",randomInt);

	std::string strRedisPass,statusDir;
	strRedisPass = BdxGenNonce(20); 

	strInstanceId = reqParams;
	stResponseInfo.keyProvision = stResponseInfo.keyProvision + "redisTemplate/" + strInstanceId;
	memset(m_httpReq, 0, _8KBLEN);
	sprintf(m_httpReq,redisTemplateValue,randomchar,randomchar,randomchar,10240000L,strRedisPass.c_str());

	statusDir = "./redis";//_"+std::string(randomchar)+".conf";
	if(!m_clFile.FileBeExists(statusDir.c_str())) 
	{
		m_clFile.FileCreatDir(statusDir.c_str());
	}

	statusDir= statusDir + "/redis." +std::string(randomchar)+".conf";
	etcdRedisValue = "{\"port\":\"" + std::string(randomchar)+ "\",\"pass\":\"" + strRedisPass + "\",\"filename\":\"" + statusDir + "\",\"bing\":\"0\"}";
	//store redis port and pass,and bining info
	etcd_client.Set(stResponseInfo.keyProvision,etcdRedisValue);
	
	m_pFile = fopen(statusDir.c_str(), "a");
	fprintf(m_pFile,"%s",m_httpReq);
	fflush(m_pFile);

	if(m_pFile){
		fclose(m_pFile);
		printf("closing file\n");
		m_pFile = NULL;
	}

	printf("File:%s,Line:%d,Create Redis Template...\n",__FILE__,__LINE__);
	//delete jReader;
	return SUCCESS;
}

int CTaskMain::BdxGenRedisTemplateContainer(BDXREQUEST_S stRequestInfo,BDXRESPONSE_S stResponseInfo,std::string reqParams)
{
	std::string strInstanceId;
	//int iPos;
	//Json::Reader *jReader= new Json::Reader(Json::Features::strictMode());
	Json::Value jValue;
	//etcd::Client<example::RapidReply>etcd_client(g_remoteIp, g_remotePort);

	//std::string iCapcity = 1024000;
	char randomchar[20];
	memset(randomchar,0,20);
	sprintf(randomchar,"%ld",atoi(stRequestInfo.m_strReqContent.c_str())*1024*1024L);
	std::string iCapcity = std::string(randomchar);
	
	etcdRedisValue = BdxTaskMainReplace_All(etcdRedisValue,std::string("API_NAME"),strInstanceId);
	etcdRedisValue = BdxTaskMainReplace_All(etcdRedisValue,std::string("MEMORY_CAPCITY"),iCapcity);
	//store redis port and pass,and bining info
	etcd_client.Set(stResponseInfo.keyProvision,etcdRedisValue);

	printf("File:%s,Line:%d,Create Redis Template...\n",__FILE__,__LINE__);
	//delete jReader;
	return SUCCESS;
}

int CTaskMain::BdxDelRedisTemplate(BDXREQUEST_S stRequestInfo,BDXRESPONSE_S stResponseInfo,std::string reqParams)
{
	std::string strInstanceId;
	//int iPos;
	//Json::Reader *jReader= new Json::Reader(Json::Features::strictMode());
	Json::Value jValue;
	//etcd::Client<example::RapidReply>etcd_client(g_remoteIp, g_remotePort);
	char m_httpReq[_8KBLEN];
	
	srand((int)time(0)+123456789);
	int randomInt = (rand()%(5000-3000))+3000;	
	//(rand() % (b-a))+ a
	printf("randomInt=%d\n",randomInt);
	char randomchar[5];
	memset(randomchar,0,5);
	sprintf(randomchar,"%d",randomInt);

	std::string strRedisPass,statusDir;
	strRedisPass = BdxGenNonce(20); 

	strInstanceId = reqParams;
	stResponseInfo.keyProvision = stResponseInfo.keyProvision + "redisTemplate/" + strInstanceId;
	memset(m_httpReq, 0, _8KBLEN);
	sprintf(m_httpReq,redisTemplateValue,randomchar,randomchar,randomchar,10240000L,strRedisPass.c_str());

	statusDir = "./redis";//_"+std::string(randomchar)+".conf";
	if(!m_clFile.FileBeExists(statusDir.c_str())) 
	{
		m_clFile.FileCreatDir(statusDir.c_str());
	}

	statusDir= statusDir + "/redis." +std::string(randomchar)+".conf";
	etcdRedisValue = "{\"port\":\"" + std::string(randomchar)+ "\",\"pass\":\"" + strRedisPass + "\",\"filename\":\"" + statusDir + "\",\"bing\":\"0\"}";
	//store redis port and pass,and bining info
	etcd_client.Set(stResponseInfo.keyProvision,etcdRedisValue);
	
	m_pFile = fopen(statusDir.c_str(), "w");
	fprintf(m_pFile,"%s",m_httpReq);
	fflush(m_pFile);

	if(m_pFile){
		fclose(m_pFile);
		printf("closing file\n");
		m_pFile = NULL;
	}

	printf("File:%s,Line:%d,Create Redis Template...\n",__FILE__,__LINE__);
	//delete jReader;
	return SUCCESS;
}

BDXSERVICEPARAM_S CTaskMain::BdxGetCatlogParamValue(BDXRESPONSE_S& stResponseInfo)
{
	Json::Reader *jReader= new Json::Reader(Json::Features::strictMode());
	Json::Value jValue,jValue2;
	std::string strCatlogValue;
	unsigned int iArraySize,iArraySize2;
	BDXSERVICEPARAM_S mCatlogValue;
	example::RapidReply reply = etcd_client.Get(stResponseInfo.keyCatalog);
	if(jReader->parse(reply.ReplyToString(), jValue))
	{
		if(jReader->parse(jValue["node"].toStyledString(), jValue))
		{	 
			strCatlogValue = jValue["value"].toStyledString();
			strCatlogValue = BdxTaskMainReplace_All(strCatlogValue,std::string("\\"),std::string(""));	
			if(jReader->parse(strCatlogValue,jValue))
			{	
				if(!jValue["services"].isNull())
				{	
					if(jReader->parse(jValue["services"].asString(), jValue))
					{
						for(iArraySize=0;iArraySize < jValue.size();iArraySize++)
						{
							mCatlogValue.mServiceId = jValue[iArraySize]["id"].asString()+":" +mCatlogValue.mServiceId;
							if(jReader->parse(jValue[iArraySize]["plans"].asString(), jValue2))
							{
								for(iArraySize2=0;iArraySize2 < jValue.size();iArraySize2++)
								{

									mCatlogValue.mPlanId =jValue2[iArraySize2]["id"].asString()+":" +mCatlogValue.mPlanId;
									
								}
							}
						}
					}
				}
			}

		}
	}
	
	delete jReader;
	return mCatlogValue;
}
BDXSERVICEPARAM_S CTaskMain::BdxGetProvisionParamValue(std::string &reqParams)
{
	Json::Reader *jReader= new Json::Reader(Json::Features::strictMode());
	Json::Value jValue;
	BDXSERVICEPARAM_S sProvisionValue;
	if(jReader->parse(reqParams,jValue))
	{
		if(!jValue["service_id"].isNull() && !jValue["plan_id"].isNull())
		{
			sProvisionValue.mServiceId = jValue["service_id"].asString();
			sProvisionValue.mPlanId	 = jValue["plan_id"].asString();		
		}
	}

	delete jReader;
	return sProvisionValue;
}

BDXSERVICEPARAM_S CTaskMain::BdxGetBindParamValue(std::string &reqParams)
{
	BDXSERVICEPARAM_S mBindParamValue;
	return mBindParamValue;
}

BDXREDISHOSTINFO_S CTaskMain::BdxGetHostInfo(std::string &reqParams)
{
	Json::Reader *jReader= new Json::Reader(Json::Features::strictMode());
	Json::Value jValue;
	int iPos,jPos;
	//std::string  strHostInfo,strPort,strPass;
	BDXREDISHOSTINFO_S strRedisHostInfo;
	std::string strCredentials;

	strRedisHostInfo.mHostInfo = getenv("ETCD_IP");		
	if(jReader->parse(reqParams, jValue))
	{
		printf("ttttttttttttt\n");
		if(jReader->parse(jValue["node"].toStyledString(), jValue))
		{	 
			printf("ttttttttttttt\n");
			strCredentials = jValue["value"].toStyledString();
			iPos = strCredentials.find("\"",0);
			jPos = strCredentials.rfind("\"",strCredentials.length());
			strCredentials = strCredentials.substr(iPos+1,jPos - iPos-1);
			strCredentials = BdxTaskMainReplace_All(strCredentials,std::string("\\"),std::string(""));
			printf("strCredentials=%s\n",strCredentials.c_str());
			if(jReader->parse(strCredentials,jValue))
			{
				printf("ttttttttttttt\n");
				strRedisHostInfo.mPort = jValue["port"].toStyledString();
				strRedisHostInfo.mPort = BdxTaskMainReplace_All(strRedisHostInfo.mPort,std::string("\""),std::string(""));
				strRedisHostInfo.mPort = BdxTaskMainReplace_All(strRedisHostInfo.mPort,std::string("\n"),std::string(""));
				printf("strRedisHostInfo.mPort=%s\n",strRedisHostInfo.mPort.c_str());
				strRedisHostInfo.mPassWord = jValue["pass"].toStyledString();
				strRedisHostInfo.mPassWord = BdxTaskMainReplace_All(strRedisHostInfo.mPassWord,std::string("\""),std::string(""));
				strRedisHostInfo.mPassWord = BdxTaskMainReplace_All(strRedisHostInfo.mPassWord,std::string("\n"),std::string(""));
				strRedisHostInfo.mFileName = jValue["filename"].toStyledString();
				strRedisHostInfo.mFileName = BdxTaskMainReplace_All(strRedisHostInfo.mFileName,std::string("\""),std::string(""));
				strRedisHostInfo.mFileName = BdxTaskMainReplace_All(strRedisHostInfo.mFileName,std::string("\n"),std::string(""));
	
			}
		}		
	}
return strRedisHostInfo;
}


