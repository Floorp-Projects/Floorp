/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Bradley Baetz <bbaetz@cs.mcgill.ca>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsFTPDirListingConv.h"
#include "nsMemory.h"
#include "plstr.h"
#include "prlog.h"
#include "nsIAtom.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsCOMPtr.h"
#include "nsEscape.h"
#include "nsNetUtil.h"
#include "nsIStringStream.h"
#include "nsILocaleService.h"
#include "nsIComponentManager.h"
#include "nsDateTimeFormatCID.h"
#include "nsIStreamListener.h"
#include "nsCRT.h"
#include "nsMimeTypes.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID);
static NS_DEFINE_CID(kDateTimeCID, NS_DATETIMEFORMAT_CID);

#if defined(PR_LOGGING)
//
// Log module for FTP dir listing stream converter logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsFTPDirListConv:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
//
PRLogModuleInfo* gFTPDirListConvLog = nsnull;

#endif /* PR_LOGGING */

// nsISupports implementation
NS_IMPL_THREADSAFE_ISUPPORTS3(nsFTPDirListingConv,
                              nsIStreamConverter,
                              nsIStreamListener, 
                              nsIRequestObserver);

// Common code of Convert and AsyncConvertData function

static FTP_Server_Type
DetermineServerType (nsCString &fromMIMEString, const PRUnichar *aFromType)
{
    fromMIMEString.AssignWithConversion(aFromType);
    const char *from = fromMIMEString.get();
    NS_ASSERTION(from, "nsCString/PRUnichar acceptance failed.");

    from = PL_strstr(from, "/ftp-dir-");
    if (!from) return ERROR_TYPE;
    from += 9;
    fromMIMEString = from;

    if (-1 != fromMIMEString.Find("unix")) {
        return UNIX;
    } else if (-1 != fromMIMEString.Find("nt")) {
        return NT;
    } else if (-1 != fromMIMEString.Find("dcts")) {
        return DCTS;
    } else if (-1 != fromMIMEString.Find("ncsa")) {
        return NCSA;
    } else if (-1 != fromMIMEString.Find("peter_lewis")) {
        return PETER_LEWIS;
    } else if (-1 != fromMIMEString.Find("machten")) {
        return MACHTEN;
    } else if (-1 != fromMIMEString.Find("cms")) {
        return CMS;
    } else if (-1 != fromMIMEString.Find("tcpc")) {
        return TCPC;
    } else if (-1 != fromMIMEString.Find("os2")) {
        return OS_2;
    }

    return GENERIC;
}

// nsIStreamConverter implementation

#define CONV_BUF_SIZE (4*1024)
NS_IMETHODIMP
nsFTPDirListingConv::Convert(nsIInputStream *aFromStream,
                             const PRUnichar *aFromType,
                             const PRUnichar *aToType,
                             nsISupports *aCtxt, nsIInputStream **_retval) {
    nsresult rv;

    // set our internal state to reflect the server type
    nsCString fromMIMEString;

    mServerType = DetermineServerType(fromMIMEString, aFromType);
    if (mServerType == ERROR_TYPE) return NS_ERROR_FAILURE;

    char buffer[CONV_BUF_SIZE];
    int i = 0;
    while (i < CONV_BUF_SIZE) {
        buffer[i] = '\0';
        i++;
    }
    CBufDescriptor desc(buffer, PR_TRUE, CONV_BUF_SIZE);
    nsCAutoString aBuffer(desc);
    nsCAutoString convertedData;

    NS_ASSERTION(aCtxt, "FTP dir conversion needs the context");
    // build up the 300: line
    nsXPIDLCString spec;
    nsCOMPtr<nsIURI> uri(do_QueryInterface(aCtxt, &rv));
    if (NS_FAILED(rv)) return rv;

    rv = uri->GetSpec(getter_Copies(spec));
    if (NS_FAILED(rv)) return rv;

    convertedData.Append("300: ");
    convertedData.Append(spec);
    convertedData.Append(char(nsCRT::LF));
    // END 300:

    // build up the column heading; 200:
    convertedData.Append("200: filename content-length last-modified file-type\n");
    // END 200:


    // build up the body
    while (1) {
        PRUint32 amtRead = 0;

        rv = aFromStream->Read(buffer+aBuffer.Length(),
                               CONV_BUF_SIZE-aBuffer.Length(), &amtRead);
        if (NS_FAILED(rv)) return rv;

        if (!amtRead) {
            // EOF
            break;
        }

        aBuffer = DigestBufferLines(buffer, convertedData);
    }
    // end body building

#ifndef DEBUG_valeski
    PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, ("::OnData() sending the following %d bytes...\n\n%s\n\n", 
        convertedData.Length(), convertedData.get()) );
