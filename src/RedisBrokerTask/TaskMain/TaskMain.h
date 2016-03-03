/*
 * TaskMain.h

 */

#ifndef __TASK_MAIN__
#define __TASK_MAIN__

#include "../RedisBrokerTask.h"
#include "../../CommonTools/Conf/Conf.h"

class CTaskMain : public CUserQueryTask {

public:
	CTaskMain();
	CTaskMain(CTcpSocket* pclSock);
	virtual ~CTaskMain();
	
	UESRQUERYRPORT_S m_st;

	virtual int BdxRunTask(BDXREQUEST_S& stRequestInfo, BDXRESPONSE_S& stResponseInfo);
//protected:

	int BdxGetHttpPacket(BDXREQUEST_S& stRequestInfo,BDXRESPONSE_S &stResponseInfo);
	int BdxParseHttpPacket(char*& pszBody, u_int& uiBodyLen, const u_int uiParseLen);
	int BdxParseBody(char *pszBody, u_int uiBodyLen, BDXREQUEST_S& stRequestInfo);
	int BdxSendEmpyRespones(std::string &errorMsg);
	int BdxSendRespones(BDXREQUEST_S& stRequestInfo, BDXRESPONSE_S& stAdxRes);
	std::string BdxTaskMainGetDate(const time_t ttime = 0);
	std::string BdxTaskMainGetNextDate(const time_t ttime = 0);
	std::string BdxTaskMainGetLastDate(const time_t ttime = 0);
	std::string BdxTaskMainGetMonth(const time_t ttime = 0);
	std::string BdxTaskMainReplace_All(string    str,   string   old_value,   string   new_value);
	std::string BdxTaskMainGetTime(const time_t ttime = 0);
	std::string BdxTaskMainGetMinute(const time_t ttime = 0);
	std::string BdxTaskMainGetFullTime(const time_t ttime=0);
	std::string BdxTaskMainGetUCTime(const time_t ttime = 0);
	std::string BdxGenNonce(int length);
	std::string GenPasswordDigest(std::string utcTime, std::string nonce, std::string appSecret);
	std::string BdxGetParamSign(const std::string& strParam, const std::string& strSign);
	int BdxCheckRemoteServer(std::string serverIP,uint16_t serverPORT);
	int BdxCheckEtcdKeyIsExists(BDXRESPONSE_S& stResponseInfo,std::string serverIP,uint16_t serverPORT,std::string etcdKey);
	int BdxCatalog(BDXREQUEST_S& stRequestInfo,BDXRESPONSE_S& stResponseInfo);
	int BdxProvision(BDXREQUEST_S& stRequestInfo,BDXRESPONSE_S& stResponseInfo,std::string &reqParams);
	int BdxDeProvision(BDXREQUEST_S& stRequestInfo,BDXRESPONSE_S& stResponseInfo,std::string &reqParams);
	int BdxLastOperation(BDXREQUEST_S& stRequestInfo,BDXRESPONSE_S& stResponseInfo,std::string &reqParams);
	int BdxUpdate(BDXREQUEST_S& stRequestInfo,BDXRESPONSE_S& stResponseInfo,std::string &reqParams);
	int BdxBind(BDXREQUEST_S& stRequestInfo,BDXRESPONSE_S& stResponseInfo,std::string &reqParams);
	int BdxUnbind(BDXREQUEST_S& stRequestInfo,BDXRESPONSE_S& stResponseInfo,std::string &reqParams);
	int BdxRunTask();
	int BdxGetRequestMethod(std::string &reqParams);
	BDXREQUESTURLINFO_S BdxGetReqUrlAndContent(std::string &reqParams);
	int BdxGetRequestURI(std::string &reqParams);

private:
	char m_pszAdxBuf[_8KBLEN];
	static const char* m_pszHttpHeaderEnd;
	static const char* m_pszHttpLineEnd;
	static const std::string keyEdcpMd5Sign;
	std::string ssCountKeyReq;
	std::string ssCountKeyRes;
	std::string ssCountKeyEmptyRes;
	CMd5 mdSHA1;
	int m_httpType ;
	int m_httpUri;
	CConf  mConf;	
	std::map<std::string,std::string> m_mapUserValue;
	CTime m_cTime;
	
};

#endif /* __TASK_MAIN__ */
