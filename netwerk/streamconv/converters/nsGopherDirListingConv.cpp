/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the gopher-directory to http-index code.
 *
 * The Initial Developer of the Original Code is
 * Bradley Baetz.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bradley Baetz <bbaetz@student.usyd.edu.au>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* This code is based on the ftp directory translation code */

#include "plstr.h"
#include "nsMemory.h"
#include "nsCRT.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsEscape.h"
#include "nsIStreamListener.h"
#include "nsIStreamConverter.h"
#include "nsIStringStream.h"
#include "nsIRequestObserver.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"

#include "nsGopherDirListingConv.h"

// nsISupports implementation
NS_IMPL_THREADSAFE_ISUPPORTS3(nsGopherDirListingConv,
                              nsIStreamConverter,
                              nsIStreamListener,
                              nsIRequestObserver)

// nsIStreamConverter implementation

#define CONV_BUF_SIZE (4*1024)

NS_IMETHODIMP
nsGopherDirListingConv::Convert(nsIInputStream *aFromStream,
                                const char *aFromType,
                                const char *aToType,
                                nsISupports *aCtxt, nsIInputStream **_retval) {
    
    nsresult rv;

    char buffer[CONV_BUF_SIZE] = {0};
    nsFixedCString aBuffer(buffer, CONV_BUF_SIZE, 0);
    nsCAutoString convertedData;

    NS_ASSERTION(aCtxt, "Gopher dir conversion needs the context");
    // build up the 300: line
    nsCAutoString spec;
    mUri = do_QueryInterface(aCtxt, &rv);
    if (NS_FAILED(rv)) return rv;
    
    rv = mUri->GetAsciiSpec(spec);
    if (NS_FAILED(rv)) return rv;

    convertedData.Append("300: ");
    convertedData.Append(spec);
    convertedData.Append(char(nsCRT::LF));
    // END 300:
    
    //Column headings
    // We should also send the content-type, probably. The stuff in
    // nsGopherChannel::GetContentType should then be moved out - 
    // maybe into an nsIGopherURI interface/class?

    // Should also possibly use different hosts as a symlink, but the directory
    // viewer stuff doesn't support SYM-FILE or SYM-DIRECTORY
    convertedData.Append("200: description filename file-type\n");

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
    
    // send the converted data out.
    return NS_NewCStringInputStream(_retval, convertedData);
}

// Stream converter service calls this to initialize the actual
// stream converter (us).
NS_IMETHODIMP
nsGopherDirListingConv::AsyncConvertData(const char *aFromType,
                                         const char *aToType,
                                         nsIStreamListener *aListener,
                                         nsISupports *aCtxt) {
    NS_ASSERTION(aListener && aFromType && aToType,
                 "null pointer passed into gopher dir listing converter");
    nsresult rv;

    // hook up our final listener. this guy gets the various On*() calls
    // we want to throw at him.
    mFinalListener = aListener;
    NS_ADDREF(mFinalListener);
    
    // we need our own channel that represents the content-type of the
    // converted data.
    NS_ASSERTION(aCtxt, "Gopher dir listing needs a context (the uri)");
    mUri = do_QueryInterface(aCtxt,&rv);
    if (NS_FAILED(rv)) return rv;

    // XXX this seems really wrong!!
    rv = NS_NewInputStreamChannel(&mPartChannel,
                                  mUri,
                                  nsnull,
                                  NS_LITERAL_CSTRING(APPLICATION_HTTP_INDEX_FORMAT),
                                  EmptyCString());
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

// nsIStreamListener implementation
NS_IMETHODIMP
nsGopherDirListingConv::OnDataAvailable(nsIRequest *request,
                                        nsISupports *ctxt,
                                        nsIInputStream *inStr,
                                        PRUint32 sourceOffset,
                                        PRUint32 count) {
    nsresult rv;

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

    if (!mBuffer.IsEmpty()) {
        // we have data left over from a previous OnDataAvailable() call.
        // combine the buffers so we don't lose any data.
        mBuffer.Append(buffer);
        nsMemory::Free(buffer);
        buffer = ToNewCString(mBuffer);
        mBuffer.Truncate();
    }

    if (!mSentHeading) {
        // build up the 300: line
        nsCAutoString spec;
        rv = mUri->GetAsciiSpec(spec);
        if (NS_FAILED(rv)) return rv;

        //printf("spec is %s\n",spec.get());
        
        indexFormat.Append("300: ");
        indexFormat.Append(spec);
        indexFormat.Append(char(nsCRT::LF));
        // END 300:

        // build up the column heading; 200:
        indexFormat.Append("200: description filename file-type\n");
        // END 200:
        
        mSentHeading = PR_TRUE;
    }
    char *line = buffer;
    line = DigestBufferLines(line, indexFormat);
    // if there's any data left over, buffer it.
    if (line && *line) {
        mBuffer.Append(line);
    }
    
    nsMemory::Free(buffer);
    
    // send the converted data out.
    nsCOMPtr<nsIInputStream> inputData;
    
    rv = NS_NewCStringInputStream(getter_AddRefs(inputData), indexFormat);
    if (NS_FAILED(rv)) return rv;
    
    rv = mFinalListener->OnDataAvailable(mPartChannel, ctxt, inputData,
                                         0, indexFormat.Length());
    if (NS_FAILED(rv)) return rv;
    
    return NS_OK;
}

// nsIRequestObserver implementation
NS_IMETHODIMP
nsGopherDirListingConv::OnStartRequest(nsIRequest *request, nsISupports *ctxt) {
    // we don't care about start. move along... but start masqeurading 
    // as the http-index channel now.
    return mFinalListener->OnStartRequest(mPartChannel, ctxt);
}

NS_IMETHODIMP
nsGopherDirListingConv::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                                      nsresult aStatus) {
    // we don't care about stop. move along...
    nsCOMPtr<nsILoadGroup> loadgroup;
    nsresult rv = mPartChannel->GetLoadGroup(getter_AddRefs(loadgroup));
    if (NS_FAILED(rv)) return rv;
    if (loadgroup)
        (void)loadgroup->RemoveRequest(mPartChannel, nsnull, aStatus);

    return mFinalListener->OnStopRequest(mPartChannel, ctxt, aStatus);
}

