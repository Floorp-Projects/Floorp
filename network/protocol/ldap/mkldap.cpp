/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
//
// mkldap.cpp -- Handles LDAP-related URLs for the core Navigator, without 
// requiring libmsg. mkldap is responsible for
//     1. basic async searching of LDAP servers, including authentication,
//        capability negotiation with the server, and parsing results
//     2. Reading LDAP entries and converting them into HTML for display 
//        in the browsers
//     3. Reading an LDAP entry and converting it for use in the local
//        Address Book
//     4. Replicating a large section of an LDAP directory into a local
//        Address Book using changelog deltas
// 

#include "rosetta.h"
#include "mkutils.h" 

#include "mkgeturl.h"
#include "mkldap.h"
#include "xpgetstr.h"
#include "abcom.h"
#include "addrbook.h"
#include "msgcom.h"
#include "msgnet.h"
#if 0
#include "msgpane.h"
#include "msgurlq.h"
#endif
#include "dirprefs.h"
#include "net.h"
#include "xp_str.h"
#include HG87373
#include "secnav.h"
#include "libi18n.h"
#include "intl_csi.h"
#if 0
#include "pw_public.h"
#endif

#ifdef MOZ_LDAP
  #ifdef MOZ_LI
    #include "LILDAPAllOps.h"
  #endif

  #define NEEDPROTOS
  #define LDAP_REFERRALS
  #include "lber.h"
  #include "ldap.h"
#endif


extern "C"
{
	extern int MK_OUT_OF_MEMORY;
	extern int MK_MSG_SEARCH_FAILED;

	extern int MK_LDAP_COMMON_NAME;   
	extern int MK_LDAP_FAX_NUMBER;    
	extern int MK_LDAP_GIVEN_NAME;    
	extern int MK_LDAP_LOCALITY;      
	extern int MK_LDAP_PHOTOGRAPH;    
	extern int MK_LDAP_EMAIL_ADDRESS; 
	extern int MK_LDAP_MANAGER;       
	extern int MK_LDAP_ORGANIZATION;  
	extern int MK_LDAP_OBJECT_CLASS;  
	extern int MK_LDAP_ORG_UNIT;      
	extern int MK_LDAP_POSTAL_ADDRESS;
	extern int MK_LDAP_SECRETARY;     
	extern int MK_LDAP_SURNAME;       
	extern int MK_LDAP_STREET;        
	extern int MK_LDAP_PHONE_NUMBER;  
	extern int MK_LDAP_CAR_LICENSE;
	extern int MK_LDAP_BUSINESS_CAT;
	extern int MK_LDAP_DEPT_NUMBER;
	extern int MK_LDAP_DESCRIPTION;
	extern int MK_LDAP_EMPLOYEE_TYPE;
	extern int MK_LDAP_POSTAL_CODE;
	extern int MK_LDAP_TITLE;

	extern int MK_LDAP_ADD_SERVER_TO_PREFS;
	extern int MK_MALFORMED_URL_ERROR;
	HG73226
	extern int MK_LDAP_HTML_TITLE;

	extern int MK_LDAP_AUTH_PROMPT;

	extern int XP_LDAP_OPEN_SERVER_FAILED;
	extern int XP_LDAP_BIND_FAILED;
	extern int XP_LDAP_SEARCH_FAILED;
}


#ifdef MOZ_LDAP

//*****************************************************************************
//
// net_LdapConnectionData is the base class which knows how to run
// ldap URLs asynchronously and pull out search results.
// 
// If you want to do something special with the results of an LDAP
// search, you probably want to derive from this class and use/make
// virtual functions.
// 

class net_LdapConnectionData
{
public:
	net_LdapConnectionData (ActiveEntry*, XP_Bool bInitLdap = TRUE);
	virtual ~net_LdapConnectionData ();

	virtual int Load (ActiveEntry *ce);
	virtual int Process ();
	virtual int Interrupt ();

protected:
	typedef enum _LdapConnectionType
	{
		kBaseConnection,
		kHtmlConnection,
		kReplicatorConnection,
		kAddressBookConnection,
		kLIConnection
	} LdapConnectionType;

	virtual int Unbind ();
	virtual int	OnBindCompleted () { return 0; }
	virtual int OnEntryFound (LDAPMessage *) { return 0; }
	virtual int OnSearchCompleted () { return MK_CONNECTED; }
	virtual XP_Bool OnError(int err) { return TRUE; }
	virtual int16 GetLocalCharSet ();
	virtual LdapConnectionType GetConnectionType() { return kBaseConnection; }

	void AddLdapServerToPrefs (LDAPURLDesc* pDesc);

	char *ConvertFromServerCharSet (char *s);
	char *ConvertToServerCharSet (char *s);
	// sub classes may have strings prefixed to the url...We need to filter them out here. 
	virtual char * ExtractLDAPEncoding(char * s);  

	XP_Bool ShouldAddUrlDescToPrefs (MWContext *context, XP_List *serverList, LDAPURLDesc *pDesc);
	void ConvertUrlDescToDirServer (LDAPURLDesc *pDesc, DIR_Server **pServer);

	const char *GetHostDescription ();
	char *GetAttributeFromDN (char *dn, const char *name);
	const char *GetAttributeString (DIR_AttributeId id);

	void DisplayError (int templateId, const char *contextString, int ldapErrorId);

	int16 m_csid;
	LDAP *m_ldap;
	int m_messageId;
	ActiveEntry *m_ce;
	DIR_Server *m_server;
};

net_LdapConnectionData::net_LdapConnectionData (ActiveEntry *ce, XP_Bool bInitLdap)
{
	 // We init LDAP with no server name and port since we expect all
	 // such information to be spedified by URLs in our case.
	 //
	if (bInitLdap)
		m_ldap = ldap_init(NULL,0);
	else
		m_ldap = NULL;

	m_messageId = -1; // Init to an illegal value so we can detect errors

	m_server = NULL;

	m_csid = -1;
	m_ce = ce;
	m_ce->socket = NULL;

	// Netlib magic so we get a timeslice without waiting for a read-ready socket. 
	// It would be nice to ditch this, but the LDAP SDK manages the sockets.
	NET_SetCallNetlibAllTheTime(m_ce->window_id, "mkldap");
}


net_LdapConnectionData::~net_LdapConnectionData ()
{
	// This is the normal-termination case to let go of the connection
	Unbind();

	// Netlib magic so we won't get called anymore for this URL
	// It would be nice to ditch this, but the LDAP SDK manages the sockets.
	NET_ClearCallNetlibAllTheTime(m_ce->window_id, "mkldap");
}

int16 net_LdapConnectionData::GetLocalCharSet ()
{
	if (m_csid == -1)
		m_csid = INTL_DefaultDocCharSetID(0);
	return m_csid;
}

void net_LdapConnectionData::AddLdapServerToPrefs (LDAPURLDesc* pDesc) 
{
	// Takes an ActiveEntry from the current operation, a url descriptor from ldap_url_parse, 
	// determines (by asking the user) if we should add it to our LDAP prefs, and adds it if necessary.

	// Do we want to add the server specified by the URL to our LDAP prefs?
	XP_List *serverList = FE_GetDirServers(); 
	if (serverList && ShouldAddUrlDescToPrefs(m_ce->window_id, serverList, pDesc))
	{
		ConvertUrlDescToDirServer (pDesc, &m_server);
		if (m_server)
		{
			XP_ListAddObjectToEnd (serverList, m_server);
			DIR_SaveServerPreferences (serverList);
		}
	}
}


int net_LdapConnectionData::Load (ActiveEntry *ce) 
{
	int ldapErr = 0;
	LDAPURLDesc *pDesc = NULL;

	XP_ASSERT(ce);
	XP_ASSERT(ce->URL_s);
	XP_ASSERT(ce->URL_s->address);

	if (m_ldap)
	{
		// addbook and html have prefixes in front of them (i.e. addbook-ldap). We now want
		// to parse that out of the utf8 encoding string that will be sent to the server.
		// In addition, we want to convert the resulting encoding to utf8

		// we do not need to free encoding because it is a part of the url passed in....
		char * encoding = ExtractLDAPEncoding(ce->URL_s->address);

		// now take the ldap encoding and convert to the server char set.
		char *utf8Encoding = utf8Encoding = ConvertToServerCharSet (encoding); 

		if (utf8Encoding)
		{
			ldapErr = ldap_url_parse (utf8Encoding, &pDesc);

			if (!pDesc || ldapErr != LDAP_SUCCESS)
				ldapErr = MK_MALFORMED_URL_ERROR;

			if (ldapErr == 0)
			{
				// RFC 1959 allows this syntax, but we don't really have a way
				// to infer an LDAP server, and it's a pretty strange X.500-ism anyway
				if (!pDesc->lud_host)
					ldapErr = MK_MALFORMED_URL_ERROR;

				AddLdapServerToPrefs (pDesc);

				HG29237
				if (ldapErr == 0)
				{
					// Initiate the search on the server
					int msgId = ldap_url_search (m_ldap, utf8Encoding, 0); 
					if (msgId != -1)
						m_messageId = msgId;
					else 
						ldapErr = -1;
				}
			}
			else
				ldapErr = MK_MALFORMED_URL_ERROR;

			XP_FREE(utf8Encoding);
		}
		else
			ldapErr = -1;
	}
	else
			XP_ASSERT(pDesc);
		ldapErr = -1;

	if (pDesc)
		ldap_free_urldesc (pDesc);

	return ldapErr;
}


// Interpret all data we get from the server as being UTF-8 
char *net_LdapConnectionData::ConvertFromServerCharSet (char *s)
{
	char *converted = NULL;
	XP_ASSERT(s);
	if (s)
	{
		// Use the win_csid for browsers but the GUI csid for the AB
		int16 destCsid = GetLocalCharSet();
		converted = DIR_ConvertFromServerCharSet (m_server, s, destCsid);
	}
	return converted;
}

char * net_LdapConnectionData::ExtractLDAPEncoding(char * url)
{
	// for the base class, we don't know about stripping any prefixes off...
	return url;
}

// Send all data to the server in UTF-8
char *net_LdapConnectionData::ConvertToServerCharSet (char *s)
{
	char *converted = NULL;
 	XP_ASSERT(s);
	if (s)
	{
		// Use the win_csid for browsers but the GUI csid for the AB
		int16 srcCsid = GetLocalCharSet();
		converted = DIR_ConvertToServerCharSet (m_server, s, srcCsid);
	}
	return converted;
}

const char *net_LdapConnectionData::GetHostDescription()
{
	// since the description is optional, it might be null, so pick up
	// the server's DNS name if necessary
	return m_server->description ? m_server->description : m_server->serverName;
}

char *net_LdapConnectionData::GetAttributeFromDN (char *dn, const char *name)
{
	// Pull an attribute out of the distinguished name
	// e.g. the cn attribute from cn=John Smith,o=Netscape Communications Corp.,c=US

	char *result = NULL;
	char **explodedDn = ldap_explode_dn (dn, FALSE /* suppress types? */);
	if (explodedDn)
	{
		int i = 0;
		while (explodedDn[i] && !result)
		{
			const char *nameFromDn = XP_STRTOK (explodedDn[i], "=");
			if (!XP_STRCASECMP(name, nameFromDn))
			{
				char *tmpResult = XP_STRDUP(explodedDn[i] + XP_STRLEN(nameFromDn) + 1);
				result = ConvertFromServerCharSet (tmpResult);
				XP_FREE(tmpResult);
			}
			i++;
		}
		ldap_value_free (explodedDn);
	}
	return result;
}


const char *net_LdapConnectionData::GetAttributeString (DIR_AttributeId id)
{
	// This function is a bottleneck for DIR_GetAttributeName. I think most
	// of the time we'll have a DIR_Server, but I think it can also be NULL
	// if the user clicked on an ldap: URL for which there's no server in
	// the preferences

	return DIR_GetFirstAttributeString (m_server, id);
}

void net_LdapConnectionData::DisplayError (int templateId, const char *contextString, int ldapErrorId)
{
	// This builds up error dialogs which look like:
	// "Failed to open connection to 'ldap.mcom.com' due to LDAP error 'bad DN name' (0x22)"

	char *templateString = XP_GetString (templateId);
	char *ldapErrString = NULL;

	if (ldapErrorId != 0)
		ldapErrString = ldap_err2string (ldapErrorId);
	if (templateString && contextString && (ldapErrorId == 0 || ldapErrString))
	{
		int len = XP_STRLEN(templateString) + XP_STRLEN(contextString) + 10;
		if (ldapErrString)
			len += XP_STRLEN(ldapErrString);
		char *theBigString = (char*) XP_ALLOC (len);
		if (theBigString)
		{
			if (ldapErrorId)
				PR_snprintf (theBigString, len, templateString, contextString, ldapErrString, ldapErrorId);
			else
				PR_snprintf (theBigString, len, templateString, contextString);
			FE_Alert (m_ce->window_id, theBigString);
			XP_FREE(theBigString);
		}
	}
}