#else
    char *unescData = ToNewCString(convertedData);
    nsUnescape(unescData);
    printf("::OnData() sending the following %d bytes...\n\n%s\n\n", convertedData.Length(), unescData);
    nsMemory::Free(unescData);
#endif // DEBUG_valeski

    // send the converted data out.
    nsCOMPtr<nsIInputStream> inputData;
    nsCOMPtr<nsISupports>    inputDataSup;

    rv = NS_NewCStringInputStream(getter_AddRefs(inputDataSup), convertedData);
    if (NS_FAILED(rv)) return rv;

    inputData = do_QueryInterface(inputDataSup, &rv);
    if (NS_FAILED(rv)) return rv;

    *_retval = inputData.get();
    NS_ADDREF(*_retval);

    return NS_OK;

}




// Stream converter service calls this to initialize the actual stream converter (us).
NS_IMETHODIMP
nsFTPDirListingConv::AsyncConvertData(const PRUnichar *aFromType, const PRUnichar *aToType,
                                      nsIStreamListener *aListener, nsISupports *aCtxt) {
    NS_ASSERTION(aListener && aFromType && aToType, "null pointer passed into FTP dir listing converter");
    nsresult rv;

    // hook up our final listener. this guy gets the various On*() calls we want to throw
    // at him.
    mFinalListener = aListener;
    NS_ADDREF(mFinalListener);

    // set our internal state to reflect the server type
    nsCString fromMIMEString;

    mServerType = DetermineServerType(fromMIMEString, aFromType);
    if (mServerType == ERROR_TYPE) return NS_ERROR_FAILURE;


    // we need our own channel that represents the content-type of the
    // converted data.
    NS_ASSERTION(aCtxt, "FTP dir listing needs a context (the uri)");
    nsIURI *uri;
    rv = aCtxt->QueryInterface(NS_GET_IID(nsIURI), (void**)&uri);
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewInputStreamChannel(&mPartChannel,
                                  uri,
                                  nsnull,
                                  APPLICATION_HTTP_INDEX_FORMAT,
                                  -1);          // XXX fix contentLength
    NS_RELEASE(uri);
    if (NS_FAILED(rv)) return rv;

    PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, 
        ("nsFTPDirListingConv::AsyncConvertData() converting FROM raw %s, TO application/http-index-format\n", fromMIMEString.get()));

    return NS_OK;
}


// nsIStreamListener implementation
NS_IMETHODIMP
nsFTPDirListingConv::OnDataAvailable(nsIRequest* request, nsISupports *ctxt,
                                  nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count) {
    NS_ASSERTION(request, "FTP dir listing stream converter needs a request");
    
    nsresult rv;

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request, &rv);
    if (NS_FAILED(rv)) return rv;
    
    PRUint32 read, streamLen;
    nsCAutoString indexFormat;
    indexFormat.SetCapacity(72); // quick guess 

    rv = inStr->Available(&streamLen);
    if (NS_FAILED(rv)) return rv;

    char *buffer = (char*)nsMemory::Alloc(streamLen + 1);
    rv = inStr->Read(buffer, streamLen, &read);
    if (NS_FAILED(rv)) return rv;

    // the dir listings are ascii text, null terminate this sucker.
    buffer[streamLen] = '\0';

    PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, ("nsFTPDirListingConv::OnData(request = %x, ctxt = %x, inStr = %x, sourceOffset = %d, count = %d)\n", request, ctxt, inStr, sourceOffset, count));

    if (!mBuffer.IsEmpty()) {
        // we have data left over from a previous OnDataAvailable() call.
        // combine the buffers so we don't lose any data.
        mBuffer.Append(buffer);
        nsMemory::Free(buffer);
        buffer = ToNewCString(mBuffer);
        mBuffer.Truncate();
    }

