/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *                 Christopher A. Aillon <christopher@aillon.com>
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

#include "nsIndexedToHTML.h"
#include "nsNetUtil.h"
#include "nsIStringStream.h"
#include "nsIFileURL.h"
#include "nsEscape.h"
#include "nsIDirIndex.h"
#include "prtime.h"
#include "nsDateTimeFormatCID.h"
#include "nsCRT.h"

NS_IMPL_THREADSAFE_ISUPPORTS4(nsIndexedToHTML,
                              nsIDirIndexListener,
                              nsIStreamConverter,
                              nsIRequestObserver,
                              nsIStreamListener)

static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);

NS_METHOD
nsIndexedToHTML::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult) {
    nsresult rv;
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
    
    nsIndexedToHTML* _s = new nsIndexedToHTML();
    if (_s == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    
    rv = _s->QueryInterface(aIID, aResult);
    return rv;
}

nsresult
nsIndexedToHTML::Init(nsIStreamListener* aListener) {
    nsresult rv = NS_OK;

    mListener = aListener;

    mDateTime = do_CreateInstance(kDateTimeFormatCID, &rv);

    nsCOMPtr<nsIStringBundleService> sbs =
        do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = sbs->CreateBundle("chrome://necko/locale/necko.properties",
                           getter_AddRefs(mBundle));

    mRowCount = 0;

    return rv;
}

NS_IMETHODIMP
nsIndexedToHTML::Convert(nsIInputStream* aFromStream,
                         const PRUnichar* aFromType,
                         const PRUnichar* aToType,
                         nsISupports* aCtxt,
                         nsIInputStream** res) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsIndexedToHTML::AsyncConvertData(const PRUnichar *aFromType,
                                  const PRUnichar *aToType,
                                  nsIStreamListener *aListener,
                                  nsISupports *aCtxt) {
    return Init(aListener);
}

static NS_NAMED_LITERAL_STRING(tableHeading,"<table>\n");

NS_IMETHODIMP
nsIndexedToHTML::OnStartRequest(nsIRequest* request, nsISupports *aContext) {
    nsresult rv;

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    nsCOMPtr<nsIURI> uri;
    rv = channel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    mParser = do_CreateInstance("@mozilla.org/dirIndexParser;1",&rv);
    if (NS_FAILED(rv)) return rv;

    rv = mParser->SetListener(this);
    if (NS_FAILED(rv)) return rv;
    
    rv = mParser->OnStartRequest(request, aContext);
    if (NS_FAILED(rv)) return rv;

    nsCAutoString baseUri,titleUri;
    rv = uri->GetAsciiSpec(baseUri);
    if (NS_FAILED(rv)) return rv;
    titleUri = baseUri;

    nsCString parentStr;

    // XXX - should be using the 300: line from the parser.
    // We can't guarantee that that comes before any entry, so we'd have to
    // buffer, and do other painful stuff.
    // I'll deal with this when I make the changes to handle welcome messages
    // The .. stuff should also come from the lower level protocols, but that
    // would muck up the XUL display
    // - bbaetz

    // ftp urls don't always end in a /
    PRBool isScheme = PR_FALSE;
    if (NS_SUCCEEDED(uri->SchemeIs("ftp", &isScheme)) && isScheme) {
        // strip out the password here, so it doesn't show in the page title
        // This is done by the 300: line generation in ftp, but we don't use
        // that - see above
        
        // if there was a password, strip it out
        nsCAutoString pw;
        rv = uri->GetPassword(pw);
        if (NS_FAILED(rv)) return rv;
        if (!pw.IsEmpty()) {
             nsCOMPtr<nsIURI> newUri;
             rv = uri->Clone(getter_AddRefs(newUri));
             if (NS_FAILED(rv)) return rv;
             rv = newUri->SetPassword(NS_LITERAL_CSTRING(""));
             if (NS_FAILED(rv)) return rv;
             rv = newUri->GetAsciiSpec(titleUri);
             if (NS_FAILED(rv)) return rv;
             if (titleUri.Last() != '/')
                 titleUri.Append('/');
        }

        if (baseUri.Last() != '/')
            baseUri.Append('/');

        nsCString::const_iterator start, finish;
        baseUri.BeginReading(start);
        baseUri.EndReading(finish);
        finish.advance(-2); // don't count the last /
        
        // No RFindChar
        while(finish != start && *finish != '/')
            --finish;

        if (Distance(start, finish) > (sizeof("ftp://") - 1)) {
            ++finish; // include the end '/'
            parentStr = Substring(start, finish);
        }

    } else if (NS_SUCCEEDED(uri->SchemeIs("file", &isScheme)) && isScheme) {
        nsCOMPtr<nsIFileURL> fileUrl = do_QueryInterface(uri);
        nsCOMPtr<nsIFile> file;
        rv = fileUrl->GetFile(getter_AddRefs(file));
        if (NS_FAILED(rv)) return rv;
        
        nsCAutoString url;
        rv = NS_GetURLSpecFromFile(file, url);
        if (NS_FAILED(rv)) return rv;
        baseUri.Assign(url);
        
        nsCOMPtr<nsIFile> parent;
        rv = file->GetParent(getter_AddRefs(parent));
        
        if (parent && NS_SUCCEEDED(rv)) {
            NS_GetURLSpecFromFile(parent, url);
            if (NS_FAILED(rv)) return rv;
            parentStr.Assign(url);
        }
    }

    nsString buffer;
    buffer.Assign(NS_LITERAL_STRING("<?xml version=\"1.0"));

    buffer.Append(NS_LITERAL_STRING("\" encoding=\"UTF-8") +
                  NS_LITERAL_STRING("\"?>\n") +
                  NS_LITERAL_STRING("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" ") +
                  NS_LITERAL_STRING("\"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n"));

    // Anything but a gopher url needs to end in a /,
    // otherwise we end up linking to file:///foo/dirfile

    char* spec = nsCRT::strdup(titleUri.get());

    buffer.Append(NS_LITERAL_STRING("<html xmlns=\"http://www.w3.org/1999/xhtml\">\n<head><title>"));

    nsXPIDLString title;
    nsString uniSpec; 
    nsXPIDLCString encoding;
    rv = mParser->GetEncoding(getter_Copies(encoding));
    if (NS_FAILED(rv)) return rv;

    if (!mTextToSubURI) {
        mTextToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;
    }

    PRUnichar   *unEscapeSpec = nsnull;
    rv = mTextToSubURI->UnEscapeAndConvert(encoding, spec, &unEscapeSpec);
    if (NS_FAILED(rv)) return rv;
    
    uniSpec.Adopt(unEscapeSpec);

    const PRUnichar* formatTitle[] = {
        uniSpec.get()
    };

    rv = mBundle->FormatStringFromName(NS_LITERAL_STRING("DirTitle").get(),
                                       formatTitle,
                                       sizeof(formatTitle)/sizeof(PRUnichar*),
                                       getter_Copies(title));
    if (NS_FAILED(rv)) return rv;
    buffer.Append(title);

    buffer.Append(NS_LITERAL_STRING("</title><base href=\""));
    buffer.Append(NS_ConvertASCIItoUCS2(baseUri));
    buffer.Append(NS_LITERAL_STRING("\"/>\n"));

    buffer.Append(NS_LITERAL_STRING("<style type=\"text/css\">\n") +
                  NS_LITERAL_STRING("img { border: 0; padding: 0 2px; vertical-align: text-bottom; }\n") +
                  NS_LITERAL_STRING("td  { font-family: monospace; padding: 2px 3px; text-align: right; vertical-align: bottom; }\n") +
                  NS_LITERAL_STRING("td:first-child { text-align: left; padding: 2px 10px 2px 3px; }\n") +
                  NS_LITERAL_STRING("table { border: 0; }\n") +
                  NS_LITERAL_STRING("a.symlink { font-style: italic; }\n") +
                  NS_LITERAL_STRING("</style>\n"));

    buffer.Append(NS_LITERAL_STRING("</head>\n<body>\n<h1>"));
    
    const PRUnichar* formatHeading[] = {
        uniSpec.get()
    };

    rv = mBundle->FormatStringFromName(NS_LITERAL_STRING("DirTitle").get(),
                                       formatHeading,
                                       sizeof(formatHeading)/sizeof(PRUnichar*),
                                       getter_Copies(title));
    if (NS_FAILED(rv)) return rv;
    
    buffer.Append(title);
    buffer.Append(NS_LITERAL_STRING("</h1>\n<hr/>") + tableHeading);

    //buffer.Append(NS_LITERAL_STRING("<tr><th>Name</th><th>Size</th><th>Last modified</th></tr>\n"));

    if (!parentStr.IsEmpty()) {
        nsXPIDLString parentText;
        rv = mBundle->GetStringFromName(NS_LITERAL_STRING("DirGoUp").get(),
                                        getter_Copies(parentText));
        if (NS_FAILED(rv)) return rv;
        
        buffer.Append(NS_LITERAL_STRING("<tr><td colspan=\"3\"><a href=\"") +
                      NS_ConvertASCIItoUCS2(parentStr) +
                      NS_LITERAL_STRING("\">") +
                      parentText +
                      NS_LITERAL_STRING("</a></td></tr>\n"));
    }

    // Push buffer to the listener now, so the initial HTML will not
    // be parsed in OnDataAvailable().

    rv = mListener->OnStartRequest(request, aContext);
    if (NS_FAILED(rv)) return rv;

    rv = FormatInputStream(request, aContext, buffer);

    return rv;
}

NS_IMETHODIMP
nsIndexedToHTML::OnStopRequest(nsIRequest* request, nsISupports *aContext,
                               nsresult aStatus) {
    nsresult rv = NS_OK;
    nsString buffer;
    buffer.Assign(NS_LITERAL_STRING("</table><hr/></body></html>\n"));

    rv = FormatInputStream(request, aContext, buffer);
    if (NS_FAILED(rv)) return rv;

    rv = mParser->OnStopRequest(request, aContext, aStatus);
    if (NS_FAILED(rv)) return rv;

    mParser = 0;
    
    return mListener->OnStopRequest(request, aContext, aStatus);
}

nsresult
nsIndexedToHTML::FormatInputStream(nsIRequest* aRequest, nsISupports *aContext, const nsAString &aBuffer) 
{
    nsresult rv = NS_OK;
    NS_ConvertUCS2toUTF8 buffer(aBuffer);
    nsCOMPtr<nsIInputStream> inputData;
    rv = NS_NewCStringInputStream(getter_AddRefs(inputData), buffer);
    if (NS_FAILED(rv)) return rv;
    rv = mListener->OnDataAvailable(aRequest, aContext,
                                    inputData, 0, buffer.Length());
    return (rv);
}

NS_IMETHODIMP
nsIndexedToHTML::OnDataAvailable(nsIRequest *aRequest,
                                 nsISupports *aCtxt,
                                 nsIInputStream* aInput,
                                 PRUint32 aOffset,
                                 PRUint32 aCount) {
    return mParser->OnDataAvailable(aRequest, aCtxt, aInput, aOffset, aCount);
}

// This defines the number of rows we are going to have per table
// splitting this up makes things faster, by helping layout
#define ROWS_PER_TABLE 250

NS_IMETHODIMP
nsIndexedToHTML::OnIndexAvailable(nsIRequest *aRequest,
                                  nsISupports *aCtxt,
                                  nsIDirIndex *aIndex) {
    if (!aIndex)
        return NS_ERROR_NULL_POINTER;

    nsString pushBuffer;
    pushBuffer.Append(NS_LITERAL_STRING("<tr>\n <td><a"));

    PRUint32 type;
    aIndex->GetType(&type);
    if (type == nsIDirIndex::TYPE_SYMLINK) {
        pushBuffer.Append(NS_LITERAL_STRING(" class=\"symlink\""));
    }

    pushBuffer.Append(NS_LITERAL_STRING(" href=\""));

    nsXPIDLCString loc;
    aIndex->GetLocation(getter_Copies(loc));
    
    pushBuffer.AppendWithConversion(loc);

    pushBuffer.Append(NS_LITERAL_STRING("\"><img src=\""));

    switch (type) {
    case nsIDirIndex::TYPE_DIRECTORY:
    case nsIDirIndex::TYPE_SYMLINK:
        pushBuffer.Append(NS_LITERAL_STRING("internal-gopher-menu\" alt=\"Directory: "));
        break;
    case nsIDirIndex::TYPE_FILE:
    case nsIDirIndex::TYPE_UNKNOWN:
        pushBuffer.Append(NS_LITERAL_STRING("internal-gopher-unknown\" alt=\"File: "));
        break;
    }
    pushBuffer.Append(NS_LITERAL_STRING("\"/>"));

    nsXPIDLString tmp;
    aIndex->GetDescription(getter_Copies(tmp));
    PRUnichar* escaped = nsEscapeHTML2(tmp.get(), tmp.Length());
    pushBuffer.Append(escaped);
    nsMemory::Free(escaped);

    pushBuffer.Append(NS_LITERAL_STRING("</a></td>\n <td>"));

    PRUint32 size;
    aIndex->GetSize(&size);
    
    if (size != PRUint32(-1) &&
        type != nsIDirIndex::TYPE_DIRECTORY &&
        type != nsIDirIndex::TYPE_SYMLINK) {
        nsAutoString  sizeString;
        FormatSizeString(size, sizeString);
        pushBuffer.Append(sizeString);
    }

    pushBuffer.Append(NS_LITERAL_STRING("</td>\n <td>"));

    PRTime t;
    aIndex->GetLastModified(&t);

    if (t == -1) {
        pushBuffer.Append(NS_LITERAL_STRING("</td>\n <td>"));
    } else {
        nsAutoString formatted;
        mDateTime->FormatPRTime(nsnull,
                                kDateFormatShort,
                                kTimeFormatNone,
                                t,
                                formatted);
        pushBuffer.Append(formatted);
        pushBuffer.Append(NS_LITERAL_STRING("</td>\n <td>"));
        mDateTime->FormatPRTime(nsnull,
                                kDateFormatNone,
                                kTimeFormatSeconds,
                                t,
                                formatted);
        pushBuffer.Append(formatted);
    }

    pushBuffer.Append(NS_LITERAL_STRING("</td>\n</tr>\n"));

    // Split this up to avoid slow layout performance with large tables
    // - bug 85381
    if (++mRowCount > ROWS_PER_TABLE) {
        pushBuffer.Append(NS_LITERAL_STRING("</table>\n") + tableHeading);
        mRowCount = 0;
    }
    
    return FormatInputStream(aRequest, aCtxt, pushBuffer);
}

void nsIndexedToHTML::FormatSizeString(PRUint32 inSize, nsString& outSizeString)
{
    outSizeString.Truncate();
    if (inSize > 0) {
        // round up to the nearest Kilobyte
        PRUint32  upperSize = (inSize + 1023) / 1024;
        outSizeString.AppendInt(upperSize);
        outSizeString.Append(NS_LITERAL_STRING(" KB"));
    }
}

nsIndexedToHTML::nsIndexedToHTML() {
    NS_INIT_ISUPPORTS();
}

nsIndexedToHTML::~nsIndexedToHTML() {
}
