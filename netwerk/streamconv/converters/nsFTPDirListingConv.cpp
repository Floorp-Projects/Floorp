/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsFTPDirListingConv.h"
#include "nsIAllocator.h"
#include "plstr.h"
#include "prlog.h"
#include "nsIStringStream.h"
#include "nsIHTTPChannel.h"
#include "nsIAtom.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"
#include "nsCOMPtr.h"
#include "nsEscape.h"
#include "nsIIOService.h"
#include "nsIStringStream.h"
#include "nsILocaleService.h"
#include "nsIComponentManager.h"
#include "nsDateTimeFormatCID.h"
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


PRBool is_charAlpha(char chr) {
    if ( ( ((chr >= 'a') && (chr <= 'z')) || ((chr >= 'A') && (chr <= 'Z')) ) ) 
        return PR_TRUE;
    return PR_FALSE;
}

#define NET_IS_SPACE(x) ((((unsigned int) (x)) > 0x7f) ? 0 : (x == ' ' || x == '\r' || x == '\n' || x == '\t') )

#define IS_DIGITx(x) ( (PRBool) ( x >= '0' && x <= '9' ) ? PR_TRUE : PR_FALSE )


// nsISupports implementation
NS_IMPL_ISUPPORTS2(nsFTPDirListingConv, nsIStreamConverter, nsIStreamListener);


// nsIStreamConverter implementation

// No syncronous conversion at this time.
NS_IMETHODIMP
nsFTPDirListingConv::Convert(nsIInputStream *aFromStream,
                          const PRUnichar *aFromType,
                          const PRUnichar *aToType,
                          nsISupports *aCtxt, nsIInputStream **_retval) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

// Stream converter service calls this to initialize the actual stream converter (us).
NS_IMETHODIMP
nsFTPDirListingConv::AsyncConvertData(const PRUnichar *aFromType, const PRUnichar *aToType,
                                   nsIStreamListener *aListener, nsISupports *aCtxt) {
    NS_ASSERTION(aListener && aFromType && aToType, "null pointer passed into FTP dir listing converter");
    nsresult rv;

    // hook up our final listener. this guy gets the various On*() calls we want to throw
    // at him.
    //
    mFinalListener = aListener;
    NS_ADDREF(mFinalListener);

    // set our internal state to reflect the server type
    nsCString fromMIMEString(aFromType);
    const char *from = fromMIMEString.GetBuffer();
    NS_ASSERTION(from, "nsCString/PRUnichar acceptance failed.");

    from = PL_strstr(from, "/ftp-dir-");
    if (!from) return NS_ERROR_FAILURE;
    from += 9;
    fromMIMEString = from;

    if (-1 != fromMIMEString.Find("unix")) {
        mServerType = UNIX;
    } else if (-1 != fromMIMEString.Find("nt")) {
        mServerType = NT;
    } else if (-1 != fromMIMEString.Find("dcts")) {
        mServerType = DCTS;
    } else if (-1 != fromMIMEString.Find("ncsa")) {
        mServerType = NCSA;
    } else if (-1 != fromMIMEString.Find("peter_lewis")) {
        mServerType = PETER_LEWIS;
    } else if (-1 != fromMIMEString.Find("machten")) {
        mServerType = MACHTEN;
    } else if (-1 != fromMIMEString.Find("cms")) {
        mServerType = CMS;
    } else if (-1 != fromMIMEString.Find("tcpc")) {
        mServerType = TCPC;
    } else if (-1 != fromMIMEString.Find("vms")) {
        mServerType = VMS;
    } else {
        mServerType = GENERIC;
    }


    // we need our own channel that represents the content-type of the
    // converted data.
    NS_ASSERTION(aCtxt, "FTP dir listing needs a context (the uri)");
    nsIURI *uri;
    rv = aCtxt->QueryInterface(NS_GET_IID(nsIURI), (void**)&uri);
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = serv->NewInputStreamChannel(uri, "application/http-index-format", nsnull, &mPartChannel);
    NS_RELEASE(uri);
    if (NS_FAILED(rv)) return rv;

    PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, 
        ("nsFTPDirListingConv::AsyncConvertData() converting FROM raw %s, TO application/http-index-format\n", fromMIMEString.GetBuffer()));

    return NS_OK;
}