XP_Bool net_LdapConnectionData::ShouldAddUrlDescToPrefs (MWContext *context, XP_List *serverList, LDAPURLDesc *pDesc)
{
	XP_Bool shouldAdd = TRUE;

	// First look through the prefs to see if the server already exists there
	for (int i = 1; i <= XP_ListCount(serverList) && shouldAdd; i++)
	{
		DIR_Server *server = (DIR_Server*) XP_ListGetObjectNum (serverList, i);
		if (server && server->serverName && pDesc->lud_host)
		{
			if (!XP_STRCASECMP(pDesc->lud_host, server->serverName))
			{
				// The server does exist -- don't ask to add it again
				shouldAdd = FALSE;
				if (!m_server)
					m_server = server;
			}
		}
	}

	// If the server doesn't exist, ask the user if they want to add it to their prefs
	if (shouldAdd)
	{
		char *prompt = PR_smprintf (XP_GetString(MK_LDAP_ADD_SERVER_TO_PREFS), pDesc->lud_host);
		if (prompt)
		{
			shouldAdd = FE_Confirm (context, prompt);
			XP_FREE(prompt);
		}
		else
			shouldAdd = FALSE;
	}

	return shouldAdd;
}


void net_LdapConnectionData::ConvertUrlDescToDirServer (LDAPURLDesc *pDesc, DIR_Server **ppServer)
{
	*ppServer = (DIR_Server*) XP_ALLOC (sizeof(DIR_Server));
	if (*ppServer)
	{
		XP_BZERO(*ppServer, sizeof(DIR_Server));

		// These fields just map right over
		(*ppServer)->serverName = XP_STRDUP (pDesc->lud_host);
		(*ppServer)->searchBase = XP_STRDUP (pDesc->lud_dn ? pDesc->lud_dn : "");
		(*ppServer)->port = pDesc->lud_port;

		// These could probably be in a generic DIR_Server initializer. Do we need that?
		(*ppServer)->efficientWildcards = TRUE;
		(*ppServer)->saveResults = TRUE;
		(*ppServer)->maxHits = 100;

		// The address book needs a valid filename. Use the hostname to derive one.
		char *fileName = XP_STRTOK(pDesc->lud_host, ".");
		if (fileName)
			(*ppServer)->fileName = WH_FileName (fileName, xpAddrBook);
		else
			(*ppServer)->fileName = WH_FileName (pDesc->lud_host, xpAddrBook);

		// Try to derive a reasonable name for DIR_Server.description by looking
		// at the distinguished name and picking out the most specific RDN 
		// (i.e. the last OU, or any OU, or any O, or any C).
		if (pDesc->lud_dn)
		{
			char **explodedDn = ldap_explode_dn (pDesc->lud_dn, TRUE /*suppress types? */);
			if (explodedDn)
			{
				if (explodedDn[0])
					(*ppServer)->description = XP_STRDUP (explodedDn[0]);

				ldap_value_free (explodedDn);
			}
		}

		// If we couldn't find any reasonable-looking DN component, just use the host name
		if (NULL == (*ppServer)->description)
			(*ppServer)->description = XP_STRDUP ((*ppServer)->serverName);
	}
}


int net_LdapConnectionData::Process ()
{
	LDAPMessage *message = NULL;
	struct timeval timeout;
	XP_BZERO (&timeout, sizeof(timeout));
	int result = ldap_result (m_ldap, m_messageId, 0, &timeout, &message);

	switch (result)
	{
	case LDAP_RES_SEARCH_ENTRY:
		result = OnEntryFound (message);
		break;
	case LDAP_RES_SEARCH_RESULT:
		result = OnSearchCompleted ();
		break;
	case LDAP_RES_BIND:
		result = OnBindCompleted ();
		break;
	case LDAP_TIMEOUT:
		result = 0;
		break;
	case 0: 
		break;
	default:
		if (result > 0)
		{
			int err = ldap_result2error (m_ldap, message, 0);
			result = ((err != 0) ? err : result);
		}

		if (OnError(result))
		{
			DisplayError (XP_LDAP_OPEN_SERVER_FAILED, GetHostDescription(), result);
			result = -1;
		}
		break;
	}

	if (message != NULL)
		ldap_msgfree (message);
	return result;
}

	
int net_LdapConnectionData::Interrupt ()
{
	// The destructor will unbind.
	int ldapErr = ldap_abandon (m_ldap, m_messageId);
	return ldapErr;
}


int net_LdapConnectionData::Unbind ()
{
	int ldapErr = 0;
	if (m_ldap)
	{
		ldapErr = ldap_unbind(m_ldap);
		m_ldap = NULL;
	}
	return ldapErr;
}


//*****************************************************************************
//
// net_LdapToHtml does the work of converting LDAP entries into HTML
// which can be displayed in a browser window
//

const int kNumAttributeNames = 24;

class net_LdapToHtml : public net_LdapConnectionData
{
public:
	net_LdapToHtml (ActiveEntry*);

	virtual int OnEntryFound (LDAPMessage *entry);
	virtual int OnSearchCompleted();
	virtual int16 GetLocalCharSet ();

	typedef struct
	{
		char *attributeName;
		int resourceId;
	} AttributeName;
    
protected:
	virtual LdapConnectionType GetConnectionType() { return kHtmlConnection; }

	int OpenStream ();
	int CloseStream ();
	int WriteLineToStream (const char *line, XP_Bool bold, XP_Bool lineBreak, XP_Bool isMailto = FALSE, 
                           XP_Bool isUri = FALSE, XP_Bool aligntop = FALSE, XP_Bool isCell = FALSE);
	NET_StreamClass *m_stream;

	const char *LookupAttributeName (const char *attrib);
	char *GetTableCaption (LDAPMessage*);

	int WriteEntryToStream (LDAPMessage*);
	int WriteValue (const char *, XP_Bool isMailto = FALSE, XP_Bool isUri = FALSE);
	int WriteAttribute (const char *);
	int WritePlainLine (const char *);


	XP_Bool IsMailtoAttribute (const char *);
	XP_Bool IsUriAttribute (const char *);

	char *ConvertDnToUri (const char *);
	char *EscapeSpaces (const char *);
	char *InferUrlBase ();

	static AttributeName m_names[kNumAttributeNames];
};


// Note that these are sorted. Someday I'll match attribs by binary search
//
// These are initialized to zero instead of their real resource IDs because 
// the HP/UX compiler seems not to be capable of doing that

net_LdapToHtml::AttributeName net_LdapToHtml::m_names[kNumAttributeNames] = { 
	{ "carlicense",               0 },
	{ "cn",                       0 },
	{ "businesscategory",         0 },
	{ "departmentnumber",         0 },
	{ "description",              0 },
	{ "employeetype",             0 },
	{ "facsimiletelephonenumber", 0 },
	{ "givenname",                0 },
	{ "l",                        0 },
	{ "jpegPhoto",                0 },
	{ "mail",                     0 },
	{ "manager",                  0 },
	{ "o",                        0 },
	{ "objectclass",              0 },
	{ "ou",                       0 },
	{ "postaladdress",            0 },
	{ "postalcode",               0 },
	{ "secretary",                0 },
	{ "sn",                       0 },
	{ "street",                   0 },
	{ "telephonenumber",          0 },
	{ "title",                    0 }
	HG38731
};


net_LdapToHtml::net_LdapToHtml (ActiveEntry *ce) : net_LdapConnectionData (ce)
{
	m_stream = NULL;
}


int net_LdapToHtml::OnEntryFound (LDAPMessage *message)
{
	int status = 0;
	if (!m_stream)
		status = OpenStream();
	if (0 == status)
		status = WriteEntryToStream (message);
	return status;
}

int net_LdapToHtml::OnSearchCompleted ()
{
	CloseStream();
	return net_LdapConnectionData::OnSearchCompleted();
}


int16 net_LdapToHtml::GetLocalCharSet ()
{
	if (m_csid == -1)
	{
		INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_ce->window_id);
		m_csid = INTL_GetCSIWinCSID(c);
		if (CS_DEFAULT == m_csid)
			m_csid = INTL_DefaultWinCharSetID (0);
	}
	return m_csid;
}


int net_LdapToHtml::OpenStream ()
{
	int err = 0;

	if (NULL != m_stream)
		return err;

	if (!m_ce->URL_s->content_type)
		StrAllocCopy(m_ce->URL_s->content_type, TEXT_HTML);
	m_stream = NET_StreamBuilder(m_ce->format_out, m_ce->URL_s, m_ce->window_id);
	if (!m_stream)
		return MK_OUT_OF_MEMORY;

	// Scribble HTML-starting stuff into the stream
	char *htmlHeaders = PR_smprintf ("<HTML><HEAD><TITLE>%s</TITLE></HEAD>%s<BODY>%s", XP_GetString(MK_LDAP_HTML_TITLE), LINEBREAK, LINEBREAK);
	if (htmlHeaders)
		err = (*(m_stream)->put_block)(m_stream, htmlHeaders, XP_STRLEN(htmlHeaders));
	else
		err = MK_OUT_OF_MEMORY;

	if (1 == err)
		err = 0; //### which is it: 1 for success or 0 for success?

	return err;
}


int net_LdapToHtml::CloseStream ()
{
	int err = 0;

	// Never got any hits for this operation
	if (!m_stream)
	{
		if (((err = OpenStream()) == 0) && m_stream)
		{
			char *noMatches = PR_smprintf ("<H1>%s</H1>", XP_GetString(MK_MSG_SEARCH_FAILED));
			if (noMatches)
			{
				err = (*(m_stream)->put_block)(m_stream, noMatches, XP_STRLEN(noMatches));
				XP_FREE(noMatches);
			}
		}
	}

	// Scribble HTML-ending stuff into the stream
	if (m_stream)
	{
		char htmlFooters[32];
		PR_snprintf (htmlFooters, sizeof(htmlFooters), "</BODY>%s</HTML>%s", LINEBREAK, LINEBREAK);
		err = (*(m_stream)->put_block)(m_stream, htmlFooters, XP_STRLEN(htmlFooters));
		if (1 == err)
			err = 0; //### which is it: 1 for success or 0 for success?

		// complete the stream 
		(*m_stream->complete) (m_stream);
		XP_FREE(m_stream);
	}

	return err;
}


const char *net_LdapToHtml::LookupAttributeName (const char *attrib)
{
	// If we know about this attribute (it has a DIR_AttributeId and we
	// have a DIR_Server to ask, get the name from the DIR_Server
	DIR_AttributeId id;
	if (0 == DIR_AttributeNameToId (attrib, &id) && m_server)
		return DIR_GetAttributeName (m_server, id);

	// If we didn't get the name from LDAP prefs, get it using our
	// own hacky internal table. ### This should really move into the LDAP SDK.

	// Fill these resource IDs in here since the HP/UX compiler can't do it
	// in the initializer for the static array.
	if (m_names[0].resourceId == 0)
	{
		m_names[0].resourceId = MK_LDAP_CAR_LICENSE;   
		m_names[1].resourceId = MK_LDAP_COMMON_NAME;   
		m_names[2].resourceId = MK_LDAP_BUSINESS_CAT;  
		m_names[3].resourceId = MK_LDAP_DEPT_NUMBER;   
		m_names[4].resourceId = MK_LDAP_DESCRIPTION;   
		m_names[5].resourceId = MK_LDAP_EMPLOYEE_TYPE; 
		m_names[6].resourceId = MK_LDAP_FAX_NUMBER;    
		m_names[7].resourceId = MK_LDAP_GIVEN_NAME;    
		m_names[8].resourceId = MK_LDAP_LOCALITY;      
		m_names[9].resourceId = MK_LDAP_PHOTOGRAPH;    
		m_names[10].resourceId = MK_LDAP_EMAIL_ADDRESS; 
		m_names[11].resourceId = MK_LDAP_MANAGER;       
		m_names[12].resourceId = MK_LDAP_ORGANIZATION;  
		m_names[13].resourceId = MK_LDAP_OBJECT_CLASS;  
		m_names[14].resourceId = MK_LDAP_ORG_UNIT;      
		m_names[15].resourceId = MK_LDAP_POSTAL_ADDRESS;
		m_names[16].resourceId = MK_LDAP_POSTAL_CODE;   
		m_names[17].resourceId = MK_LDAP_SECRETARY;     
		m_names[18].resourceId = MK_LDAP_SURNAME;       
		m_names[19].resourceId = MK_LDAP_STREET;        
		m_names[20].resourceId = MK_LDAP_PHONE_NUMBER;  
		m_names[21].resourceId = MK_LDAP_TITLE;         
		m_names[22].resourceId = HG87272;
		m_names[23].resourceId = HG73272;
	}

	for (int i = 0; i < kNumAttributeNames; i++)
	{
		if (!XP_STRCASECMP(attrib, m_names[i].attributeName))
			return XP_GetString (m_names[i].resourceId);
	}
	
	// ### Boy, is this hacky. Maybe we can push this into default prefs now that we have excluded attributes
	if (XP_STRCASECMP(attrib, "userpassword") && XP_STRCMP(attrib, "objectclass") && 
		XP_STRCASECMP(attrib, "subtreeaci"))
		return attrib;
	return NULL;
}


