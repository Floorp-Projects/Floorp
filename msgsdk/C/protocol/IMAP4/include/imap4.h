/*
 *	 The contents of this file are subject to the Netscape Public
 *	 License Version 1.1 (the "License"); you may not use this file
 *	 except in compliance with the License. You may obtain a copy of
 *	 the License at http://www.mozilla.org/NPL/
 *	
 *	 Software distributed under the License is distributed on an "AS
 *	 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 *	 implied. See the License for the specific language governing
 *	 rights and limitations under the License.
 *	
 *	 The Original Code is the Netscape Messaging Access SDK Version 3.5 code, 
 *	released on or about June 15, 1998.  *	
 *	 The Initial Developer of the Original Code is Netscape Communications 
 *	 Corporation. Portions created by Netscape are
 *	 Copyright (C) 1998 Netscape Communications Corporation. All
 *	 Rights Reserved.
 *	
 *	 Contributor(s): ______________________________________. 
*/
  
/* 
* Copyright (c) 1997 and 1998 Netscape Communications Corporation
* (http://home.netscape.com/misc/trademarks.html) 
*/

#ifndef imap4_H 
#define imap4_H 

/************************************************************************************* 
* File: imap4.h 
* These functions are the client level API 
*************************************************************************************/ 

#include <stdio.h> 
#include "nsmail.h" 

/************************************************************************************ 
* Data Structures and typedefs 
************************************************************************************/ 

/***********************************************************************************  
* imap4Client_t is the structure that holds a reference to all 
* the major objects within the IMAP SDK as well as data members 
***********************************************************************************/ 
typedef struct imap4Client imap4Client_t;
  
