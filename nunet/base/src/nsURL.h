/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _nsURL_h_
#define _nsURL_h_

/* 
    The nsURL class holds the default implementation of the nsICoolURL class. 
    For explaination on the implementation see - 

         http://www.w3.org/Addressing/URL/url-spec.txt
         and the headers for nsICoolURL.h and nsIURI.h
    
    - Gagan Saksena
 */

#include "nsICoolURL.h"

class nsURL : public nsICoolURL
{

public:
    //Constructor
                        nsURL(const char* i_URL);
    
    //Functions from nsISupports
    
    NS_DECL_ISUPPORTS

    //Functions from nsIURI
    //Core parsing functions. 
    /* 
        The Scheme is the protocol that this URL refers to. 
    */
    NS_METHOD           GetScheme(const char* *o_Scheme) const;
    NS_METHOD           SetScheme(const char* i_Scheme);

    /* 
        The PreHost portion includes elements like the optional 
        username:password, or maybe other scheme specific items. 
    */
    NS_METHOD           GetPreHost(const char* *o_PreHost) const;
    NS_METHOD           SetPreHost(const char* i_PreHost);

    /* 
        The Host is the internet domain name to which this URL refers. 
        Note that it could be an IP address as well. 
    */
    NS_METHOD           GetHost(const char* *o_Host) const;
    NS_METHOD           SetHost(const char* i_Host);

    /* 
        A return value of -1 indicates that no port value is set and the 
        implementor of the specific scheme will use its default port. 
        Similarly setting a value of -1 indicates that the default is to be used.
        Thus as an example-
            for HTTP, Port 80 is same as a return value of -1. 
        However after setting a port (even if its default), the port number will
        appear in the ToString function.
    */
    NS_METHOD_(PRInt32) GetPort(void) const;
    NS_METHOD           SetPort(PRInt32 i_Port);

    /* 
        Note that the path includes the leading '/' Thus if no path is 
        available the GetPath will return a "/" 
        For SetPath if none is provided, one would be prefixed to the path. 
    */
    NS_METHOD           GetPath(const char* *o_Path) const;
    NS_METHOD           SetPath(const char* i_Path);

    //Functions from nsICoolURL.h 
    /* 
        Note: The GetStream function also opens a connection using 
        the available information in the URL. This is the same as 
        calling OpenInputStream on the connection returned from 
        OpenProtocolInstance.
    */
    NS_METHOD           GetStream(nsIInputStream* *o_InputStream);

    /* 
        The OpenProtocolInstance method sets up the connection as decided by the 
        protocol implementation. This may then be used to get various 
        connection specific details like the input and the output streams 
        associated with the connection, or the header information by querying
        on the connection type which will be protocol specific.
    */
    NS_METHOD          OpenProtocolInstance(nsIProtocolInstance* *o_ProtocolInstance);


    //Other utility functions
    /* 
        Note that this comparison is only on char* level. Cast nsIURL to 
        the scheme specific URL to do a more thorough check. For example--
        in HTTP-
            http://foo.com:80 == http://foo.com
        but this function through nsIURL alone will not return equality.
    */
    NS_METHOD_(PRBool)  Equals(const nsIURI* i_URI) const;

    /* 
        Writes a string representation of the URL. 
    */
    NS_METHOD           ToString(const char* *o_URLString) const;

#ifdef DEBUG
    NS_METHOD           DebugString(const char* *o_URLString) const;
#endif //DEBUG

protected:
    
    //Called only from Release
    virtual ~nsURL();

    typedef enum 
    {
        SCHEME,
        PREHOST,
        HOST,
        PATH,
        TOTAL_PARTS // Must always be the last one. 
    } Part;

    /*
        Parses the portions of the URL into individual pieces 
        like spec, prehost, host, path etc.
    */
    NS_METHOD           Parse();

    /*
        Extract a portion from the given part id.
    */
    NS_METHOD           Extract(const char* *o_OutputString, Part i_id) const;

    /* 
        This holds the offsets and the lengths of each part. 
        Optimizes on storage and speed. 
    */
    int m_Position[TOTAL_PARTS][2];

    char*       m_URL;
    PRInt32     m_Port;
private:
    NS_METHOD           ExtractPortFrom(int start, int length);
};

inline
NS_METHOD          
nsURL::GetHost(const char* *o_Host) const
{
    return Extract(o_Host, HOST);
}

inline
NS_METHOD          
nsURL::GetScheme(const char* *o_Scheme) const
{
    return Extract(o_Scheme, SCHEME);
}

inline
NS_METHOD          
nsURL::GetPreHost(const char* *o_PreHost) const
{
    return Extract(o_PreHost, PREHOST);
}

inline
NS_METHOD_(PRInt32)          
nsURL::GetPort() const
{
    return m_Port;
}

inline
NS_METHOD          
nsURL::GetPath(const char* *o_Path) const
{
    return Extract(o_Path, PATH);
}

inline
NS_METHOD
nsURL::ToString(const char* *o_URLString) const
{
    *o_URLString = m_URL;
    return NS_OK;
}

//Possible bad url passed.
#define NS_ERROR_URL_PARSING    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 100);
#define NS_ERROR_BAD_URL        NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_NETWORK, 101);

#endif /* _nsURL_h_ */