// nsIStreamListener implementation
NS_IMETHODIMP
nsFTPDirListingConv::OnDataAvailable(nsIChannel *channel, nsISupports *ctxt,
                                  nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count) {
    NS_ASSERTION(channel, "FTP dir listing stream converter needs a channel");
    nsresult rv;
    PRUint32 read, streamLen;
    nsCString indexFormat;
    indexFormat.SetCapacity(72); // quick guess 

    rv = inStr->Available(&streamLen);
    if (NS_FAILED(rv)) return rv;

    char *buffer = (char*)nsAllocator::Alloc(streamLen + 1);
    rv = inStr->Read(buffer, streamLen, &read);
    if (NS_FAILED(rv)) return rv;

    // the dir listings are ascii text, null terminate this sucker.
    buffer[streamLen] = '\0';

    PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, ("nsFTPDirListingConv::OnData(channel = %x, ctxt = %x, inStr = %x, sourceOffset = %d, count = %d)\n", channel, ctxt, inStr, sourceOffset, count));

    if (!mBuffer.IsEmpty()) {
        // we have data left over from a previous OnDataAvailable() call.
        // combine the buffers so we don't lose any data.
        mBuffer.Append(buffer);
        nsAllocator::Free(buffer);
        buffer = mBuffer.ToNewCString();
        mBuffer.Truncate();
    }

    PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, ("::OnData() received the following %d bytes...\n\n%s\n\n", streamLen, buffer) );

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
        indexFormat.Append('\n');
        nsAllocator::Free(spec);
        // END 300:

        // build up the column heading; 200:
        indexFormat.Append("200: filename content-length last-modified file-type\n");
        // END 200:

        mSentHeading = PR_TRUE;
    }

    char *line = buffer;
    char *eol;
    PRBool cr = PR_FALSE;

    // while we have new lines, parse 'em into application/http-index-format.
    while ( line && (eol = PL_strchr(line, '\n')) ) {
        // yank any carriage returns too.
        if (eol > line && *(eol-1) == '\r') {
            eol--;
            *eol = '\0';
            cr = PR_TRUE;
        } else {
            *eol = '\0';
            cr = PR_FALSE;
        }
        indexEntry *thisEntry = nsnull;
        NS_NEWXPCOM(thisEntry, indexEntry);
        if (!thisEntry) return NS_ERROR_OUT_OF_MEMORY;

        // XXX we need to handle comments in the raw stream.

        // special case windows servers who masquerade as unix servers
        if (NT == mServerType && !NET_IS_SPACE(line[8]))
            mServerType = UNIX;


        char *escName = nsnull;
        switch (mServerType) {

        case UNIX:
        case PETER_LEWIS:
        case MACHTEN:
        {
            // don't bother w/ these lines.
            if(!PL_strncmp(line, "total ", 6)  
					|| (PL_strstr(line, "Permission denied") != NULL)
                    || 	(PL_strstr(line, "not available") != NULL)) {
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
                for (i = len - 1; (i > 3) && (!NET_IS_SPACE(line[i])
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

			/* add a trailing slash to all directories */
			if(thisEntry->mType == Dir)
                thisEntry->mName.Append('/');

            break; // END UNIX, PETER_LEWIS, MACHTEN
        }

        case NCSA:
        case TCPC:
        {
            escName = nsEscape(line, url_Path);
            thisEntry->mName = escName;
            nsAllocator::Free(escName);

            if (thisEntry->mName.Last() == '/')
                thisEntry->mType = Dir;

            break; // END NCSA, TCPC
        }

        case CMS:
        {
            escName = nsEscape(line, url_Path);
            thisEntry->mName = escName;
            nsAllocator::Free(escName);
            break; // END CMS
        }
        case VMS:
        {
            /* Interpret and edit LIST output from VMS server */
            /* and convert information lines to zero length.  */
            rv = ParseVMSLine(line, thisEntry);
            if (NS_FAILED(rv)) {
                NS_DELETEXPCOM(thisEntry);
                if (cr)
                    line = eol+2;
                else
                    line = eol+1;
                continue;
            }

            /** Trim off VMS directory extensions **/
            //len = thisEntry->mName.Length();
//            if ((len > 4) && !PL_strcmp(&entry_info->filename[len-4], ".dir"))
//              {
//                entry_info->filename[len-4] = '\0';
//				entry_info->special_type = NET_DIRECTORY;
//                remove_size=TRUE; /* size is not useful */
//				/* add trailing slash to directories
//				 */
//				StrAllocCat(entry_info->filename, "/");
//              }
            break; // END VMS
        }
        case NT:
        {
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
                    thisEntry->mContentLen = atol(size.GetBuffer());
                }

                ConvertDOSDate(date, thisEntry->mMDTM);

                escName = nsEscape(name, url_Path);
                thisEntry->mName = escName;
            } else {
                escName = nsEscape(line, url_Path);
                thisEntry->mName = escName;
			}
            nsAllocator::Free(escName);
            break; // END NT
        }
        default:
        {
            escName = nsEscape(line, url_Path);
            thisEntry->mName = escName;
            nsAllocator::Free(escName);
            break; // END default (catches GENERIC, DCTS)
        }

        } // end switch (mServerType)

        // blast the index entry into the indexFormat buffer as a 201: line.
        indexFormat.Append("201: ");
        // FILENAME
        indexFormat.Append(thisEntry->mName);
        indexFormat.Append(' ');

        // CONTENT LENGTH
        if (!thisEntry->mSupressSize) {
            indexFormat.Append(thisEntry->mContentLen);
        } else {
            indexFormat.Append('0');
        }
        indexFormat.Append(' ');


        // CONTENT TYPE (not very useful for ftp listings)
        // XXX this field is currently meaningless for FTP
        //indexFormat.Append(thisEntry->mContentType);
        //indexFormat.Append('\t');


        // MODIFIED DATE
        nsString lDate;
        nsStr::Initialize(lDate, eOneByte);
        rv = mDateTimeFormat->FormatPRTime(mLocale, kDateFormatShort, kTimeFormatNoSeconds, thisEntry->mMDTM, lDate);
        if (NS_FAILED(rv)) {
            NS_DELETEXPCOM(thisEntry);
            return rv;
        }

        char *escapedDate = nsEscape(lDate.GetBuffer(), url_Path);

        indexFormat.Append(escapedDate);
        nsAllocator::Free(escapedDate);
        indexFormat.Append(' ');


        // ENTRY TYPE
        switch (thisEntry->mType) {
        case Dir:
            indexFormat.Append("Directory");
            break;
        case Link:
            indexFormat.Append("Sym-Link");
            break;
        default:
            indexFormat.Append("File");
        }
        indexFormat.Append(' ');

        indexFormat.Append('\n'); // complete this line
        // END 201:

        NS_DELETEXPCOM(thisEntry);

        if (cr)
            line = eol+2;
        else
            line = eol+1;
    } // end while(eol)

    PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, ("::OnData() sending the following %d bytes...\n\n%s\n\n", 
        indexFormat.Length(), indexFormat.GetBuffer()) );

    // if there's any data left over, buffer it.
    if (line && *line) {
        mBuffer.Append(line);
        PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, ("::OnData() buffering the following %d bytes...\n\n%s\n\n",
            PL_strlen(line), line) );
    }

    nsAllocator::Free(buffer);

    // send the converted data out.
    nsIInputStream *inputData = nsnull;
    nsISupports *inputDataSup = nsnull;

    rv = NS_NewStringInputStream(&inputDataSup, indexFormat);
    if (NS_FAILED(rv)) return rv;

    rv = inputDataSup->QueryInterface(NS_GET_IID(nsIInputStream), (void**)&inputData);
    NS_RELEASE(inputDataSup);
    if (NS_FAILED(rv)) return rv;

    rv = mFinalListener->OnDataAvailable(mPartChannel, ctxt, inputData, 0, indexFormat.Length());
    NS_RELEASE(inputData);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