int net_LdapToHtml::WriteLineToStream (const char *line, XP_Bool bold, XP_Bool lineBreak, XP_Bool isMailto, XP_Bool isUri, XP_Bool aligntop, XP_Bool isCell)
{
#define kMailtoTemplate "<A HREF=\"mailto:%s\">%s</A>"
#define kUriTemplate "<A HREF=\"%s\">%s</A>"

	int err = 0;
	int htmlLen = XP_STRLEN(line) + XP_STRLEN("<TD VALIGN=TOP></TD>");
	if (bold)
		htmlLen += XP_STRLEN ("<B></B>");
	if (lineBreak)
		htmlLen += XP_STRLEN(LINEBREAK);
	if (isMailto)
		htmlLen += XP_STRLEN(kMailtoTemplate) + XP_STRLEN(line);
	if (isUri)
		htmlLen += XP_STRLEN(kUriTemplate) + XP_STRLEN(line);

	char *htmlLine = (char *) XP_ALLOC(htmlLen + 1);
	if (htmlLine)
	{
		htmlLine[0] = '\0';
		if (isCell)
		{
			if (aligntop) 
				XP_STRCAT (htmlLine, "<TD VALIGN=TOP>");
			else
				XP_STRCAT (htmlLine, "<TD>");
		}

		if (bold)
			XP_STRCAT (htmlLine, "<B>");

		if (isMailto)
		{
			char *mailtoHtml = PR_smprintf (kMailtoTemplate, line, line);
			if (mailtoHtml)
			{
				XP_STRCAT(htmlLine, mailtoHtml);
				XP_FREE(mailtoHtml);
			}
		} 
		else if (isUri)
		{
			// RFC 2079 sez the URL comes first, then zero or more spaces, then an optional label
			char *label = NULL;
			char *url = XP_STRDUP(line);
			if (url)
			{
				int i = 0;
				while (url[i] && url[i] != ' ')
					i++;
				if (url[i] == ' ')
				{
					url[i] = '\0';

					int j = i + 1;
					while (url[j] == ' ')
						j++;

					label = &url[j];
				}
			}

			char *uriHtml = PR_smprintf (kUriTemplate, url ? url : line, label ? label : line);
			if (uriHtml)
			{
				XP_STRCAT(htmlLine, uriHtml);
				XP_FREE(uriHtml);
			}
			XP_FREEIF(url);
		} 
		else 
			XP_STRCAT (htmlLine, line);

		if (bold)
			XP_STRCAT (htmlLine, "</B> ");
		if (isCell)
			XP_STRCAT (htmlLine, "</TD>");
		if (lineBreak)
			XP_STRCAT (htmlLine, LINEBREAK);
		err = (*(m_stream)->put_block)(m_stream, htmlLine, XP_STRLEN(htmlLine));
		XP_FREE (htmlLine);
	}
	else
		err = MK_OUT_OF_MEMORY;
	return err;
}


int net_LdapToHtml::WriteAttribute (const char *attrib)
{
	return WriteLineToStream (attrib, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE);
}


int net_LdapToHtml::WriteValue (const char *value, XP_Bool isMailto, XP_Bool isUri)
{
	return WriteLineToStream (value, TRUE, TRUE, isMailto, isUri, FALSE, TRUE);
}


int net_LdapToHtml::WritePlainLine (const char *line)
{
	return WriteLineToStream (line, FALSE, TRUE);
}


XP_Bool net_LdapToHtml::IsMailtoAttribute (const char *attrib)
{
	if (m_server)
	{
		const char **strings = DIR_GetAttributeStrings (m_server, mail);
		if (strings)
		{
			int i = 0;
			while (strings[i])
			{
				if (!XP_STRCASECMP(attrib, strings[i]))
					return TRUE;
				i++;
			}
		}
	}
	else
		if (!XP_STRCASECMP(attrib, "mail"))
			return TRUE;

	return FALSE;
}


XP_Bool net_LdapToHtml::IsUriAttribute (const char *attrib)
{
	switch (XP_TO_LOWER(attrib[0]))
	{
	case 'l':
		if (!XP_STRCASECMP(attrib, "labeleduri") || // preferred
			!XP_STRCASECMP(attrib, "labeledurl"))   // old-style
			return TRUE;
		break;
	case 'u':
		if (!XP_STRCASECMP(attrib, "url")) // apparently some servers do this. be lenient in what we accept
			return TRUE;
		break;
	}
	return FALSE;
}


char *net_LdapToHtml::InferUrlBase ()
{
	// Look in Netlib's data structures to find the URL we're running right now
	// and assume that new URLs have the same host, port, etc 

	// I'm guessing we can just look for everything up to the third slash. If this
	// turns out to be not true, we can do the more expensive thing of running it
	// through ldap_url_parse and then reforming it from the ldap_url_desc

	char *url = m_ce->URL_s->address;

	// Small memory wastage here, but this buf is so short-lived, it's probably
	// not worth figuring out the real right size
	char *urlBase = (char*) XP_ALLOC (XP_STRLEN(url)); 

	if (urlBase)
	{
		char *tmpSrc = url;
		char *tmpDst = urlBase;
		int slashCount = 0;

		while (*tmpSrc)
		{
			if (*tmpSrc == '/')
				slashCount++;
			if (slashCount == 3)
			{
				*tmpDst = '\0';
				break;
			}
			else
				*tmpDst++ = *tmpSrc;
			tmpSrc++;
		}

	}
	return urlBase;

}


char *net_LdapToHtml::EscapeSpaces (const char *string)
{
	int length = XP_STRLEN (string) + 1; // +1 for trailing null
	const char *tmp = string;
	while (*tmp)
	{
		if (*tmp == 0x20)
			length += 2; // From " " to "%20"
		tmp++;
	}

	char *result = (char*) XP_ALLOC (length);
	if (result)
	{
		tmp = string;
		char *tmp2 = result;

		while (*tmp)
		{
			if (*tmp == 0x20)
			{
				*tmp2++ = '%';
				*tmp2++ = '2';
				*tmp2++ = '0';
			}
			else
				*tmp2++ = *tmp;
			*tmp++;
		}
		*tmp2 = '\0';
	}

	return result;
}


char *net_LdapToHtml::ConvertDnToUri (const char *dn)
{
	char *uri = NULL;
	char **explodedDn = ldap_explode_dn (dn, TRUE /* suppress types? */);
	if (explodedDn)
	{
		char *urlBase = InferUrlBase();
		if (urlBase)
		{
			char *unescapedUrl = PR_smprintf ("%s/%s", urlBase, dn);
			if (unescapedUrl)
			{
				char *escapedUrl = EscapeSpaces (unescapedUrl);
				if (escapedUrl)
				{
					uri = PR_smprintf ("%s %s", escapedUrl, explodedDn[0]);
					XP_FREE(escapedUrl);
				}
				XP_FREE(unescapedUrl);
			}
			XP_FREE(urlBase);
		}
		ldap_value_free (explodedDn);
	}

	return uri;
}


char *net_LdapToHtml::GetTableCaption (LDAPMessage *message)
{
	// Here's a somewhat arbitrary way to decide what the table caption for an entry should be

	char *caption = NULL;

	// Look first for a "cn" attribute.
	const char *cnAttrName = GetAttributeString (cn);
	if (cnAttrName)
	{
		char **cnValues = ldap_get_values (m_ldap, message, cnAttrName);
		if (cnValues)
		{
			if (cnValues[0])
				caption = ConvertFromServerCharSet (cnValues[0]);
			ldap_value_free (cnValues);
		}
	}

	if (!caption)
	{
		// Use the left-most (most specific) component of the DN
		char *dn = ldap_get_dn (m_ldap, message);
		if (dn)
		{
			char **explodedDn = ldap_explode_dn (dn, TRUE /* suppress types? */);
			if (explodedDn && explodedDn[0])
				caption = ConvertFromServerCharSet (explodedDn[0]);
			ldap_value_free (explodedDn);
			ldap_memfree(dn); 
		}
	}

	return caption;
}


int net_LdapToHtml::WriteEntryToStream (LDAPMessage *message)
{
	// Write an LDAP directory entry to the HTML stream

	XP_ASSERT(message && m_stream);
	if (!message || !m_stream)
		return -1;

	WritePlainLine ("<TABLE ALIGN=CENTER>");

	char *caption = GetTableCaption (message);
	if (caption)
	{
		char *captionHtml = PR_smprintf ("<TD><CAPTION><FONT COLOR=\"#1908B9\"><FONT SIZE=+2>%s</FONT></FONT> </CAPTION></TD>", caption);
		if (captionHtml)
		{
			WritePlainLine (captionHtml);
			XP_FREE(captionHtml);
		}
		XP_FREE(caption);
	}

	BerElement *ber;
	char *attrib = ldap_first_attribute (m_ldap, message, &ber);
	while (attrib)
	{
		const char *attribName = LookupAttributeName (attrib);
		if (attribName && !DIR_IsAttributeExcludedFromHtml (m_server, attrib))
		{
			int valueIndex = 0;

			HG73211
			if (XP_STRCASECMP(attrib, "jpegPhoto") == 0 || XP_STRCASECMP(attrib, "audio") == 0)
			{
				struct berval **berImages = ldap_get_values_len (m_ldap, message, attrib);
				while (berImages[valueIndex])
				{
					WritePlainLine ("<TR>");
					if (valueIndex == 0)
						WriteAttribute (attribName);
					else
						WritePlainLine("<TD></TD>");

					// ### There are Win16 problems with this strategy. We could use libmime 
					// to stream the base64 bits one line at a time, but we'd still need to get the 
					// LDAP SDK to do something similar, or to use huge pointers.

					char *base64Encoding = NET_Base64Encode (
						berImages[valueIndex]->bv_val,
						berImages[valueIndex]->bv_len);
					if (base64Encoding)
					{
						char *htmlLine = NULL;
						if (XP_STRCASECMP(attrib, "jpegPhoto") == 0)
							htmlLine = PR_smprintf ("<TD><IMG SRC=\"data:image/jpeg;base64,%s\"></TD>", base64Encoding);
						else
							htmlLine = PR_smprintf ("<TD><A HREF=\"data:audio/basic;base64,%s\"><IMG SRC=\"/ns-icons/sound.gif\"></A></TD>", base64Encoding);
						if (htmlLine)
						{
							WritePlainLine (htmlLine);
							XP_FREE (htmlLine);
						}
						XP_FREE(base64Encoding);
					}
					WritePlainLine ("</TR>");
					valueIndex++;
				}
				ldap_value_free_len (berImages);
			}
			else
			{
				char **values = ldap_get_values (m_ldap, message, attrib);
				if (values)
				{
					XP_Bool isMailtoAttribute = IsMailtoAttribute (attrib);
					XP_Bool isUriAttribute = IsUriAttribute (attrib);
					XP_Bool isDnAttribute = DIR_IsDnAttribute (m_server, attrib); 
					XP_Bool isEscapedAttribute = DIR_IsEscapedAttribute (m_server, attrib);

					while (values[valueIndex])
					{
						char *decodedUtf8 = ConvertFromServerCharSet (values[valueIndex]);
						if (decodedUtf8)
						{
							WritePlainLine ("<TR>");
							if (valueIndex == 0)
								WriteAttribute (attribName);
							else
								WritePlainLine("<TD></TD>");

							// This unescaping code is mandated by. DNs are one class of thing, and 
							// attributes which are "productions" of strings are another class of thing
							// and plain old string attributes get left alone.
							if (isDnAttribute)
							{
								// Here's some special code to make values which look like DNs into
								// links to LDAP URLs. Cool, huh?
								char *tmp = ConvertDnToUri (decodedUtf8);
								if (tmp)
								{
									XP_FREE(decodedUtf8);
									decodedUtf8 = tmp;
									isUriAttribute = TRUE;
								}
							}
							else if (isEscapedAttribute)
							{
								// Some attributes which are "productions" of a couple of strings have
								// special encoding rules. Decode 'em into something useful here.
								char *tmp = DIR_Unescape (decodedUtf8, TRUE /*makeHtml?*/);
								if (tmp)
								{
									XP_FREE(decodedUtf8);
									decodedUtf8 = tmp;
								}
							}

							WriteValue (decodedUtf8, isMailtoAttribute, isUriAttribute);
							XP_FREE(decodedUtf8);
							WritePlainLine ("</TR>");
						}
						valueIndex++;
					}
					ldap_value_free (values);
				}
			}
		}
		ldap_memfree (attrib);
		attrib = ldap_next_attribute (m_ldap, message, ber);
	}

	WritePlainLine ("</TABLE>");
	WritePlainLine ("<HR>");

	return 0;
}


//*****************************************************************************
//
// net_LdapReplicator is the class which knows how to read replication entries
// from the LDAP server to the local Address Book using the "change log" format 
// specified in:
// http://ds.internic.net/internet-drafts/draft-good-ldap-changelog-00.txt
//

#include "garray.h"

extern "C"
{
	extern int MK_OUT_OF_MEMORY;
	extern int MK_LDAP_REPL_CANT_SYNC_REPLICA;
	extern int MK_LDAP_REPL_DSE_BOGUS;
	extern int MK_LDAP_REPL_CHANGELOG_BOGUS;
	extern int MK_LDAP_REPL_PROGRESS_TITLE;
	extern int MK_LDAP_REPL_CONNECTING;
	extern int MK_LDAP_REPL_CHANGE_ENTRY;
	extern int MK_LDAP_REPL_NEW_ENTRY;
	extern int MK_LDAP_AUTHDN_LOOKUP_FAILED;
}

