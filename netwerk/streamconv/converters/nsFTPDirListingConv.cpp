/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFTPDirListingConv.h"
#include "nsMemory.h"
#include "plstr.h"
#include "prlog.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsCOMPtr.h"
#include "nsEscape.h"
#include "nsNetUtil.h"
#include "nsStringStream.h"
#include "nsIComponentManager.h"
#include "nsDateTimeFormatCID.h"
#include "nsIStreamListener.h"
#include "nsCRT.h"
#include "nsMimeTypes.h"
#include "nsAutoPtr.h"

#include "ParseFTPList.h"

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
PRLogModuleInfo* gFTPDirListConvLog = nullptr;

#endif /* PR_LOGGING */

// nsISupports implementation
NS_IMPL_ISUPPORTS3(nsFTPDirListingConv,
                   nsIStreamConverter,
                   nsIStreamListener, 
                   nsIRequestObserver)


// nsIStreamConverter implementation
NS_IMETHODIMP
nsFTPDirListingConv::Convert(nsIInputStream *aFromStream,
                             const char *aFromType,
                             const char *aToType,
                             nsISupports *aCtxt, nsIInputStream **_retval) {
    return NS_ERROR_NOT_IMPLEMENTED;
}


// Stream converter service calls this to initialize the actual stream converter (us).
NS_IMETHODIMP
nsFTPDirListingConv::AsyncConvertData(const char *aFromType, const char *aToType,
                                      nsIStreamListener *aListener, nsISupports *aCtxt) {
    NS_ASSERTION(aListener && aFromType && aToType, "null pointer passed into FTP dir listing converter");

    // hook up our final listener. this guy gets the various On*() calls we want to throw
    // at him.
    mFinalListener = aListener;
    NS_ADDREF(mFinalListener);

    PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, 
        ("nsFTPDirListingConv::AsyncConvertData() converting FROM raw, TO application/http-index-format\n"));

    return NS_OK;
}


// nsIStreamListener implementation
NS_IMETHODIMP
nsFTPDirListingConv::OnDataAvailable(nsIRequest* request, nsISupports *ctxt,
                                  nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count) {
    NS_ASSERTION(request, "FTP dir listing stream converter needs a request");
    
    nsresult rv;

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    PRUint32 read, streamLen;

    rv = inStr->Available(&streamLen);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoArrayPtr<char> buffer(new char[streamLen + 1]);
    NS_ENSURE_TRUE(buffer, NS_ERROR_OUT_OF_MEMORY);

    rv = inStr->Read(buffer, streamLen, &read);
    NS_ENSURE_SUCCESS(rv, rv);

    // the dir listings are ascii text, null terminate this sucker.
    buffer[streamLen] = '\0';

    PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, ("nsFTPDirListingConv::OnData(request = %x, ctxt = %x, inStr = %x, sourceOffset = %d, count = %d)\n", request, ctxt, inStr, sourceOffset, count));

    if (!mBuffer.IsEmpty()) {
        // we have data left over from a previous OnDataAvailable() call.
        // combine the buffers so we don't lose any data.
        mBuffer.Append(buffer);

        buffer = new char[mBuffer.Length()+1];
        NS_ENSURE_TRUE(buffer, NS_ERROR_OUT_OF_MEMORY);

        strncpy(buffer, mBuffer.get(), mBuffer.Length()+1);
        mBuffer.Truncate();
    }

#ifndef DEBUG_dougt
    PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, ("::OnData() received the following %d bytes...\n\n%s\n\n", streamLen, buffer.get()) );
#else
    printf("::OnData() received the following %d bytes...\n\n%s\n\n", streamLen, buffer);
#endif // DEBUG_dougt

    nsCAutoString indexFormat;
    if (!mSentHeading) {
        // build up the 300: line
        nsCOMPtr<nsIURI> uri;
        rv = channel->GetURI(getter_AddRefs(uri));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = GetHeaders(indexFormat, uri);
        NS_ENSURE_SUCCESS(rv, rv);

        mSentHeading = true;
    }

    char *line = buffer;
    line = DigestBufferLines(line, indexFormat);

#ifndef DEBUG_dougt
    PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, ("::OnData() sending the following %d bytes...\n\n%s\n\n", 
        indexFormat.Length(), indexFormat.get()) );
#else
    char *unescData = ToNewCString(indexFormat);
    NS_ENSURE_TRUE(unescData, NS_ERROR_OUT_OF_MEMORY);
    
    nsUnescape(unescData);
    printf("::OnData() sending the following %d bytes...\n\n%s\n\n", indexFormat.Length(), unescData);
    nsMemory::Free(unescData);
#endif // DEBUG_dougt

    // if there's any data left over, buffer it.
    if (line && *line) {
        mBuffer.Append(line);
        PR_LOG(gFTPDirListConvLog, PR_LOG_DEBUG, ("::OnData() buffering the following %d bytes...\n\n%s\n\n",
            PL_strlen(line), line) );
    }

    // send the converted data out.
    nsCOMPtr<nsIInputStream> inputData;

    rv = NS_NewCStringInputStream(getter_AddRefs(inputData), indexFormat);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mFinalListener->OnDataAvailable(request, ctxt, inputData, 0, indexFormat.Length());

    return rv;
}


// nsIRequestObserver implementation
NS_IMETHODIMP
nsFTPDirListingConv::OnStartRequest(nsIRequest* request, nsISupports *ctxt) {
    // we don't care about start. move along... but start masqeurading 
    // as the http-index channel now.
    return mFinalListener->OnStartRequest(request, ctxt);
}