// nsIStreamObserver implementation
NS_IMETHODIMP
nsFTPDirListingConv::OnStartRequest(nsIChannel *channel, nsISupports *ctxt) {
    // we don't care about start. move along... but start masqeurading 
    // as the http-index channel now.
    return mFinalListener->OnStartRequest(mPartChannel, ctxt);
}

NS_IMETHODIMP
nsFTPDirListingConv::OnStopRequest(nsIChannel *channel, nsISupports *ctxt,
                                nsresult status, const PRUnichar *errorMsg) {
    // we don't care about stop. move along...
    return mFinalListener->OnStopRequest(mPartChannel, ctxt, status, errorMsg);
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
    NS_WITH_SERVICE(nsILocaleService, localeSvc, kLocaleServiceCID, &rv);
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

    PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, ("nsFTPDirListingConv::MonthNumber(month = %s) ", month) );

    switch (*month) {
    case 'f': case 'F':
        rv = 1; break;
    case 'm': case 'M':
        if (c1 == 'a' || c1 == 'A') 
            if (c2 == 'r' || c2 == 'R')
                rv = 2;
            else if (c2 == 'y' || c2 == 'Y')
                rv = 4;
        break;
    case 'a': case 'A':
        if (c1 == 'p' || c1 == 'P')
            rv = 3;
        else if (c1 == 'u' || c1 == 'U')
            rv = 7;
        break;
    case 'j': case 'J':
        if (c1 == 'u' || c1 == 'U') {
            if (c2 == 'n' || c2 == 'N')
                rv = 5;
            else if (c2 == 'l' || c2 == 'L')
                rv = 6;
        } else if (c1 == 'a' || c1 == 'A') {
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

    PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, ("returning %d\n", rv) );

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
    if (!is_charAlpha(*aCStr++) || !is_charAlpha(*aCStr++) || !is_charAlpha(*aCStr++))
        return PR_FALSE;

    /* space */
    if (*aCStr != ' ')
        return PR_FALSE;
    aCStr++;

    /* space or digit */
    if ((*aCStr != ' ') && !IS_DIGITx(*aCStr))
        return PR_FALSE;
    aCStr++;

    /* digit */
    if (!IS_DIGITx(*aCStr))
        return PR_FALSE;
    aCStr++;

    /* space */
    if (*aCStr != ' ')
        return PR_FALSE;
    aCStr++;

    /* space or digit */
    if ((*aCStr != ' ') && !IS_DIGITx(*aCStr))
        return PR_FALSE;
    aCStr++;

    /* digit */
    if (!IS_DIGITx(*aCStr))
        return PR_FALSE;
    aCStr++;

    /* colon or digit */
    if ((*aCStr != ':') && !IS_DIGITx(*aCStr))
        return PR_FALSE;
    aCStr++;

    /* digit */
    if (!IS_DIGITx(*aCStr))
        return PR_FALSE;
    aCStr++;

    /* space or digit */
    if ((*aCStr != ' ') && !IS_DIGITx(*aCStr))
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
nsFTPDirListingConv::ConvertUNIXDate(char *aCStr, PRTime& outDate) {

    PRExplodedTime curTime;

    PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &curTime);

    char *bcol = aCStr;         /* Column begin */
    char *ecol;                   /* Column end */

    // defaults
    curTime.tm_usec = 0;
    curTime.tm_sec  = 0;
 
    // MONTH
    char tmpChar = bcol[3];
    bcol[3] = '\0';

    if ((curTime.tm_month = MonthNumber(bcol)) < 0)
    	return PR_FALSE;
    bcol[3] = tmpChar;

    // DAY
    ecol = &bcol[3];                        
    while (*ecol++ == ' ') ;            /* Spool to other side of day */
    while (*ecol++ != ' ') ;
    *--ecol = '\0';

    PRInt32 error;
    nsCAutoString day(bcol);
    curTime.tm_mday = day.ToInteger(&error, 10);
    //curTime.tm_wday     = 0;
    //curTime.tm_yday = 0;

    // YEAR
    bcol = ++ecol;
    if ((ecol = PL_strchr(bcol, ':')) == NULL) {
        nsCAutoString intStr(bcol);
        curTime.tm_year = intStr.ToInteger(&error, 10);
    	curTime.tm_min  = 0;
        curTime.tm_hour = 0;
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

        curTime.tm_year = curTime.tm_year;
    	//if (mktime(time_info) > curtime)
        //	--time_info->tm_year;
      }

    // set the out param
    outDate = PR_ImplodeTime(&curTime);
    return PR_TRUE;
}