#define LDAP_REPL_CHUNK_SIZE 50


static void CancelReplication(void *closure)
{
	MWContext *progressContext = (MWContext *)closure;
	if (progressContext)
		XP_InterruptContext(progressContext);
}


class net_LdapReplicator : public net_LdapConnectionData
{
public:
	net_LdapReplicator(ActiveEntry *ce);
	virtual ~net_LdapReplicator();

	virtual int Interrupt ();

	virtual int	OnBindCompleted ();
	virtual int OnEntryFound (LDAPMessage *message);
	virtual int OnSearchCompleted ();
	virtual XP_Bool OnError(int err);

protected:

	typedef enum _LdapReplicationState
	{
		kInitializing,
		kAuthorizing,
		kBinding,
		kParsingRootDSE,
		kReplicatingChanges,
		kReplicatingAdditions,
		kReplicatingEntries
	} LdapReplicationState;

	virtual LdapConnectionType GetConnectionType() { return kReplicatorConnection; }

	XP_Bool CollectUserCredentials ();
	void SaveCredentialsToPrefs ();
	void UpdateProgress ();

	int Load (ActiveEntry *);
	int Process ();
	int Initialize ();
	int Authorize ();
	int Bind ();
	int SearchRootDSE ();
	int Replicate (XP_Bool initial = FALSE);
	int ProcessAdditions ();

	int ParseAuthorizationEntry (LDAPMessage *);
	int ParseRootDSE (LDAPMessage *);
	int ParseChangelogEntry (LDAPMessage *);
	int ParseInitialEntry (LDAPMessage *);
	void ParseReplicationUrl(const char *);

	XP_Bool m_initialReplicationOkay;
	XP_Bool m_changesReturned;
	char *m_authDn;
	char *m_authPwd;
	char *m_valueUsedToFindDn;
	char *m_dnOfChangelog;		    // Root DSE's setting for the DN suffix of changelog entries
	char *m_dataVersion;		    // Root DSE's setting to scope change numbers. The data version
	int32 m_firstChangeNumber;	    // Root DSE's setting for oldest change it knows
	int32 m_lastChangeNumber;	    // Root DSE's setting for newest change it knows
								    // gets reset whenever the server's DB gets reloaded from LDIF

	int32      m_progressEntry;
	int32      m_progressTotal;
	MWContext *m_progressContext;
	pw_ptr     m_progressWindow;

	LdapReplicationState m_replState;
	CXP_GrowableArray m_dnsToAdd;

	AB_ContainerInfo *m_container;

	static char *m_rootReplicationAttribs[5];
	static char *m_entryReplicationAttribs[3];
};

char *net_LdapReplicator::m_rootReplicationAttribs[5] =
    { "changelog", "firstChangeNumber", "lastChangeNumber", "dataVersion", NULL };
char *net_LdapReplicator::m_entryReplicationAttribs[3] =
    { "targetdn", "changetype", NULL };


net_LdapReplicator::net_LdapReplicator (ActiveEntry *ce) : net_LdapConnectionData (ce, FALSE)
{
	m_replState = kInitializing;

	m_initialReplicationOkay = FALSE;
	m_changesReturned = FALSE;
	m_authDn = NULL;
	m_authPwd = NULL;
	m_valueUsedToFindDn = NULL;
	m_dnOfChangelog = NULL;
	m_dataVersion = NULL;
	m_firstChangeNumber = 0;
	m_lastChangeNumber = 0;

	m_progressEntry = 0;
	m_progressTotal = 0;
	m_progressContext = NULL;
	m_progressWindow = NULL;

	m_container = NULL;
}

net_LdapReplicator::~net_LdapReplicator ()
{
	int size;

	XP_FREEIF(m_authDn);
	XP_FREEIF(m_authPwd);
	XP_FREEIF(m_dnOfChangelog);
	XP_FREEIF(m_dataVersion);

	size = m_dnsToAdd.Size();
	if (size > 0)
	{
		while (--size >= 0)
			XP_FREE(m_dnsToAdd[size]);
	}

	if (m_container)
	{
		AB_EndReplication(m_container);
		m_container = NULL;
	}

	if (m_progressWindow)
	{
		PW_Hide(m_progressWindow);
		PW_Destroy(m_progressWindow);

	    if (m_progressContext)
		    PW_DestroyProgressContext(m_progressContext);
	}
}


int	net_LdapReplicator::OnBindCompleted ()
{
	return SearchRootDSE();
}

int net_LdapReplicator::OnEntryFound (LDAPMessage *message)
{
	int status = 0;

	switch (m_replState)
	{
	case kAuthorizing:
		status = ParseAuthorizationEntry (message);
		break;
	case kParsingRootDSE:
		status = ParseRootDSE (message);
		break;
	case kReplicatingChanges:
		status = ParseChangelogEntry (message);
		break;
	case kReplicatingAdditions:
	case kReplicatingEntries:
		status = ParseInitialEntry (message);
		break;
	default:
		XP_ASSERT(FALSE);
	}

	return status;
}

int net_LdapReplicator::OnSearchCompleted ()
{
	int status = 0;

	switch (m_replState)
	{
	case kAuthorizing:
		status = Bind();
		break;
	case kParsingRootDSE:
		status = Replicate (TRUE);
		break;
	case kReplicatingAdditions:
		m_server->replInfo->lastChangeNumber++;
		/* fall-through */
	case kReplicatingChanges:
		status = ProcessAdditions ();
		break;
	case kReplicatingEntries:
		if (m_initialReplicationOkay)
			m_server->replInfo->lastChangeNumber = m_lastChangeNumber;
		if (m_server->replInfo->lastChangeNumber >= 0)
		{
			SaveCredentialsToPrefs ();
			DIR_SaveServerPreferences (FE_GetDirServers());
		}
		status = MK_CONNECTED;
		break;
	default:
		XP_ASSERT(FALSE);
	}

	return status;
}

XP_Bool net_LdapReplicator::OnError(int err)
{
	/* If we were performing the initial replication, destroy the subset of
	 * entries that we had replicated so far.
	 */
	if (m_server->replInfo->lastChangeNumber < 0)
		AB_RemoveReplicaEntries(m_container);

	/* Because replication can be very expensive, we must try to save the data
	 * we have already replicated even in the event of an error.
	 */
	DIR_SaveServerPreferences (FE_GetDirServers());

	return TRUE;
}

XP_Bool net_LdapReplicator::CollectUserCredentials ()
{
	/* First look in the DIR_Server we read out of the prefs. If it already
	 * knows the user's credentials, there's no need to ask again here.
	 */
	if (m_server->authDn && XP_STRLEN(m_server->authDn))
		m_authDn = XP_STRDUP (m_server->authDn);
	if (m_server->savePassword && m_server->password && XP_STRLEN(m_server->password))
		m_authPwd = XP_STRDUP (m_server->password);
	if (m_authDn && m_authPwd)
		return TRUE;

	XP_Bool rc = FALSE;
	char *prompt = PR_smprintf (XP_GetString(MK_LDAP_AUTH_PROMPT),
	                            DIR_GetAttributeName (m_server, auth),
								m_server->description);
	if (prompt)
	{
		/* TBD: Only prompt for password if we already have the DN
		 */
		char *username = NULL, *password = NULL;
		if (m_authDn)
		{
			password = FE_PromptPassword (m_ce->window_id, prompt);
			rc = (password != NULL);
		}
		else
			rc = FE_PromptUsernameAndPassword (m_ce->window_id, prompt, &username, &password);

		if (rc)
		{
			if (username)
				m_valueUsedToFindDn = username;
			if (password)
			{
				m_authPwd = SECNAV_MungeString (password);
				XP_FREE(password);
			}
		}

		XP_FREE(prompt);
	}

	return rc;
}

void net_LdapReplicator::SaveCredentialsToPrefs ()
{
	if (m_authDn)
		DIR_SetAuthDN (m_server, m_authDn);

	if (m_server->savePassword && m_authPwd)
		DIR_SetPassword (m_server, m_authPwd);
}

void net_LdapReplicator::UpdateProgress ()
{
	++m_progressEntry;

	if (!m_progressContext)
		return;

	if (m_replState == kReplicatingAdditions)
	{
		int percent = (m_progressEntry * 100) / m_progressTotal;
		FE_SetProgressBarPercent (m_progressContext, percent);
	}

	if ((m_progressEntry % MAX(1, m_progressEntry / 10)) == 0)
	{
		char *status;
		
		if (m_replState == kReplicatingAdditions)
			status = PR_smprintf (XP_GetString(MK_LDAP_REPL_CHANGE_ENTRY), m_progressEntry);
		else
			status = PR_smprintf (XP_GetString(MK_LDAP_REPL_NEW_ENTRY), m_progressEntry);

		if (status)
		{
			FE_Progress (m_progressContext, status);
			XP_FREE(status);
		}
	}
}


int net_LdapReplicator::Load (ActiveEntry *ce) 
{
	XP_ASSERT(ce);
	XP_ASSERT(ce->URL_s);
	XP_ASSERT(ce->URL_s->address);

	/* If this URL was not started via NET_ReplicateDirectory, we need to
	 * create a progress window for it.
	 */
	if (!ce->URL_s->internal_url)
	{
		m_progressContext = PW_CreateProgressContext();
		if (m_progressContext)
		{
			m_progressWindow = PW_Create(NULL, pwStandard);
			if (m_progressWindow)
			{
				PW_AssociateWindowWithContext(m_progressContext, m_progressWindow);
				PW_SetWindowTitle(m_progressWindow, XP_GetString(MK_LDAP_REPL_PROGRESS_TITLE));
				PW_SetLine1(m_progressWindow, NULL);
				PW_SetLine2(m_progressWindow, NULL);
				PW_SetProgressRange(m_progressWindow, 0, 0);
				PW_SetCancelCallback(m_progressWindow, CancelReplication, m_progressContext);
				PW_Show(m_progressWindow);
			}
			else
			{
				PW_DestroyProgressContext(m_progressContext);
				m_progressContext = NULL;
			}
		}
	}
	else
		m_progressContext = ce->window_id;

	if (m_progressContext)
	{
		FE_Progress (m_progressContext, XP_GetString(MK_LDAP_REPL_CONNECTING));
		FE_SetProgressBarPercent (m_progressContext, 0);
	}

	/* Get the DIR_Server that matches the hostname in the URL.
	 */
	ParseReplicationUrl (ce->URL_s->address);
	if (m_server == NULL)
		return -1;

	/* Initialize the LDAP structure
	 *
	 * TBD: need to account for the case where one host name corresponds to
	 *      more than one machine (e.g. directory.netscape.com).
	 */
	m_ldap = ldap_init(m_server->serverName, m_server->port);
	if (!m_ldap)
		return MK_OUT_OF_MEMORY;

	return 0;
}

int net_LdapReplicator::Process ()
{
	int status = 0;

	if (m_replState == kInitializing)
		status = Initialize ();
	else
		status = net_LdapConnectionData::Process ();

	return status;
}

int net_LdapReplicator::Initialize ()
{
	int status = 0;

	/* If we are supposed to authenticate with the server, attempt to bind to
	 * the server using authentication info from the prefs or the user.
	 */
	if (m_server->enableAuth)
	{
		if (CollectUserCredentials())
		{
			/* If we don't have an authorization DN but we have an email id,
			 * then try to look up the authorization DN.
			 */
			if (!m_authDn && m_valueUsedToFindDn)
				return Authorize();
			else
				return Bind();
		}
	}

	return SearchRootDSE();
}

int net_LdapReplicator::Authorize ()
{
	int status = 0;

	char *attribs[2];
	attribs[0] = XP_STRDUP(DIR_GetFirstAttributeString (m_server, cn));
	attribs[1] = NULL;

	char *filter = PR_smprintf ("(%s=%s)", DIR_GetFirstAttributeString (m_server, auth), m_valueUsedToFindDn);

	int msgId = -1;
	if (attribs[0] && filter)
		msgId = ldap_search (m_ldap, m_server->searchBase, LDAP_SCOPE_SUBTREE, filter, &attribs[0], 0);
	else
		status = MK_OUT_OF_MEMORY;

	XP_FREEIF(attribs[0]);
	XP_FREEIF(filter);

	if (msgId != -1)
	{
		m_replState = kAuthorizing;
		m_messageId = msgId;
	}
	else if (status == 0)
	{
		DisplayError (XP_LDAP_SEARCH_FAILED, GetHostDescription(), ldap_get_lderrno(m_ldap,NULL,NULL));
		status = -1;
	}

	return status;
}

int net_LdapReplicator::Bind ()
{
	char *upwd = SECNAV_UnMungeString (m_authPwd);
	if (upwd)
	{
		int msgId = ldap_simple_bind(m_ldap, m_authDn, upwd);
		XP_FREE(upwd);

		if (msgId != -1)
		{
			m_replState = kBinding;
			m_messageId = msgId;
		}
		else
		{
			DisplayError (XP_LDAP_BIND_FAILED, GetHostDescription(), ldap_get_lderrno(m_ldap,NULL,NULL));
			return -1;
		}
	}
	else
		return MK_OUT_OF_MEMORY;

	return 0;
}