NS_IMETHODIMP
nsFTPDirListingConv::OnStopRequest(nsIRequest* request, nsISupports *ctxt,
                                   nsresult aStatus) {
    // we don't care about stop. move along...

    return mFinalListener->OnStopRequest(request, ctxt, aStatus);
}


// nsFTPDirListingConv methods
nsFTPDirListingConv::nsFTPDirListingConv() {
    mFinalListener      = nullptr;
    mSentHeading        = false;
}

nsFTPDirListingConv::~nsFTPDirListingConv() {
    NS_IF_RELEASE(mFinalListener);
}

nsresult
nsFTPDirListingConv::Init() {
#if defined(PR_LOGGING)
    //
    // Initialize the global PRLogModule for FTP Protocol logging 
    // if necessary...
    //
    if (nullptr == gFTPDirListConvLog) {
        gFTPDirListConvLog = PR_NewLogModule("nsFTPDirListingConv");
    }
#endif /* PR_LOGGING */

    return NS_OK;
}

nsresult
nsFTPDirListingConv::GetHeaders(nsACString& headers,
                                nsIURI* uri)
{
    nsresult rv = NS_OK;
    // build up 300 line
    headers.AppendLiteral("300: ");

    // Bug 111117 - don't print the password
    nsCAutoString pw;
    nsCAutoString spec;
    uri->GetPassword(pw);
    if (!pw.IsEmpty()) {
         rv = uri->SetPassword(EmptyCString());
         if (NS_FAILED(rv)) return rv;
         rv = uri->GetAsciiSpec(spec);
         if (NS_FAILED(rv)) return rv;
         headers.Append(spec);
         rv = uri->SetPassword(pw);
         if (NS_FAILED(rv)) return rv;
    } else {
        rv = uri->GetAsciiSpec(spec);
        if (NS_FAILED(rv)) return rv;
        
        headers.Append(spec);
    }
    headers.Append(char(nsCRT::LF));
    // END 300:

    // build up the column heading; 200:
    headers.AppendLiteral("200: filename content-length last-modified file-type\n");
    // END 200:
    return rv;
}

char *
nsFTPDirListingConv::DigestBufferLines(char *aBuffer, nsCString &aString) {
    char *line = aBuffer;
    char *eol;
    bool cr = false;

    list_state state;
    state.magic = 0;

    // while we have new lines, parse 'em into application/http-index-format.
    while ( line && (eol = PL_strchr(line, nsCRT::LF)) ) {
        // yank any carriage returns too.
        if (eol > line && *(eol-1) == nsCRT::CR) {
            eol--;
            *eol = '\0';
            cr = true;
        } else {
            *eol = '\0';
            cr = false;
        }

        list_result result;

        int type = ParseFTPList(line, &state, &result );

        // if it is other than a directory, file, or link -OR- if it is a 
        // directory named . or .., skip over this line.
        if ((type != 'd' && type != 'f' && type != 'l') || 
            (result.fe_type == 'd' && result.fe_fname[0] == '.' &&
            (result.fe_fnlen == 1 || (result.fe_fnlen == 2 &&  result.fe_fname[1] == '.'))) )
        {
            if (cr)
                line = eol+2;
            else
                line = eol+1;
            
            continue;
        }

        // blast the index entry into the indexFormat buffer as a 201: line.
        aString.AppendLiteral("201: ");
        // FILENAME

        // parsers for styles 'U' and 'W' handle sequence " -> " themself
	if (state.lstyle != 'U' && state.lstyle != 'W') {
            const char* offset = strstr(result.fe_fname, " -> ");
            if (offset) {
                result.fe_fnlen = offset - result.fe_fname;
            }
        }

        nsCAutoString buf;
        aString.Append('\"');
        aString.Append(NS_EscapeURL(Substring(result.fe_fname, 
                                              result.fe_fname+result.fe_fnlen),
                                    esc_Minimal|esc_OnlyASCII|esc_Forced,buf));
        aString.AppendLiteral("\" ");
 
        // CONTENT LENGTH
        
        if (type != 'd') 
        {
            for (int i = 0; i < int(sizeof(result.fe_size)); ++i)
            {
                if (result.fe_size[i] != '\0')
                    aString.Append((const char*)&result.fe_size[i], 1);
            }
            
            aString.Append(' ');
        }
        else
            aString.AppendLiteral("0 ");


        // MODIFIED DATE
        char buffer[256] = "";
        // Note: The below is the RFC822/1123 format, as required by
        // the application/http-index-format specs
        // viewers of such a format can then reformat this into the
        // current locale (or anything else they choose)
        PR_FormatTimeUSEnglish(buffer, sizeof(buffer),
                               "%a, %d %b %Y %H:%M:%S", &result.fe_time );

        char *escapedDate = nsEscape(buffer, url_Path);
        aString.Append(escapedDate);
        nsMemory::Free(escapedDate);
        aString.Append(' ');

        // ENTRY TYPE
        if (type == 'd')
            aString.AppendLiteral("DIRECTORY");
        else if (type == 'l')
            aString.AppendLiteral("SYMBOLIC-LINK");
        else
            aString.AppendLiteral("FILE");
        
        aString.Append(' ');

        aString.Append(char(nsCRT::LF)); // complete this line
        // END 201:

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
    NS_PRECONDITION(aFTPDirListingConv != nullptr, "null ptr");
    if (! aFTPDirListingConv)
        return NS_ERROR_NULL_POINTER;

    *aFTPDirListingConv = new nsFTPDirListingConv();
    if (! *aFTPDirListingConv)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aFTPDirListingConv);
    return (*aFTPDirListingConv)->Init();
}
