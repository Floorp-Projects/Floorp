/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

//
// TODOs for this file-
//  - Check error states returned from all occurences of ExtractPortFrom
//  - Grep on other TODOs...
//

#include "nsURL.h"
#include <stdlib.h>
#include "plstr.h"
#include "nsIProtocolHandler.h"
#include "nsIComponentManager.h"

static const PRInt32 DEFAULT_PORT = -1;

#define SET_POSITION(__part,__start,__length)   \
{                                               \
    m_Position[__part][0] = __start;            \
    m_Position[__part][1] = __length;           \
}


NS_NET
nsICoolURL* CreateURL(const char* i_URL)
{
    nsICoolURL* pReturn = new nsURL(i_URL);
    if (pReturn)
        pReturn->AddRef();
    return pReturn;
}

nsURL::nsURL(const char* i_URL):m_Port(DEFAULT_PORT),mRefCnt(0)
{
    NS_PRECONDITION( (0 != i_URL), "No url string specified!");

    if (i_URL)
    {
        m_URL = new char[PL_strlen(i_URL) + 1];
        if (m_URL)
        {
            PL_strcpy(m_URL, i_URL);
        }
    }
    else
    {
        m_URL = new char[1];
        *m_URL = '\0';
    }

    for (int i=0; i<TOTAL_PARTS; ++i) 
    {
        for (int j=0; j<2; ++j)
        {
            m_Position[i][j] = -1;
        }
    }

    Parse();
}

nsURL::~nsURL()
{
    if (m_URL)
    {
        delete[] m_URL;
        m_URL = 0;
    }
}


NS_IMPL_ADDREF(nsURL);

PRBool
nsURL::Equals(const nsIURI* i_URI) const
{
    NS_NOTYETIMPLEMENTED("Eeeks!");
    return PR_FALSE;
}

nsresult
nsURL::Extract(const char* *o_OutputString, nsURL::Part i_id) const
{
	int i = (int) i_id;
	if (o_OutputString)
	{
		*o_OutputString = new char(m_Position[i][1]+1);
        if (!*o_OutputString)
            return NS_ERROR_OUT_OF_MEMORY;

        char* dest = (char*) *o_OutputString;
        char* src = m_URL + m_Position[i][0];

		for (int j=0; j<m_Position[i][1]; ++j)
		{
            *dest++ = *src++; 
		}
        *dest = '\0';
        return NS_OK;
	}
    return NS_ERROR_URL_PARSING; 
}

nsresult
nsURL::ExtractPortFrom(int start, int length)
{
    char* port = new char[length +1];
    if (!port)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    PL_strncpy(port, m_URL+start, length);
    *(port + length) = '\0';
    m_Port = atoi(port);
    delete[] port;
    return NS_OK;
}

nsresult 
nsURL::GetStream(nsIInputStream* *o_InputStream)
{
    NS_PRECONDITION( (0 != m_URL), "GetStream called on empty url!");
	nsresult result = NS_OK; // change to failure
	//OpenProtocolInstance and request the input stream
	nsIProtocolInstance* pi = 0;
	result = OpenProtocolInstance(&pi);
   
	if (NS_OK != result)
		return result;
 	return pi->GetInputStream(o_InputStream);		
}

nsresult nsURL::OpenProtocolInstance(nsIProtocolInstance* *o_ProtocolInstance)
{
    const char* scheme =0;
    GetScheme(&scheme);
    // If the expected behaviour is to have http as the default scheme, a 
    // caller must check that before calling this function. We will not 
    // make any assumptions here. 
    if (0 == scheme)
        return NS_ERROR_BAD_URL; 

    nsIProtocolHandler *pHandler=0;
    nsresult rv = nsComponentManager::CreateInstance(
        NS_COMPONENT_NETSCAPE_NETWORK_PROTOCOLS "http", // TODO replace!
        nsnull,
        nsIProtocolHandler::GetIID(),
        (void**)&pHandler);

    return pHandler ? pHandler->GetProtocolInstance(this, o_ProtocolInstance) : rv;

#if 0 // For now hardcode it...
    if (0==PL_strcasecmp(scheme, "http"))
    {
        nsIHTTPHandler* pHandler= 0;
		// TODO This has to change to Factory interface
        //if (NS_OK == CreateOrGetHTTPHandler(&pHandler))
        {
            if (pHandler)
            {
                return pHandler->GetProtocolInstance(this, o_ProtocolInstance);
            }
        }
    }
    return NS_ERROR_NOT_IMPLEMENTED;
#endif

}