#ifndef DEBUG_valeski
    PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, ("::OnData() received the following %d bytes...\n\n%s\n\n", streamLen, buffer) );
#else
    printf("::OnData() received the following %d bytes...\n\n%s\n\n", streamLen, buffer);
#endif // DEBUG_valeski


    if (!mSentHeading) {
        // build up the 300: line
        char *spec = nsnull;
        nsIURI *uri = nsnull;
        rv = channel->GetURI(&uri);
        if (NS_FAILED(rv)) return rv;

        rv = uri->GetSpec(&spec);
        NS_RELEASE(uri);
        if (NS_FAILED(rv)) return rv;

        indexFormat.Append("300: ");
        indexFormat.Append(spec);
        indexFormat.Append(char(nsCRT::LF));
        nsMemory::Free(spec);
        // END 300:

        // build up the column heading; 200:
        indexFormat.Append("200: filename content-length last-modified file-type\n");
        // END 200:

        mSentHeading = PR_TRUE;
    }

    char *line = buffer;
    line = DigestBufferLines(line, indexFormat);

#ifndef DEBUG_valeski
    PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, ("::OnData() sending the following %d bytes...\n\n%s\n\n", 
        indexFormat.Length(), indexFormat.get()) );
#else
    char *unescData = ToNewCString(indexFormat);
    nsUnescape(unescData);
    printf("::OnData() sending the following %d bytes...\n\n%s\n\n", indexFormat.Length(), unescData);
    nsMemory::Free(unescData);
#endif // DEBUG_valeski

    // if there's any data left over, buffer it.
    if (line && *line) {
        mBuffer.Append(line);
        PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, ("::OnData() buffering the following %d bytes...\n\n%s\n\n",
            PL_strlen(line), line) );
    }

    nsMemory::Free(buffer);

    // send the converted data out.
    nsCOMPtr<nsIInputStream> inputData;
    nsCOMPtr<nsISupports>    inputDataSup;

    rv = NS_NewCStringInputStream(getter_AddRefs(inputDataSup), indexFormat);
    if (NS_FAILED(rv)) return rv;

    inputData = do_QueryInterface(inputDataSup, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = mFinalListener->OnDataAvailable(mPartChannel, ctxt, inputData, 0, indexFormat.Length());
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


// nsIRequestObserver implementation
NS_IMETHODIMP
nsFTPDirListingConv::OnStartRequest(nsIRequest* request, nsISupports *ctxt) {
    // we don't care about start. move along... but start masqeurading 
    // as the http-index channel now.
    return mFinalListener->OnStartRequest(mPartChannel, ctxt);
}

NS_IMETHODIMP
nsFTPDirListingConv::OnStopRequest(nsIRequest* request, nsISupports *ctxt,
                                   nsresult aStatus) {
    // we don't care about stop. move along...

    nsresult rv;

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request, &rv);
    if (NS_FAILED(rv)) return rv;


    nsCOMPtr<nsILoadGroup> loadgroup;
    rv = channel->GetLoadGroup(getter_AddRefs(loadgroup));
    if (NS_FAILED(rv)) return rv;

    if (loadgroup)
        (void)loadgroup->RemoveRequest(mPartChannel, nsnull, aStatus);

    return mFinalListener->OnStopRequest(mPartChannel, ctxt, aStatus);
}


// nsFTPDirListingConv methods
nsFTPDirListingConv::nsFTPDirListingConv() {
    NS_INIT_ISUPPORTS();
    mFinalListener      = nsnull;
    mServerType         = GENERIC;
    mPartChannel        = nsnull;
    mSentHeading        = PR_FALSE;
}

nsFTPDirListingConv::~nsFTPDirListingConv() {
    NS_IF_RELEASE(mFinalListener);
    NS_IF_RELEASE(mPartChannel);
    NS_RELEASE(mLocale);
    NS_RELEASE(mDateTimeFormat);
}