PRBool
nsFTPDirListingConv::ConvertVMSDate(char *aCStr, PRTime& outDate) {

    PRExplodedTime curTime;

    PR_ExplodeTime(PR_Now(), PR_GMTParameters, &curTime);

    char *col;

    if ((col = strtok(aCStr, "-")) == NULL)
    	return PR_FALSE;

    // DAY
    nsCAutoString intStr(col);
    PRInt32 error;
    curTime.tm_mday = intStr.ToInteger(&error, 10);

    // XXX the following two sets may be skewing the Imploded time
    // XXX we're not supposed to set wday and yday (see prtime.h)
    curTime.tm_wday = 0;
    curTime.tm_yday = 0;

    if ((col = strtok(nsnull, "-")) == nsnull || (curTime.tm_month = MonthNumber(col)) < 0)
    	return PR_FALSE;

    // YEAR
    if ((col = strtok(NULL, " ")) == NULL)               
    	return PR_FALSE;


    intStr = col;
    curTime.tm_year = intStr.ToInteger(&error, 10);

    // HOUR
    if ((col = strtok(NULL, ":")) == NULL)               
    	return PR_FALSE;
    
    intStr = col;
    curTime.tm_hour = intStr.ToInteger(&error, 10);

    // MINS
    if ((col = strtok(NULL, " ")) == NULL)
    	return PR_FALSE;

    intStr = col;
    curTime.tm_min = intStr.ToInteger(&error, 10);
    curTime.tm_sec = 0;

    outDate = PR_ImplodeTime(&curTime);
    return PR_TRUE;
}