// This code will need thorough testing. A lot of this has to do with 
// usability and expected behaviour when a user types something in the
// location bar.
nsresult nsURL::Parse(void)
{
    NS_PRECONDITION( (0 != m_URL), "Parse called on empty url!");
    if (!m_URL)
    {
        return NS_ERROR_URL_PARSING;
    }

    //Remember to remove leading/trailing spaces, etc. TODO
    int len = PL_strlen(m_URL);
    static const char delimiters[] = "/:@"; //this order is optimized.
    char* brk = PL_strpbrk(m_URL, delimiters);
    char* lastbrk = brk;
    if (brk) 
    {
        switch (*brk)
        {
        case '/' :
            // If the URL starts with a slash then everything is a path
            if (brk == m_URL)
            {
                SET_POSITION(PATH, 0, len);
                return NS_OK;
            }
            else // The first part is host, so its host/path
            {
                SET_POSITION(HOST, 0, (brk - m_URL));
                SET_POSITION(PATH, (brk - m_URL), (len - (brk - m_URL)));
                return NS_OK;
            }
            break;
        case ':' :
            // If the first colon is followed by // then its definitely a spec
            if ((*(brk+1) == '/') && (*(brk+2) == '/')) // e.g. http://
            {
                SET_POSITION(SCHEME, 0, (brk - m_URL));    
                lastbrk = brk+3;
                brk = PL_strpbrk(lastbrk, delimiters);
                if (brk)
                {
                    switch (*brk)
                    {
                        case '/' : // standard case- http://host/path
                            SET_POSITION(HOST, (lastbrk - m_URL), (brk - lastbrk));
                            SET_POSITION(PATH, (brk - m_URL), (len - (brk - m_URL)));
                            return NS_OK;
                            break;
                        case ':' : 
                            {
                                // It could be http://user:pass@host/path or http://host:port/path
                                // For the first case, there has to be an at after this colon, so...
                                char* atSign = PL_strchr(brk, '@');
                                if (atSign)
                                {
                                    SET_POSITION(PREHOST, (lastbrk - m_URL), (atSign - lastbrk));
                                    brk = PL_strpbrk(atSign+1, "/:");
                                    if (brk) // http://user:pass@host:port/path or http://user:pass@host/path
                                    {
                                        SET_POSITION(HOST, (atSign+1 - m_URL), (brk - (atSign+1)));
                                        if (*brk == '/')
                                        {
                                            SET_POSITION(PATH, (brk - m_URL), len - (brk - m_URL));
                                            return NS_OK;
                                        }
                                        else // we have a port since (brk == ':')
                                        {
                                            lastbrk = brk+1;
                                            brk = PL_strchr(lastbrk, '/');
                                            if (brk) // http://user:pass@host:port/path
                                            {
                                                ExtractPortFrom((lastbrk - m_URL), (brk-lastbrk));
                                                SET_POSITION(PATH, (brk-m_URL), len - (brk-m_URL));
                                                return NS_OK;
                                            }
                                            else // http://user:pass@host:port
                                            {
                                                ExtractPortFrom((lastbrk - m_URL), len - (lastbrk - m_URL));
                                                return NS_OK;
                                            }
                                        }

                                    }
                                    else // its just http://user:pass@host
                                    {
                                        SET_POSITION(HOST, (atSign+1 - m_URL), len - (atSign+1 - m_URL));
                                        return NS_OK;
                                    }
                                }
                                else // definitely the port option, i.e. http://host:port/path
                                {
                                    SET_POSITION(HOST, (lastbrk-m_URL), (brk-lastbrk));
                                    lastbrk = brk+1;
                                    brk = PL_strchr(lastbrk, '/');
                                    if (brk)    // http://host:port/path
                                    {
                                        ExtractPortFrom((lastbrk-m_URL),(brk-lastbrk));
                                        SET_POSITION(PATH, (brk-m_URL), len - (brk-m_URL));
                                        return NS_OK;
                                    }
                                    else        // http://host:port
                                    {
                                        ExtractPortFrom((lastbrk-m_URL),len - (lastbrk-m_URL));
                                        return NS_OK;
                                    }
                                }
                            }
                            break;
                        case '@' : 
                            // http://user@host...
                            {
                                SET_POSITION(PREHOST, (lastbrk-m_URL), (brk-lastbrk));
                                lastbrk = brk+1;
                                brk = PL_strpbrk(lastbrk, ":/");
                                if (brk)
                                {
                                    SET_POSITION(HOST, (lastbrk-m_URL), (brk - lastbrk));
                                    if (*brk == ':') // http://user@host:port...
                                    {
                                        lastbrk = brk+1;
                                        brk = PL_strchr(lastbrk, '/');
                                        if (brk)    // http://user@host:port/path
                                        {
                                            ExtractPortFrom((lastbrk-m_URL),(brk-lastbrk));
                                            SET_POSITION(PATH, (brk-m_URL), len - (brk-m_URL));
                                            return NS_OK;
                                        }
                                        else        // http://user@host:port
                                        {
                                            ExtractPortFrom((lastbrk-m_URL),len - (lastbrk-m_URL));
                                            return NS_OK;
                                        }

                                    }
                                    else // (*brk == '/') so no port just path i.e. http://user@host/path
                                    {
                                        SET_POSITION(PATH, (brk - m_URL), len - (brk - m_URL));
                                        return NS_OK;
                                    }
                                }
                                else // its just http://user@host
                                {
                                    SET_POSITION(HOST, (lastbrk+1 - m_URL), len - (lastbrk+1 - m_URL));
                                    return NS_OK;
                                }

                            }
                            break;
                        default: NS_POSTCONDITION(0, "This just can't be!");
                            break;
                    }

                }
                else // everything else is a host, as in http://host
                {
                    SET_POSITION(HOST, (lastbrk - m_URL), len - (lastbrk - m_URL));
                    return NS_OK;
                }

            }
            else // host:port...
            {
                SET_POSITION(HOST, 0, (brk - m_URL));
                lastbrk = brk+1;
                brk = PL_strpbrk(lastbrk, delimiters);
                if (brk)
                {
                    switch (*brk)
                    {
                        case '/' : // The path, so its host:port/path
                            ExtractPortFrom(lastbrk-m_URL, brk-lastbrk);
                            SET_POSITION(PATH, brk- m_URL, len - (brk-m_URL));
                            return NS_OK;
                            break;
                        case ':' : 
                            return NS_ERROR_URL_PARSING; 
                            break;
                        case '@' :
                            // This is a special case of user:pass@host... so 
                            // Cleanout our earliar knowledge of host
                            SET_POSITION(HOST, -1, -1);

                            SET_POSITION(PREHOST, 0, (brk-m_URL));
                            lastbrk = brk+1;
                            brk = PL_strpbrk(lastbrk, ":/");
                            if (brk)
                            {
                                SET_POSITION(HOST, (lastbrk-m_URL), (brk-lastbrk));
                                if (*brk == ':') // user:pass@host:port...
                                {
                                    lastbrk = brk+1;
                                    brk = PL_strchr(lastbrk, '/');
                                    if (brk)    // user:pass@host:port/path
                                    {
                                        ExtractPortFrom((lastbrk-m_URL),(brk-lastbrk));
                                        SET_POSITION(PATH, (brk-m_URL), len - (brk-m_URL));
                                        return NS_OK;
                                    }
                                    else        // user:pass@host:port
                                    {
                                        ExtractPortFrom((lastbrk-m_URL),len - (lastbrk-m_URL));
                                        return NS_OK;
                                    }
                                }
                                else // (*brk == '/') so user:pass@host/path
                                {
                                    SET_POSITION(PATH, (brk - m_URL), len - (brk - m_URL));
                                    return NS_OK;
                                }
                            }
                            else // its user:pass@host so everthing else is just the host
                            {
                                SET_POSITION(HOST, (lastbrk-m_URL), len - (lastbrk-m_URL));
                                return NS_OK;
                            }

                            break;
                        default: NS_POSTCONDITION(0, "This just can't be!");
                            break;
                    }
                }
                else // Everything else is just the port
                {
                    ExtractPortFrom(lastbrk-m_URL, len - (lastbrk-m_URL));
                    return NS_OK;
                }
            }
            break;
        case '@' :
            //Everything before the @ is the prehost stuff
            SET_POSITION(PREHOST, 0, brk-m_URL);
            lastbrk = brk+1;
            brk = PL_strpbrk(lastbrk, ":/");
            if (brk)
            {
                SET_POSITION(HOST, (lastbrk-m_URL), (brk-lastbrk));
                if (*brk == ':') // user@host:port...
                {
                    lastbrk = brk+1;
                    brk = PL_strchr(lastbrk, '/');
                    if (brk)    // user@host:port/path
                    {
                        ExtractPortFrom((lastbrk-m_URL),(brk-lastbrk));
                        SET_POSITION(PATH, (brk-m_URL), len - (brk-m_URL));
                        return NS_OK;
                    }
                    else        // user@host:port
                    {
                        ExtractPortFrom((lastbrk-m_URL),len - (lastbrk-m_URL));
                        return NS_OK;
                    }
                }
                else // (*brk == '/') so user@host/path
                {
                    SET_POSITION(PATH, (brk - m_URL), len - (brk - m_URL));
                    return NS_OK;
                }
            }
            else // its user@host so everything else is just the host
            {
                SET_POSITION(HOST, (lastbrk-m_URL), (len - (lastbrk-m_URL)));
                return NS_OK;
            }
            break;
        default:
            NS_POSTCONDITION(0, "This just can't be!");
            break;
        }
    }
    else // everything is a host
    {
        SET_POSITION(HOST, 0, len);
    }
    return NS_OK;
}