nsresult
nsFTPDirListingConv::Init() {
    // Grab the nsILocale for date parsing.
    nsresult rv;
    nsCOMPtr<nsILocaleService> localeSvc = 
             do_GetService(kLocaleServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = localeSvc->GetApplicationLocale(&mLocale);
    if (NS_FAILED(rv)) return rv;

    // Grab the date/time formatter
    nsIComponentManager *comMgr;
    rv = NS_GetGlobalComponentManager(&comMgr);
    if (NS_FAILED(rv)) return rv;

    rv = comMgr->CreateInstance(kDateTimeCID, nsnull, NS_GET_IID(nsIDateTimeFormat), (void**)&mDateTimeFormat);
    if (NS_FAILED(rv)) return rv;

#if defined(PR_LOGGING)
    //
    // Initialize the global PRLogModule for FTP Protocol logging 
    // if necessary...
    //
    if (nsnull == gFTPDirListConvLog) {
        gFTPDirListConvLog = PR_NewLogModule("nsFTPDirListingConv");
    }
#endif /* PR_LOGGING */

    return NS_OK;
}


PRInt8
nsFTPDirListingConv::MonthNumber(const char *month) {
    NS_ASSERTION(month && month[1] && month[2], "bad month");
    if (!month || !month[0] || !month[1] || !month[2])
        return -1;

    char c1 = month[1], c2 = month[2];
    PRInt8 rv = -1;

    //PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, ("nsFTPDirListingConv::MonthNumber(month = %s) ", month) );

    switch (*month) {
    case 'f': case 'F':
        rv = 1; break;
    case 'm': case 'M':
        // c1 == 'a' || c1 == 'A'
        if (c2 == 'r' || c2 == 'R')
            rv = 2;
        else
            // c2 == 'y' || c2 == 'Y'
            rv = 4;
        break;
    case 'a': case 'A':
        if (c1 == 'p' || c1 == 'P')
            rv = 3;
        else
            // c1 == 'u' || c1 == 'U'
            rv = 7;
        break;
    case 'j': case 'J':
        if (c1 == 'u' || c1 == 'U') {
            if (c2 == 'n' || c2 == 'N')
                rv = 5;
            else
                // c2 == 'l' || c2 == 'L'
                rv = 6;
        } else {
            // c1 == 'a' || c1 == 'A'
            rv = 0;
        }
        break;
    case 's': case 'S':
        rv = 8; break;
    case 'o': case 'O':
        rv = 9; break;
    case 'n': case 'N':
        rv = 10; break;
    case 'd': case 'D':
        rv = 11; break;
    default:
        rv = -1;
    }

   //PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, ("returning %d\n", rv) );

    return rv;
}


// Return true if the string is of the form:
//    "Sep  1  1990 " or
//    "Sep 11 11:59 " or
//    "Dec 12 1989  " or
//    "FCv 23 1990  " ...
PRBool
nsFTPDirListingConv::IsLSDate(char *aCStr) {

    /* must start with three alpha characters */
    if (!nsCRT::IsAsciiAlpha(*aCStr++) || !nsCRT::IsAsciiAlpha(*aCStr++) || !nsCRT::IsAsciiAlpha(*aCStr++))
        return PR_FALSE;

    /* space */
    if (*aCStr != ' ')
        return PR_FALSE;
    aCStr++;

    /* space or digit */
    if ((*aCStr != ' ') && !nsCRT::IsAsciiDigit(*aCStr))
        return PR_FALSE;
    aCStr++;

    /* digit */
    if (!nsCRT::IsAsciiDigit(*aCStr))
        return PR_FALSE;
    aCStr++;

    /* space */
    if (*aCStr != ' ')
        return PR_FALSE;
    aCStr++;

    /* space or digit */
    if ((*aCStr != ' ') && !nsCRT::IsAsciiDigit(*aCStr))
        return PR_FALSE;
    aCStr++;

    /* digit */
    if (!nsCRT::IsAsciiDigit(*aCStr))
        return PR_FALSE;
    aCStr++;

    /* colon or digit */
    if ((*aCStr != ':') && !nsCRT::IsAsciiDigit(*aCStr))
        return PR_FALSE;
    aCStr++;

    /* digit */
    if (!nsCRT::IsAsciiDigit(*aCStr))
        return PR_FALSE;
    aCStr++;

    /* space or digit */
    if ((*aCStr != ' ') && !nsCRT::IsAsciiDigit(*aCStr))
        return PR_FALSE;
    aCStr++;

    /* space */
    if (*aCStr != ' ')
        return PR_FALSE;
    aCStr++;

    return PR_TRUE;
}

       
// Converts a date string from 'ls -l' to a PRTime number
// "Sep  1  1990 " or
// "Sep 11 11:59 " or
// "Dec 12 1989  " or
// "FCv 23 1990  " ...
// Returns 0 on error.
PRBool
nsFTPDirListingConv::ConvertUNIXDate(char *aCStr, PRExplodedTime& outDate) {

    PRExplodedTime curTime;
    InitPRExplodedTime(curTime);

    char *bcol = aCStr;         /* Column begin */
    char *ecol;                   /* Column end */

    // MONTH
    char tmpChar = bcol[3];
    bcol[3] = '\0';

    if ((curTime.tm_month = MonthNumber(bcol)) < 0)
    	return PR_FALSE;
    bcol[3] = tmpChar;

    // DAY
    ecol = &bcol[3];                        
    while (*(++ecol) == ' ') ;
    while (*(++ecol) != ' ') ;
    *ecol = '\0';
    bcol = ecol+1;
    while (*(--ecol) != ' ') ;

    PRInt32 error;
    nsCAutoString day(ecol);
    curTime.tm_mday = day.ToInteger(&error, 10);

    // YEAR
    if ((ecol = PL_strchr(bcol, ':')) == NULL) {
        nsCAutoString intStr(bcol);
        curTime.tm_year = intStr.ToInteger(&error, 10);
    } else { 
        // TIME
    	/* If the time is given as hh:mm, then the file is less than 1 year
         *	old, but we might shift calandar year. This is avoided by checking
         *	if the date parsed is future or not. 
		 */
    	*ecol = '\0';
        nsCAutoString intStr(++ecol);
        curTime.tm_min = intStr.ToInteger(&error, 10);   // Right side of ':' 

        intStr = bcol;
        curTime.tm_hour = intStr.ToInteger(&error, 10);  // Left side of ':' 

        PRExplodedTime nowETime;
        PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &nowETime);
        curTime.tm_year = nowETime.tm_year;

        PRBool thisCalendarYear = PR_FALSE;
        if (nowETime.tm_month > curTime.tm_month) {
            thisCalendarYear = PR_TRUE;
        } else if (nowETime.tm_month == curTime.tm_month
                   && nowETime.tm_mday > curTime.tm_mday) {
            thisCalendarYear = PR_TRUE;
        } else if (nowETime.tm_month == curTime.tm_month
                   && nowETime.tm_mday == curTime.tm_mday
                   && nowETime.tm_hour > curTime.tm_hour) {
            thisCalendarYear = PR_TRUE;
        } else if (nowETime.tm_month == curTime.tm_month
                   && nowETime.tm_mday == curTime.tm_mday
                   && nowETime.tm_hour == curTime.tm_hour
                   && nowETime.tm_min >= curTime.tm_min) {
            thisCalendarYear = PR_TRUE;
        }

        if (!thisCalendarYear) curTime.tm_year--;
    }

    // set the out param
    outDate = curTime;
    return PR_TRUE;
}

