/*
 * TaskMain.cpp
 */

#include "TaskMain.h"
#include "../../CommonTools/UrlEncode/UrlEncode.h"
#include "../../RedisBrokerWorkThreads/RedisBrokerWorkThreads.h"

#include "../../CommonTools/Base64Encode/Base64.h"
#include "../../CommonTools/Base64Encode/Base64_2.h"
#include "../../../include/json/json.h"
#include <arpa/inet.h>
#include <stdlib.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/err.h>
//#include <memory>


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

static const string http=" HTTP/1.1";

static const char http200ok[] = "HTTP/1.1 200 OK\r\nServer: Bdx DMP/0.1.0\r\nCache-Control: must-revalidate\r\nExpires: Thu, 01 Jan 1970 00:00:00 GMT\r\nPragma: no-cache\r\nConnection: Keep-Alive\r\nContent-Type: application/json;charset=UTF-8\r\nDate: ";
//static const char http200ok[] = "";
static const char httpReq[]="GET %s HTTP/1.1\r\nHost: %s\r\nAccept-Encoding: identity\r\n\r\n";

#define __EXPIRE__


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
	LOG(DEBUG,"BdxGetHttpPacket iRes=%d",iRes);
	//printf("strErrorMsg=%s\n",strErrorMsg.c_str());
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
	//int iReqMethod = 0;
	std::string errorMsg;
	int iQueryCategory = 1;
	m_httpType = 0; //local
	bool bUpdateDatabase = false;
	bool bQueryUser = false;
	bool bQueryGoods = false;
	int iNoDataFlag = 0,isQueryAction = 0;
	std::string	
	ssUser,ssValue,ssKey,ssmoidValue,strUser,filterDate,strToken,strKey,strKeyType,strKeyFilter,tempstrKeyFilter,strShopId,strGoodsId,strProvince,strOperator,strRetKey,strMoId;
	std::string strProvinceReq,strProvinceRes,strProvinceEmptyRes,strProvinceResTag,strOperatorName;
	std::map<std::string,std::string> map_UserValueKey;
	std::map<std::string,std::string>::iterator iter2;
	std::map<std::string,BDXPERMISSSION_S>::iterator iter;
	std::vector<std::string>::iterator itRights;
	//int iIncludeNoFields = 0;
	//int mapActionFilterSize;
	std::map<std::string,int> mapActionFilter;
	std::string 
	mResUserAttributes,mResUserAttributes2,strTempValue,strCurrentHour;
	Json::Value jValue,jRoot,jResult,jTemp;
	int lenStrTemp;
	Json::FastWriter jFastWriter;
	int isExpire = 0, iIsLocal=0,iRemoteApiFlag = 0;
	string strCreateTime,strCreateTime2,strLastDataTime,strLastDataTime2,mResValueLocal,mResValueRemote;
	struct tm ts;
	time_t timetNow,timetCreateTime,timetCreateTime2;
	std::string strRef="ctyun_bdcsc_asia";
	std::string strPassWord="81c1b4ee6bac4c16";
	std::string strProvinceList="110000,120000,130000,140000,150000,210000,220000,230000,310000,320000,330000,340000,350000,360000,370000,410000,420000,430000,440000,450000,460000,500000,510000,520000,530000,540000,610000,620000,630000,640000,650000";
	std::string strKeyTypeList = "1,2,3,4,5,M";
	std::string strMd5Pass,signError,signOK;
	std::string strSeed;
	std::string strSign,reqParams;
	
	//Json::Reader jReader;
	Json::Reader *jReader= new Json::Reader(Json::Features::strictMode()); // turn on strict verify mode

	char *temp[PACKET]; 
	int  index = 0;
	char bufTemp[PACKET];
	char *buf;
	char *buf2;
	char *outer_ptr = NULL;  
	char *inner_ptr = NULL;  
	char m_httpReq[_8KBLEN];

	memset(m_httpReq, 0, _8KBLEN);
	memset(bufTemp, 0, PACKET);
	memset(m_pszAdxBuf, 0, _8KBLEN);
	
	iRes = m_pclSock->TcpRead(m_pszAdxBuf, _8KBLEN);
	if( iRes > 0 )
	{
		printf("m_pszAdxBuf=%s\n",m_pszAdxBuf);
	}
	
	if(iRes <= (int)http.length()) 
	{		
		LOG(DEBUG, "[thread: %d]Read Socket Error [%d].", m_uiThreadId, iRes);
		stResponseInfo.ssErrorMsg="E0001";
		return LINKERROR;
	}

	std::string ssContent = std::string(m_pszAdxBuf);
	m_httpType = BdxGetRequestMethod(ssContent);

	switch(m_httpType)
	{
		case CATALOG:
				BdxCatalog();
				break;
		case PROVISION:
				BdxProvision();
				break;
		case DEPROVISION:
				BdxDeProvision();
				break;
		case LASTOPERATION:
				BdxLastOperation();
				break;
		case UPDATE:
				BdxUpdate();
				break;
		case BIND:
				BdxBind();
				break;
		case UNBIND:
				BdxUnbind();
				break;

		default:
				printf("no match mothod.....\n");
				break;

	}
	printf("ssContent=%s\n",ssContent.c_str());
	printf("m_httpType=%d\n",m_httpType);
	printf("m_httpUri=%d\n",m_httpUri);

	return SUCCESS;
	
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
		sprintf((char *)m_pszAdxResponse, "%s%sContent-Length: %d\r\n\r\n", http200ok,BdxGetHttpDate().c_str(),(int)stAdxRes.mResValue.length());
		int iHeadLen = strlen(m_pszAdxResponse);
		memcpy(m_pszAdxResponse + iHeadLen, stAdxRes.mResValue.c_str(),stAdxRes.mResValue.length());
	}
	else
	{
		sprintf((char *)m_pszAdxResponse,"%s",stAdxRes.mResValue.c_str());
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
        		printf("Line:%d,str=%s\n",__LINE__,str.c_str());
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

int CTaskMain::BdxCatalog()
{
	return SUCCESS;
}

int CTaskMain::BdxProvision()
{
	return SUCCESS;
}
int CTaskMain::BdxDeProvision()
{
	return SUCCESS;
}
int CTaskMain::BdxLastOperation()
{
	return SUCCESS;
}
int CTaskMain::BdxUpdate()
{
	return SUCCESS;
}
int CTaskMain::BdxBind()
{
	return SUCCESS;
}
int CTaskMain::BdxUnbind()
{
	return SUCCESS;
}

int CTaskMain::BdxGetRequestMethod(std::string &reqParams)
{
	printf("reqParams=%s\n",reqParams.c_str());
	m_httpUri = BdxGetRequestURI(reqParams);
	printf("m_httpUri=%d\n",m_httpUri);
	if (strcasecmp(reqParams.substr(0,reqParams.find(BLANK,0)).c_str(),REQ_TYPE_GET)== 0)
	{	
		printf("+++++++++++++++reqParams=%s\n",reqParams.c_str());
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
			return UPDATE;
	}
	return OTHERERROR;
	
}

int CTaskMain::BdxGetRequestURI(std::string &reqParams)
{
	printf("reqParams=%s\n",reqParams.c_str());
	
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


