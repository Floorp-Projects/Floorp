#ifndef PROCESSMSG_H_
#define PROCESSMSG_H_

SSMStatus
SSMControlConnection_ProcessDecodeAndCreateTempCert(SSMControlConnection * ctrl,
                                                    SECItem * msg);
SSMStatus
SSMControlConnection_ProcessDestroyCert(SSMControlConnection * ctrl,
                                        SECItem * msg);
SSMStatus
SSMControlConnection_ProcessGetKeyChoiceList(SSMControlConnection * ctrl, 
		     SECItem * msg);

/* This function runs on a separate thread and has a single argument */
typedef struct {
SSMControlConnection * ctrl;
SECItem * msg;
} genKeyArg;
void
SSMControlConnection_ProcessGenKeyOldStyle(void * arg); 
SSMStatus
SSMControlConnection_ProcessDecodeCertRequest(SSMControlConnection * ctrl,
                                              SECItem * msg);
SSMStatus
SSMControlConnection_ProcessFindCertByNickname(SSMControlConnection *ctrl, SECItem *msg);

SSMStatus
SSMControlConnection_ProcessFindCertByKey(SSMControlConnection *ctrl, SECItem *msg);

SSMStatus
SSMControlConnection_ProcessFindCertByEmailAddr(SSMControlConnection *ctrl, SECItem *msg);

SSMStatus
SSMControlConnection_ProcessAddCertToDB(SSMControlConnection *ctrl, SECItem *msg);

SSMStatus
SSMControlConnection_ProcessMatchUserCert(SSMControlConnection *ctrl, SECItem *msg);

SSMStatus SSMControlConnection_ProcessPickleSecurityStatusRequest(SSMControlConnection* ctrl,
								 SECItem* msg);

SSMStatus
SSMControlConnection_ProcessLocalizedTextRequest(SSMControlConnection *ctrl,
                                                 SECItem * msg);

SSMStatus
SSMControlConnection_ProcessRedirectCompare(SSMControlConnection *ctrl, 
					    SECItem * msg);

SSMStatus
SSMControlConnection_ProcessDecodeCRLRequest(SSMControlConnection *ctrl, 
					     SECItem *msg);


PRStatus 
SSMControlConnection_ProcessSecurityAdvsiorRequest(SSMControlConnection *ctrl, 
						   SECItem *msg);

SSMStatus
SSMControlConnection_ProcessGetExtensionRequest(SSMControlConnection *ctrl, 
                                                SECItem              *msg);

SSMStatus
SSMControlConnection_ProcessHTMLCertInfoRequest(SSMControlConnection *ctrl, 
						SECItem              *msg);
#endif /*PROCESSMSG_H_*/