PRBool
nsFTPDirListingConv::ConvertDOSDate(char *aCStr, PRExplodedTime& outDate) {

    PRExplodedTime curTime, nowTime;
    PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &nowTime);
    PRInt16 century = (nowTime.tm_year/1000 + nowTime.tm_year/100) * 100;

    InitPRExplodedTime(curTime);

    curTime.tm_month      = (aCStr[1]-'0')-1;

    curTime.tm_mday     = (((aCStr[3]-'0')*10) + aCStr[4]-'0');
    curTime.tm_year     = century + (((aCStr[6]-'0')*10) + aCStr[7]-'0'); 
    curTime.tm_hour     = (((aCStr[10]-'0')*10) + aCStr[11]-'0'); 

	if(aCStr[15] == 'P')
        curTime.tm_hour += 12;

    curTime.tm_min = (((aCStr[13]-'0')*10) + aCStr[14]-'0');

    outDate = curTime;
    return PR_TRUE;
}

nsresult
nsFTPDirListingConv::ParseLSLine(char *aLine, indexEntry *aEntry) {

    PRInt32 base=1;
	PRInt32 size_num=0;
	char save_char;
	char *ptr, *escName;

    for (ptr = &aLine[PL_strlen(aLine) - 1];
            (ptr > aLine+13) && (!nsCRT::IsAsciiSpace(*ptr) || !IsLSDate(ptr-12)); ptr--)
                ; /* null body */
	save_char = *ptr;
    *ptr = '\0';
    if (ptr > aLine+13) {
        ConvertUNIXDate(ptr-12, aEntry->mMDTM);
    } else {
	    // must be a dl listing
		// unterminate the line
		*ptr = save_char;
		// find the first whitespace and  terminate
		for(ptr=aLine; *ptr != '\0'; ptr++)
			if(nsCRT::IsAsciiSpace(*ptr)) {
				*ptr = '\0';
				break;
            }
        escName = nsEscape(aLine, url_Path);
        aEntry->mName = escName;
        nsMemory::Free(escName);
	
		return NS_OK;
    }

    escName = nsEscape(ptr+1, url_Path);
    aEntry->mName = escName;
    nsMemory::Free(escName);

	// parse size
	if(ptr > aLine+15) {
        ptr -= 14;
        while (nsCRT::IsAsciiDigit(*ptr)) {
            size_num += ((PRInt32) (*ptr - '0')) * base;
            base *= 10;
            ptr--;
        }
    
        aEntry->mContentLen = size_num;
    }
    return NS_OK;
}

