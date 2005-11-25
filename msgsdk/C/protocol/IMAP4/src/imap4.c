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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imap4.h"
#include "imapdata.h"

/*********************************************************************************** 
* Function prototypes
************************************************************************************/ 

int deployCommand(imap4Client_t* in_pimap4, char* in_cmdTokens[], char** out_ppTagID);
int imap4_sendCmd(imap4Client_t* in_pimap4, char* in_command, 
					char* in_tagID, nsmail_outputstream_t* in_pOutputStream);
int resolveType(char* in_szToken1, char* in_szToken2, char* in_szToken3);
typedef void (*endLine)(imap4SinkPtr_t , char* , const char* , const char* );
int parseEndingLine(imap4Client_t* in_pimap4, char* in_data, endLine in_sinkMethod);
typedef void (*oknobad)(imap4SinkPtr_t in_pimap4Sink, const char* in_responseCode, const char* in_information);
int parseOkNoBad(imap4Client_t* in_pimap4, char* in_data, oknobad in_sinkMethod);
int parseNoBad(imap4Client_t* in_pimap4, const char* in_response);
char* matchBrackets(char* in_data, char** out_ppValue);
typedef void (*parseMbox)(imap4SinkPtr_t in_pimap4Sink, const char* in_attribute, const char* in_delimiter, const char* in_name);
int parseMailboxInfo(imap4Client_t* in_pimap4, char* in_data, parseMbox in_sinkMethod);
typedef void (*messageNumber)(imap4SinkPtr_t in_pimap4Sink, int in_messages);
int getNumber(imap4Client_t* in_pimap4, char* in_data, messageNumber in_sinkMethod);
int resolveFetchDataItem(imap4Client_t* in_pimap4, char* in_dataItem);
char* matchFieldBrackets(char* in_data, char** out_ppValue);
char* matchFieldQuotes(char* in_data, char** out_ppValue);
char* skipWhite(char* in_data, int in_length);
int readHeaderLine(char* in_data, char** in_ppLeftOver, int in_length, int in_start, char** out_ppHeaderLine, int* out_pEnd);


/*********************************************************************************** 
* General Commands
************************************************************************************/ 

/**
* Initializes the IMAP client
* @param out_ppimap4 The imap4 client structure
* @param in_pimap4Sink The data sink
* @return int The return code
*/
int imap4_initialize(imap4Client_t** out_ppimap4, imap4Sink_t* in_pimap4Sink)
{
	imap4Client_i_t * l_pimap4;
	int l_nReturnCode;

    /* Parameter validation. */
    if ( out_ppimap4 == NULL || (*out_ppimap4) != NULL || in_pimap4Sink == NULL )
    {
		return NSMAIL_ERR_INVALIDPARAM;
    }
	
	l_pimap4 = (imap4Client_i_t*)malloc(sizeof(imap4Client_i_t));
	if(l_pimap4 == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_pimap4, 0, sizeof(imap4Client_i_t));

	l_pimap4->timeout = -1;
	l_pimap4->pPendingCommands = NULL;
	l_pimap4->chunkSize = 1024;
	l_pimap4->pimap4Sink = in_pimap4Sink;
	l_pimap4->pLiteralStream = NULL;
	l_pimap4->tagID = 0;
	l_pimap4->bReadHeader = FALSE;
	l_pimap4->bConnected = FALSE;

	/*Initialize the IO Handler structure*/
    l_pimap4->pIO = (IO_t *)malloc(sizeof(IO_t));
    if (l_pimap4->pIO == NULL)
    {
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_pimap4->pIO, 0, sizeof(IO_t));

	l_nReturnCode = IO_initialize(l_pimap4->pIO, l_pimap4->chunkSize, l_pimap4->chunkSize);
    if(l_nReturnCode != NSMAIL_OK)
	{
		return l_nReturnCode;
	}

    (*out_ppimap4) = l_pimap4;

	return NSMAIL_OK;
}


/**
* Free the memory that the IMAP client holds
* @param in_ppimap4 A pointer to the imap4 client structure
* @return int The return code
*/
int imap4_free(imap4Client_t** in_ppimap4)
{
	if((*in_ppimap4) == NULL)
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	/*Free IO*/
	IO_free((*in_ppimap4)->pIO);
	myFree((*in_ppimap4)->pIO);

	/*Free callback list*/
	RemoveAllElements(&((*in_ppimap4)->pPendingCommands));

	myFree((*in_ppimap4));
	(*in_ppimap4) = NULL;

    return NSMAIL_OK;
}


/**
* Allocates and initializes the sink structure
* @param out_ppimap4Sink The imap4 sink structure
* @return int The return code
*/
int imap4Sink_initialize(imap4Sink_t** out_ppimap4Sink)
{
    /* Parameter validation. */
    if(out_ppimap4Sink == NULL || (*out_ppimap4Sink) != NULL)
    {
		return NSMAIL_ERR_INVALIDPARAM;
    }

    /* Create the sink */
    (*out_ppimap4Sink) = (imap4Sink_t*)malloc( sizeof(imap4Sink_t) );
    if((*out_ppimap4Sink) == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
    memset((*out_ppimap4Sink), 0, sizeof(imap4Sink_t) );

    return NSMAIL_OK;
}


/**
* Free the sink structure
* @param out_ppimap4Sink The imap4 sink structure
* @return int The return code
*/
int imap4Sink_free(imap4Sink_t** out_ppimap4Sink)
{
	if((*out_ppimap4Sink) == NULL)
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}
	
	myFree(*out_ppimap4Sink);
	(*out_ppimap4Sink) = NULL;

    return NSMAIL_OK;
}


/**
* Free the tag and re-initializes pointer to NULL
* @param out_ppimap4Tag The tag
* @return int The return code
*/
int imap4Tag_free(char** out_ppimap4Tag)
{
	if((*out_ppimap4Tag) == NULL)
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}
	myFree(*out_ppimap4Tag);
	(*out_ppimap4Tag) = NULL;

    return NSMAIL_OK;
}


/**
* Sets the size of the message data chunk returned in fetchData
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_size The size of the data chunk
* @return int The return code
*/
int imap4_setChunkSize(imap4Client_t* in_pimap4, int in_size)
{
	if((in_pimap4 == NULL) || (in_size < 1))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}
	in_pimap4->chunkSize = in_size;

    return NSMAIL_OK;
}

/**      
* Sets the socket timeout for reading data from the socket 
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_timeout The time out in milliseconds
*/
int imap4_setTimeout(imap4Client_t* in_pimap4,int in_timeout)
{	
    if((in_pimap4 == NULL) || (in_timeout < -1))
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    return IO_setTimeout(in_pimap4->pIO, in_timeout);
}

/**      
* Sets the sink to in_pSink. It will replace the sink that was
* passed into imap4_initialize.
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_pSink The notification sink.
*/
int imap4_setResponseSink(imap4Client_t* in_pimap4, imap4Sink_t *in_pimap4Sink)
{
    if((in_pimap4 == NULL) || (in_pimap4Sink == NULL) )
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    in_pimap4->pimap4Sink = in_pimap4Sink;

    return NSMAIL_OK;
}

/**
* Function used for setting io and threading models.
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_option Option that you want to set. 
* @param in_pOptdata Pointer to the buffer in which the value of 
* the option will be returned. 
* @return int
*/
int imap4_set_option(imap4Client_t* in_pimap4, int in_option, void *in_pOptionData )
{
    if((in_pimap4 == NULL) || (in_pOptionData == NULL))
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    switch(in_option)
    {
        case NSMAIL_OPT_IO_FN_PTRS:
            IO_setFunctions(in_pimap4->pIO, (nsmail_io_fns_t *)in_pOptionData );
            return NSMAIL_OK;
            break;
        default:
            return NSMAIL_ERR_UNEXPECTED;
            break;
    }
}

/**
* Function used for getting io and threading models.
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_option Option that you want to retrieve. 
* @param in_pOptdata Pointer to the buffer in which the value of 
* the option will be returned. 
* @return int
*/
int imap4_get_option(imap4Client_t* in_pimap4, int in_option, void *in_pOptionData )
{
    if(in_pimap4 == NULL)
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

    switch(in_option)
    {
        case NSMAIL_OPT_IO_FN_PTRS:
            IO_getFunctions(in_pimap4->pIO, (nsmail_io_fns_t *)in_pOptionData);
            return NSMAIL_OK;
            break;
        default:
            return NSMAIL_ERR_UNEXPECTED;
            break;
    }
}