int net_LdapReplicator::SearchRootDSE ()
{
	/* Kick things off with a search of the Root DSE for the replication data.
	 */
	int msgId = ldap_search(m_ldap, "", LDAP_SCOPE_BASE, "objectclass=*",
					        m_rootReplicationAttribs, 0);
	if (msgId != -1)
	{
		m_replState = kParsingRootDSE;
		m_messageId = msgId;
	}
	else
	{
		DisplayError (XP_LDAP_SEARCH_FAILED, GetHostDescription(), ldap_get_lderrno(m_ldap,NULL,NULL));
		return -1;
	}

	return 0;
}

int net_LdapReplicator::Replicate (XP_Bool initial)
{
	int msgId = -1;

	/* If the last completed change is -1, we need to perform an initial
	 * replication.
	 */
	if (m_server->replInfo->lastChangeNumber == -1)
	{
		/* We're going to replicate every entry.
		 */
		m_replState = kReplicatingEntries;

		/* Delete the old replica database and start a fresh one.
		 */
		AB_RemoveReplicaEntries(m_container);

		/* Try to do a search which will return every entry (limited by the
		 * filter preference) on the server.
		 *
		 * TBD: This fails even on thehole unless Directory Manager is used.
		 *      We must have a special replication dn or find some other way
		 *      to initialize the database.
		 */
		msgId = ldap_search(m_ldap, m_server->searchBase, LDAP_SCOPE_SUBTREE,
		                   	DIR_GetReplicationFilter(m_server), AB_GetReplicaAttributeNames(m_container), 0);
	}
	else if (m_lastChangeNumber > m_server->replInfo->lastChangeNumber)
	{
		/* We're going to replicate only the changes.
		 */
		m_replState = kReplicatingChanges;

		if (initial)
			m_progressTotal = m_lastChangeNumber - m_server->replInfo->lastChangeNumber;

		char *filter = PR_smprintf ("(&(changenumber>=%d)(changenumber<=%d))",
		                            m_server->replInfo->lastChangeNumber + 1,
		                            MIN(m_lastChangeNumber,
									    m_server->replInfo->lastChangeNumber + LDAP_REPL_CHUNK_SIZE));
		if (filter)
		{
			msgId = ldap_search(m_ldap, m_dnOfChangelog, LDAP_SCOPE_ONELEVEL,
		                    	filter, m_entryReplicationAttribs, 0);
			XP_FREE(filter);
		}
	}
	else
	{
		if (!initial)
		{
			SaveCredentialsToPrefs ();
			DIR_SaveServerPreferences (FE_GetDirServers());
		}
		return MK_CONNECTED;
	}

	if (msgId != -1)
		m_messageId = msgId;
	else
	{
		DisplayError (XP_LDAP_SEARCH_FAILED, GetHostDescription(), ldap_get_lderrno(m_ldap,NULL,NULL));
		return -1;
	}

	return 0;
}


int net_LdapReplicator::ProcessAdditions()
{
	int size = m_dnsToAdd.Size();
	if (size > 0)
	{
		int msgId = -1;
		char *targetDn = (char *)(m_dnsToAdd[size - 1]);

		msgId = ldap_search(m_ldap, targetDn, LDAP_SCOPE_SUBTREE,
		                   	DIR_GetReplicationFilter(m_server), AB_GetReplicaAttributeNames(m_container), 0);

		XP_FREE(targetDn);
		m_dnsToAdd.SetSize(size - 1);

		if (msgId != -1)
		{
			m_messageId = msgId;
			m_replState = kReplicatingAdditions;
		}
		else
		{
			DisplayError (XP_LDAP_SEARCH_FAILED, GetHostDescription(), ldap_get_lderrno(m_ldap,NULL,NULL));
			return -1;
		}
	}
	else
	{
		/* We must verify that the changelog search returned at least one
		 * entry.  It may return none if the user has insufficient access.
		 * If so, we must abort the replication; otherwise, we will get stuck
		 * in an infinite loop.
		 */
	    if (m_changesReturned)
		{
			return Replicate();
		}
		else
		{
			DisplayError (XP_LDAP_SEARCH_FAILED, GetHostDescription(), LDAP_INSUFFICIENT_ACCESS);
			return -1;
		}
	}

	return 0;
}

int net_LdapReplicator::Interrupt ()
{
	/* Put the last processed change number into prefs
	 */
	if (m_server->replInfo && m_server->replInfo->lastChangeNumber >= 0)
		DIR_SaveServerPreferences (FE_GetDirServers());

	/* If we were performing the initial replication, destroy the subset of
	 * entries that we had replicated so far.
	 */
	else
		AB_RemoveReplicaEntries(m_container);

	if (m_ldap)
		ldap_abandon (m_ldap, m_messageId);

	return net_LdapConnectionData::Interrupt(); // base class will unbind
}


void net_LdapReplicator::ParseReplicationUrl(const char *ldap_url)
{
	char *url = XP_STRDUP(ldap_url);
	LDAPURLDesc *desc = (LDAPURLDesc *)XP_CALLOC(1, sizeof(LDAPURLDesc));
	if (desc && url)
	{
		char *next;
		char *fullurl = url;

		/* Pull out the connection type (and fill in the default port numbers).
		 */
		if (XP_STRCASECMP(url, "repl-ldaps://") == 0)
		{
			url += 13;
			desc->lud_options |= LDAP_URL_OPT_SECURE;
			desc->lud_port = LDAPS_PORT;
		}
		else
		{
			url += 12;
			desc->lud_port = LDAP_PORT;
		}

		/* Pull out the authorized DN and password if they are present and
		 * requested.
		 */
		char *authDn=NULL, *authPwd=NULL;
		next = XP_STRCHR(url, '@');
		if (next)
		{
			next[0] = '\0';

			char *pwd = XP_STRCHR(url, ':');
			if (pwd)
			{
				pwd[0] = '\0';
				authPwd = pwd + 1;
			}
			authDn = url;

			url = next + 1;
		}

		/* Pull out the host name, the port number and the base dn.
		 */
		next = XP_STRCHR(url, ':');
		if (!next)
			next = XP_STRCHR(url, '/');
		if (!next)
			desc->lud_host = url;
		else
		{
			if (next[0] == ':')
				desc->lud_port = 0;
			next[0] = '\0';
				
			desc->lud_host = url;
			url = next + 1;

			if (desc->lud_port == 0)
			{
				next = XP_STRCHR(url, '/');
				if (next)
					next[0] = '\0';
				desc->lud_port = XP_ATOI(url);
			}

			if (next)
				desc->lud_dn = next + 1;
		}

		/* The host name, port number and base dn must be present and valid.
		 */
		if (desc->lud_host && desc->lud_port != 0 && desc->lud_dn)
		{
			AddLdapServerToPrefs(desc);

			if (m_server)
			{
				if (!m_server->authDn && authDn)
					m_server->authDn = XP_STRDUP(authDn);
				if (!m_server->password && authPwd)
					m_server->password = XP_STRDUP(authPwd);
			}
		}

		url = fullurl;
	}

	XP_FREEIF(desc);
	XP_FREEIF(url);
}


int net_LdapReplicator::ParseAuthorizationEntry (LDAPMessage *message)
{
	if (!m_authDn)
	{
		char *authDn = ldap_get_dn (m_ldap, message);
		m_authDn = XP_STRDUP(authDn);
		ldap_memfree(authDn);
	}
	else
	{
		/* Better not have more than one hit for this search.
		 * We don't have a way for the user to choose which "phil" they are.
		 */
		DisplayError (MK_LDAP_AUTHDN_LOOKUP_FAILED, GetHostDescription(), 0);
		return -1;
	}

	return 0;
}

int net_LdapReplicator::ParseRootDSE (LDAPMessage *message)
{
	int i;
	char **values = NULL;

	for (i = 0; i < 4; i++)
	{
		values = ldap_get_values (m_ldap, message, m_rootReplicationAttribs[i]);
		if (values && values[0])
		{
			switch (i) {
			case 0:
				m_dnOfChangelog = XP_STRDUP (values[0]);
				break;
			case 1:
				m_firstChangeNumber = XP_ATOI (values[0]);
				break;
			case 2:
				m_lastChangeNumber = XP_ATOI (values[0]);
				break;
			case 3:
				m_dataVersion = XP_STRDUP (values[0]);
				break;
			}

			ldap_value_free (values);
		}
		else
		{
			DisplayError (MK_LDAP_REPL_DSE_BOGUS, GetHostDescription(), 0);
			return -1;
		}
	}

	/* Parse the root DSE.  DIR_ValidateRootDSE can return three different
	 * types of values;
	 *   <0 - There is no replication information available for this server;
	 *        we should abort replication.
	 *    0 - Either our replication data matches what the server returned or
	 *        we have never replicated.  In the former case, perform an
	 *        incremental update.  In the later case, generate the replica
	 *        from scratch.
	 *   >0 - The server's replication data does not match ours; reinitialize
	 *        our replica from scratch.
	 */
	int rc = DIR_ValidateRootDSE (m_server, m_dataVersion, m_firstChangeNumber, m_lastChangeNumber);
	if (rc < 0)
	{
		DisplayError (MK_LDAP_REPL_DSE_BOGUS, GetHostDescription(), 0);
		return -1;
	}

	/* Get access to the container info that will manage the interaction
	 * between ldap server connection and the replica database.
	 *
	 * NOTE: This must be called after DIR_ValidateRootDSE, because the later
	 *       is what sets up the replication information in the DIR_Server
	 *       which in turn is required by the container to replicate.
	 */
	m_container = AB_BeginReplication(m_progressContext, m_server);
	if (m_container == NULL)
	{
		DisplayError (MK_LDAP_REPL_CANT_SYNC_REPLICA, GetHostDescription(), 0);
		return -1;
	}

	return 0;
}


// Here we get an entry from the changelog and try to figure out what kind of 
// change happened to the entry given by the targetdn.
int net_LdapReplicator::ParseChangelogEntry (LDAPMessage *message)
{
	char **values = NULL;
	char *targetDn = NULL;

	values = ldap_get_values (m_ldap, message, "targetdn");
	if (values && values[0])
	{
		targetDn = XP_STRDUP(values[0]);
		ldap_value_free (values);
	}
	else
	{
		DisplayError (MK_LDAP_REPL_CHANGELOG_BOGUS, GetHostDescription(), 0);
		return -1;
	}

	values = ldap_get_values (m_ldap, message, "changetype");
	if (values && values[0])
	{
		if (!XP_STRCASECMP(values[0], "add"))
		{
			m_dnsToAdd.Add (targetDn);
		}
		else if (!XP_STRCASECMP(values[0], "mod"))
		{
			AB_DeleteReplicaEntry (m_container,targetDn);
			m_dnsToAdd.Add (targetDn);
		}
		else if (!XP_STRCASECMP(values[0], "del"))
		{
			AB_DeleteReplicaEntry (m_container,targetDn);
			UpdateProgress();
		}
		ldap_value_free (values);
	}
	else
	{
		DisplayError (MK_LDAP_REPL_CHANGELOG_BOGUS, GetHostDescription(), 0);
		return -1;
	}

	m_changesReturned = TRUE;
	return 0;
}

/* ParseInitialEntry
 *
 * Method for adding an entry to the replication database.
 */
int net_LdapReplicator::ParseInitialEntry (LDAPMessage *message)
{
	message = ldap_first_entry (m_ldap, message);
	if (message)
	{
		int mapSize = AB_GetNumReplicaAttributes(m_container);
		char **mapNames = AB_GetReplicaAttributeNames(m_container);
		char **valueList = (char **)XP_ALLOC(sizeof(char *) * mapSize);
		if (!valueList)
		{
			m_initialReplicationOkay = FALSE;
			return MK_OUT_OF_MEMORY;
		}
			
		/* Copy each of the attribute values from the LDAP result into the index
		 * in the value array that corresponds to the same index in the attribute
		 * mapping structure.
		 */
		int i, indexCn = -1, indexOrg = -1;
		char **scratchArray = NULL;
		for (i = 0; i < mapSize; i++)
		{
			scratchArray = ldap_get_values (m_ldap, message, mapNames[i]);
			if (scratchArray && scratchArray[0])
			{
				valueList[i] = DIR_ConvertFromServerCharSet (m_server, scratchArray[0], GetLocalCharSet());
				ldap_value_free (scratchArray);
			}
			else
			{
				if (AB_ReplicaAttributeMatchesId(m_container, i, cn))
					indexCn = i;
				else if (AB_ReplicaAttributeMatchesId(m_container, i, o))
					indexOrg = i;

				valueList[i] = NULL;
			}
		}

		/* If we didn't get some attributes we wanted, try to pick them up from the DN
		 */
		if (indexOrg != -1 || indexCn != -1)
		{
			char *scratch;
			if (scratch = ldap_get_dn (m_ldap, message))
			{
				if ((scratchArray = ldap_explode_dn (scratch, FALSE)) != NULL)
				{
					char *dnComponent = NULL;
					for (i = 0; (dnComponent = scratchArray[i]) != NULL; i++)
					{
						if (indexOrg != -1 && !XP_STRNCASECMP (dnComponent, "o=", 2))
						{
							valueList[indexOrg] = DIR_ConvertFromServerCharSet (m_server, dnComponent+2, GetLocalCharSet());
							indexOrg = -1;
						}
						if (indexCn != -1 && !XP_STRNCASECMP (dnComponent, "cn=", 3))
						{
							valueList[indexCn] = DIR_ConvertFromServerCharSet (m_server, dnComponent+3, GetLocalCharSet());
							indexCn = -1;
						}
					}
					ldap_value_free (scratchArray);
				}
				ldap_memfree (scratch);
			}
		}

		/* If the entry is valid, add it to the replica database.  Also set the
		 * initial replication okay flag to true in case we are performing an
		 * initial replication.  Finally, update the progress window.
		 */
		if (indexCn == -1 && AB_AddReplicaEntry (m_container, valueList) == AB_SUCCESS)
		{
			m_initialReplicationOkay = TRUE;
			UpdateProgress();
		}

		for (i = 0; i < mapSize; i++)
			XP_FREEIF(valueList[i]);
		XP_FREE(valueList);
	}

	return 0;
}