void
nsFTPDirListingConv::InitPRExplodedTime(PRExplodedTime& aTime) {
    aTime.tm_usec = 0;
    aTime.tm_sec  = 0;
    aTime.tm_min  = 0;
    aTime.tm_hour = 0;
    aTime.tm_mday = 0;
    aTime.tm_month= 0;
    aTime.tm_year = 0;
    aTime.tm_wday = 0;
    aTime.tm_yday = 0;
    aTime.tm_params.tp_gmt_offset = 0;
    aTime.tm_params.tp_dst_offset = 0;
}

char *
nsFTPDirListingConv::DigestBufferLines(char *aBuffer, nsCAutoString &aString) {
    nsresult rv;
    char *line = aBuffer;
    char *eol;
    PRBool cr = PR_FALSE;

    // while we have new lines, parse 'em into application/http-index-format.
    while ( line && (eol = PL_strchr(line, nsCRT::LF)) ) {
        // yank any carriage returns too.
        if (eol > line && *(eol-1) == nsCRT::CR) {
            eol--;
            *eol = '\0';
            cr = PR_TRUE;
        } else {
            *eol = '\0';
            cr = PR_FALSE;
        }
        indexEntry *thisEntry = nsnull;
        NS_NEWXPCOM(thisEntry, indexEntry);
        if (!thisEntry) return nsnull;

        // XXX we need to handle comments in the raw stream.

        // special case windows servers who masquerade as unix servers
        if (NT == mServerType && !nsCRT::IsAsciiSpace(line[8]))
            mServerType = UNIX;

        // check for an eplf response
        if (line[0] == '+')
            mServerType = EPLF;

        char *escName = nsnull;
        switch (mServerType) {

        case UNIX:
        case PETER_LEWIS:
        case MACHTEN:
        {
            // don't bother w/ these lines.
            if(!PL_strncmp(line, "total ", 6)
                || !PL_strncmp(line, "ls: total", 9)
				|| (PL_strstr(line, "Permission denied") != NULL)
                || (PL_strstr(line, "not available") != NULL)) {
                NS_DELETEXPCOM(thisEntry);
                if (cr)
                    line = eol+2;
                else
                    line = eol+1;
                continue;
            }

            PRInt32 len = PL_strlen(line);

            // check first character of ls -l output
            // For example: "dr-x--x--x" is what we're starting with.
            if (line[0] == 'D' || line[0] == 'd') {
                /* it's a directory */
                thisEntry->mType = Dir;
                thisEntry->mSupressSize = PR_TRUE;
            } else if (line[0] == 'l') {
                thisEntry->mType = Link;
                thisEntry->mSupressSize = PR_TRUE;

                /* strip off " -> pathname" */
                PRInt32 i;
                for (i = len - 1; (i > 3) && (!nsCRT::IsAsciiSpace(line[i])
                    || (line[i-1] != '>') 
                    || (line[i-2] != '-')
                    || (line[i-3] != ' ')); i--)
                         ; /* null body */
                if (i > 3) {
                    line[i-3] = '\0';
                    len = i - 3;
                }
            }

            rv = ParseLSLine(line, thisEntry);
            if ( NS_FAILED(rv) || (thisEntry->mName.Equals("..")) || (thisEntry->mName.Equals(".")) ) {
                NS_DELETEXPCOM(thisEntry);
                if (cr)
                    line = eol+2;
                else
                    line = eol+1;
                continue;
            }

            break; // END UNIX, PETER_LEWIS, MACHTEN
        }

        case NCSA:
        case TCPC:
        {
            escName = nsEscape(line, url_Path);
            thisEntry->mName = escName;
            nsMemory::Free(escName);

            if (thisEntry->mName.Last() == '/') {
                thisEntry->mType = Dir;
                thisEntry->mName.Truncate(thisEntry->mName.Length()-1);
            }

            break; // END NCSA, TCPC
        }

        case CMS:
        {
            escName = nsEscape(line, url_Path);
            thisEntry->mName = escName;
            nsMemory::Free(escName);
            break; // END CMS
        }
        case NT:
        {
		    // don't bother w/ these lines.
            if(!PL_strncmp(line, "total ", 6)
                || !PL_strncmp(line, "ls: total", 9)
				|| (PL_strstr(line, "Permission denied") != NULL)
                || (PL_strstr(line, "not available") != NULL)) {
                NS_DELETEXPCOM(thisEntry);
                if (cr)
                    line = eol+2;
                else
                    line = eol+1;
                continue;
            }
    
            char *date, *size_s, *name;

			if(PL_strlen(line) > 37) {
				date = line;
				line[17] = '\0';
				size_s = &line[18];
				line[38] = '\0';
				name = &line[39];

                if(PL_strstr(size_s, "<DIR>")) {
                    thisEntry->mType = Dir;
                } else {
                    nsCAutoString size(size_s);
                    size.StripWhitespace();
                    thisEntry->mContentLen = atol(size.get());
                }

                ConvertDOSDate(date, thisEntry->mMDTM);

                escName = nsEscape(name, url_Path);
                thisEntry->mName = escName;
            } else {
                escName = nsEscape(line, url_Path);
                thisEntry->mName = escName;
			}
            nsMemory::Free(escName);
            break; // END NT
        }
        case EPLF:
        {
            
            int flagcwd = 0;
            int when = 0;
            int flagsize = 0;
            unsigned long size;
            PRBool processing = PR_TRUE;
            while (*line && processing)
                switch (*line) {
                  case '\t':
                  {
                      if (flagcwd) {
                          thisEntry->mType = Dir;
                          thisEntry->mContentLen = 0;
                      } else {
                          thisEntry->mType = File;
                          thisEntry->mContentLen = size;
                      }

                      escName = nsEscape(line+1, url_Path);
                      thisEntry->mName = escName;
                      
                      if (flagsize) {
                        thisEntry->mSupressSize = PR_FALSE;
                        // Mutiply what the last modification date to get usecs.  
                        PRInt64 usecs = LL_Zero();
                        PRInt64 seconds = LL_Zero();
                        PRInt64 multiplier = LL_Zero();
                        LL_I2L(seconds, when);
                        LL_I2L(multiplier, PR_USEC_PER_SEC);
                        LL_MUL(usecs, seconds, multiplier);
                        PR_ExplodeTime(usecs, PR_LocalTimeParameters, &thisEntry->mMDTM);
                      } else {
                          thisEntry->mSupressSize = PR_TRUE;
                      }

                      processing = PR_FALSE;
                    }
                    break;
                    case 's':
                      flagsize = 1;
                      size = 0;
                      while (*++line && nsCRT::IsAsciiDigit(*line))
                          size = size * 10 + (*line - '0');
                      break;
                    case 'm':
                      while (*++line && nsCRT::IsAsciiDigit(*line))
                          when = when * 10 + (*line - '0');
                      break;
                    case '/':
                      flagcwd = 1;
                    default:
                      while (*line) if (*line++ == ',') break;
            }
            break; //END EPLF
        }

        case OS_2:
        {
            if(!PL_strncmp(line, "total ", 6)
               || (PL_strstr(line, "not authorized") != NULL)
               || (PL_strstr(line, "Path not found") != NULL)
               || (PL_strstr(line, "No Files") != NULL)) {
                NS_DELETEXPCOM(thisEntry);
                if (cr)
                    line = eol+2;
                else
                    line = eol+1;
                continue;
            }

            char *name;
            nsCAutoString str;

            if(PL_strstr(line, "DIR")) {
                thisEntry->mType = Dir;
                thisEntry->mSupressSize = PR_TRUE;
            }
            else
                thisEntry->mType = File;

            PRInt32 error;
            line[18] = '\0';
            str = line;
            str.StripWhitespace();
            thisEntry->mContentLen = str.ToInteger(&error, 10);

            InitPRExplodedTime(thisEntry->mMDTM);
            line[37] = '\0';
            str = &line[35];
            thisEntry->mMDTM.tm_month = str.ToInteger(&error, 10) - 1;

            line[40] = '\0';
            str = &line[38];
            thisEntry->mMDTM.tm_mday = str.ToInteger(&error, 10);

            line[43] = '\0';
            str = &line[41];
            thisEntry->mMDTM.tm_year = str.ToInteger(&error, 10);

            line[48] = '\0';
            str = &line[46];
            thisEntry->mMDTM.tm_hour = str.ToInteger(&error, 10);

            line[51] = '\0';
            str = &line[49];
            thisEntry->mMDTM.tm_min = str.ToInteger(&error, 10);

            name = &line[53];
            escName = nsEscape(name, url_Path);
            thisEntry->mName = escName;

            nsMemory::Free(escName);
            break;
        }	

        default:
        {
            escName = nsEscape(line, url_Path);
            thisEntry->mName = escName;
            nsMemory::Free(escName);
            break; // END default (catches GENERIC, DCTS)
        }

        } // end switch (mServerType)

        // blast the index entry into the indexFormat buffer as a 201: line.
        aString.Append("201: ");
        // FILENAME
        aString.Append(thisEntry->mName);
        aString.Append(' ');

        // CONTENT LENGTH
        if (!thisEntry->mSupressSize) {
            aString.AppendInt(thisEntry->mContentLen);
        } else {
            aString.Append('0');
        }
        aString.Append(' ');

        // MODIFIED DATE
        char buffer[256] = "";
        // Note: The below is the RFC822/1123 format, as required by
        // the application/http-index-format specs
        // viewers of such a format can then reformat this into the
        // current locale (or anything else they choose)
        PR_FormatTimeUSEnglish(buffer, sizeof(buffer),
                               "%a, %d %b %Y %H:%M:%S GMT", &thisEntry->mMDTM );
        char *escapedDate = nsEscape(buffer, url_Path);

        aString.Append(escapedDate);
        nsMemory::Free(escapedDate);
        aString.Append(' ');


        // ENTRY TYPE
        switch (thisEntry->mType) {
        case Dir:
            aString.Append("DIRECTORY");
            break;
        case Link:
            aString.Append("SYMBOLIC-LINK");
            break;
        default:
            aString.Append("FILE");
        }
        aString.Append(' ');

        aString.Append(char(nsCRT::LF)); // complete this line
        // END 201:

        NS_DELETEXPCOM(thisEntry);

        if (cr)
            line = eol+2;
        else
            line = eol+1;
    } // end while(eol)

    return line;
}

nsresult
NS_NewFTPDirListingConv(nsFTPDirListingConv** aFTPDirListingConv)
{
    NS_PRECONDITION(aFTPDirListingConv != nsnull, "null ptr");
    if (! aFTPDirListingConv)
        return NS_ERROR_NULL_POINTER;

    *aFTPDirListingConv = new nsFTPDirListingConv();
    if (! *aFTPDirListingConv)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aFTPDirListingConv);
    return (*aFTPDirListingConv)->Init();
}