// nsGopherDirListingConv methods
nsGopherDirListingConv::nsGopherDirListingConv() {
    mFinalListener      = nsnull;
    mPartChannel        = nsnull;
    mSentHeading        = PR_FALSE;
}

nsGopherDirListingConv::~nsGopherDirListingConv() {
    NS_IF_RELEASE(mFinalListener);
    NS_IF_RELEASE(mPartChannel);
}

nsresult
nsGopherDirListingConv::Init() {
    return NS_OK;
}

char*
nsGopherDirListingConv::DigestBufferLines(char* aBuffer, nsCAutoString& aString) {
    char *line = aBuffer;
    char *eol;
    PRBool cr = PR_FALSE;

    // while we have new lines, parse 'em into application/http-index-format.
    while (line && (eol = PL_strchr(line, nsCRT::LF)) ) {
        // yank any carriage returns too.
        if (eol > line && *(eol-1) == nsCRT::CR) {
            eol--;
            *eol = '\0';
            cr = PR_TRUE;
        } else {
            *eol = '\0';
            cr = PR_FALSE;
        }

        if (line[0]=='.' && line[1]=='\0') {
            if (cr)
                line = eol+2;
            else
                line = eol+1;
            continue;
        }

        char type;
        nsCAutoString desc, selector, host;
        PRInt32 port = GOPHER_PORT;

        type = line[0];
        line++;
        char* tabPos = PL_strchr(line,'\t');

        /* Get the description */
        if (tabPos) {
            /* if the description is not empty */
            if (tabPos != line) {
                char* descStr = PL_strndup(line,tabPos-line);
                char* escName = nsEscape(descStr,url_Path);
                desc = escName;
                nsMemory::Free(escName);
                nsMemory::Free(descStr);
            } else {
                desc = "%20";
            }
            line = tabPos+1;
            tabPos = PL_strchr(line,'\t');
        }

        /* Get selector */
        if (tabPos) {
            char* sel = PL_strndup(line,tabPos-line);
            char* escName = nsEscape(sel,url_Path);
            selector = escName;
            nsMemory::Free(escName);
            nsMemory::Free(sel);
            line = tabPos+1;
            tabPos = PL_strchr(line,'\t');
        }

        /* Host and Port - put together because there is
           no tab after the port */
        if (tabPos) {
            host = nsCString(line,tabPos-line);
            line = tabPos+1;
            tabPos = PL_strchr(line,'\t');
            if (tabPos==NULL)
                tabPos = PL_strchr(line,'\0');

            /* Port */
            nsCAutoString portStr(line,tabPos-line);
            port = atol(portStr.get());
            line = tabPos+1;
        }
        
        // Now create the url
        nsCAutoString filename;
        if (type != '8' && type != 'T') {
            filename.Assign("gopher://");
            filename.Append(host);
            if (port != GOPHER_PORT) {
                filename.Append(':');
                filename.AppendInt(port);
            }
            filename.Append('/');
            filename.Append(type);
            filename.Append(selector);
        } else {
            // construct telnet/tn3270 url.
            // Moz doesn't support these, so this is UNTESTED!!!!!
            // (I do get the correct error message though)
            if (type == '8')
                // telnet
                filename.Assign("telnet://");
            else
                // tn3270
                filename.Assign("tn3270://");
            if (!selector.IsEmpty()) {
                filename.Append(selector);
                filename.Append('@');
            }
            filename.Append(host);
            if (port != 23) { // telnet port
                filename.Append(':');
                filename.AppendInt(port);
            }
        }

        if (tabPos) {
            /* Don't display error messages or informative messages
               because they could be selected, and they'll be sorted
               out of order.
               If FTP displays .messages/READMEs ever, then I could use the
               same method to display these
            */
            if (type != '3' && type != 'i') {
                aString.Append("201: ");
                aString.Append(desc);
                aString.Append(' ');
                aString.Append(filename);
                aString.Append(' ');
                if (type == '1')
                    aString.Append("DIRECTORY");
                else
                    aString.Append("FILE");
                aString.Append(char(nsCRT::LF));
            } else if(type == 'i'){
                aString.Append("101: ");
                aString.Append(desc);
                aString.Append(char(nsCRT::LF));
            }
        } else {
            NS_WARNING("Error parsing gopher directory response.\n");
            //printf("Got: %s\n",filename.get());
        }
        
        if (cr)
            line = eol+2;
        else
            line = eol+1;
    }
    return line;
}

nsresult
NS_NewGopherDirListingConv(nsGopherDirListingConv** aGopherDirListingConv)
{
    NS_PRECONDITION(aGopherDirListingConv, "null ptr");
    if (! aGopherDirListingConv)
        return NS_ERROR_NULL_POINTER;

    *aGopherDirListingConv = new nsGopherDirListingConv();
    if (! *aGopherDirListingConv)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aGopherDirListingConv);
    return (*aGopherDirListingConv)->Init();
}