//*****************************************************************************
//
// net_OfflineLdapReplicator is the class which knows which directory replicas
// should be updated and how to replicate them in succession.
//

class net_OfflineLdapReplicator
{
public:
	net_OfflineLdapReplicator(MSG_Pane *, DIR_Server *server = NULL);
	~net_OfflineLdapReplicator();

	XP_Bool ReplicateDirectory();

	static void ReplicateNextDirectory(URL_Struct *, int, MWContext *);

protected:
	MSG_Pane  *m_pane;
	XP_List   *m_serverList;
	MWContext *m_progressContext;
	pw_ptr     m_progressWindow;
};


net_OfflineLdapReplicator::net_OfflineLdapReplicator(MSG_Pane *pane, DIR_Server *server)
{
	m_pane = pane;

	m_progressContext = NULL;
	m_progressWindow = NULL;

	m_serverList = XP_ListNew();
	if (m_serverList)
	{
		if (server)
		{
			if (DIR_TestFlag(server, DIR_REPLICATION_ENABLED))
			{
				/* Create a progress window.
				 */
				m_progressContext = PW_CreateProgressContext();
				if (m_progressContext)
				{
					m_progressWindow = PW_Create(NULL, pwStandard);
					if (m_progressWindow)
					{
						PW_AssociateWindowWithContext(m_progressContext, m_progressWindow);
						PW_SetWindowTitle(m_progressWindow, XP_GetString(MK_LDAP_REPL_PROGRESS_TITLE));
						PW_SetLine1(m_progressWindow, NULL);
						PW_SetLine2(m_progressWindow, NULL);
						PW_SetProgressRange(m_progressWindow, 0, 0);
						PW_SetCancelCallback(m_progressWindow, CancelReplication, m_progressContext);
						PW_Show(m_progressWindow);
					}
					else
					{
						PW_DestroyProgressContext(m_progressContext);
					}
				}

				XP_ListAddObjectToEnd(m_serverList, server);
			}

		}
		else
		{
			XP_ASSERT(m_pane);
			m_progressContext = m_pane->GetContext();

			XP_List *serverList = FE_GetDirServers();
			for (int i = 1; i <= XP_ListCount(serverList); i++)
			{
				server = (DIR_Server *)XP_ListGetObjectNum(serverList, i);
				if (DIR_TestFlag(server, DIR_REPLICATION_ENABLED))
					XP_ListAddObjectToEnd(m_serverList, server);
			}
		}

		if (XP_ListCount(m_serverList) == 0)
		{
			XP_ListDestroy(m_serverList);
			m_serverList = NULL;
		}
	}
}

net_OfflineLdapReplicator::~net_OfflineLdapReplicator()
{
	if (m_serverList)
		XP_ListDestroy(m_serverList);

	/* We destroy the progress window, but the pane destroys the context?
	 * This seems kind of weird, but it's the way things work.
	 */
	if (m_progressWindow)
	{
		PW_Hide(m_progressWindow);
		PW_Destroy(m_progressWindow);
	}
}


XP_Bool net_OfflineLdapReplicator::ReplicateDirectory()
{
	/* Return FALSE iff there are no more servers to replicate. 
	 */
	DIR_Server *server = (DIR_Server *)XP_ListRemoveTopObject(m_serverList);
	if (!server)
		return FALSE;

	char *auth = NULL;
	if (server->enableAuth && server->authDn)
	{
		if (server->password)
			auth = PR_smprintf("%s:%s@", server->authDn, server->password);
		else
			auth = PR_smprintf("%s@", server->authDn);
	}
			
    char *url = PR_smprintf("repl-ldap%s://%s%s:%d/%s", server->isSecure ? "s" : "",
							auth ? auth : "", server->serverName, server->port, server->searchBase);
	if (url)
	{
		URL_Struct *URL_s = NET_CreateURLStruct(url, NET_DONT_RELOAD);
		if(URL_s)
		{
			URL_s->owner_data = this;
			URL_s->internal_url = TRUE;
			URL_s->msg_pane = m_pane;
			if (m_pane)
				MSG_UrlQueue::AddUrlToPane(URL_s, net_OfflineLdapReplicator::ReplicateNextDirectory, m_pane, TRUE, FO_PRESENT);
			else
				NET_GetURL(URL_s, FO_PRESENT, m_progressContext, net_OfflineLdapReplicator::ReplicateNextDirectory);
		}
		XP_FREE(url);
	}
	XP_FREEIF(auth);

	return TRUE;
}


void net_OfflineLdapReplicator::ReplicateNextDirectory(URL_Struct *URL_s, int, MWContext *)
{
	net_OfflineLdapReplicator *replicator = (net_OfflineLdapReplicator *)(URL_s->owner_data);
	if (replicator)
	{
		/* ReplicateDirectory returns FALSE if there are no more directories
		 * left to replicate.  It is our job to clean up the replicator object
		 * when we're done.
		 */
		if (!replicator->ReplicateDirectory())
			delete replicator;
	}
}


//*****************************************************************************
//
// net_LdapToAddressBook does the work of converting LDAP entries into a format
// that can be stored in the local Address Book database.
//
class net_LdapToAddressBook : public net_LdapConnectionData
{
public:
	net_LdapToAddressBook (ActiveEntry*);
	virtual ~net_LdapToAddressBook();

	// Base class overrides
	virtual int OnEntryFound (LDAPMessage*);

	// Address Book specific stuff
	void ConvertEntryToPerson (LDAPMessage *, PersonEntry &);
	void ConvertEntryToPerson (LDAPMessage *message, AB_AttributeValue ** valuesArray, uint16 * numItems); // new version for new AB
	int WriteEntryToAddressBook (LDAPMessage*);
	virtual char * ExtractLDAPEncoding(char * url);

protected:
	virtual LdapConnectionType GetConnectionType() { return kAddressBookConnection; }

	// used to take an ldap attribute, value pair and conver it to one or more address book attribute
	// values, adding those values to the abAttributes list.
	int AddLDAPAttribute(const char * dirAttrib, const char * dirValue, XP_List * abAttributes);

	AB_AddressBookCopyInfo * m_abCopyInfo; // used for new address book...
};

// prototypes fo net_LdapToAddressBook helper functions...
AB_AttributeValue * findAttributeInList(AB_AttribID attrib, XP_List * abAttributes);

net_LdapToAddressBook::net_LdapToAddressBook (ActiveEntry *ce) : net_LdapConnectionData(ce)
{
	if (ce && ce->URL_s)
		m_abCopyInfo = (AB_AddressBookCopyInfo *) ce->URL_s->owner_data;
	else
		m_abCopyInfo = NULL;
}

net_LdapToAddressBook::~net_LdapToAddressBook()
{
	// copy info freed by url exit function...
//	if (m_abCopyInfo)
//		AB_FreeAddressBookCopyInfo(m_abCopyInfo); // also destroys allocated memory..
	m_abCopyInfo = NULL;
}

char * net_LdapToAddressBook::ExtractLDAPEncoding(char * url)
{
	// we want to strip off the addbook- part for the LDAP encoding...
	// caller should not be freeing this return string because it is part of the url..
	return url+8; // 8 for "addbook-"
}

// helper function used to find an attribute in the list
AB_AttributeValue * findAttributeInList(AB_AttribID attrib, XP_List * abAttributes)
{
	if (abAttributes)
	{
		for (int i = 1; i <= XP_ListCount(abAttributes); i++)
		{
			AB_AttributeValue *v = (AB_AttributeValue*) XP_ListGetObjectNum (abAttributes, i);
			if (v && v->attrib == attrib)
				return v;
		}
	}

	return NULL;
}

int net_LdapToAddressBook::AddLDAPAttribute(const char * attrib, const char * dirValue, XP_List * abAttributes)
{
	int status = 0;
	AB_AttributeValue * value = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue));
	if (value)
	{
		status = -1; 
		// we can make a minor hack to optmize this...since all of
		// them are string attributes, we only need to set the
		// value->attrib field in the if statements...then if
		// we found a match, set the value->u.string outside the
		// if statements...

		if (!XP_STRCASECMP(attrib, GetAttributeString(cn)))
		{
			value->attrib = AB_attribDisplayName;
			status = 0;
		}
		if (!XP_STRCASECMP(attrib, GetAttributeString (givenname)))
		{
			value->attrib = AB_attribGivenName;
			status = 0;
		}
		else if (!XP_STRCASECMP(attrib, GetAttributeString(sn)))
		{
			value->attrib = AB_attribFamilyName;
			status = 0;
		}
		else if (!XP_STRCASECMP(attrib, GetAttributeString(o)))
		{
			value->attrib = AB_attribCompanyName;
			status = 0;
		}
		else if (!XP_STRCASECMP(attrib, GetAttributeString(l)))
		{
			value->attrib = AB_attribLocality;
			status = 0;
		}
		else if (!XP_STRCASECMP(attrib, GetAttributeString(mail)))
		{
			value->attrib = AB_attribEmailAddress;
			status = 0;
		}
		else if (!XP_STRCASECMP(attrib, "title"))
		{
			value->attrib = AB_attribTitle;
			status = 0;
		}
		else if (!XP_STRCASECMP(attrib, "postaladdress"))
		{
			value->attrib = AB_attribPOAddress;
			status = 0;
		}
		else if (!XP_STRCASECMP(attrib, "st"))
		{
			value->attrib = AB_attribRegion;
			status = 0;
		}
		else if (!XP_STRCASECMP(attrib, "postalcode"))
		{
			value->attrib = AB_attribZipCode;
			status = 0;
		}
		else if (!XP_STRCASECMP(attrib, GetAttributeString(telephonenumber)))
		{
			value->attrib = AB_attribWorkPhone;
			status = 0;
		}
		else if (!XP_STRCASECMP(attrib, "facsimiletelephonenumber"))
		{
			value->attrib = AB_attribFaxPhone;
			status = 0;
		}
		else if (!XP_STRCASECMP(attrib, "dlshost"))
		{
			value->attrib = AB_attribCoolAddress;
			// we also need to set the use server...
			AB_AttributeValue * nextValue = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue));
			if (nextValue)
			{
				nextValue->attrib = AB_attribUseServer;
				nextValue->u.shortValue = kSpecificDLS;
				XP_ListAddObjectToEnd(abAttributes, (void *) nextValue);
			}
		}
		else if (!XP_STRCASECMP(attrib, "host"))
		{
			value->attrib = AB_attribCoolAddress;
			// we also need to set the use server...
			AB_AttributeValue * nextValue = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue));
			if (nextValue)
			{
				nextValue->attrib = AB_attribUseServer;
				nextValue->u.shortValue = kHostOrIPAddress;
				XP_ListAddObjectToEnd(abAttributes, (void *) nextValue);
			}
		}

		if (status == 0) // we found a match
		{
			value->u.string = XP_STRDUP(dirValue);
			XP_ListAddObjectToEnd(abAttributes, (void *) value);
		}

		else // we didn't find a match....
			XP_FREEIF(value);
	}
	else
		status = MK_OUT_OF_MEMORY;

	return status; 
}