PRBool
nsFTPDirListingConv::ConvertDOSDate(char *aCStr, PRTime& outDate) {

    PRExplodedTime curTime;

    PR_ExplodeTime(PR_Now(), PR_GMTParameters, &curTime);

    curTime.tm_month      = (aCStr[1]-'0')-1;

    curTime.tm_mday     = (((aCStr[3]-'0')*10) + aCStr[4]-'0');
    curTime.tm_year     = (((aCStr[6]-'0')*10) + aCStr[7]-'0'); 
    curTime.tm_hour     = (((aCStr[10]-'0')*10) + aCStr[11]-'0'); 

	if(aCStr[15] == 'P')
        curTime.tm_hour += 12;

    curTime.tm_min = (((aCStr[13]-'0')*10) + aCStr[14]-'0');

    curTime.tm_wday = 0;
    curTime.tm_yday = 0;
    curTime.tm_sec = 0;

    outDate = PR_ImplodeTime(&curTime);
    return PR_TRUE;
}

nsresult
nsFTPDirListingConv::ParseLSLine(char *aLine, indexEntry *aEntry) {

    PRInt32 base=1;
	PRInt32 size_num=0;
	char save_char;
	char *ptr, *escName;

    for (ptr = &aLine[PL_strlen(aLine) - 1];
            (ptr > aLine+13) && (!NET_IS_SPACE(*ptr) || !IsLSDate(ptr-12)); ptr--)
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
			if(NET_IS_SPACE(*ptr)) {
				*ptr = '\0';
				break;
            }
        escName = nsEscape(aLine, url_Path);
        aEntry->mName = escName;
        nsAllocator::Free(escName);
	
		return NS_OK;
    }

    escName = nsEscape(ptr+1, url_Path);
    aEntry->mName = escName;
    nsAllocator::Free(escName);

	// parse size
	if(ptr > aLine+15) {
        ptr -= 14;
        while (IS_DIGITx(*ptr)) {
            size_num += ((PRInt32) (*ptr - '0')) * base;
            base *= 10;
            ptr--;
        }
    
        aEntry->mContentLen = size_num;
    }
    return NS_OK;
}