#ifdef __cplusplus 
extern "C" { 
#endif /* __cplusplus */ 

/***********************************************************************************  
* This structure represents the data sink for all imap4 commands
***********************************************************************************/ 
typedef struct imap4Sink * imap4SinkPtr_t;

typedef struct imap4Sink
{
	void *pOpaqueData;
    void (*taggedLine)(imap4SinkPtr_t in_pimap4Sink, char* in_tag, const char* in_status, const char* in_reason);
    void (*error)(imap4SinkPtr_t in_pimap4Sink, const char* in_tag, const char* in_status, const char* in_reason);
    void (*ok)(imap4SinkPtr_t in_pimap4Sink, const char* in_responseCode, const char* in_information);
    void (*rawResponse)(imap4SinkPtr_t in_pimap4Sink, const char* in_data);

    /*Fetch Response*/
    void (*fetchStart)(imap4SinkPtr_t in_pimap4Sink, int in_msg);
    void (*fetchEnd)(imap4SinkPtr_t in_pimap4Sink);
    void (*fetchSize)(imap4SinkPtr_t in_pimap4Sink, int in_size);
    void (*fetchData)(imap4SinkPtr_t in_pimap4Sink, const char* in_data, int in_bytesRead, int in_totalBytes);
    void (*fetchFlags)(imap4SinkPtr_t in_pimap4Sink, const char* in_flags);
    void (*fetchBodyStructure)(imap4SinkPtr_t in_pimap4Sink, const char* in_bodyStructure);
    void (*fetchEnvelope)(imap4SinkPtr_t in_pimap4Sink, const char** in_value, int in_valueLength);
    void (*fetchInternalDate)(imap4SinkPtr_t in_pimap4Sink, const char* in_internalDate);
    void (*fetchHeader)(imap4SinkPtr_t in_pimap4Sink, const char* in_field, const char* in_value);
    void (*fetchUid)(imap4SinkPtr_t in_pimap4Sink, int in_uid);

    /*Lsub Response*/
    void (*lsub)(imap4SinkPtr_t in_pimap4Sink, const char* in_attribute, const char* in_delimeter, const char* in_name);

    /*List Response*/
    void (*list)(imap4SinkPtr_t in_pimap4Sink, const char* in_attribute, const char* in_delimeter, const char* in_name);

    /*Search Response*/
	void (*searchStart)(imap4SinkPtr_t in_pimap4Sink);
    void (*search)(imap4SinkPtr_t in_pimap4Sink, int in_message);
	void (*searchEnd)(imap4SinkPtr_t in_pimap4Sink);

    /*Status Response*/
    void (*statusMessages)(imap4SinkPtr_t in_pimap4Sink, int in_messages);
    void (*statusRecent)(imap4SinkPtr_t in_pimap4Sink, int in_recent);
    void (*statusUidnext)(imap4SinkPtr_t in_pimap4Sink, int in_uidNext);
    void (*statusUidvalidity)(imap4SinkPtr_t in_pimap4Sink, int in_uidValidity);
    void (*statusUnseen)(imap4SinkPtr_t in_pimap4Sink, int in_unSeen);

    /*Capability Response*/
    void (*capability)(imap4SinkPtr_t in_pimap4Sink, const char* in_listing);

    /*Exists Response*/
    void (*exists)(imap4SinkPtr_t in_pimap4Sink, int in_messages);

    /*Expunge Response*/
    void (*expunge)(imap4SinkPtr_t in_pimap4Sink, int in_message);

    /*Recent Response*/
    void (*recent)(imap4SinkPtr_t in_pimap4Sink, int in_messages);

    /*Flags Response*/
    void (*flags)(imap4SinkPtr_t in_pimap4Sink, const char* in_flags);

    /*Bye Response*/
    void (*bye)(imap4SinkPtr_t in_pimap4Sink, const char* in_reason);

    /*Namespace Response*/
    void (*nameSpaceStart)(imap4SinkPtr_t in_pimap4Sink);
    void (*nameSpacePersonal)(imap4SinkPtr_t in_pimap4Sink, const char* in_personal);
    void (*nameSpaceOtherUsers)(imap4SinkPtr_t in_pimap4Sink, const char* in_otherUsers);
    void (*nameSpaceShared)(imap4SinkPtr_t in_pimap4Sink, const char* in_shared);
    void (*nameSpaceEnd)(imap4SinkPtr_t in_pimap4Sink);


    /*ACL Responses*/
    void (*aclStart)(imap4SinkPtr_t in_pimap4Sink, const char* in_mailbox);
    void (*aclIdentifierRight)(imap4SinkPtr_t in_pimap4Sink, const char* in_identifier, const char* in_rights);
    void (*aclEnd)(imap4SinkPtr_t in_pimap4Sink);

    /*LISTRIGHTS Responses*/
    void (*listRightsStart)(imap4SinkPtr_t in_pimap4Sink, const char* in_mailbox, const char* in_identifier);
    void (*listRightsRequiredRights)(imap4SinkPtr_t in_pimap4Sink, const char* in_requiredRights);
    void (*listRightsOptionalRights)(imap4SinkPtr_t in_pimap4Sink, const char* in_optionalRights);
    void (*listRightsEnd)(imap4SinkPtr_t in_pimap4Sink);

    /*MYRIGHTS Responses*/
    void (*myRights)(imap4SinkPtr_t in_pimap4Sink, const char* in_mailbox, const char* in_rights);

}imap4Sink_t;


/*********************************************************************************** 
* General Commands 
************************************************************************************/ 

int imap4_initialize(imap4Client_t** in_ppimap4, imap4Sink_t* in_pimap4Sink); 
int imap4_free(imap4Client_t** in_ppimap4); 
int imap4Sink_initialize(imap4Sink_t** out_ppimap4Sink);
int imap4Sink_free(imap4Sink_t** out_ppimap4Sink);
int imap4Tag_free(char** out_ppimap4Tag);

int imap4_setChunkSize(imap4Client_t* in_pimap4, int in_size); 
int imap4_setTimeout(imap4Client_t* in_pimap4, int in_timeout); 
int imap4_setResponseSink(imap4Client_t* in_pimap4, imap4Sink_t *in_pimap4Sink); 

int imap4_set_option(imap4Client_t* in_pimap4, int in_option, void* in_pOptionData); 
int imap4_get_option(imap4Client_t* in_pimap4, int in_option, void* in_pOptionData); 


int imap4_connect(imap4Client_t * in_pimap4, const char * in_host, 
				  unsigned short in_port, char** out_ppTagID);
int imap4_disconnect( imap4Client_t * in_pimap4 );

int imap4_sendCommand(imap4Client_t* in_pimap4, const char* in_command, char** out_ppTagID); 

int imap4_processResponses(imap4Client_t* in_pimap4); 

/*********************************************************************************** 
* The non-authenticated state 
************************************************************************************/ 

int imap4_login(imap4Client_t* in_pimap4, const char* in_user, 
				const char* in_password, char** out_ppTagID); 
int imap4_logout(imap4Client_t* in_pimap4, char** out_ppTagID); 
int imap4_noop(imap4Client_t* in_pimap4, char** out_ppTagID); 
int imap4_capability(imap4Client_t* in_pimap4, char** out_ppTagID); 

/*********************************************************************************** 
* The authenticated state 
************************************************************************************/ 

int imap4_append(imap4Client_t* in_pimap4, const char* in_mailbox, 
				 const char* in_optFlags, const char* in_optDateTime, 
				 nsmail_inputstream_t* in_literal, char** out_ppTagID); 

int imap4_create(imap4Client_t* in_pimap4, const char* in_mailbox, char** out_ppTagID); 

int imap4_delete(imap4Client_t* in_pimap4, const char* in_mailbox, char** out_ppTagID); 

int imap4_examine(imap4Client_t* in_pimap4, const char* in_mailbox, char** out_ppTagID); 

int imap4_list( imap4Client_t* in_pimap4, const char* in_refName, 
                const char* in_mailbox, char** out_ppTagID); 

int imap4_lsub( imap4Client_t* in_pimap4, const char* in_refName, 
                const char* in_mailbox, char** out_ppTagID); 

int imap4_rename(	imap4Client_t* in_pimap4, const char* in_currentMB, 
                    const char* in_newMB, char** out_ppTagID); 

int imap4_select(imap4Client_t* in_pimap4, const char* in_mailbox, char** out_ppTagID); 

int imap4_status(   imap4Client_t* in_pimap4, const char* in_mailbox, 
                    const char* in_statusData, char** out_ppTagID); 

int imap4_subscribe(imap4Client_t* in_pimap4, const char* in_mailbox, char** out_ppTagID); 

int imap4_unsubscribe(imap4Client_t* in_pimap4, const char* in_mailbox, char** out_ppTagID); 
 

/*********************************************************************************** 
* The selected state 
************************************************************************************/ 

int imap4_check(imap4Client_t* in_pimap4, char** out_ppTagID); 

int imap4_close(imap4Client_t* in_pimap4, char** out_ppTagID); 

int imap4_copy( imap4Client_t* in_pimap4, const char* in_msgSet, 
				const char* in_mailbox, char** out_ppTagID); 
int imap4_uidCopy(	imap4Client_t* in_pimap4, const char* in_msgSet, 
					const char* in_mailbox, char** out_ppTagID); 

int imap4_expunge(imap4Client_t* in_pimap4, char** out_ppTagID); 

/*Message body data stored in file(s) at in_fileLocation or in memory if in_fileLocation NULL*/ 
int imap4_fetch(imap4Client_t* in_pimap4, const char* in_msgSet, 
                const char* in_fetchCriteria, char** out_ppTagID); 

/*Message body data stored in file(s) at in_fileLocation or in memory if in_fileLocation NULL*/ 
int imap4_uidFetch( imap4Client_t* in_pimap4, const char* in_msgSet, 
					const char* in_fetchCriteria, char** out_ppTagID); 

int imap4_search(imap4Client_t* in_pimap4, const char* in_criteria, char** out_ppTagID); 
int imap4_uidSearch(imap4Client_t* in_pimap4, const char* in_criteria, char** out_ppTagID); 

int imap4_store(imap4Client_t* in_pimap4, const char* in_msgSet, const char* in_dataItem, 
                const char* in_value, char** out_ppTagID); 
int imap4_uidStore(imap4Client_t* in_pimap4, const char* in_msgSet, const char* in_dataItem, 
                const char* in_value, char** out_ppTagID); 

/*********************************************************************************** 
* Extended IMAP commands
************************************************************************************/ 
int imap4_nameSpace(imap4Client_t* in_pimap4, char** out_ppTagID); 
int imap4_setACL(	imap4Client_t* in_pimap4, const char* in_mailbox, const char* in_authID, 
					const char* in_accessRight, char** out_ppTagID); 
int imap4_deleteACL(imap4Client_t* in_pimap4, const char* in_mailbox, 
					const char* in_authID, char** out_ppTagID); 
int imap4_getACL(imap4Client_t* in_pimap4, const char* in_mailbox, char** out_ppTagID); 
int imap4_myRights(	imap4Client_t* in_pimap4, const char* in_mailbox, char** out_ppTagID); 
int imap4_listRights(	imap4Client_t* in_pimap4, const char* in_mailbox, 
						const char* in_authID, char** out_ppTagID); 

#ifdef __cplusplus 
} 
#endif /* __cplusplus */ 

#endif /*imap4_H*/ 
  
  