void net_LdapToAddressBook::ConvertEntryToPerson (LDAPMessage *message, AB_AttributeValue ** valuesArray, uint16 * numItems)
{
	XP_List * abAttributes = XP_ListNew();
	AB_AttributeValue * values = NULL;
	uint16 numAttributes = 0; 

	if (abAttributes)
	{
		// loop over each attribute in the entry, and pick out the ones the address book
		// is interested in. We are ignoring the case where the same attribute has multiple
		// values. The AB doesn't have a way to deal with that. 

		char *tmpCn = NULL;
		BerElement *ber;
		char *attrib = ldap_first_attribute (m_ldap, message, &ber);
		while (attrib)
		{
			char ** values = ldap_get_values(m_ldap, message, attrib);
			if (m_server && values && values[0])
			{
				char *decodedUtf8 = ConvertFromServerCharSet (values[0]);
				if (decodedUtf8)
				{
					AddLDAPAttribute(attrib, decodedUtf8, abAttributes);
					if (!XP_STRCASECMP(attrib, GetAttributeString (cn)))
						tmpCn = XP_STRDUP(decodedUtf8); // hang on to cn in case we need it later
				}

				XP_FREEIF (decodedUtf8);
			}
			if (values)
				ldap_value_free (values);
			ldap_memfree (attrib);
			attrib = ldap_next_attribute (m_ldap, message, ber);
		} // add the next attribute....

		// now we have some fixing up to do...adding CSID, 
		AB_AttributeValue * abValue = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue));
		if (abValue)
		{
			abValue->attrib = AB_attribWinCSID;
			abValue->u.shortValue = INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(m_ce->window_id));
			XP_ListAddObjectToEnd(abAttributes, (void *) abValue);
		}

		// If the entry doesn't have an 'o', get it from the DN
		char *dn = NULL;
		AB_AttributeValue * company = findAttributeInList(AB_attribCompanyName, abAttributes);
		if (!company)
		{
			if (!dn)
				dn = ldap_get_dn (m_ldap, message);
			if (dn)
			{
				const char *oAttribName = GetAttributeString (o);
				AB_AttributeValue * abValue = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue));
				if (abValue)
				{
					abValue->attrib = AB_attribCompanyName;
					abValue->u.string = GetAttributeFromDN (dn, oAttribName);
					XP_ListAddObjectToEnd(abAttributes, (void *) abValue);
				}
			}
		}

		// If the entry doesn't have 'givenname' or 'sn' try to use 'cn'
		AB_AttributeValue * givenName = findAttributeInList(AB_attribGivenName, abAttributes);
		AB_AttributeValue * familyName = findAttributeInList(AB_attribFamilyName, abAttributes);
		if (!givenName /* || !familyName */)
		{
			char * useThisForGivenName = NULL;
			// prefer CN as attribute to CN from dist name
			if (tmpCn) 
			{
				useThisForGivenName = tmpCn;
				tmpCn = NULL; 
			}
			else
			{
				if (!dn)
					dn = ldap_get_dn (m_ldap, message);
				if (dn)
				{
					const char *cnAttribName = GetAttributeString (cn);
					useThisForGivenName = GetAttributeFromDN (dn, cnAttribName);
				}
			}

			if (useThisForGivenName)
			{
				AB_AttributeValue * abValue = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue));
				if (abValue)
				{
					abValue->attrib = AB_attribGivenName;
					abValue->u.string = useThisForGivenName;
					XP_ListAddObjectToEnd(abAttributes, (void *) abValue);
				}
			}
		}	// !givenName
		
		if (dn)
			ldap_memfree(dn); 
		if (tmpCn)
			XP_FREE(tmpCn);

		// our last step is to turn the XP_List into an array of attribute values...
		numAttributes = XP_ListCount(abAttributes);
		if (numAttributes)
		{
			values = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue) * numAttributes);
			if (valuesArray)
			{
				// iterate through the list adding each attribute to our static array
				for (uint16 index = 1; index <= numAttributes; index++)
				{
					AB_AttributeValue * value = (AB_AttributeValue *) XP_ListGetObjectNum(abAttributes, index);
					AB_CopyEntryAttributeValue(value, &values[index-1]);
				}
			}
		}
		
		// free the list attributes...
		for (int i = 1; i <= XP_ListCount(abAttributes); i++)
		{
			AB_AttributeValue * value = (AB_AttributeValue *) XP_ListGetObjectNum(abAttributes, i);
			AB_FreeEntryAttributeValue(value);

		}

		XP_ListDestroy (abAttributes);

	} // if abAttributes

	// set up the return values...
	if (numItems)
		*numItems = numAttributes;
	if (valuesArray)
		*valuesArray = values;
	else
		AB_FreeEntryAttributeValues(values, numAttributes);
}


#ifdef MOZ_NEWADDR
int net_LdapToAddressBook::WriteEntryToAddressBook (LDAPMessage *message)
{
	int status = 0; 
	if (m_abCopyInfo && m_abCopyInfo->destContainer)
	{
		AB_AttributeValue * valuesArray = NULL;
		uint16 numItems = 0;
		ConvertEntryToPerson(message, &valuesArray, &numItems);
		if (valuesArray)
		{
			// If there is only one URL in the queue, we bring up the UI, but
			// if there is more than one, we add them silently. There could be
			// a ton of URLs, and we don't have a Yes/No/All dialog
			status = AB_AddUserAB2(m_abCopyInfo->destContainer, valuesArray, numItems, NULL /* we don't care about the entry id...*/); 
			AB_FreeEntryAttributeValues(valuesArray, numItems);
		}
	}
	else
		status = ABError_NullPointer;

	return status; 
}
#else
int net_LdapToAddressBook::WriteEntryToAddressBook (LDAPMessage *message)
{
	// Add an entry we found on an LDAP directory into the local AB DB

	int err = 0;
	PersonEntry person;
	person.Initialize ();
	ConvertEntryToPerson (message, person); 

	XP_List *dirServers = FE_GetDirServers ();
	XP_ASSERT(dirServers);
	if (dirServers)
	{
		DIR_Server *pab = NULL;
		DIR_GetPersonalAddressBook (dirServers, &pab);
		XP_ASSERT(pab);
		if (pab)
		{
			ABook *ab = FE_GetAddressBook (NULL /* don't have a pane? */); 
			AB_BreakApartFirstName (ab, &person);
			
			// If there is only one URL in the queue, we bring up the UI, but
			// if there is more than one, we add them silently. There could be
			// a ton of URLs, and we don't have a Yes/No/All dialog

			if (MSG_GetUrlQueueSize (m_ce->URL_s->address, m_ce->window_id) <= 1)
				err = AB_AddUserWithUI (m_ce->window_id, &person, pab, TRUE);
			else
			{
				// Don't allow any external URLs to add batches of people to the AB
				if (m_ce->URL_s->internal_url)
				{
					ABID unusedId;
					err = AB_AddUser (pab, ab, &person, &unusedId);
				}
			}
		}
	}

	person.CleanUp();
	return err;
}
#endif

int net_LdapToAddressBook::OnEntryFound (LDAPMessage *message)
{
	return WriteEntryToAddressBook (message);
}

void net_LdapToAddressBook::ConvertEntryToPerson (LDAPMessage *message, PersonEntry &person)
{
	// Loop over each attribute in the entry, and pick out the ones the
	// Address Book is interested in. Note that we're ignoring cases where
	// the same attribute has multiple values. The AB doesn't have a way to 
	// deal with that.

	char *tmpCn = NULL;
	BerElement *ber;
	char *attrib = ldap_first_attribute (m_ldap, message, &ber);
	while (attrib) 
	{
		char **values = ldap_get_values (m_ldap, message, attrib);

		// We better have a DIR_Server for this, otherwise GetAttributeString 
		// will crash. Since the only way to get to this code is through the Search
		// Directory dialog, I think this is an ok restriction. We could certainly
		// add default names in dirprefs.c if necessary.
		XP_ASSERT(m_server && values && values[0]);
		if (!m_server || !values || !values[0])
			return;

		char *decodedUtf8 = ConvertFromServerCharSet (values[0]);
		if (!decodedUtf8)
			return;

		// ### maybe it would be nice to have a few more of these 
		// expressed as DIR_Prefs attributeIds???

		if (!XP_STRCASECMP(attrib, GetAttributeString (cn)))
			tmpCn = XP_STRDUP(decodedUtf8); // hang on to cn in case we need it later
		if (!XP_STRCASECMP(attrib, GetAttributeString (givenname)))
			person.pGivenName = XP_STRDUP(decodedUtf8);
		else if (!XP_STRCASECMP(attrib, GetAttributeString(sn)))
			person.pFamilyName = XP_STRDUP(decodedUtf8);
		else if (!XP_STRCASECMP(attrib, GetAttributeString(o)))
			person.pCompanyName = XP_STRDUP(decodedUtf8);
		else if (!XP_STRCASECMP(attrib, GetAttributeString(l)))
			person.pLocality = XP_STRDUP(decodedUtf8);
		else if (!XP_STRCASECMP(attrib, GetAttributeString(mail)))
			person.pEmailAddress = XP_STRDUP(decodedUtf8);
		else if (!XP_STRCASECMP(attrib, "title"))
			person.pTitle = XP_STRDUP(decodedUtf8);
		else if (!XP_STRCASECMP(attrib, "postaladdress"))
			person.pPOAddress = XP_STRDUP(decodedUtf8);
		else if (!XP_STRCASECMP(attrib, "st"))
			person.pRegion = XP_STRDUP(decodedUtf8);
		else if (!XP_STRCASECMP(attrib, "postalcode"))
			person.pZipCode = XP_STRDUP(decodedUtf8);
		else if (!XP_STRCASECMP(attrib, GetAttributeString(telephonenumber)))
			person.pWorkPhone = XP_STRDUP(decodedUtf8);
		else if (!XP_STRCASECMP(attrib, "facsimiletelephonenumber"))
			person.pFaxPhone = XP_STRDUP(decodedUtf8);
		else if (!XP_STRCASECMP(attrib, "dlshost"))
		{
			person.UseServer = kSpecificDLS;
			person.pCoolAddress = XP_STRDUP(decodedUtf8);
		}
		else if (!XP_STRCASECMP(attrib, "host"))
		{
			person.UseServer = kHostOrIPAddress;
			person.pCoolAddress = XP_STRDUP(decodedUtf8);
		}
		

		ldap_value_free (values);
		XP_FREE (decodedUtf8);
		ldap_memfree (attrib);

		attrib = ldap_next_attribute (m_ldap, message, ber);
	}

	// If the entry doesn't have an 'o', get it from the DN
	char *dn = NULL;
	if (!person.pCompanyName)
	{
		if (!dn)
			dn = ldap_get_dn (m_ldap, message);
		if (dn)
		{
			const char *oAttribName = GetAttributeString (o);
			person.pCompanyName = GetAttributeFromDN (dn, oAttribName);
		}
	}

	// If the entry doesn't have 'givenname' or 'sn' try to use 'cn'
	if (!person.pGivenName || !person.pFamilyName)
	{
		// prefer CN as attribute to CN from dist name
		if (tmpCn) 
		{
			person.pGivenName = tmpCn;
			tmpCn = NULL; // PersonEntry now owns that memory, so don't free it
		}
		else
		{
			if (!dn)
				dn = ldap_get_dn (m_ldap, message);
			if (dn)
			{
				const char *cnAttribName = GetAttributeString (cn);
				person.pGivenName = GetAttributeFromDN (dn, cnAttribName);
			}
		}
	}

	if (dn)
		ldap_memfree(dn); 
	if (tmpCn)
		XP_FREE(tmpCn);

	person.WinCSID = INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(m_ce->window_id));
}

#ifdef MOZ_LI

/*
 * Class net_LdapLIConnectionData
 * ==============================
 *  This class is used for LI LDAP operations.  It expects a LDAPOperation* to be
 *  passed in via the URL_s->owner_data.  The operation to be performed is determined
 *  by URL_s->method.  The results of the operations are returned in the LDAPOperation;
 *  the caller is responsible for creating and deleting the operation object.
 */
class net_LdapLIConnectionData : public net_LdapConnectionData
{
public:
	net_LdapLIConnectionData(ActiveEntry *ce);
	virtual ~net_LdapLIConnectionData (); // The base class handles it correctly.

	virtual int Load (ActiveEntry *ce);
	virtual int Process ();
	virtual int Interrupt ();

	virtual int Unbind ();


protected:
	virtual LdapConnectionType GetConnectionType() { return kLIConnection; }

private:
	XP_Bool m_bCallerProvidedConnection; // Did the caller provide the m_ldap connection?
 	LDAPOperation* m_pParams;		 // Parameters.
};

/* net_LdapLIConnectionData Constructor
 * ====================================
 * Create a connection data object class for an LI operation; set up a connection if
 * the caller didn't provide one.  We tell the base class not to initialize m_ldap since
 * a LDAP* may be passed in to us.
 */
net_LdapLIConnectionData::net_LdapLIConnectionData(ActiveEntry *ce) : net_LdapConnectionData(ce,FALSE)
{
	m_pParams = (LDAPOperation*)ce->URL_s->owner_data;

	if(m_pParams && (m_pParams->getConnection() || (m_pParams->getURLMethod() == LDAP_LI_BIND_METHOD)))
	{
		// Set the connection in the base class.
		// Free the LDAP* struct if created by the base class.
		m_ldap = m_pParams->getConnection();
		m_bCallerProvidedConnection = TRUE;
	}
	else
	{
		m_ldap = ldap_init(m_pParams->getURL()->getHost(),m_pParams->getURL()->getPort());
		m_bCallerProvidedConnection = FALSE;
	}

}

/* net_LdapLIConnectionData Destructor
 * ====================================
 * Remember not to unbind if the caller provided the connection.
 */
net_LdapLIConnectionData::~net_LdapLIConnectionData()
{
	// Base class destructor will try to unbind, so we have to prevent that if
	// the caller provided the connection.
	if(m_bCallerProvidedConnection)
		m_ldap = NULL;	
}

/* Load()
 * ======
 * Called by netlib when starting an LI operation.  This function is responsible to starting
 * the LDAP operations.
 */