nsresult
nsURL::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr)
        return NS_ERROR_NULL_POINTER;

    *aInstancePtr = NULL;
    
    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

    if (aIID.Equals(nsICoolURL::GetIID())) {
        *aInstancePtr = (void*) ((nsICoolURL*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIURI::GetIID())) {
        *aInstancePtr = (void*) ((nsIURI*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}
 
NS_IMPL_RELEASE(nsURL);

nsresult
nsURL::SetHost(const char* i_Host)
{
    NS_NOTYETIMPLEMENTED("Eeeks!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsURL::SetPath(const char* i_Path)
{
    NS_NOTYETIMPLEMENTED("Eeeks!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsURL::SetPort(PRInt32 i_Port)
{
    if (m_Port != i_Port)
    {
        m_Port = i_Port;
        if (DEFAULT_PORT == m_Port)
        {
            //Just REMOVE the earliar one. 
            NS_NOTYETIMPLEMENTED("Eeeks!");
        }
        else
        {
            //Insert the string equivalent 
            NS_NOTYETIMPLEMENTED("Eeeks!");
        }
        Parse();
    }
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsURL::SetPreHost(const char* i_PreHost)
{
    NS_NOTYETIMPLEMENTED("Eeeks!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsURL::SetScheme(const char* i_Scheme)
{
    NS_NOTYETIMPLEMENTED("Eeeks!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

#ifdef DEBUG
nsresult 
nsURL::DebugString(const char* *o_String) const
{
    if (!m_URL)
        return NS_OK;
    const char* tempBuff = 0;
    char* tmp = new char[PL_strlen(m_URL) + 64];
    char* tmp2 = tmp;
    for (int i=PL_strlen(m_URL)+64; i>0 ; --i)
        *tmp2++ = '\0';
/*///
    PL_strcpy(tmp, "m_URL= ");
    PL_strcat(tmp, m_URL);
    
    GetScheme(&tempBuff);
    PL_strcat(tmp, "\nScheme= ");
    PL_strcat(tmp, tempBuff);
    
    GetPreHost(&tempBuff);
    PL_strcat(tmp, "\nPreHost= ");
    PL_strcat(tmp, tempBuff);
    
    GetHost(&tempBuff);
    PL_strcat(tmp, "\nHost= ");
    PL_strcat(tmp, tempBuff);
    
    char* tempPort = new char[6];
    if (tempPort)
    {
        itoa(m_Port, tempPort, 10);
        PL_strcat(tmp, "\nPort= ");
        PL_strcat(tmp, tempPort);
        delete[] tempPort;
    }
    else
        PL_strcat(tmp, "\nPort couldn't be conv'd to string!");

    GetPath(&tempBuff);
    PL_strcat(tmp, "\nPath= ");
    PL_strcat(tmp, tempBuff);
    PL_strcat(tmp, "\n");
/*///
    char delimiter[] = ",";
#define TEMP_AND_DELIMITER PL_strcat(tmp, tempBuff); \
    PL_strcat(tmp, delimiter)

    PL_strcpy(tmp, m_URL);
    PL_strcat(tmp, "\n");

    GetScheme(&tempBuff);
    TEMP_AND_DELIMITER;

    GetPreHost(&tempBuff);
    TEMP_AND_DELIMITER;

    GetHost(&tempBuff);
    TEMP_AND_DELIMITER;

    char* tempPort = new char[6];
    if (tempPort)
    {
        itoa(m_Port, tempPort, 10);
        PL_strcat(tmp, tempPort);
        PL_strcat(tmp, delimiter);
        delete[] tempPort;
    }
    else
        PL_strcat(tmp, "\nPort couldn't be conv'd to string!");

    GetPath(&tempBuff);
    PL_strcat(tmp, tempBuff);
    PL_strcat(tmp, "\n");

//*/// 
    *o_String = tmp;
    return NS_OK;
}
#endif // DEBUG
