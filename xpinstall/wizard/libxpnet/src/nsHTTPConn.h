/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Samir Gehani <sgehani@netscape.com>
 */

#ifndef _NS_HTTPCONN_H_
#define _NS_HTTPCONN_H_

class nsSocket;  

typedef int (*HTTPGetCB)(int aBytesRd, int aTotal);

class nsHTTPConn
{
public:
    nsHTTPConn(char *aHost, int aPort, char *aPath);
    nsHTTPConn(char *aURL);
    nsHTTPConn(char *aHost, int aPort, char *aPath, int (*aEventPumpCB)(void));
    nsHTTPConn(char *aURL, int (*aEventPumpCB)(void));
    ~nsHTTPConn();

    int Open();
    int ResumeOrGet(HTTPGetCB aCallback, char *aDestFile);
    int Get(HTTPGetCB aCallback, char *aDestFile);
    int Get(HTTPGetCB aCallback, char *aDestFile, int aResumePos);
    int GetResponseCode() { return mResponseCode; }
    int Close();

    void SetProxyInfo(char *aProxiedURL, char *aProxyUser, 
                                         char *aProxyPswd);
    static int ParseURL(const char *aProto, char *aURL, char **aHost, 
                        int *aPort, char **aPath);

    enum 
    {
        OK                  = 0,
        E_MEM               = -801,
        E_PARAM             = -802,
        E_MALFORMED_URL     = -803,
        E_REQ_INCOMPLETE    = -804,
        E_B64_ENCODE        = -805,
        E_OPEN_FILE         = -806,
        E_SEEK_FILE         = -807,
        E_HTTP_RESPONSE     = -808,
        E_USER_CANCEL       = -813
    };

private:
    int Request(int aResumePos);
    int Response(HTTPGetCB aCallback, char *aDestFile, int aResumePos);
    void ParseContentLength(const char *aBuf, int *aLength);
    void ParseResponseCode(const char *aBuf, int *aCode);
    int Base64Encode(const unsigned char *in_str, int in_len,
                     char *out_str, int out_len);

    int (*mEventPumpCB)(void);
    char *mHost;
    char *mPath;
    int   mPort;
    char *mProxiedURL;
    char *mProxyUser;
    char *mProxyPswd;
    char *mDestFile;
    int   mHostPathAllocd;
    nsSocket *mSocket;
    int   mResponseCode;
};
    
#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifdef DUMP
#undef DUMP
#endif
#if defined(DEBUG_sgehani) || defined(DEBUG)
#define DUMP(_vargs)                        \
do {                                        \
    printf("%s %d: ", __FILE__, __LINE__);  \
    printf _vargs;                          \
} while(0);
#else
#define DUMP(_vargs) 
#endif 

#endif /* _NS_HTTPCONN_H_ */