int net_LdapLIConnectionData::Load(ActiveEntry *ce)
{
	int ldapErr = 0;
	int msgId = -1;
	char *attrs[2] = { "modifyTimeStamp" , NULL };
	const LDAPURL* url = m_pParams->getURL();
	char *utf8Encoding = NULL;

	if(url->getParseError() != LDAP_SUCCESS)
	{
		ldapErr = MK_MALFORMED_URL_ERROR;
		goto net_LdapLIConnectionDataLoadExit;
	}

	if(m_pParams->started())
	{
		ldapErr = LI_LDAP_ALREADY_STARTED;
		goto net_LdapLIConnectionDataLoadExit;
	}

	XP_ASSERT(m_ce);
	XP_ASSERT(m_ce->URL_s);
	XP_ASSERT(m_ce->URL_s->address);
	XP_ASSERT(m_ldap);
	
	/* Convert the URL to UTF8 */
	utf8Encoding = ConvertToServerCharSet (ce->URL_s->address); 

	if(!utf8Encoding)
		return LI_LDAP_MEMORY_ERROR;

	/* The URL is assumed to be valid (checked by
	 * LDAPOperation before calling NET_GetURL)
	 */

	/* This is where the real work begins */

	// let the world know what we are doing.  
	FE_GraphProgressInit(ce->window_id, NULL, 1);

	// Initiate the operation on the server
	switch(m_pParams->getURLMethod()) 
	{
		case LDAP_LI_BIND_METHOD:
			// Bind operation is not really async yet, the initial IP stuff is still synchronous in the LDAP API.
			msgId = ldap_simple_bind(m_ldap,m_pParams->getURL()->getDN(),((LDAPBindOperation*)m_pParams)->getPassword());
			break;
		case LDAP_LI_SEARCH_METHOD:
			msgId = ldap_search(m_ldap,
								url->getDN(),
								url->getScope(),
								url->getFilter(),
								url->getAttrs(),  
								((LDAPSearchOperation*)m_pParams)->getAttrsOnly());
			break;
		case LDAP_LI_ADD_METHOD:
		case LDAP_LI_ADDGLM_METHOD:
				// Note that since we take apart the URL, we end up ignoring everything but the DN.
	 			msgId = ldap_add(m_ldap, url->getDN(), ((LDAPAddOperation*)m_pParams)->getLDAPModArray()->getLDAPModStarStar());
			break;
		case LDAP_LI_MOD_METHOD:
		case LDAP_LI_MODGLM_METHOD:
				// Note that since we take apart the URL, we end up ignoring everything but the DN.
	 			msgId = ldap_modify(m_ldap, url->getDN(), ((LDAPModOperation*)m_pParams)->getLDAPModArray()->getLDAPModStarStar());
			break;
		case LDAP_LI_DEL_METHOD:
				msgId = ldap_delete(m_ldap, url->getDN());
			break;
		case LDAP_LI_GETLASTMOD_METHOD:
			{
			msgId = ldap_search(m_ldap,
								url->getDN(),
								url->getScope(),
								url->getFilter(),
								attrs,
								((LDAPSearchOperation*)m_pParams)->getAttrsOnly());
			}
			break;
		case LDAP_SUCCESS:
			break;
	}

	if (msgId != -1)
	{
		m_messageId = msgId;
	} else {
		m_pParams->setErrNo(ldap_get_lderrno(m_ldap,NULL,NULL));
		ldapErr = LI_LDAP_ERROR;
	}

net_LdapLIConnectionDataLoadExit:

	XP_FREEIF(utf8Encoding);

	return ldapErr;
}

/* Process()
 * =========
 * Process LI Gets/Puts, etc.  Only one entry can be gotten/put at a time.
 * The result of the process is a LDAPMessage* put into the LDAPOperation*
 * in the URL_s->owner_data in the ActiveEntry.
 * The caller of NET_GetURL is required to free this data via ldap_msgfree()
 * if it is non-NULL in the LDAPOperation object.
 */
int net_LdapLIConnectionData::Process()
{
	LDAPMessage *message = NULL;
	struct timeval timeout;

	// REMIND: put in the timeout stuff.

	XP_BZERO (&timeout, sizeof(timeout));
	int ldapErr = ldap_result (m_ldap, m_messageId, 1, &timeout, &message);

	XP_ASSERT(m_pParams);

	/* Check for network error */
	// We don't set result to NULL here because it may have already been set to
	// something, e.g. by the LDAP_LI_ADDGLM or LDAP_LI_MODGLM methods.
	if(ldapErr == -1)
	{
		m_pParams->setErrNo(ldap_get_lderrno(m_ldap,NULL,NULL));
		return LI_LDAP_ERROR;
	}

	switch (ldapErr)
	{
		/*
		 * We can only get one entry at a time in the current implementation, so we
		 * terminate when we have at least one.
		 */
		case LDAP_RES_SEARCH_ENTRY:
		case LDAP_RES_SEARCH_RESULT:
		case LDAP_RES_MODIFY:
		case LDAP_RES_DELETE:
		case LDAP_RES_ADD:
			FE_GraphProgress(m_ce->window_id, NULL, 0, 1, -2); // -2 specifies that we want items not bytes
		case LDAP_RES_BIND:
			m_pParams->setResults(message);
			ldapErr = LI_LDAP_DONE;  /* Special value to indicate to netlib that we're done. */
						   /* We don't use -1 here so we can differentiate between */
						   /* a valid result and an ldap error					   */
			m_pParams->setErrNo(ldap_result2error(m_ldap,message,0));
			break;
	 	case LDAP_SUCCESS:
			break;
		default:
	 		if(message)
				m_pParams->setResults(message);
			m_pParams->setErrNo(ldap_result2error(m_ldap,message,0));
			ldapErr = LI_LDAP_ERROR;
	 		break;
	}

	/* If the ldapErr is -2, it means we're done and we've gotten something */
	if(ldapErr == LI_LDAP_DONE) {

		char *utf8Encoding = NULL;

		// the last modified time is given by the modifyTimeStamp attribute.
		char *attrs[2] = { "modifyTimeStamp" , NULL };
		int msgID;
		const LDAPURL* url = m_pParams->getURL();

		// For LDAP_LI_MODGLM_METHOD and LDAP_LI_ADDGLM_METHOD, we have to
		//  do another search to get the last modified time if the first
		//  search completes successfully.  To this, we simply start the search
		//  and don't return -1 so netlib will keep calling process().  We also
		//  change m_method to LDAP_LI_SEARCH_METHOD so that we won't end up in this
		//  if statement again.  Note that we don't change it in m_ce->URL_s->method
		//  This is mainly to avoid netlib weirdness.
		switch(m_pParams->getURLMethod()) {
			case LDAP_LI_ADDGLM_METHOD:
			case LDAP_LI_MODGLM_METHOD:

				// Determine if the ADD part succeeded. We don't free the result because
				//  it's the policy of the URL handler to always return the result when
				//  it's available.  We'll have to free if it the ADD/MOD succeed so that
				//  the GLM result can be returned.
				if(m_pParams-> getErrNo() != LDAP_SUCCESS) {
						// m_pParams->result and param->status already set above.
						((LDAPModGLMOperation*)m_pParams)->setModSucceeded(FALSE);
					break; // exit the ADDGLM and MODGLM stuff.
				}

				// setResults will free message, since setResults(message) was called earlier.
				m_pParams->setResults(NULL);
				message = NULL;
	 			((LDAPModGLMOperation*)m_pParams)->setModSucceeded(TRUE);

				msgID = ldap_search(m_ldap,url->getDN(),url->getScope(),"(objectclass=*)",attrs,0);

				// each time we initiate a new ldap operation we need to increase the count
				FE_GraphProgressInit(m_ce->window_id, NULL, 1);

				// If msgID == -1, then there was a network error; so we set the error condition and
				// leave ldapErr = -1 so netlib will exit the url handler.
				if(msgID == -1) {
						m_pParams->setErrNo(ldap_get_lderrno(m_ldap,NULL,NULL));
	 					ldapErr = LI_LDAP_ERROR;
				} else {
					ldapErr = 1;  // >0 indicates to netlib to continue processing
					m_messageId = msgID;
					((LDAPModGLMOperation*)m_pParams)->changeModToGLM();
				}
				break;
		} // switch
		FE_GraphProgressDestroy(m_ce->window_id, NULL, 1, 1);
	} // if

	return ldapErr;
}

/* Unbind()
 * ========
 * Unbind from the server if the caller didn't provide a connection.
 */
int net_LdapLIConnectionData::Unbind()
{
	// If the caller did not provide the connection, we do the standard unbind.
	// If the caller did provide the connection, we have a sticky situation
	// because net_LdapConnectionData::Unbind() is called in the base class'
	// destructor, and that destructor is guaranteed to be called.  To avoid
	// unbinding a caller provided connection, we just set m_ldap to NULL so that
	// the base class' Unbind() won't do anything.
	if(!m_bCallerProvidedConnection)
		return net_LdapConnectionData::Unbind();
	else
		m_ldap = NULL;

	return 0; // Don't know what else we can return.
}

/* Interrupt()
 * ===========
 * netlib wants to interrupt this connect; we rely on the base class to do most of
 * the work and just set the appropriate error.
 */
int net_LdapLIConnectionData::Interrupt()
{
	return net_LdapConnectionData::Interrupt();
}

#endif // MOZ_LI
#endif // MOZ_LDAP


//****************************************************************************
//
// Callbacks from NET_GetURL 
//

extern "C" int32 net_LoadLdap (ActiveEntry *ce)
{
#ifdef MOZ_LDAP
	if (NET_IsOffline())
		return MK_OFFLINE;

	net_LdapConnectionData *cd = NULL;
	switch (NET_URL_Type (ce->URL_s->address))
	{
	case LDAP_TYPE_URL:
	case SECURE_LDAP_TYPE_URL:
#ifdef MOZ_LI
		// If we're doing an LDAP operation, we
		// have to figure out if it's an LI operation
		// and instantiate the right connection data object.
		if((LDAP_SEARCH_METHOD < ce->URL_s->method) && (LDAP_LI_BIND_METHOD >= ce->URL_s->method))
			cd = new net_LdapLIConnectionData(ce);
		else
#endif
			cd = new net_LdapToHtml (ce);
		break;
	case LDAP_QUERY_DSE_TYPE_URL: // I think we need to query the DSE for any operation, so this is in the base class
		cd = new net_LdapToHtml (ce);
		break;
	case ADDRESS_BOOK_LDAP_TYPE_URL:
		cd = new net_LdapToAddressBook (ce);
		break;
	case LDAP_REPLICATION_TYPE_URL:
		cd = new net_LdapReplicator (ce);
		break;
	default:
		XP_ASSERT(FALSE);
		return MK_MALFORMED_URL_ERROR;
	}
	if (!cd)
		return MK_OUT_OF_MEMORY;

	ce->con_data = cd;
	int err = cd->Load (ce);
	if (err)
	{
		ce->status = err;
		delete cd;
		ce->con_data = NULL;
	}

	return err;
#else
	return MK_MALFORMED_URL_ERROR;
#endif
}

extern "C" int32 net_ProcessLdap (ActiveEntry *ce)
{
#ifdef MOZ_LDAP
	net_LdapConnectionData *cd = (net_LdapConnectionData*) ce->con_data;

	int err = cd->Process();
	if (err < 0 || err == MK_CONNECTED)
	{
		delete cd;
		ce->con_data = NULL;
	}

	if(err == MK_CONNECTED)
		err = -1;

	return err;
#else
	return MK_MALFORMED_URL_ERROR;
#endif
}


extern "C" int32 net_InterruptLdap (ActiveEntry * ce)
{
#ifdef MOZ_LDAP
	net_LdapConnectionData *cd = (net_LdapConnectionData*) ce->con_data;

	int err = cd->Interrupt ();
	delete cd;
	ce->con_data = NULL;
	ce->status = MK_INTERRUPTED;

	return err;
#else
	return -1;
#endif
}

extern "C" void
net_CleanupLdap(void)
{
}

extern "C" void
NET_InitLDAPProtocol(void)
{
    static NET_ProtoImpl ldap_proto_impl;

    ldap_proto_impl.init = net_LoadLdap;
    ldap_proto_impl.process = net_ProcessLdap;
    ldap_proto_impl.interrupt = net_InterruptLdap;
    ldap_proto_impl.resume = NULL;
    ldap_proto_impl.cleanup = net_CleanupLdap;

    NET_RegisterProtocolImplementation(&ldap_proto_impl, LDAP_TYPE_URL);
    NET_RegisterProtocolImplementation(&ldap_proto_impl, SECURE_LDAP_TYPE_URL);
    NET_RegisterProtocolImplementation(&ldap_proto_impl, ADDRESS_BOOK_LDAP_TYPE_URL);
}


//****************************************************************************
//
// Helper APIs
//

extern "C" XP_Bool NET_ReplicateDirectory (MSG_Pane *pane, DIR_Server *server)
{
#ifdef MOZ_LDAP
	net_OfflineLdapReplicator *replicator = new net_OfflineLdapReplicator(pane, server);

	if (replicator->ReplicateDirectory())
		return TRUE;

	delete replicator;
#endif
	return FALSE;
}