nsresult
nsFTPDirListingConv::ParseVMSLine(char *aLine, indexEntry *aEntry) {
        int i, j;
        int32 ialloc;
        char *cp, *cpd, *cps, date[64], *sp = " ", *escName;
        static char ThisYear[5];
        static PRBool HaveYear = PR_FALSE; 

        //  Get rid of blank lines, and information lines.
        //  Valid lines have the ';' version number token.
        if (!PL_strlen(aLine) || (cp=PL_strchr(aLine, ';')) == NULL) 
		  {
           // entry_info->display = FALSE;
            return NS_ERROR_FAILURE;
          }

        // Cut out file or directory name at VMS version number. 
    	*cp++ ='\0';
        escName = nsEscape(aLine, url_Path);
        aEntry->mName = escName;
        nsAllocator::Free(escName);

        // Cast VMS file and directory names to lowercase. 
        aEntry->mName.ToLowerCase();

        /** Uppercase terminal .z's or _z's. **/
    	//if ((--i > 2) && entry_info->filename[i] == 'z' &&
        //     	(entry_info->filename[i-1] == '.' || entry_info->filename[i-1] == '_'))
        //    entry_info->filename[i] = 'Z';

        /** Convert any tabs in rest of line to spaces. **/
    	cps = cp-1;
        while ((cps=PL_strchr(cps+1, '\t')) != NULL)
            *cps = ' ';

        /** Collapse serial spaces. **/
        i = 0; j = 1;
    	cps = cp;
        while (cps[j] != '\0') 
		  {
            if (cps[i] == ' ' && cps[j] == ' ')
                j++;
            else
                cps[++i] = cps[j++];
		  }
        cps[++i] = '\0';

        /* Save the current year.       
    	 * It could be wrong on New Year's Eve.
		 */
    	if (!HaveYear) 
	 	  {
            PRExplodedTime curTime;
            PR_ExplodeTime(PR_Now(), PR_GMTParameters, &curTime);
            nsCAutoString year(curTime.tm_year);
            const char *yearCStr = year.GetBuffer();
            for (PRInt8 x = 0; x < 4; x++)
                ThisYear[x] = yearCStr[x];
        	HaveYear = PR_TRUE;
    	  }

        // get the date. 
        if ((cpd=PL_strchr(cp, '-')) != NULL &&
                PL_strlen(cpd) > 9 && IS_DIGITx(*(cpd-1)) &&
                is_charAlpha(*(cpd+1)) && *(cpd+4) == '-') 
		  {

            /* Month 
			 */
            *(cpd+4) = '\0';

            // lowercase these two chars
            PRInt8 shift = 'A' - 'a';
            if ( !(*(cpd+2) <= 'z') && !(*(cpd+2) >= 'a') )
                *(cpd+2) = *(cpd+2) - shift;

            if ( !(*(cpd+3) <= 'z') && !(*(cpd+2) >= 'a') )
                *(cpd+3) = *(cpd+3) - shift;

            sprintf(date, "%.32s ", cpd+1);
            *(cpd+4) = '-';

            /** Day **/
            *cpd = '\0';
            if (IS_DIGITx(*(cpd-2)))
                sprintf(date+4, "%.32s ", cpd-2);
            else
                sprintf(date+4, " %.32s ", cpd-1);
            *cpd = '-';

            /** Time or Year **/
            if (!PL_strncmp(ThisYear, cpd+5, 4) && PL_strlen(cpd) > 15 && *(cpd+12) == ':')
			  {
                *(cpd+15) = '\0';
                sprintf(date+7, "%.32s", cpd+10);
                *(cpd+15) = ' ';
              } 
			else 
			  {
                *(cpd+9) = '\0';
                sprintf(date+7, " %.32s", cpd+5);
                *(cpd+9) = ' ';
              }

            ConvertVMSDate(date, aEntry->mMDTM);
          }

        /* get the size 
		 */
        if ((cpd=PL_strchr(cp, '/')) != NULL) 
		  {
            /* Appears be in used/allocated format */
            cps = cpd;
            while (IS_DIGITx(*(cps-1)))
                cps--;
            if (cps < cpd)
                *cpd = '\0';
            aEntry->mContentLen = atol(cps);
            cps = cpd+1;
            while (IS_DIGITx(*cps))
                cps++;
            *cps = '\0';
            ialloc = atoi(cpd+1);
            /* Check if used is in blocks or bytes */
            if (aEntry->mContentLen <= ialloc)
                aEntry->mContentLen *= 512;
          }
        else if ((cps=strtok(cp, sp)) != NULL) 
		  {
            /* We just initialized on the version number 
             * Now let's find a lone, size number   
			 */
            while ((cps=strtok(NULL, sp)) != NULL) 
			  {
                 cpd = cps;
                 while (IS_DIGITx(*cpd))
                     cpd++;
                 if (*cpd == '\0') 
				   {
                     /* Assume it's blocks */
                     aEntry->mContentLen = atol(cps) * 512;
                     break;
                   }
               }
          }
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// Factory
////////////////////////////////////////////////////////////////////////
FTPDirListingFactory::FTPDirListingFactory(const nsCID &aClass, 
                                   const char* className,
                                   const char* progID)
    : mClassID(aClass), mClassName(className), mProgID(progID)
{
    NS_INIT_ISUPPORTS();
}

FTPDirListingFactory::~FTPDirListingFactory()
{
    NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMPL_ISUPPORTS(FTPDirListingFactory, NS_GET_IID(nsIFactory));

NS_IMETHODIMP
FTPDirListingFactory::CreateInstance(nsISupports *aOuter,
                                 const nsIID &aIID,
                                 void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    *aResult = nsnull;

    nsresult rv = NS_OK;

    nsISupports *inst = nsnull;
    if (mClassID.Equals(kFTPDirListingConverterCID)) {
        nsFTPDirListingConv *conv = new nsFTPDirListingConv();
        if (!conv) return NS_ERROR_OUT_OF_MEMORY;
        rv = conv->Init();
        if (NS_FAILED(rv)) return rv;

        rv = conv->QueryInterface(NS_GET_IID(nsISupports), (void**)&inst);
        if (NS_FAILED(rv)) return rv;
    }
    else {
        return NS_ERROR_NO_INTERFACE;
    }

    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(inst);
    *aResult = inst;
    NS_RELEASE(inst);
    return rv;
}

nsresult
FTPDirListingFactory::LockFactory(PRBool aLock){
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// Factory END
////////////////////////////////////////////////////////////////////////