/**
* Connects to in_host at in_portNumber
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_host The host to connect to
* @param in_port The port to connect to
* @param out_ppTagid The commands associated tag
* @return int
*/
int imap4_connect(imap4Client_t * in_pimap4, const char * in_host, 
				  unsigned short in_port, char** out_ppTagID)
{
	int l_nReturnCode = NSMAIL_OK;
	CommandObject_t *l_pCommandObject = NULL;
	char* l_szTagID = NULL;
	int l_nTagLength = 0;
	char l_tagBuffer[100];

	/*Parameter validation*/
	if((in_pimap4 == NULL) || (in_host == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	if(in_pimap4->bConnected)
	{
		return NSMAIL_ERR_IO_CONNECT;
	}
	l_nReturnCode = IO_connect(in_pimap4->pIO, in_host, in_port);
	if(l_nReturnCode != NSMAIL_OK)
	{
		return l_nReturnCode;
	}
	
	in_pimap4->bConnected = TRUE;

	/*Insert a command object into the pending commands list*/
	/*Store the callback information in a list.*/
	l_pCommandObject = (CommandObject_t*)malloc(sizeof(CommandObject_t));
    if(l_pCommandObject == NULL)
    {
        return NSMAIL_ERR_OUTOFMEMORY;
    }
	memset(l_pCommandObject, 0, sizeof(CommandObject_t));

	/*Create a tag*/
	memset(l_tagBuffer, 0, 100);
	l_nTagLength = sprintf(l_tagBuffer, "%d", in_pimap4->tagID);
	in_pimap4->tagID++;

	/*Store the internal tag*/
	l_szTagID = (char*)malloc((l_nTagLength + 1)*sizeof(char));
	if(l_szTagID == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szTagID, 0, (l_nTagLength + 1)*sizeof(char));
	memcpy(l_szTagID, l_tagBuffer, l_nTagLength);

	/*Store the output tag*/
	if(out_ppTagID != NULL)
	{
		(*out_ppTagID) = (char*)malloc((l_nTagLength + 1)*sizeof(char));
		if((*out_ppTagID) == NULL)
		{
			return NSMAIL_ERR_OUTOFMEMORY;
		}
		memset((*out_ppTagID), 0, (l_nTagLength + 1)*sizeof(char));
		memcpy((*out_ppTagID), l_tagBuffer, l_nTagLength);
	}

	l_pCommandObject->pTag = l_szTagID;
	l_pCommandObject->pOutputStream = NULL;
	l_pCommandObject->pCommand = (char*)g_szConnect;
	l_pCommandObject->next = NULL;

    return AddElement(&(in_pimap4->pPendingCommands), l_pCommandObject);
}

/**
* Disconnects from the host specified in connect
* @param in_pimap4 A pointer to the imap4 client structure
* @return int
*/
int imap4_disconnect(imap4Client_t * in_pimap4)
{
	int l_nReturnCode = NSMAIL_OK;

	/*Parameter validation*/
	if(in_pimap4 == NULL)
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

    /* Disconnect from the server. */
    l_nReturnCode = IO_disconnect(in_pimap4->pIO);
	if(l_nReturnCode != NSMAIL_OK)
	{
		return l_nReturnCode;
	}

	/*Remove all pending commands*/
	RemoveAllElements(&in_pimap4->pPendingCommands);

	/*Re-initialize internal variables*/
	in_pimap4->timeout = -1;
	in_pimap4->pPendingCommands = NULL;
	in_pimap4->chunkSize = 1024;
	in_pimap4->pLiteralStream = NULL;
	in_pimap4->tagID = 0;
	in_pimap4->bReadHeader = FALSE;
	in_pimap4->bConnected = FALSE;

	return l_nReturnCode;
}

/**
* Sends client commands to IMAP server. 
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_command The IMAP command to invoke on the server
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_sendCommand(imap4Client_t* in_pimap4, const char* in_command, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_command == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)in_command;

	return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}

/**
* Processes the server responses for all API commands executed prior to 
* the invocation of this command. The appropriate functions on the notification 
* sink will be called to relay the results back to the user.
* @param in_pimap4 A pointer to the imap4 client structure
* @return int The return code
*/
int imap4_processResponses(imap4Client_t* in_pimap4)
{
	char* l_szResponse = NULL;
    int l_nReturnCode = NSMAIL_OK;
	char* l_szToken1 = NULL;
	char* l_szToken2 = NULL;
	char* l_szToken3 = NULL;
	char* l_szBegin = NULL;
	char* l_szEnd = NULL;
	int l_nLength = 0;

	/*Check parameters*/
    if((in_pimap4 == NULL) || (in_pimap4->pIO == NULL))
    {
	    return NSMAIL_ERR_INVALIDPARAM;
    }

	/*Check for a connection*/
	if(!in_pimap4->bConnected)
	{
		return NSMAIL_ERR_IO_CONNECT;
	}

    /* Make sure that all buffered commands are sent. */
    l_nReturnCode = IO_flush(in_pimap4->pIO);
    if ( l_nReturnCode != NSMAIL_OK )
    {
		return l_nReturnCode;
	}

    /* Process the responses of all the commands that have been issued
       or a timeout occurs. */
    while (in_pimap4->pPendingCommands != NULL)
    {
		l_nReturnCode = IO_readDLine(in_pimap4->pIO, &l_szResponse);
        if ( l_nReturnCode != NSMAIL_OK )
        {
	        /* A timeout or an error has occured. */
            return l_nReturnCode;
        }

		/*Extract the first three tokens of in_response*/

		/*Retrieve first l_token*/
		l_szBegin = l_szResponse;
		l_szEnd = strchr(l_szBegin, ' ');
		if(l_szEnd == NULL)
		{
			return NSMAIL_ERR_PARSE;
		}
		l_nLength = l_szEnd - l_szBegin;
		l_szToken1 = (char*)malloc((l_nLength + 1)*sizeof(char));
		if(l_szToken1 == NULL)
		{
			return NSMAIL_ERR_OUTOFMEMORY;
		}
		memset(l_szToken1, 0, (l_nLength + 1)*sizeof(char));
		memcpy(l_szToken1, l_szResponse, l_nLength);
		l_szBegin = l_szEnd + 1;

		/*Retrieve second l_token*/
		l_szEnd = strchr(l_szBegin, ' ');
		if(l_szEnd == NULL)
		{
			l_szEnd = strchr(l_szBegin, '\r');
			if(l_szEnd == NULL)
			{
				return NSMAIL_ERR_PARSE;
			}
		}
		l_nLength = l_szEnd - l_szBegin;
		l_szToken2 = (char*)malloc((l_nLength + 1)*sizeof(char));
		if(l_szToken2 == NULL)
		{
			return NSMAIL_ERR_OUTOFMEMORY;
		}
		memset(l_szToken2, 0, (l_nLength + 1)*sizeof(char));
		memcpy(l_szToken2, l_szBegin, l_nLength);
		l_szBegin = l_szEnd + 1;

		/*Retrieve third l_token*/
		l_szEnd = strchr(l_szBegin, ' ');
		if(l_szEnd == NULL)
		{
			l_szEnd = strchr(l_szBegin, '\r');
		}

		if(l_szEnd == NULL)
		{
			l_nLength = 0;
		}
		else
		{
			l_nLength = l_szEnd - l_szBegin;
		}

		l_szToken3 = (char*)malloc((l_nLength + 1)*sizeof(char));
		if(l_szToken3 == NULL)
		{
			return NSMAIL_ERR_OUTOFMEMORY;
		}
		memset(l_szToken3, 0, (l_nLength + 1)*sizeof(char));

		if(l_nLength > 0)
		{
			memcpy(l_szToken3, l_szBegin, l_nLength);
		}
		
		/*Parse the server response and dispatch to appropriate method on the sink*/
		switch(resolveType(l_szToken1, l_szToken2, l_szToken3))
		{
			case RT_OKTAGGED:
			{
				l_nReturnCode = parseTaggedLine(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_NOTAGGED:
			case RT_BADTAGGED:
			{
				l_nReturnCode = parseError(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_OK:
			{
				l_nReturnCode = parseOk(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_BAD:
			case RT_NO:
			{
				l_nReturnCode = parseNoBad(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_CAPABILITY:
			{
				l_nReturnCode = parseCapability(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_LIST:
			{
				l_nReturnCode = parseList(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_LSUB:
			{
				l_nReturnCode = parseLsub(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_STATUS:
			{
				l_nReturnCode = parseStatus(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_SEARCH:
			{
				l_nReturnCode = parseSearch(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_FLAGS:
			{
				l_nReturnCode = parseFlags(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_BYE:
			{
				l_nReturnCode = parseBye(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_RECENT:
			{
				l_nReturnCode = parseRecent(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_EXISTS:
			{
				l_nReturnCode = parseExists(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_EXPUNGE:
			{
				l_nReturnCode = parseExpunge(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_FETCH:
			{
				l_nReturnCode = parseFetch(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_PLUS:
			{
				l_nReturnCode = parsePlus(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_NAMESPACE:
			{
				l_nReturnCode = parseNamespace(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_ACL:
			{
				l_nReturnCode = parseAcl(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_MYRIGHTS:
			{
				l_nReturnCode = parseMyrights(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_LISTRIGHTS:
			{
				l_nReturnCode = parseListrights(in_pimap4, l_szResponse);
				if(l_nReturnCode != NSMAIL_OK)
				{
					return l_nReturnCode;
				}
				break;
			}
			case RT_RAWRESPONSE:
			{
				sink_rawResponse(in_pimap4->pimap4Sink, l_szResponse);
				break;
			}
			default:
			{
				break;
			}
		
		}
		myFree(l_szResponse);
		myFree(l_szToken1);
		myFree(l_szToken2);
		myFree(l_szToken3);
		l_szResponse = NULL;
		l_szToken1 = NULL;
		l_szToken2 = NULL;
		l_szToken3 = NULL;
	}

	return NSMAIL_OK;
}


/*********************************************************************************** 
* The non-authenticated state 
************************************************************************************/ 

/**
* Identifies the client to the server and carries the plaintext password 
* authenticating this user.
* This passes your password over the network in the clear!
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_user The user name
* @param in_password The password of this user
* @param out_ppTagID The commands associated tag
* @return int The return code
*/
int imap4_login(imap4Client_t* in_pimap4, const char* in_user, 
				const char* in_password, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_user == NULL) || (in_password == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szLogin;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_user;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_password;

	return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}	

/**
* Informs the server that the client is done with the connection.
* @param in_pimap4 A pointer to the imap4 client structure
* @param out_ppTagID The commands associated tag
* @return int The return code
*/
int imap4_logout(imap4Client_t* in_pimap4, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;
	int l_nReturnCode = NSMAIL_OK;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL ))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}
	
	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szLogout;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}

/**
* The NOOP command can be used as a periodic poll for new 
* messages or message status updates during a period of inactivity.
* It can also be used to reset any inactivity autologout timer on
* the server.
* @param in_pimap4 A pointer to the imap4 client structure
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_noop(imap4Client_t* in_pimap4, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}
	
	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szNoop;

    return deployCommand(	in_pimap4, l_cmdTokens, out_ppTagID);

}

/**
* Requests a listing of capabilities that the server supports.
* @param in_pimap4 A pointer to the imap4 client structure
* @param out_ppTagID The invoke id associated with this command
* @return int The return code
*/
int imap4_capability(imap4Client_t* in_pimap4, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL ))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}
	
	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szCapability;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**      
* Authenticates a server using various SASL mechanisms.  TODO: Define this method completely
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_authenticator authentication mechanism to use
* @param out_ppCommand The command object
* @return int The return code
*/
/*int imap4_authenticate(imap4Client_t* in_pimap4,
                 SASL_mechanism_driver_t** in_ppMechanisms,
                 char* in_userid,
                 char* in_serverFQDN, 
                 SASL_driver_properties_t* in_pProps)
{
	return 1;
}*/


/*********************************************************************************** 
* The authenticated state 
************************************************************************************/ 

/**
* Appends the literal argument as a new message to the end of the 
* specified destination mailbox. 
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_mailbox The mailbox name
* @param in_optFlags The optional flags
* @param in_optDateTime The optional date / time
* @param in_literal The message literal
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_append(imap4Client_t* in_pimap4, const char* in_mailbox, 
				 const char* in_optFlags, const char* in_optDateTime, 
				 nsmail_inputstream_t* in_literal, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;
	char* l_szBuffer = NULL;
	char l_szSizeBuffer[100];
	int l_nBytesRead = 0;
	int l_nTotalBytes = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_mailbox == NULL) || (in_literal == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	/*Construct the command*/
	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szAppend;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_mailbox;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;

	if( (in_optFlags != NULL) && (strlen(in_optFlags) > 0))
	{
		l_cmdTokens[l_nIndex++] = (char*)in_optFlags;
		l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	}
	if((in_optDateTime != NULL) && (strlen(in_optDateTime) > 0))
	{
		l_cmdTokens[l_nIndex++] = (char*)in_optDateTime;
		l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	}
	
	/*Allocate a buffer that's needed to determine the size of the stream*/
	l_szBuffer = (char*)malloc(in_pimap4->chunkSize*sizeof(char));
	if(l_szBuffer == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szBuffer, 0, in_pimap4->chunkSize*sizeof(char));

	/*Determine the size of the input stream*/
	while(l_nBytesRead != -1)
	{
		l_nBytesRead = in_literal->read(in_literal->rock, l_szBuffer, in_pimap4->chunkSize);
		if(l_nBytesRead != -1)
		{
			l_nTotalBytes = l_nTotalBytes + l_nBytesRead;
		}
	}
	in_literal->rewind(in_literal->rock);

	/*Insert the size into the command*/
	memset(l_szSizeBuffer, 0, 100);
	sprintf(l_szSizeBuffer, "%d", l_nTotalBytes);
	
	l_cmdTokens[l_nIndex++] = (char*)g_szLeftCurlyBrace;
	l_cmdTokens[l_nIndex++] = (char*)l_szSizeBuffer;
	l_cmdTokens[l_nIndex++] = (char*)g_szRightCurlyBrace;

	/*Store the literal*/
	in_pimap4->pLiteralStream = in_literal;

	/*Clean up*/	
	myFree(l_szBuffer);
	
	/*Send the first part of the command*/
    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}

/**
* Creates a mailbox with the given name. You'll need to be
* careful to use the correct separator character if you want to create
* a directory on the server side.
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_mailbox Name of the mailbox
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_create(imap4Client_t* in_pimap4, const char* in_mailbox, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_mailbox == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szCreate;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_mailbox;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* This command PERMANENTLY removes the mailbox with the given name. 
* You can not and should not DELETE the INBOX. The system needs it.
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_mailbox String with the name of the mailbox to delete
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_delete(imap4Client_t* in_pimap4, const char* in_mailbox, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_mailbox == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szDelete;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_mailbox;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}

/**
* EXAMINE of a mailbox (select but readonly).
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_mailbox Name of the mailbox
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_examine(imap4Client_t* in_pimap4, const char* in_mailbox, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_mailbox == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szExamine;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_mailbox;
	
    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* Returns a subset of names from the complete set of all names available
* to the client.  
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_refName String with reference name
* @param in_mailbox String with mailbox name 
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_list( imap4Client_t* in_pimap4, const char* in_refName, 
                const char* in_mailbox, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_refName == NULL) || (in_mailbox == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szList;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_refName;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_mailbox;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* Return a subset of names from the set of names that the user has 
* declared as being "active" or "subscribed".
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_refName The reference name
* @param in_mailbox_name Mailbox name with possible wildcards
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_lsub( imap4Client_t* in_pimap4, const char* in_refName, 
                const char* in_mailbox, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_refName == NULL) || (in_mailbox == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szLsub;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_refName;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_mailbox;
	
    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* This command renames a mailbox. 
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_currentMB String with the mailbox's current name
* @param in_newMB String with the mailbox's new name
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_rename(	imap4Client_t* in_pimap4, const char* in_currentMB, 
                    const char* in_newMB, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_currentMB == NULL) || (in_newMB == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szRename;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_currentMB;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_newMB;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* SELECT of a mailbox for use on this connection.
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_mailbox Name of the mailbox
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_select(imap4Client_t* in_pimap4, const char* in_mailbox, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_mailbox == NULL)) 
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}
	
	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szSelect;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_mailbox;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* Requests the status of the indicated mailbox.
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_mailbox The mailbox name
* @param in_statusData The status data items
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_status(   imap4Client_t* in_pimap4, const char* in_mailbox, 
                    const char* in_statusData, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_mailbox == NULL) ||	(in_statusData == NULL)) 
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}
	
	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szStatus;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_mailbox;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_statusData;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* Add the specified mailbox name to the server's set of "active" or 
* "subscribed" mailboxes.
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_mailbox  The mailbox to add
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_subscribe(imap4Client_t* in_pimap4, const char* in_mailbox, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_mailbox == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szSubscribe;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_mailbox;
	
    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* Remove the specified mailbox name to the server's set of "active" or 
* "subscribed" mailboxes.
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_mailbox  The mailbox to remove
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_unsubscribe(imap4Client_t* in_pimap4, const char* in_mailbox, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_mailbox == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szUnsubscribe;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_mailbox;
	
    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/*********************************************************************************** 
* The selected state 
************************************************************************************/ 

/**
* This function is called to get the server to (possibly) checkpoint 
* its state, if that is necessary. Note: This does not imply that you 
* will get new EXISTS messages. If you want that type of stuff, use 
* NOOP. Note: This may not instantaneous. For more info on this and the 
* differences from NOOP, see RFC 1730.
* @param in_pimap4 A pointer to the imap4 client structure
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_check(imap4Client_t* in_pimap4, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szCheck;
	
    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}



/**
* CLOSE causes a mailbox to go away. 
* Note: If there are pending deletions (i.e. Messages marked with 
* the \Deleted flag) they get deleted as a result of this. 
* However, you won't get any notification of this. 
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_pCommandCB Pointer to the callback function
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_close(imap4Client_t* in_pimap4, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szClose;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* Copies the specified message(s) from the currently selected mailbox 
* to the end of the specified destination mailbox.
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_msgNum Message number
* @param in_mailbox Name of target mailbox
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_copy( imap4Client_t* in_pimap4, const char* in_msgSet, 
				const char* in_mailbox, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_msgSet == NULL) || (in_mailbox == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szCopy;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_msgSet;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_mailbox;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* This function is called to COPY a message from one mailbox to another. 
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_msgNum Message number
* @param in_mailbox Name of target mailbox
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_uidCopy(	imap4Client_t* in_pimap4, const char* in_msgSet, 
					const char* in_mailbox, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_msgSet == NULL) || (in_mailbox == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szUid;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)g_szCopy;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_msgSet;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_mailbox;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* This function is called to delete all the messages in the 
* mailbox with the \Deleted flag set. As per RFC 1730, if there is 
* an EXPUNGEd messages this will result in all messages with higher 
* numbers automatically being decremented by one. This happens BEFORE 
* this call returns, so immediately following this call, you should 
* probably think about what the state of the message cache might be.
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_pCommandCB Pointer to the callback function
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_expunge(imap4Client_t* in_pimap4, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szExpunge;
	
    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* Retrieves data associated as specified by the fetch criteria for 
* the given message set
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_msgSet  The message set
* @param in_fetchCriteria The message data item names
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_fetch(imap4Client_t* in_pimap4, const char* in_msgSet, 
                const char* in_fetchCriteria, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_msgSet == NULL) || (in_fetchCriteria == NULL)) 
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}
	
	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szFetch;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_msgSet;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_fetchCriteria;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* A call to "UID FETCH in_msgSet in_fetchCriteria" 
* This call maps directly to the FETCH as described in RFC2060
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_msgSet  The message set
* @param in_fetchCriteria The message data item names
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_uidFetch( imap4Client_t* in_pimap4, const char* in_msgSet, 
					const char* in_fetchCriteria, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_msgSet == NULL) || (in_fetchCriteria == NULL)) 
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}
	
	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szUid;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)g_szFetch;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_msgSet;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_fetchCriteria;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* Searches the mailbox for messages that match the given searching 
* criteria.
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_criteria The search criteria consisting of one or more search 
* keys
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_search(imap4Client_t* in_pimap4, const char* in_criteria, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_criteria == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szSearch;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_criteria;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* Searches the mailbox for messages that match the given searching 
* criteria.
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_criteria The search criteria consisting of one or more search 
* keys
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_uidSearch(imap4Client_t* in_pimap4, const char* in_criteria, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_criteria == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szUid;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)g_szSearch;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_criteria;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* Alters data associated with a message in the mailbox.
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_set The message set
* @param in_dataItem The message data item name
* @param in_value The value for the message data item
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_store(imap4Client_t* in_pimap4, const char* in_msgSet, const char* in_dataItem, 
                const char* in_value, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_msgSet == NULL) || (in_dataItem == NULL) || 
		(in_value == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szStore;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_msgSet;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_dataItem;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_value;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}

/**
* Alters data associated with a message in the mailbox.
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_msgSet The message set
* @param in_dataItem The message data item name
* @param in_value The value for the message data item
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_uidStore(imap4Client_t* in_pimap4, const char* in_msgSet, 
				   const char* in_dataItem, const char* in_value, 
				   char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_msgSet == NULL) || (in_dataItem == NULL) || 
		(in_value == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szUid;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)g_szStore;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_msgSet;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_dataItem;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_value;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* Namespace command
* @param in_pimap4 A pointer to the imap4 client structure
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_nameSpace(imap4Client_t* in_pimap4, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szNameSpace;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* Changes the access control list on the specified mailbox so that the
* specified identifier is granted permissions as specified in the 
* third argument
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_mailbox The mailbox name
* @param in_authId The authentication identifier
* @param in_accessRight The access right modification
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_setACL(	imap4Client_t* in_pimap4, const char* in_mailbox, const char* in_authID, 
					const char* in_accessRight, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_mailbox == NULL) || (in_authID == NULL) || 
		(in_accessRight == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szSetACL;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_mailbox;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_authID;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_accessRight;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* Removes any <identifier, rights> pair for the specified identifier
* from the access control list for the spcified mailbox
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_mailbox The mailbox name
* @param in_authId The authentication identifier
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_deleteACL(imap4Client_t* in_pimap4, const char* in_mailbox, 
					const char* in_authID, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_mailbox == NULL) || (in_authID == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szDeleteACL;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_mailbox;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_authID;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}

/**
* Retrieves the access control list for mailbox in an untagged ACL reply
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_mailbox The mailbox name
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_getACL(imap4Client_t* in_pimap4, const char* in_mailbox, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_mailbox == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szGetACL;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_mailbox;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* Retrieves the access control list for mailbox in an untagged ACL reply
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_mailbox The mailbox name
* @param in_authId The authentication identifier
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_listRights(	imap4Client_t* in_pimap4, const char* in_mailbox, 
						const char* in_authID, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_mailbox == NULL) || (in_authID == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szListRights;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_mailbox;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_authID;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);
}


/**
* Retrieves the set of rights that the user has to mailbox
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_mailbox The mailbox name
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_myRights(	imap4Client_t* in_pimap4, const char* in_mailbox, char** out_ppTagID)
{
	char* l_cmdTokens[MAXTOKENS];
	int l_nIndex = 0;

	/*Parameter validation*/
	if ((in_pimap4 == NULL) || (in_pimap4->pIO == NULL )||
		(in_mailbox == NULL))
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	memset(l_cmdTokens, 0, sizeof(l_cmdTokens));
	l_cmdTokens[l_nIndex++] = (char*)g_szMyRights;
	l_cmdTokens[l_nIndex++] = (char*)g_szSpace;
	l_cmdTokens[l_nIndex++] = (char*)in_mailbox;

    return deployCommand(in_pimap4, l_cmdTokens, out_ppTagID);

}

/****************************************************************************
* Internal Methods
*****************************************************************************/

/**
* Sends client commands to IMAP server synchronously. 
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_command The IMAP command to invoke on the server
* @param in_pCommandCB Pointer to the callback function
* @param out_ppTagid The commands associated tag
* @return int The return code
*/
int imap4_sendCmd(imap4Client_t* in_pimap4, char* in_command, 
					char* in_tagID, nsmail_outputstream_t* in_pOutputStream)
{
	int l_nReturnCode;
	CommandObject_t *l_pCommandObject;

    if((in_pimap4 == NULL) || (in_command == NULL) ||
		(in_tagID == NULL))
    {
        return NSMAIL_ERR_INVALIDPARAM;
    }

	/*Send the command.*/
    l_nReturnCode = IO_send( in_pimap4->pIO, in_command, TRUE);
    if ( l_nReturnCode != NSMAIL_OK )
	{
		return l_nReturnCode;
	}

	/*Store the callback information in a list.*/
	l_pCommandObject = (CommandObject_t*)malloc(sizeof(CommandObject_t));
    if(l_pCommandObject == NULL)
    {
        return NSMAIL_ERR_OUTOFMEMORY;
    }
	memset(l_pCommandObject, 0, sizeof(CommandObject_t));

	l_pCommandObject->pTag = in_tagID;
	l_pCommandObject->pOutputStream = in_pOutputStream;
	l_pCommandObject->pCommand = in_command;
	l_pCommandObject->next = NULL;

    return AddElement(&(in_pimap4->pPendingCommands), l_pCommandObject);
}

/**
* Prepare the command string, send the command and record the command sent
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_cmdTokens[] An array of command tokens of size 10
* @param out_ppCommand The command object
* @return int The return code
*/
int deployCommand(imap4Client_t* in_pimap4, char* in_cmdTokens[], char** out_ppTagID)
{
	int l_nReturnCode = NSMAIL_OK;
	int i = 0;
	char* l_szCommand = NULL;
	int l_nTagLength = 0;
	char l_tagBuffer[100];
	char* l_szTagID = NULL;
	int l_nCmdLength = 0;

	/*Check for connection*/
	if(!in_pimap4->bConnected)
	{
		return NSMAIL_ERR_IO_CONNECT;
	}

	/*Create a tag*/
	memset(l_tagBuffer, 0, 100);
	l_nTagLength = sprintf(l_tagBuffer, "%d", in_pimap4->tagID);
	in_pimap4->tagID++;

	/*Store the tag that will be stored within the pending list*/
	l_szTagID = (char*)malloc((l_nTagLength + 1)*sizeof(char));
	if(l_szTagID == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szTagID, 0, l_nTagLength + 1);
	memcpy(l_szTagID, l_tagBuffer, l_nTagLength);

	l_nCmdLength += l_nTagLength + 2; /*Add 1 for terminating NULL and 1 for extra space character*/

	/*Determine length of command string*/
	for(i=0; i<MAXTOKENS; i++)
	{
		if(in_cmdTokens[i] == NULL)	
		{
			break; 
		}
		l_nCmdLength += strlen(in_cmdTokens[i]);
	}

	/*Construct the command*/
	l_szCommand = (char*)malloc(l_nCmdLength * sizeof(char));
	if(l_szCommand == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szCommand, 0, l_nCmdLength * sizeof(char));
	memcpy(l_szCommand, l_szTagID, l_nTagLength);

	strcat(l_szCommand, " ");
	for(i=0; i<MAXTOKENS; i++)
	{
		if(in_cmdTokens[i] == NULL)	
		{
			break; 
		}
		strcat(l_szCommand, in_cmdTokens[i]);
	}

	/*Send the command.*/
	l_nReturnCode = imap4_sendCmd(in_pimap4, l_szCommand, l_szTagID, NULL);
	if(l_nReturnCode != NSMAIL_OK)
	{
		return l_nReturnCode;
	}

	/*Set output variables*/
	if(out_ppTagID != NULL)
	{
		/*Make a copy of the tag to give back to the user*/
		/*NOTE: imap4Tag_free must be called by the user*/
		l_szTagID = (char*)malloc((l_nTagLength + 1)*sizeof(char));
		if(l_szTagID == NULL)
		{
			return NSMAIL_ERR_OUTOFMEMORY;
		}
		memset(l_szTagID, 0, (l_nTagLength + 1)*sizeof(char));
		memcpy(l_szTagID, l_tagBuffer, l_nTagLength);
		(*out_ppTagID) = l_szTagID;
	}

	return l_nReturnCode;
}



/**
* Resolves the server response type
* @param in_szToken1 The first token of the response
* @param in_szToken2 The second token of the response
* @param in_szToken3 The third token of the response
* @return int The response type
*/
int resolveType(char* in_szToken1, char* in_szToken2, char* in_szToken3)
{
	/*Determine the server response type*/
	if(!strcmp(g_szStar, in_szToken1))
	{
		switch(in_szToken2[0])
		{
			case 'C':
			{
				if(!strcmp(g_szCapability, in_szToken2))
				{
					return RT_CAPABILITY;
				}
				break;
			}
			case 'L':
			{
				if(!strcmp(g_szList, in_szToken2))
				{
					return RT_LIST;
				}
				else if(!strcmp(g_szLsub, in_szToken2))
				{
					return RT_LSUB;
				}
				else if(!strcmp(g_szListRights, in_szToken2))
				{
					return RT_LISTRIGHTS;
				}
				break;
			}
			case 'S':
			{
				if(!strcmp(g_szStatus, in_szToken2))
				{
					return RT_STATUS;
				}
				else if(!strcmp(g_szSearch, in_szToken2))
				{
					return RT_SEARCH;
				}
				break;
			}
			case 'F':
			{
				if(!strcmp(g_szFlags, in_szToken2))
				{
					return RT_FLAGS;
				}
				break;
			}
			case 'O':
			{
				if(!strcmp(g_szOk, in_szToken2))
				{
					return RT_OK;
				}
				break;
			}
			case 'N':
			{
				if(!strcmp(g_szNameSpace, in_szToken2))
				{
					return RT_NAMESPACE;
				}
				else if(!strcmp(g_szNo, in_szToken2))
				{
					return RT_NO;
				}
				break;
			}
			case 'P':
			{
				if(!strcmp(g_szPreauth, in_szToken2))
				{
					return RT_PREAUTH;
				}
				break;
			}
			case 'B':
			{
				if(!strcmp(g_szBye, in_szToken2))
				{
					return RT_BYE;
				}
				else if(!strcmp(g_szBad, in_szToken2))
				{
					return RT_BAD;
				}
				break;
			}
			case 'A':
			{
				if(!strcmp(g_szAcl, in_szToken2))
				{
					return RT_ACL;
				}
				break;
			}
			case 'M':
			{
				if(!strcmp(g_szMyRights, in_szToken2))
				{
					return RT_MYRIGHTS;
				}
				break;
			}
			default:
			{
				break;
			}
		}

		switch(in_szToken3[0])
		{
			case 'R':
			{
				if(!strcmp(g_szRecent, in_szToken3))
				{
					return RT_RECENT;
				}
				break;
			}
			case 'E':
			{
				if(!strcmp(g_szExpunge, in_szToken3))
				{
					return RT_EXPUNGE;
				}
				else if(!strcmp(g_szExists, in_szToken3))
				{
					return RT_EXISTS;
				}
				break;
			}
			case 'F':
			{
				if(!strcmp(g_szFetch, in_szToken3))
				{
					return RT_FETCH;
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}
	else if(!strcmp(g_szPlus, in_szToken1))
	{
		return RT_PLUS;
	}
	else
	{
		switch(in_szToken2[0])
		{
			case 'O':
			{
				if(!strcmp(g_szOk, in_szToken2))
				{
					return RT_OKTAGGED;
				}
				break;
			}
			case 'N':
			{
				if(!strcmp(g_szNo, in_szToken2))
				{
					return RT_NOTAGGED;
				}
				break;
			}
			case 'B':
			{
				if(!strcmp(g_szBad, in_szToken2))
				{
					return RT_BADTAGGED;
				}
				break;
			}
			default:
			{
				break;
			}
		}

		return RT_RAWRESPONSE;

	}
	
	return NSMAIL_OK;
}

/*********************************************************************************** 
* Parsing Utilities
************************************************************************************/ 

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseTaggedLine(imap4Client_t* in_pimap4, char* in_data)
{
	return parseEndingLine(in_pimap4, in_data, sink_taggedLine);
}

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseError(imap4Client_t* in_pimap4, char* in_data)
{
	return parseEndingLine(in_pimap4, in_data, sink_error);
}

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseEndingLine(imap4Client_t* in_pimap4, char* in_data, endLine in_sinkMethod)
{
	char* l_szBegin = NULL;
	char* l_szEnd = NULL;
	char* l_szTag = NULL;
	char* l_szStatus = NULL;
	char* l_szReason = NULL;
	int l_nLength = 0;
	imap4Sink_t* l_sink = in_pimap4->pimap4Sink;
	LinkedList_t* l_pLinkedObject = NULL;
	LinkedList_t* l_pPrevLinkedObject = NULL;
	CommandObject_t* l_pCommandObject = NULL;

	/*Extract the tag*/
	l_szBegin = in_data;
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_nLength = l_szEnd - l_szBegin;
	l_szTag = (char*)malloc((l_nLength + 1)*sizeof(char));
	if(l_szTag == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szTag, 0, (l_nLength + 1)*sizeof(char));
	memcpy(l_szTag, l_szBegin, l_nLength);

	/*Extract the status*/
	l_szBegin = l_szEnd + 1;
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_nLength = l_szEnd - l_szBegin;
	l_szStatus = (char*)malloc((l_nLength + 1)*sizeof(char));
	if(l_szStatus == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szStatus, 0, (l_nLength + 1)*sizeof(char));
	memcpy(l_szStatus, l_szBegin, l_nLength);

	/*Extract the reason*/
	l_szBegin = l_szEnd + 1;
	l_szEnd = strchr(l_szBegin, '\r');
	if(l_szEnd == NULL)
	{
		l_nLength = strlen(l_szBegin);
	}
	l_nLength = l_szEnd - l_szBegin;
	l_szReason = (char*)malloc((l_nLength + 1)*sizeof(char));
	if(l_szReason == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szReason, 0, (l_nLength + 1)*sizeof(char));
	memcpy(l_szReason, l_szBegin, l_nLength);

	in_sinkMethod(l_sink, l_szTag, l_szStatus, l_szReason);

	/*Remove the command from the pending commands list*/
	l_pLinkedObject = in_pimap4->pPendingCommands;
	while(l_pLinkedObject != NULL)
	{
		l_pCommandObject = l_pLinkedObject->pData;
		if(!strcmp(l_szTag, l_pCommandObject->pTag))
		{
			/*Found the associated command object so remove it.*/
			if(l_pLinkedObject == in_pimap4->pPendingCommands)
			{
				in_pimap4->pPendingCommands = in_pimap4->pPendingCommands->next;
			}
			else
			{
				l_pPrevLinkedObject->next = l_pLinkedObject->next;
			}

			/* Free the current link in the pending commands list. */
			myFree(l_pCommandObject->pTag);
			myFree(l_pCommandObject->pCommand);
			myFree(l_pCommandObject);
			myFree(l_pLinkedObject);
			break;
		}
		l_pPrevLinkedObject = l_pLinkedObject;
		l_pLinkedObject = l_pLinkedObject->next;
	}

	myFree(l_szTag);
	myFree(l_szStatus);
	myFree(l_szReason);

	return NSMAIL_OK;

}

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseOk(imap4Client_t* in_pimap4, char* in_data)
{
	int l_nReturnCode = NSMAIL_OK;
	LinkedList_t* l_pLinkedObject = NULL;
	LinkedList_t* l_pPrevLinkedObject = NULL;
	CommandObject_t* l_pCommandObject = NULL;

	l_nReturnCode =  parseOkNoBad(in_pimap4, in_data, sink_ok);
	if(l_nReturnCode != NSMAIL_OK)
	{
		return l_nReturnCode;
	}

	/*If OK due to initial connect remove command from pending list*/
	l_pLinkedObject = in_pimap4->pPendingCommands;
	while(l_pLinkedObject != NULL)
	{
		l_pCommandObject = l_pLinkedObject->pData;
		if(!strcmp(g_szConnect, l_pCommandObject->pCommand))
		{
			/*Found the associated command object so remove it.*/
			if(l_pLinkedObject == in_pimap4->pPendingCommands)
			{
				/*Remove head*/
				in_pimap4->pPendingCommands = in_pimap4->pPendingCommands->next;
			}
			else
			{
				l_pPrevLinkedObject->next = l_pLinkedObject->next;
			}

			/* Free the current link in the pending commands list. */
			myFree(l_pCommandObject->pTag);
			myFree(l_pCommandObject);
			myFree(l_pLinkedObject);
			break;
		}
		l_pPrevLinkedObject = l_pLinkedObject;
		l_pLinkedObject = l_pLinkedObject->next;
	}

	return l_nReturnCode;
}

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseOkNoBad(imap4Client_t* in_pimap4, char* in_data, oknobad in_sinkMethod)
{
	char* l_szBegin = NULL;
	char* l_szEnd = NULL;
	char* l_szResponseCode = NULL;
	char* l_szInformation = NULL;
	int l_nLength = 0;
	imap4Sink_t* l_sink = in_pimap4->pimap4Sink;

	/*Skip over * */
	l_szBegin = in_data;
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Retrieve the response code*/
	l_szEnd = strchr(l_szBegin, '[');
	if(l_szEnd != NULL)
	{
		l_szBegin = l_szEnd;
		l_szEnd = strchr(l_szBegin, ']');
		if(l_szEnd == NULL)
		{
			NSMAIL_ERR_PARSE;
		}
		/*A response code has been found*/
		l_nLength = l_szEnd - l_szBegin + 1;
		l_szResponseCode = (char*)malloc((l_nLength + 1)*sizeof(char));
		if(l_szResponseCode == NULL)
		{
			return NSMAIL_ERR_OUTOFMEMORY;
		}
		memset(l_szResponseCode, 0, (l_nLength + 1)*sizeof(char));
		memcpy(l_szResponseCode, l_szBegin, l_nLength);
	}
	else
	{
		/*Skip over ok */
		l_szEnd = strchr(l_szBegin, ' ');
		if(l_szEnd == NULL)
		{
			return NSMAIL_ERR_PARSE;
		}
	}

	/*Retrieve the human readable information*/
	/*Find the end of line*/
	l_szBegin = l_szEnd + 1;

	/*Skip leading white spaces*/
	l_szBegin = skipWhite(l_szBegin, strlen(l_szBegin));
	
	l_szEnd = strchr(l_szBegin, '\r');
	if(l_szEnd == NULL)
	{
		l_nLength = strlen(l_szBegin);
	}
	else
	{
		l_nLength = l_szEnd - l_szBegin;
	}
	l_szInformation = (char*)malloc((l_nLength + 1) * sizeof(char));
	if(l_szInformation == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szInformation, 0, (l_nLength + 1)*sizeof(char));
	l_szInformation = memcpy(l_szInformation, l_szBegin, l_nLength);

	in_sinkMethod(l_sink, l_szResponseCode, l_szInformation);

	myFree(l_szResponseCode);
	myFree(l_szInformation);

	return NSMAIL_OK;
}

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseNoBad(imap4Client_t* in_pimap4, const char* in_response)
{
	char* l_szBegin = NULL;
	char* l_szEnd = NULL;
	char* l_szStatus = NULL;
	char* l_szInformation = NULL;
	int l_nLength = 0;
	imap4Sink_t* l_sink = in_pimap4->pimap4Sink;

	/*Retrieve the no or bad*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szStatus = (char*)malloc((l_szEnd - l_szBegin + 1) * sizeof(char));
	if(l_szStatus == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szStatus, 0, (l_szEnd - l_szBegin + 1) * sizeof(char));
	memcpy(l_szStatus, l_szBegin, l_szEnd - l_szBegin);

	/*Retrieve the human readable information*/
	/*Find the end of line*/
	l_szBegin = l_szEnd + 1;
	l_szEnd = strchr(l_szBegin, '\r');
	if(l_szEnd == NULL)
	{
		l_nLength = strlen(l_szBegin);
	}
	else
	{
		l_nLength = l_szEnd - l_szBegin;
	}
	l_szInformation = (char*)malloc((l_nLength + 1) * sizeof(char));
	if(l_szInformation == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szInformation, 0, (l_nLength + 1)*sizeof(char));
	l_szInformation = memcpy(l_szInformation, l_szBegin, l_nLength);

	sink_error(l_sink, g_szUntagged, l_szStatus, l_szInformation);

	myFree(l_szStatus);
	myFree(l_szInformation);

	return NSMAIL_OK;
}


/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseCapability(imap4Client_t* in_pimap4, char* in_data)
{
	char* l_szBegin = NULL;
	char* l_szEnd = NULL;
	char* l_szListing = NULL;
	imap4Sink_t* l_sink = in_pimap4->pimap4Sink;
	int l_nLength = 0;

	/*Skip over the * */
	l_szBegin = in_data;
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Skip over CAPABILITY*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}

	/*The rest is the capability listing*/
	l_szBegin = l_szEnd + 1;

	l_szEnd = strchr(l_szBegin, '\r');
	if(l_szEnd == NULL)
	{
		l_nLength = strlen(l_szBegin);
	}
	else
	{
		l_nLength = l_szEnd - l_szBegin;
	}
	l_szListing = (char*)malloc((l_nLength + 1)*sizeof(char));
	if(l_szListing == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szListing, 0, (l_nLength + 1)*sizeof(char));
	memcpy(l_szListing, l_szBegin, l_nLength);

	sink_capability(l_sink, l_szListing);
	
	myFree(l_szListing);

	return NSMAIL_OK;
}

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseList(imap4Client_t* in_pimap4, char* in_data)
{
	return parseMailboxInfo(in_pimap4, in_data, sink_list);
}



/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseLsub(imap4Client_t* in_pimap4, char* in_data)
{
	return parseMailboxInfo(in_pimap4, in_data, sink_lsub);
}

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseMailboxInfo(imap4Client_t* in_pimap4, char* in_data, parseMbox in_sinkMethod)
{
	char* l_szBegin = NULL;
	char* l_szEnd = NULL;
	char* l_attributes = NULL;
	char* l_delimiter = NULL;
	char* l_name = NULL;
	int l_nLength = 0;
	imap4Sink_t* l_sink = in_pimap4->pimap4Sink;

	/*Skip over the * */
	l_szBegin = in_data;
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Skip over LIST*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Extract the attributes*/
	l_szEnd = matchBrackets(l_szBegin, &l_attributes);
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}

	/*Extract the delimiter*/
	l_szBegin = l_szEnd;
	l_szEnd = strchr(l_szBegin, '"');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	l_szEnd = strchr(l_szBegin, '"');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_nLength = l_szEnd - l_szBegin;
	l_delimiter = (char*)malloc((l_nLength + 1)*sizeof(char));
	if(l_delimiter == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_delimiter, 0, (l_nLength + 1)*sizeof(char));
	memcpy(l_delimiter, l_szBegin, l_nLength);
	l_szBegin = l_szEnd;

	/*Extract the name*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	l_szEnd = strchr(l_szBegin, '\r');
	if(l_szEnd == NULL)
	{
		l_nLength = strlen(l_szBegin);	
	}
	else
	{
		l_nLength = l_szEnd - l_szBegin;
	}
	l_name = (char*)malloc((l_nLength + 1)*sizeof(char));
	if(l_name == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_name, 0, (l_nLength + 1)*sizeof(char));
	memcpy(l_name, l_szBegin, l_nLength);

	/*Dispatch the parsed data*/
	in_sinkMethod(l_sink, l_attributes, l_delimiter, l_name);

	myFree(l_attributes);
	myFree(l_delimiter);
	myFree(l_name);

	return NSMAIL_OK;
}

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseStatus(imap4Client_t* in_pimap4, char* in_data)
{
	char* l_szBegin = NULL;
	char* l_szEnd = NULL;
	char* l_szDataItem = NULL;
	char* l_szDataValue = NULL;
	int l_nValue = -1;
	imap4Sink_t* l_sink = in_pimap4->pimap4Sink;
	int l_nLength = 0;

	/*Skip over the * */
	l_szEnd = strchr(in_data, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Skip over STATUS*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Skip over the opening parenthesis*/
	l_szEnd = strchr(l_szBegin, '(');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	while(l_szBegin != NULL)
	{
		/*Extract the data item*/
		l_szEnd = strchr(l_szBegin, ' ');
		if(l_szEnd == NULL)
		{
			break;
		}
		l_nLength = l_szEnd - l_szBegin;
		l_szDataItem = (char*)malloc((l_nLength + 1)*sizeof(char));
		if(l_szDataItem == NULL)
		{
			return NSMAIL_ERR_OUTOFMEMORY;
		}
		memset(l_szDataItem, 0, (l_nLength + 1)*sizeof(char));
		memcpy(l_szDataItem, l_szBegin, l_nLength);
		l_szBegin = l_szEnd + 1;

		/*Extract the value of the data item*/
		l_szEnd = strchr(l_szBegin, ' ');
		if(l_szEnd == NULL)
		{
			l_szEnd = strchr(l_szBegin, ')');
			if(l_szEnd == NULL)
			{
				return NSMAIL_ERR_PARSE;
			}
		}
		l_nLength = l_szEnd - l_szBegin;
		l_szDataValue = (char*)malloc((l_nLength + 1)*sizeof(char));
		if(l_szDataValue == NULL)
		{
			return NSMAIL_ERR_OUTOFMEMORY;
		}
		memset(l_szDataValue, 0, (l_nLength + 1)*sizeof(char));
		memcpy(l_szDataValue, l_szBegin, l_nLength);
		l_nValue = atoi(l_szDataValue);
		l_szBegin = l_szEnd + 1;

		/*Dispatch data value to appropriate sink method*/
		l_nLength = strlen(l_szDataItem);
		switch((l_szDataItem[0]))
		{
			case 'M':
			{
				if(	(l_nLength > 7) &&
					(l_szDataItem[1] == 'E') &&
					(l_szDataItem[2] == 'S') &&
					(l_szDataItem[3] == 'S') &&
					(l_szDataItem[4] == 'A') &&
					(l_szDataItem[5] == 'G') &&
					(l_szDataItem[6] == 'E') &&
					(l_szDataItem[7] == 'S'))
				{
					sink_statusMessages(l_sink, l_nValue);
				}
				break;
			}
			case 'R':
			{
				if(	(l_nLength > 5) &&
					(l_szDataItem[1] == 'E') &&
					(l_szDataItem[2] == 'C') &&
					(l_szDataItem[3] == 'E') &&
					(l_szDataItem[4] == 'N') &&
					(l_szDataItem[5] == 'T'))
				{
					sink_statusRecent(l_sink, l_nValue);
				}
				break;
			}
			case 'U':
			{
				if(	(l_nLength > 10) &&
					(l_szDataItem[1] == 'I') &&
					(l_szDataItem[2] == 'D') &&
					(l_szDataItem[3] == 'V') &&
					(l_szDataItem[4] == 'A') &&
					(l_szDataItem[5] == 'L') &&
					(l_szDataItem[6] == 'I') &&
					(l_szDataItem[7] == 'D') &&
					(l_szDataItem[8] == 'I') &&
					(l_szDataItem[9] == 'T') &&
					(l_szDataItem[10] == 'Y'))
				{
					sink_statusUidvalidity(l_sink, l_nValue);
				}
				else if(	(l_nLength > 6) &&
							(l_szDataItem[1] == 'I') &&
							(l_szDataItem[2] == 'D') &&
							(l_szDataItem[3] == 'N') &&
							(l_szDataItem[4] == 'E') &&
							(l_szDataItem[5] == 'X') &&
							(l_szDataItem[6] == 'T'))
				{
					sink_statusUidnext(l_sink, l_nValue);
				}
				else if(	(l_nLength > 5) &&
							(l_szDataItem[1] == 'N') &&
							(l_szDataItem[2] == 'S') &&
							(l_szDataItem[3] == 'E') &&
							(l_szDataItem[4] == 'E') &&
							(l_szDataItem[5] == 'N'))
				{
					sink_statusUnseen(l_sink, l_nValue);
				}
				break;
			}
			default:
			{
				break;
			}
		}
		myFree(l_szDataItem);
		myFree(l_szDataValue);
	}

	return NSMAIL_OK;
}

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseSearch(imap4Client_t* in_pimap4, char* in_data)
{
	char* l_szBegin = NULL;
	char* l_szEnd = NULL;
	char* l_szMessage = NULL;
	void* l_reference = NULL;
	int l_nMessage = -1;
	imap4Sink_t* l_sink = in_pimap4->pimap4Sink;
	int l_nLength = 0;

	/*Skip over the * */
	l_szEnd = strchr(in_data, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Skip over SEARCH*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		l_szEnd = strchr(l_szBegin, '\r');
		if(l_szEnd == NULL)
		{
			return NSMAIL_ERR_PARSE;
		}
	}
	l_szBegin = l_szEnd + 1;

	sink_searchStart(l_sink);
	while(l_szBegin != NULL)
	{
		/*Extract the message number*/
		l_szEnd = strchr(l_szBegin, ' ');
		if(l_szEnd == NULL)
		{
			l_szEnd = strchr(l_szBegin, '\n');
			if(l_szEnd == NULL)
			{
				break;
			}
		}
		l_nLength = l_szEnd - l_szBegin;
		if(l_nLength < 1)
		{
			break;
		}
		l_szMessage = (char*)malloc((l_nLength + 1)*sizeof(char));
		if(l_szMessage == NULL)
		{
			return NSMAIL_ERR_OUTOFMEMORY;
		}
		memset(l_szMessage, 0, (l_nLength + 1)*sizeof(char));
		memcpy(l_szMessage, l_szBegin, l_nLength);
		l_nMessage = atoi(l_szMessage);
		sink_search(l_sink, l_nMessage);
		l_szBegin = l_szEnd + 1;
		myFree(l_szMessage);
	}
	sink_searchEnd(l_sink);

	return NSMAIL_OK;
}

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseFlags(imap4Client_t* in_pimap4, char* in_data)
{
	char* l_szBegin = NULL;
	char* l_szEnd = NULL;
	char* l_szFlags = NULL;
	imap4Sink_t* l_sink = in_pimap4->pimap4Sink;
	int l_nLength = 0;
	int l_nIndex = -1;

	/*Skip over the * */
	l_szEnd = strchr(in_data, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Skip over FLAGS*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Extract the value of the flags*/
	l_szEnd = matchBrackets(l_szBegin, &l_szFlags);
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	sink_flags(l_sink, l_szFlags);
	
	myFree(l_szFlags);

	return NSMAIL_OK;
}

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseBye(imap4Client_t* in_pimap4, char* in_data)
{
	char* l_szBegin = NULL;
	char* l_szEnd = NULL;
	char* l_szReason = NULL;
	imap4Sink_t* l_sink = in_pimap4->pimap4Sink;
	int l_nLength = 0;
	LinkedList_t* l_pLinkedObject = NULL;
	LinkedList_t* l_pPrevLinkedObject = NULL;
	CommandObject_t* l_pCommandObject = NULL;

	/*Skip over the * */
	l_szEnd = strchr(in_data, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Skip over BYE*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*The rest is the reason*/
	l_szEnd = strchr(l_szBegin, '\r');
	if(l_szEnd == NULL)
	{
		l_nLength = strlen(l_szBegin);
	}
	else
	{
		l_nLength = l_szEnd - l_szBegin;
	}
	l_szReason = (char*)malloc((l_nLength + 1)*sizeof(char));
	if(l_szReason == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szReason, 0, (l_nLength + 1)*sizeof(char));
	memcpy(l_szReason, l_szBegin, l_nLength);

	sink_bye(l_sink, l_szReason);
	myFree(l_szReason);

	/*Remove all pending commands*/
	/*Remove the command from the pending commands list*/
	l_pLinkedObject = in_pimap4->pPendingCommands;
	while(l_pLinkedObject != NULL)
	{
		/*Remove the head of the linked list*/
		l_pCommandObject = l_pLinkedObject->pData;
		in_pimap4->pPendingCommands = in_pimap4->pPendingCommands->next;

		/* Free the current link in the pending commands list. */
		myFree(l_pCommandObject->pTag);
		myFree(l_pCommandObject->pCommand);
		myFree(l_pCommandObject);
		myFree(l_pLinkedObject);
		l_pLinkedObject = in_pimap4->pPendingCommands;
	}

	/*Re-initialize internal variables*/
	IO_disconnect(in_pimap4->pIO);
	in_pimap4->timeout = -1;
	in_pimap4->pPendingCommands = NULL;
	in_pimap4->chunkSize = 1024;
	in_pimap4->pLiteralStream = NULL;
	in_pimap4->tagID = 0;
	in_pimap4->bReadHeader = FALSE;
	in_pimap4->bConnected = FALSE;

	return NSMAIL_OK;
}

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseRecent(imap4Client_t* in_pimap4, char* in_data)
{
	return getNumber(in_pimap4, in_data, sink_recent);
}

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseExists(imap4Client_t* in_pimap4, char* in_data)
{
	return getNumber(in_pimap4, in_data, sink_exists);
}

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseExpunge(imap4Client_t* in_pimap4, char* in_data)
{
	return getNumber(in_pimap4, in_data, sink_expunge);
}


/**
* Retrieves the number from responses of the form "* 3 RECENT"
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int getNumber(imap4Client_t* in_pimap4, char* in_data, messageNumber in_sinkMethod)
{
	char* l_szBegin = NULL;
	char* l_szEnd = NULL;
	char* l_szNumber = NULL;
	imap4Sink_t* l_sink = in_pimap4->pimap4Sink;
	int l_nLength = 0;
	int l_nNumber = -1;

	/*Skip over the * */
	l_szBegin = in_data;
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Extract the number*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_nLength = l_szEnd - l_szBegin;
	l_szNumber = (char*)malloc((l_nLength + 1)*sizeof(char));
	if(l_szNumber == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szNumber, 0, (l_nLength + 1)*sizeof(char));
	memcpy(l_szNumber, l_szBegin, l_nLength);
	l_nNumber = atoi(l_szNumber);

	in_sinkMethod(l_sink, l_nNumber);
	myFree(l_szNumber);

	return NSMAIL_OK;
}


/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseFetch(imap4Client_t* in_pimap4, char* in_data)
{
	char* l_szBegin = NULL;
	char* l_szEnd = NULL;
	int l_nLength = 0;
	imap4Sink_t* l_sink = in_pimap4->pimap4Sink;
	char* l_szMessage = NULL;
	int l_nMessage = -1;
	char* l_szDataItem = NULL;
	char* l_szDataValue = NULL;
	char** l_szEnvelopeFields = NULL;
	char* l_szLeftOverHeader = NULL;
	int l_nIndex = -1;

	char* l_szTotalSize = NULL;
	int l_nTotalSize = 0;
	int l_nBytesRead = 0;
	int l_nBytes = 0;
	int l_nReadChunk = 0;
	char* l_dataChunklet = NULL;
	char* l_szResponse = NULL;
	int l_nReturnCode = 0;

	char* l_szLine = NULL;
	char* l_szField = NULL;
	char* l_szFieldValue = NULL;
	char* l_szHBegin = NULL;
	char* l_szHEnd = NULL;

	/*Skip over the * */
	l_szBegin = in_data;
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Extract the message number*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_nLength = l_szEnd - l_szBegin;
	l_szMessage = (char*)malloc((l_nLength + 1)*sizeof(char));
	if(l_szMessage == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szMessage, 0, (l_nLength + 1)*sizeof(char));
	memcpy(l_szMessage, l_szBegin, l_nLength);
	l_nMessage = atoi(l_szMessage);
	l_szBegin = l_szEnd + 1;

	/*Skip over the FETCH*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Skip over the opening bracket */
	l_szEnd = strchr(l_szBegin, '(');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	sink_fetchStart(l_sink, l_nMessage);
	while(l_szBegin != NULL)
	{
		/*Skip leading white spaces*/
		l_szBegin = skipWhite(l_szBegin, strlen(l_szBegin));
		
		/*Extract the data item*/
		l_szEnd = strchr(l_szBegin, ' ');
		if(l_szEnd == NULL)
		{
			break;
		}
		l_nLength = l_szEnd - l_szBegin;
		l_szDataItem = (char*)malloc((l_nLength + 1)*sizeof(char));
		if(l_szDataItem == NULL)
		{
			return NSMAIL_ERR_OUTOFMEMORY;
		}
		memset(l_szDataItem, 0, (l_nLength + 1)*sizeof(char));
		memcpy(l_szDataItem, l_szBegin, l_nLength);
		l_szBegin = l_szEnd + 1;

		/*Extract the value of the data item*/

		switch(resolveFetchDataItem(in_pimap4, l_szDataItem))
		{
			case BODYSTRUCTURE:
			{
				l_szEnd = matchBrackets(l_szBegin, &l_szDataValue);
				if(l_szEnd == NULL)
				{
					return NSMAIL_ERR_PARSE;
				}
				sink_fetchBodyStructure(l_sink, l_szDataValue);
				break;
			}
			case MESSAGE:
			case RFC822TEXT:
			{
				/*Extract the size of the message block*/
				l_szEnd = strchr(l_szBegin, '{');
				if(l_szEnd == NULL)
				{
					return NSMAIL_ERR_PARSE;
				}
				l_szBegin = l_szEnd + 1;
				l_szEnd = strchr(l_szBegin, '}');
				if(l_szEnd == NULL)
				{
					return NSMAIL_ERR_PARSE;
				}
				l_nLength = l_szEnd - l_szBegin;
				l_szTotalSize = (char*)malloc((l_nLength + 1)*sizeof(char));
				if(l_szTotalSize == NULL)
				{
					return NSMAIL_ERR_OUTOFMEMORY;
				}
				memset(l_szTotalSize, 0, (l_nLength + 1)*sizeof(char));
				memcpy(l_szTotalSize, l_szBegin, l_nLength);
				l_nTotalSize = atoi(l_szTotalSize);

				/*Read in the block of data*/
				while(l_nBytesRead < l_nTotalSize)
				{
					/*Determine the size of the next chunk to read*/
					if((l_nBytesRead + in_pimap4->chunkSize) > l_nTotalSize)
					{
						l_nReadChunk = l_nTotalSize - l_nBytesRead;
						myFree(l_dataChunklet);
						l_dataChunklet = NULL;
					}
					else
					{
						l_nReadChunk = in_pimap4->chunkSize;
					}
					if(l_dataChunklet == NULL)
					{
						l_dataChunklet = (char*)malloc((l_nReadChunk + 1)*sizeof(char));
						if(l_dataChunklet == NULL)
						{
							return NSMAIL_ERR_OUTOFMEMORY;
						}
					}
					memset(l_dataChunklet, 0,  (l_nReadChunk + 1)*sizeof(char));

					/*Read the chunk in*/
					IO_read(in_pimap4->pIO, l_dataChunklet, l_nReadChunk, &l_nBytes);
					l_nBytesRead += l_nBytes;

					if(in_pimap4->bReadHeader)
					{
						/*Parse out the header field and value*/

						for(l_nIndex = 0; l_nIndex < l_nBytes; l_nIndex++)
						{
							l_nReturnCode = readHeaderLine(l_dataChunklet, &l_szLeftOverHeader, l_nBytes, l_nIndex, &l_szLine, &l_nIndex);
							if(l_nReturnCode == NSMAIL_ERR_PARSE)
							{
								/*Leftover header info*/
								myFree(l_szLine);
								l_szLine = NULL;
								break;
							}
							else if(l_nReturnCode != NSMAIL_OK)
							{
								myFree(l_szLine);
								l_szLine = NULL;
								return l_nReturnCode;
							}

							if(!strcmp(l_szLine, g_szCR))
							{
								/*This is the end of the message header*/
								in_pimap4->bReadHeader = FALSE;
								myFree(l_szLine);
								l_szLine = NULL;
								break;
							}
							else
							{
								/*Parse out the field*/
								l_szBegin = l_szLine;
								l_szEnd = strchr(l_szBegin, ':');
								if(l_szEnd == NULL)
								{
									return NSMAIL_ERR_PARSE;
								}
								l_nLength = l_szEnd - l_szBegin;
								l_szField = (char*)malloc((l_nLength + 1)*sizeof(char));
								if(l_szField == NULL)
								{
									return NSMAIL_ERR_OUTOFMEMORY;
								}
								memset(l_szField, 0, (l_nLength + 1)*sizeof(char));
								memcpy(l_szField, l_szBegin, l_nLength);
								l_szBegin = l_szEnd + 1;

								/*Parse out the field value*/
								l_szBegin = skipWhite(l_szBegin, strlen(l_szBegin));
								l_nLength = strlen(l_szBegin);
								if(l_szBegin[l_nLength -1] == '\r')
								{
									l_nLength--;
								}
								l_szFieldValue = (char*)malloc((l_nLength + 1)*sizeof(char));
								if(l_szFieldValue == NULL)
								{
									return NSMAIL_ERR_OUTOFMEMORY;
								}
								memset(l_szFieldValue, 0, (l_nLength + 1)*sizeof(char));
								memcpy(l_szFieldValue, l_szBegin, l_nLength);
								sink_fetchHeader(l_sink, l_szField, l_szFieldValue);
							}
							/*Clean up*/
							myFree(l_szLine);
							myFree(l_szField);
							myFree(l_szFieldValue);
							l_szLine = NULL;
							l_szField = NULL;
							l_szFieldValue = NULL;
						}
					}
					sink_fetchData(l_sink, l_dataChunklet, l_nBytesRead, l_nTotalSize);
				}
				
				l_nReturnCode = IO_readDLine(in_pimap4->pIO, &l_szResponse);
				if ( l_nReturnCode != NSMAIL_OK )
				{
					/* A timeout or an error has occured. */
					return l_nReturnCode;
				}
				l_szEnd = l_szResponse;

				myFree(l_dataChunklet);
				myFree(l_szTotalSize);
				break;
			}
			case RFC822SIZE:
			{
				l_szEnd = strchr(l_szBegin, ' ');
				if(l_szEnd == NULL)
				{
					l_szEnd = strchr(l_szBegin, ')');
					if(l_szEnd == NULL)
					{
						return NSMAIL_ERR_PARSE;
					}
				}
				l_nLength = l_szEnd - l_szBegin;
				l_szDataValue = (char*)malloc((l_nLength + 1)*sizeof(char));
				if(l_szDataValue == NULL)
				{
					return NSMAIL_ERR_OUTOFMEMORY;
				}
				memset(l_szDataValue, 0, (l_nLength + 1)*sizeof(char));
				memcpy(l_szDataValue, l_szBegin, l_nLength);
				sink_fetchSize(l_sink, atoi(l_szDataValue));
				break;
			}
			case UID:
			{
				l_szEnd = strchr(l_szBegin, ' ');
				if(l_szEnd == NULL)
				{
					l_szEnd = strchr(l_szBegin, ')');
					if(l_szEnd == NULL)
					{
						return NSMAIL_ERR_PARSE;
					}
				}
				l_nLength = l_szEnd - l_szBegin;
				l_szDataValue = (char*)malloc((l_nLength + 1)*sizeof(char));
				if(l_szDataValue == NULL)
				{
					return NSMAIL_ERR_OUTOFMEMORY;
				}
				memset(l_szDataValue, 0, (l_nLength + 1)*sizeof(char));
				memcpy(l_szDataValue, l_szBegin, l_nLength);
				sink_fetchUid(l_sink, atoi(l_szDataValue));
				break;
			}
			case FLAGS:
			{
				l_szEnd = matchBrackets(l_szBegin, &l_szDataValue);
				if(l_szEnd == NULL)
				{
					return NSMAIL_ERR_PARSE;
				}
				sink_fetchFlags(l_sink, l_szDataValue);
				break;
			}
			case ENVELOPE:
			{
				l_szEnvelopeFields = (char**)malloc(MAXENVFIELDS*sizeof(char*));
				if(l_szEnvelopeFields == NULL)
				{
					return NSMAIL_ERR_OUTOFMEMORY;
				}
				memset(l_szEnvelopeFields, 0, MAXENVFIELDS*sizeof(char*));

				/*Extract the entire value of the envelope*/
				l_szEnd = matchBrackets(l_szBegin, &l_szDataValue);
				if(l_szEnd == NULL)
				{
					return NSMAIL_ERR_PARSE;
				}

				/*Extract the date*/
				l_szEnd = matchFieldQuotes(l_szBegin, &l_szEnvelopeFields[0]);
				if(l_szEnd == NULL)
				{
					return NSMAIL_ERR_PARSE;
				}
				l_szBegin = l_szEnd + 1;
				
				/*Extract the subject*/
				l_szEnd = matchFieldQuotes(l_szBegin, &l_szEnvelopeFields[1]);
				if(l_szEnd == NULL)
				{
					return NSMAIL_ERR_PARSE;
				}
				l_szBegin = l_szEnd + 1;

				/*Extract the from, sender, reply-to, to, cc, bcc, in-reply-to*/
				for(l_nIndex = 2; l_nIndex < 8; l_nIndex++)
				{
					l_szEnd = matchFieldBrackets(l_szBegin, &l_szEnvelopeFields[l_nIndex]);
					if(l_szEnd == NULL)
					{
						return NSMAIL_ERR_PARSE;
					}
					l_szBegin = l_szEnd + 1;
				}

				/*Extract the in-reply-to*/
				l_szEnd = matchFieldQuotes(l_szBegin, &l_szEnvelopeFields[8]);
				if(l_szEnd == NULL)
				{
					return NSMAIL_ERR_PARSE;
				}
				l_szBegin = l_szEnd + 1;


				/*Extract the message-id*/
				l_szEnd = matchFieldQuotes(l_szBegin, &l_szEnvelopeFields[9]);
				if(l_szEnd == NULL)
				{
					return NSMAIL_ERR_PARSE;
				}
				l_szBegin = l_szEnd + 1;

				sink_fetchEnvelope(l_sink, l_szEnvelopeFields, 10);

				for(l_nIndex = 0; l_nIndex < MAXENVFIELDS; l_nIndex++)
				{
					myFree(l_szEnvelopeFields[l_nIndex]);
				}
				myFree(l_szEnvelopeFields);
				break;
			}
			case INTERNALDATE:
			{
				l_szEnd = strchr(l_szBegin, '"');
				if(l_szEnd == NULL)
				{
					return NSMAIL_ERR_PARSE;
				}
				l_szBegin = l_szEnd + 1;

				l_szEnd = strchr(l_szBegin, '"');
				if(l_szEnd == NULL)
				{
					return NSMAIL_ERR_PARSE;
				}
				l_nLength = l_szEnd - l_szBegin;
				l_szDataValue = (char*)malloc((l_nLength + 1)*sizeof(char));
				if(l_szDataValue == NULL)
				{
					return NSMAIL_ERR_OUTOFMEMORY;
				}
				memset(l_szDataValue, 0, (l_nLength + 1)*sizeof(char));
				memcpy(l_szDataValue, l_szBegin, l_nLength);

				sink_fetchInternalDate(l_sink, l_szDataValue);
				break;
			}
			default:
			{
				break;
			}
		}

		l_szBegin = l_szEnd + 1;
		myFree(l_szDataItem);
		myFree(l_szDataValue);
		l_szDataItem = NULL;
		l_szDataValue = NULL;
	}
	sink_fetchEnd(l_sink);


	myFree(l_szResponse);
	myFree(l_szMessage);

	return NSMAIL_OK;
}


/**
* Waits for a contiunation response and the send out the message literal
* @param in_data The response string
* @param in_pimap4 A pointer to the imap4 client structure
* @return int The return code
*/
static int parsePlus(imap4Client_t* in_pimap4, char* in_data)
{
	int l_nBytesRead = 0;
	int l_nReturnCode = 0;
	char* l_szBuffer = NULL;
	
	if((in_pimap4->pLiteralStream == NULL) || (in_data == NULL))
	{
		return NSMAIL_ERR_UNEXPECTED;
	}

	l_szBuffer = (char*)malloc(in_pimap4->chunkSize*sizeof(char));
	if(l_szBuffer == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szBuffer, 0, in_pimap4->chunkSize*sizeof(char));

	/*Write out the literal*/
	while(l_nBytesRead != -1)
	{
		l_nBytesRead = in_pimap4->pLiteralStream->read(in_pimap4->pLiteralStream->rock, l_szBuffer, in_pimap4->chunkSize);
		if(l_nBytesRead != -1)
		{
			l_nReturnCode = IO_write( in_pimap4->pIO, l_szBuffer, l_nBytesRead);

			if ( l_nReturnCode != NSMAIL_OK )
			{
				return l_nReturnCode;
			}

			memset(l_szBuffer, 0, in_pimap4->chunkSize*sizeof(char));
		}
	}

	l_nReturnCode = IO_write( in_pimap4->pIO, "\r\n", 2 );

	if ( l_nReturnCode != NSMAIL_OK )
	{
		return l_nReturnCode;
	}

    l_nReturnCode = IO_flush(in_pimap4->pIO);

	if ( l_nReturnCode != NSMAIL_OK )
	{
		return l_nReturnCode;
	}

	in_pimap4->pLiteralStream = NULL;
	myFree(l_szBuffer);

	return NSMAIL_OK;
}

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseNamespace(imap4Client_t* in_pimap4, char* in_data)
{
	char* l_szBegin = NULL;
	char* l_szEnd = NULL;
	char* l_szNSBegin = NULL;
	char* l_szNSEnd = NULL;
	imap4Sink_t* l_sink = in_pimap4->pimap4Sink;
	int l_nLength = 0;
	char* l_szNamespace = NULL;
	char* l_szAttribute = NULL;


	/*Skip over the * */
	l_szBegin = in_data;
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Skip over NAMESPACE*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	sink_nameSpaceStart(l_sink);

	/*Extract the personal namespace*/
	l_szEnd = matchFieldBrackets(l_szBegin, &l_szNamespace);
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;
    
	/*Extract each field*/
	l_szNSBegin = l_szNamespace + 1;
	while(l_szNSBegin < (l_szNamespace + strlen(l_szNamespace)))
	{
		l_szNSEnd = matchBrackets(l_szNSBegin, &l_szAttribute);
		if(l_szNSEnd == NULL)
		{
			break;
		}
		l_szNSBegin = l_szNSEnd + 1;

		sink_nameSpacePersonal(l_sink, l_szAttribute);
		myFree(l_szAttribute);
		l_szAttribute = NULL;
	}
	myFree(l_szNamespace);
	l_szNamespace = NULL;

	/*Extract the other user's namespace*/
	l_szEnd = matchFieldBrackets(l_szBegin, &l_szNamespace);
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;
    
	/*Extract each field*/
	l_szNSBegin = l_szNamespace + 1;
	while(l_szNSBegin < (l_szNamespace + strlen(l_szNamespace)))
	{
		l_szNSEnd = matchBrackets(l_szNSBegin, &l_szAttribute);
		if(l_szNSEnd == NULL)
		{
			break;
		}
		l_szNSBegin = l_szNSEnd + 1;

	    sink_nameSpaceOtherUsers(l_sink, l_szAttribute);
		myFree(l_szAttribute);
		l_szAttribute = NULL;
	}
	myFree(l_szNamespace);
	l_szNamespace = NULL;
    
	/*Extract the shared namespace*/
	l_szEnd = matchFieldBrackets(l_szBegin, &l_szNamespace);
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;
    
	/*Extract each field*/
	l_szNSBegin = l_szNamespace + 1;
	while(l_szNSBegin < (l_szNamespace + strlen(l_szNamespace)))
	{
		l_szNSEnd = matchBrackets(l_szNSBegin, &l_szAttribute);
		if(l_szNSEnd == NULL)
		{
			break;
		}
		l_szNSBegin = l_szNSEnd + 1;

	    sink_nameSpaceShared(l_sink, l_szAttribute);
		myFree(l_szAttribute);
		l_szAttribute = NULL;
	}
	myFree(l_szNamespace);
	l_szNamespace = NULL;
    
	sink_nameSpaceEnd(l_sink);
	
	return NSMAIL_OK;
}

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseAcl(imap4Client_t* in_pimap4, char* in_data)
{
	char* l_szBegin = NULL;
	char* l_szEnd = NULL;
	imap4Sink_t* l_sink = in_pimap4->pimap4Sink;
	int l_nLength = 0;
	char* l_szMailbox = NULL;
	char* l_szIdentifier = NULL;
	char* l_szRights = NULL;
	void* l_reference = NULL;

	/*Skip over the * */
	l_szEnd = strchr(in_data, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Skip over ACL*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Get the mailbox name*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_nLength  = l_szEnd - l_szBegin;
	l_szMailbox = (char*)malloc((l_nLength + 1)*sizeof(char));
	if(l_szMailbox == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szMailbox, 0, (l_nLength + 1)*sizeof(char));
	memcpy(l_szMailbox, l_szBegin, l_nLength);
	l_szBegin = l_szEnd + 1;

	sink_aclStart(l_sink, l_szMailbox);

    while(l_szBegin < (in_data + strlen(in_data)))
	{
		/*Get the identifier*/
		l_szEnd = strchr(l_szBegin, ' ');
		if(l_szEnd == NULL)
		{
			break;
		}
		l_nLength  = l_szEnd - l_szBegin;
		l_szIdentifier = (char*)malloc((l_nLength + 1)*sizeof(char));
		if(l_szIdentifier == NULL)
		{
			return NSMAIL_ERR_OUTOFMEMORY;
		}
		memset(l_szIdentifier, 0, (l_nLength + 1)*sizeof(char));
		memcpy(l_szIdentifier, l_szBegin, l_nLength);
		l_szBegin = l_szEnd + 1;

		/*Get the rights*/
		l_szEnd = strchr(l_szBegin, ' ');
		if(l_szEnd == NULL)
		{
			l_szEnd = strchr(l_szBegin, '\n');
			if(l_szEnd == NULL)
			{
				return NSMAIL_ERR_PARSE;
			}
		}
		l_nLength  = l_szEnd - l_szBegin;
		l_szRights = (char*)malloc((l_nLength + 1)*sizeof(char));
		if(l_szRights == NULL)
		{
			return NSMAIL_ERR_OUTOFMEMORY;
		}
		memset(l_szRights, 0, (l_nLength + 1)*sizeof(char));
		memcpy(l_szRights, l_szBegin, l_nLength - 1);
		l_szBegin = l_szEnd + 1;

		sink_aclIdentifierRight(l_sink, l_szIdentifier, l_szRights);

		myFree(l_szIdentifier);
		myFree(l_szRights);
		l_szIdentifier = NULL;
		l_szRights = NULL;
	}

    sink_aclEnd(l_sink);

	myFree(l_szMailbox);
	myFree(l_szIdentifier);
	myFree(l_szRights);

	return NSMAIL_OK;
}

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseMyrights(imap4Client_t* in_pimap4, char* in_data)
{
	char* l_szBegin = NULL;
	char* l_szEnd = NULL;
	imap4Sink_t* l_sink = in_pimap4->pimap4Sink;
	int l_nLength = 0;
	char* l_szMailbox = NULL;
	char* l_szRights = NULL;

	/*Skip over the * */
	l_szBegin = in_data;
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Skip over MYRIGHTS*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Get the mailbox name*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_nLength  = l_szEnd - l_szBegin;
	l_szMailbox = (char*)malloc((l_nLength + 1)*sizeof(char));
	if(l_szMailbox == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szMailbox, 0, (l_nLength + 1)*sizeof(char));
	memcpy(l_szMailbox, l_szBegin, l_nLength);
	l_szBegin = l_szEnd + 1;

	/*Get the rights*/
	l_szEnd = strchr(l_szBegin, '\n');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_nLength  = l_szEnd - l_szBegin;
	l_szRights = (char*)malloc((l_nLength + 1)*sizeof(char));
	if(l_szRights == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szRights, 0, (l_nLength + 1)*sizeof(char));
	memcpy(l_szRights, l_szBegin, l_nLength);

	sink_myRights(l_sink, l_szMailbox, l_szRights);

	return NSMAIL_OK;
}

/**
* Parses in_data and routes it to the sink
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_data The response string
* @return int The return code
*/
static int parseListrights(imap4Client_t* in_pimap4, char* in_data)
{
	char* l_szBegin = NULL;
	char* l_szEnd = NULL;
	imap4Sink_t* l_sink = in_pimap4->pimap4Sink;
	int l_nLength = 0;
	char* l_szMailbox = NULL;
	char* l_szIdentifier = NULL;
	char* l_szRequiredRights = NULL;
	char* l_szOptionalRights = NULL;
	void* l_reference = NULL;

	/*Skip over the * */
	l_szBegin = in_data;
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Skip over MYRIGHTS*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_szBegin = l_szEnd + 1;

	/*Get the mailbox name*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_nLength  = l_szEnd - l_szBegin;
	l_szMailbox = (char*)malloc((l_nLength + 1)*sizeof(char));
	if(l_szMailbox == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szMailbox, 0, (l_nLength + 1)*sizeof(char));
	memcpy(l_szMailbox, l_szBegin, l_nLength);
	l_szBegin = l_szEnd + 1;

	/*Get the identifier*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_nLength  = l_szEnd - l_szBegin;
	l_szIdentifier = (char*)malloc((l_nLength + 1)*sizeof(char));
	if(l_szIdentifier == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szIdentifier, 0, (l_nLength + 1)*sizeof(char));
	memcpy(l_szIdentifier, l_szBegin, l_nLength);
	l_szBegin = l_szEnd + 1;

    sink_listRightsStart(l_sink, l_szMailbox, l_szIdentifier);

	/*Get the required rights*/
	l_szEnd = strchr(l_szBegin, ' ');
	if(l_szEnd == NULL)
	{
		return NSMAIL_ERR_PARSE;
	}
	l_nLength  = l_szEnd - l_szBegin;
	l_szRequiredRights = (char*)malloc((l_nLength + 1)*sizeof(char));
	if(l_szRequiredRights == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset(l_szRequiredRights, 0, (l_nLength + 1)*sizeof(char));
	memcpy(l_szRequiredRights, l_szBegin, l_nLength);
	l_szBegin = l_szEnd + 1;

    sink_listRightsRequiredRights(l_sink, l_szRequiredRights);

	while(l_szBegin < (in_data + strlen(in_data)))
	{
		/*Get the optional rights*/	
		l_szEnd = strchr(l_szBegin, ' ');
		if(l_szEnd == NULL)
		{
			l_szEnd = strchr(l_szBegin, '\n');
			if(l_szEnd == NULL)
			{
				break;
			}
		}
		l_nLength  = l_szEnd - l_szBegin;
		l_szOptionalRights = (char*)malloc((l_nLength + 1)*sizeof(char));
		if(l_szOptionalRights == NULL)
		{
			return NSMAIL_ERR_OUTOFMEMORY;
		}
		memset(l_szOptionalRights, 0, (l_nLength + 1)*sizeof(char));
		memcpy(l_szOptionalRights, l_szBegin, l_nLength);
		l_szBegin = l_szEnd + 1;
		sink_listRightsOptionalRights(l_sink, l_szOptionalRights);

		myFree(l_szOptionalRights);
		l_szOptionalRights = NULL;
	}

	sink_listRightsEnd(l_sink);

	myFree(l_szRequiredRights);
	myFree(l_szOptionalRights);

	return NSMAIL_OK;
}


/*********************************************************************************** 
* General Parsing Utilities
************************************************************************************/ 

/**
* Retrieves the value of the first set of brackets
* @param in_data The response string
* @param out_ppValue The bracketed value
* @return char* Pointer to the end of the value
*/
static char* matchBrackets(char* in_data, char** out_ppValue)
{
	char* l_szBegin = NULL;
	char* l_szEnd = NULL;
	int l_nLeftBraces = 0;
	int l_nDataLength = 0;
	int l_nIndex = 0;

	/*Parameter checking*/
	if((in_data == NULL) || ((*out_ppValue) != NULL))
	{
		return NULL;
	}

	/*Find the matching braces*/
	l_szBegin = in_data;
	l_szEnd = strchr(l_szBegin, '(');
	if(l_szEnd == NULL)
	{
		return NULL;
	}
	l_szBegin = l_szEnd;

	l_nDataLength = strlen(in_data);
	for(l_nIndex = l_szBegin - in_data; l_nIndex < l_nDataLength; l_nIndex++)
	{
		if(in_data[l_nIndex] == '(')
		{
			l_nLeftBraces++;
		}
		else if(in_data[l_nIndex] == ')')
		{
			l_nLeftBraces--;
			if(l_nLeftBraces < 1)
			{
				/*Store the value of the braces*/
				l_nDataLength = in_data + l_nIndex - l_szBegin + 1;
				(*out_ppValue) = (char*)malloc((l_nDataLength + 1) * sizeof(char));
				if((*out_ppValue) == NULL)
				{
					return NULL;
				}
				memset((*out_ppValue), 0, (l_nDataLength + 1)*sizeof(char));
				memcpy((*out_ppValue), l_szBegin, l_nDataLength);

				l_szEnd = l_szBegin + l_nIndex;
				return l_szEnd;
			}
		}
	}

	return NULL;
}

/**
* Retrieves the value of the next set of brackets, also treats NIL
* as a bracketed value
* @param in_data The response string
* @param out_ppValue The bracketed value
* @return char* Pointer to the end of the value
*/
static char* matchFieldBrackets(char* in_data, char** out_ppValue)
{
	int l_nLength;

	in_data = skipWhite(in_data, strlen(in_data));
	l_nLength = strlen(in_data);
	if(	(l_nLength > 2) &&
		(in_data[0] == 'N') &&
		(in_data[1] == 'I') &&
		(in_data[2] == 'L'))
	{
		(*out_ppValue) = (char*)malloc((strlen(g_szNil) + 1)*sizeof(char));
		if((*out_ppValue) == NULL)
		{
			return NULL;
		}
		memset((*out_ppValue), 0, (strlen(g_szNil) + 1)*sizeof(char));
		memcpy((*out_ppValue), in_data, strlen(g_szNil));
		return (in_data + strlen(g_szNil));
	}

	return matchBrackets(in_data, out_ppValue);
}

/**
* Retrieves the value of the next set of quotes, also treats NIL
* as a quoted value
* @param in_data The response string
* @param out_ppValue The bracketed value
* @return char* Pointer to the end of the value
*/
static char* matchFieldQuotes(char* in_data, char** out_ppValue)
{
	int l_nLength;
	char* l_szBegin = NULL;
	char* l_szEnd = NULL;

	in_data = skipWhite(in_data, strlen(in_data));
	l_nLength = strlen(in_data);
	if(	(l_nLength > 2) &&
		(in_data[0] == 'N') &&
		(in_data[1] == 'I') &&
		(in_data[2] == 'L'))
	{
		(*out_ppValue) = (char*)malloc((strlen(g_szNil) + 1)*sizeof(char));
		if((*out_ppValue) == NULL)
		{
			return NULL;
		}
		memset((*out_ppValue), 0, (strlen(g_szNil) + 1)*sizeof(char));
		memcpy((*out_ppValue), in_data, strlen(g_szNil));
		return (in_data + strlen(g_szNil));
	}

	l_szBegin = in_data;
	l_szEnd = strchr(l_szBegin, '"');
	if(l_szEnd == NULL)
	{
		return NULL;
	}
	l_szBegin = l_szEnd + 1;

	l_szEnd = strchr(l_szBegin, '"');
	if(l_szEnd == NULL)
	{
		return NULL;
	}
	l_nLength = l_szEnd - l_szBegin;
	(*out_ppValue) = (char*)malloc((l_nLength + 1)*sizeof(char));
	if((*out_ppValue) == NULL)
	{
		return NULL;
	}
	memset((*out_ppValue), 0, (l_nLength + 1)*sizeof(char));
	memcpy((*out_ppValue), l_szBegin, l_nLength);
	
	return l_szEnd;
}


/**
* Resolve the data item
* @param in_pimap4 A pointer to the imap4 client structure
* @param in_dataItem The data item
* @return int The data item type
*/
static int resolveFetchDataItem(imap4Client_t* in_pimap4, char* in_dataItem)
{
	int l_nLength = -1;

	l_nLength = strlen(in_dataItem);
	if(l_nLength < 1)
	{
		return UNKNOWN;
	}
	switch(in_dataItem[0])
	{
		case 'B':
		{
			if(!strcmp(in_dataItem, g_szBodyStructure))
			{
				return BODYSTRUCTURE;
			}
			else if((l_nLength > 3) &&
					(in_dataItem[1] == 'O') &&
					(in_dataItem[2] == 'D') &&
					(in_dataItem[3] == 'Y'))
			{
				if((l_nLength > 5) &&
					(in_dataItem[4] == '['))
				{
					if(	(in_dataItem[5] == ']') ||
						(in_dataItem[5] == '0') ||
						(in_dataItem[5] == 'H'))
					{
						in_pimap4->bReadHeader = TRUE;
					}
				}
				else if((l_nLength > 10) &&
						(in_dataItem[4] == '.') &&
						(in_dataItem[5] == 'P') &&
						(in_dataItem[6] == 'E') &&
						(in_dataItem[7] == 'E') &&
						(in_dataItem[8] == 'K') &&
						(in_dataItem[9] == '['))
				{
					if(	(in_dataItem[10] == ']') ||
						(in_dataItem[10] == '0') ||
						(in_dataItem[10] == 'H'))
					{
						in_pimap4->bReadHeader = TRUE;
					}
				}
				return MESSAGE;
			}
			break;
		}
		case 'R':
		{
			if(!strcmp(in_dataItem, g_szRfc822))
			{
				in_pimap4->bReadHeader = TRUE;
				return MESSAGE;
			}
			else if(!strcmp(in_dataItem, g_szRfc822Size))
			{
				return RFC822SIZE;
			}
			else if(!strcmp(in_dataItem, g_szRfc822Text))
			{
				return RFC822TEXT;
			}
			else if(!strcmp(in_dataItem, g_szRfc822Header))
			{
				in_pimap4->bReadHeader = TRUE;
				return MESSAGE;
			}
			break;
		}
		case 'U':
		{
			if(!strcmp(in_dataItem, g_szUid))
			{
				return UID;
			}
			break;
		}
		case 'F':
		{
			if(!strcmp(in_dataItem, g_szFlags))
			{
				return FLAGS;
			}
			break;
		}
		case 'E':
		{
			if(!strcmp(in_dataItem, g_szEnvelope))
			{
				return ENVELOPE;
			}
			break;
		}
		case 'I':
		{
			if(!strcmp(in_dataItem, g_szInternalDate))
			{
				return INTERNALDATE;
			}
			break;
		}
		default:
		{
			break;
		}
	}
	return UNKNOWN;
}

/**
* Skip over any leading white spaces
* @param in_data The data to examine
* @param in_length The length of data
* @return char* Pointer to first non white character
*/
static char* skipWhite(char* in_data, int in_length)
{
	int i;

	for(i = 0; i < in_length; i++)
	{
		if((in_data[i] != ' ') && (in_data[i] != '\t'))
		{
			return in_data + i;
		}
	}
}

/**
* Extracts a full line of the header from in_data
* @param in_data The data to examine
* @param in_leftOver The left over header info
* @param in_length The length of data
* @param in_start The index to start from
* @param out_ppHeaderLine The line of the header
* @param out_pEnd The index of the end of the line
* @return int The return code. NOTE: NSMAIL_ERR_PARSE in the context of this
* function means that the last line of the header being processed is NOT
* complete!!
*/
static int readHeaderLine(char* in_data, char** in_ppLeftOver, int in_length, int in_start, char** out_ppHeaderLine, int* out_pEnd)
{
	int i;
	int l_nHeaderLine = 0;
	int l_nLeft = 0;

	for(i = in_start; i < in_length; i++)
	{
		if(in_data[i] == '\n')
		{
			if(	((in_data[i + 1] != '\t') && (in_data[i + 1] != ' ')) ||
				(((i - in_start) < 2) && ((i - in_start) > 0) && (in_data[i - 1] == '\r')))
			{
				l_nHeaderLine = i - in_start;
				if((*in_ppLeftOver) != NULL)
				{
					/*The left over data from the previous incomplete readHeaderLine is added
					  to the beginning of the current line received*/
					l_nLeft = strlen(*in_ppLeftOver);
					l_nHeaderLine += l_nLeft;
				}
				(*out_ppHeaderLine) = (char*)malloc((l_nHeaderLine + 1)*sizeof(char));
				if((*out_ppHeaderLine) == NULL)
				{
					return NSMAIL_ERR_OUTOFMEMORY;
				}
				memset((*out_ppHeaderLine), 0, (l_nHeaderLine + 1)*sizeof(char));
				if((*in_ppLeftOver) != NULL)
				{
					/*Copy the left over data into the beginning of the header line*/
					memcpy((*out_ppHeaderLine), (*in_ppLeftOver), l_nLeft);
					(*in_ppLeftOver) = NULL;
				}
				memcpy((*out_ppHeaderLine) + l_nLeft, in_data + in_start, (i - in_start)*sizeof(char));

				(*out_pEnd) = i;

				return NSMAIL_OK;
			}
		}
	}

	/*Save the left over header data in temp buffer*/
	(*in_ppLeftOver) = (char*)malloc((in_length - in_start + 1)*sizeof(char));
	if((*in_ppLeftOver) == NULL)
	{
		return NSMAIL_ERR_OUTOFMEMORY;
	}
	memset((*in_ppLeftOver), 0, (in_length - in_start + 1)*sizeof(char));
	memcpy((*in_ppLeftOver), in_data + in_start, in_length - in_start);

	return NSMAIL_ERR_PARSE;
}

/*******************************************************************************************
* The default sink methods
********************************************************************************************/

static void sink_taggedLine(imap4SinkPtr_t in_pimap4Sink, char* in_tag, 
					 const char* in_status, const char* in_reason)
{
	if(in_pimap4Sink->taggedLine != NULL)
	{
		in_pimap4Sink->taggedLine(in_pimap4Sink, in_tag, in_status, in_reason);
	}
	
}

static void sink_error(imap4SinkPtr_t in_pimap4Sink, const char* in_tag, 
				const char* in_status, const char* in_reason)
{
	if(in_pimap4Sink->error != NULL)
	{
		in_pimap4Sink->error(in_pimap4Sink, in_tag, in_status, in_reason);
	}
}

static void sink_ok(imap4SinkPtr_t in_pimap4Sink, const char* in_responseCode, 
			 const char* in_information)
{
	if(in_pimap4Sink->ok != NULL)
	{
		in_pimap4Sink->ok(in_pimap4Sink, in_responseCode, in_information);
	}
}

static void sink_rawResponse(imap4SinkPtr_t in_pimap4Sink, const char* in_data)
{
	if(in_pimap4Sink->rawResponse != NULL)
	{
		in_pimap4Sink->rawResponse(in_pimap4Sink, in_data);
	}
}

/*Fetch Response*/
static void sink_fetchStart(imap4SinkPtr_t in_pimap4Sink, int in_msg)
{
	if(in_pimap4Sink->fetchStart != NULL)
	{
		in_pimap4Sink->fetchStart(in_pimap4Sink, in_msg);
	}
}

static void sink_fetchEnd(imap4SinkPtr_t in_pimap4Sink)
{
	if(in_pimap4Sink->fetchEnd != NULL)
	{
		in_pimap4Sink->fetchEnd(in_pimap4Sink);
	}
}

static void sink_fetchSize(imap4SinkPtr_t in_pimap4Sink, int in_size)
{
	if(in_pimap4Sink->fetchSize != NULL)
	{
		in_pimap4Sink->fetchSize(in_pimap4Sink, in_size);
	}
}

static void sink_fetchData(imap4SinkPtr_t in_pimap4Sink, const char* in_data, 
						   int in_bytesRead, int in_totalBytes)
{
	if(in_pimap4Sink->fetchData != NULL)
	{
		in_pimap4Sink->fetchData(in_pimap4Sink, in_data, 
			in_bytesRead, in_totalBytes);
	}
}

static void sink_fetchFlags(imap4SinkPtr_t in_pimap4Sink, const char* in_flags)
{
	if(in_pimap4Sink->fetchFlags != NULL)
	{
		in_pimap4Sink->fetchFlags(in_pimap4Sink, in_flags);
	}
}

static void sink_fetchBodyStructure(imap4SinkPtr_t in_pimap4Sink,  
									const char* in_bodyStructure)
{
	if(in_pimap4Sink->fetchBodyStructure != NULL)
	{
		in_pimap4Sink->fetchBodyStructure(in_pimap4Sink, in_bodyStructure);
	}
}

static void sink_fetchEnvelope(imap4SinkPtr_t in_pimap4Sink, const char** in_value, 
							   int in_valueLength)
{
	if(in_pimap4Sink->fetchEnvelope != NULL)
	{
		in_pimap4Sink->fetchEnvelope(in_pimap4Sink, in_value, in_valueLength);
	}
}

static void sink_fetchInternalDate(imap4SinkPtr_t in_pimap4Sink, const char* in_internalDate)
{
	if(in_pimap4Sink->fetchInternalDate != NULL)
	{
		in_pimap4Sink->fetchInternalDate(in_pimap4Sink, in_internalDate);
	}
}

static void sink_fetchHeader(imap4SinkPtr_t in_pimap4Sink, const char* in_field, 
							 const char* in_value)
{
	if(in_pimap4Sink->fetchHeader != NULL)
	{
		in_pimap4Sink->fetchHeader(in_pimap4Sink, in_field, in_value);
	}
}

static void sink_fetchUid(imap4SinkPtr_t in_pimap4Sink, int in_uid)
{
	if(in_pimap4Sink->fetchUid != NULL)
	{
		in_pimap4Sink->fetchUid(in_pimap4Sink, in_uid);
	}
}

/*Lsub Response*/
static void sink_lsub(imap4SinkPtr_t in_pimap4Sink, const char* in_attribute, 
			   const char* in_delimiter, const char* in_name)
{
	if(in_pimap4Sink->lsub != NULL)
	{
		in_pimap4Sink->lsub(in_pimap4Sink, in_attribute, in_delimiter, in_name);
	}
}

/*List Response*/
static void sink_list(imap4SinkPtr_t in_pimap4Sink, const char* in_attribute, 
			   const char* in_delimiter, const char* in_name)
{
	if(in_pimap4Sink->list != NULL)
	{
		in_pimap4Sink->list(in_pimap4Sink, in_attribute, in_delimiter, in_name);
	}
}

/*Search Response*/
static void sink_searchStart(imap4SinkPtr_t in_pimap4Sink)
{
	if(in_pimap4Sink->searchStart != NULL)
	{
		in_pimap4Sink->searchStart(in_pimap4Sink);
	}
}

static void sink_search(imap4SinkPtr_t in_pimap4Sink, int in_message)
{
	if(in_pimap4Sink->search != NULL)
	{
		in_pimap4Sink->search(in_pimap4Sink, in_message);
	}
}

static void sink_searchEnd(imap4SinkPtr_t in_pimap4Sink)
{
	if(in_pimap4Sink->searchEnd != NULL)
	{
		in_pimap4Sink->searchEnd(in_pimap4Sink);
	}
}

/*Status Response*/
static void sink_statusMessages(imap4SinkPtr_t in_pimap4Sink, int in_messages)
{
	if(in_pimap4Sink->statusMessages != NULL)
	{
		in_pimap4Sink->statusMessages(in_pimap4Sink, in_messages);
	}
}

static void sink_statusRecent(imap4SinkPtr_t in_pimap4Sink, int in_recent)
{
	if(in_pimap4Sink->statusRecent != NULL)
	{
		in_pimap4Sink->statusRecent(in_pimap4Sink, in_recent);
	}
}

static void sink_statusUidnext(imap4SinkPtr_t in_pimap4Sink, int in_uidNext)
{
	if(in_pimap4Sink->statusUidnext != NULL)
	{
		in_pimap4Sink->statusUidnext(in_pimap4Sink, in_uidNext);
	}
}

static void sink_statusUidvalidity(imap4SinkPtr_t in_pimap4Sink, int in_uidValidity)
{
	if(in_pimap4Sink->statusUidvalidity != NULL)
	{
		in_pimap4Sink->statusUidvalidity(in_pimap4Sink, in_uidValidity);
	}
}

static void sink_statusUnseen(imap4SinkPtr_t in_pimap4Sink, int in_unSeen)
{
	if(in_pimap4Sink->statusUnseen != NULL)
	{
		in_pimap4Sink->statusUnseen(in_pimap4Sink, in_unSeen);
	}
}

/*Capability Response*/
static void sink_capability(imap4SinkPtr_t in_pimap4Sink, const char* in_listing)
{
	if(in_pimap4Sink->capability != NULL)
	{
		in_pimap4Sink->capability(in_pimap4Sink, in_listing);
	}
}

/*Exists Response*/
static void sink_exists(imap4SinkPtr_t in_pimap4Sink, int in_messages)
{
	if(in_pimap4Sink->exists != NULL)
	{
		in_pimap4Sink->exists(in_pimap4Sink, in_messages);
	}
}

/*Expunge Response*/
static void sink_expunge(imap4SinkPtr_t in_pimap4Sink, int in_message)
{
	if(in_pimap4Sink->expunge != NULL)
	{
		in_pimap4Sink->expunge(in_pimap4Sink, in_message);
	}
}

/*Recent Response*/
static void sink_recent(imap4SinkPtr_t in_pimap4Sink, int in_messages)
{
	if(in_pimap4Sink->recent != NULL)
	{
		in_pimap4Sink->recent(in_pimap4Sink, in_messages);
	}
}

/*Flags Response*/
static void sink_flags(imap4SinkPtr_t in_pimap4Sink, const char* in_flags)
{
	if(in_pimap4Sink->flags != NULL)
	{
		in_pimap4Sink->flags(in_pimap4Sink, in_flags);
	}
}

/*Bye Response*/
static void sink_bye(imap4SinkPtr_t in_pimap4Sink, const char* in_reason)
{
	if(in_pimap4Sink->bye != NULL)
	{
		in_pimap4Sink->bye(in_pimap4Sink, in_reason);
	}
}

/*Namespace Response*/
static void sink_nameSpaceStart(imap4SinkPtr_t in_pimap4Sink)
{
	if(in_pimap4Sink->nameSpaceStart != NULL)
	{
		in_pimap4Sink->nameSpaceStart(in_pimap4Sink);
	}
}

static void sink_nameSpacePersonal(imap4SinkPtr_t in_pimap4Sink, 
						  const char* in_personal)
{
	if(in_pimap4Sink->nameSpacePersonal != NULL)
	{
		in_pimap4Sink->nameSpacePersonal(in_pimap4Sink, in_personal);
	}
}

static void sink_nameSpaceOtherUsers(imap4SinkPtr_t in_pimap4Sink, 
							const char* in_otherUsers)
{
	if(in_pimap4Sink->nameSpaceOtherUsers != NULL)
	{
		in_pimap4Sink->nameSpaceOtherUsers(in_pimap4Sink, in_otherUsers);
	}
}

static void sink_nameSpaceShared(imap4SinkPtr_t in_pimap4Sink, const char* in_shared)
{
	if(in_pimap4Sink->nameSpaceShared != NULL)
	{
		in_pimap4Sink->nameSpaceShared(in_pimap4Sink, in_shared);
	}
}

static void sink_nameSpaceEnd(imap4SinkPtr_t in_pimap4Sink)
{
	if(in_pimap4Sink->nameSpaceEnd != NULL)
	{
		in_pimap4Sink->nameSpaceEnd(in_pimap4Sink);
	}
}


/*ACL Responses*/
static void sink_aclStart(imap4SinkPtr_t in_pimap4Sink, const char* in_mailbox)
{
	if(in_pimap4Sink->aclStart != NULL)
	{
		in_pimap4Sink->aclStart(in_pimap4Sink, in_mailbox);
	}
}

static void sink_aclIdentifierRight(imap4SinkPtr_t in_pimap4Sink, 
						   const char* in_identifier, const char* in_rights)
{
	if(in_pimap4Sink->aclIdentifierRight != NULL)
	{
		in_pimap4Sink->aclIdentifierRight(in_pimap4Sink, in_identifier, in_rights);
	}
}

static void sink_aclEnd(imap4SinkPtr_t in_pimap4Sink)
{
	if(in_pimap4Sink->aclEnd != NULL)
	{
		in_pimap4Sink->aclEnd(in_pimap4Sink);
	}
}

/*LISTRIGHTS Responses*/
static void sink_listRightsStart(imap4SinkPtr_t in_pimap4Sink, const char* in_mailbox, 
						 const char* in_identifier)
{
	if(in_pimap4Sink->listRightsStart != NULL)
	{
		in_pimap4Sink->listRightsStart(in_pimap4Sink, in_mailbox, in_identifier);
	}
}

static void sink_listRightsRequiredRights(imap4SinkPtr_t in_pimap4Sink, 
								 const char* in_requiredRights)
{
	if(in_pimap4Sink->listRightsRequiredRights != NULL)
	{
		in_pimap4Sink->listRightsRequiredRights(in_pimap4Sink, in_requiredRights);
	}
}

static void sink_listRightsOptionalRights(imap4SinkPtr_t in_pimap4Sink, 
								 const char* in_optionalRights)
{
	if(in_pimap4Sink->listRightsOptionalRights != NULL)
	{
		in_pimap4Sink->listRightsOptionalRights(in_pimap4Sink, in_optionalRights);
	}
}

static void sink_listRightsEnd(imap4SinkPtr_t in_pimap4Sink)
{
	if(in_pimap4Sink->listRightsEnd != NULL)
	{
		in_pimap4Sink->listRightsEnd(in_pimap4Sink);
	}
}

/*MYRIGHTS Responses*/
static void sink_myRights(imap4SinkPtr_t in_pimap4Sink, const char* in_mailbox, const char* in_rights)
{
	if(in_pimap4Sink->myRights != NULL)
	{
		in_pimap4Sink->myRights(in_pimap4Sink, in_mailbox, in_rights);
	}
}

/*********************************************************************************** 
* General helpers
************************************************************************************/ 

static void myFree(void* in_block)
{
	if(in_block != NULL)
	{
		free(in_block);
	}
}


