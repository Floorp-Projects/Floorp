/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bradley Baetz <bbaetz@cs.mcgill.ca>
 *   Christopher A. Aillon <christopher@aillon.com>
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

#include "nsIndexedToHTML.h"
#include "nsNetUtil.h"
#include "nsIStringStream.h"
#include "nsIFileURL.h"
#include "nsEscape.h"
#include "nsIDirIndex.h"
#include "prtime.h"
#include "nsDateTimeFormatCID.h"
#include "nsURLHelper.h"
#include "nsCRT.h"
#include "nsIPlatformCharset.h"
#include "nsInt64.h"

NS_IMPL_THREADSAFE_ISUPPORTS4(nsIndexedToHTML,
                              nsIDirIndexListener,
                              nsIStreamConverter,
                              nsIRequestObserver,
                              nsIStreamListener)

static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);

static void ConvertNonAsciiToNCR(const nsAString& in, nsAFlatString& out)
{
  nsAString::const_iterator start, end;

  in.BeginReading(start);
  in.EndReading(end);

  out.Truncate();

  while (start != end) {
    if (*start < 128) {
      out.Append(*start++);
    } else {
      out.Append(NS_LITERAL_STRING("&#x"));
      nsAutoString hex;
      hex.AppendInt(*start++, 16);
      out.Append(hex);
      out.Append((PRUnichar)';');
    }
  }
}

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
    mExpectAbsLoc = PR_FALSE;

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

NS_IMETHODIMP
nsIndexedToHTML::OnStartRequest(nsIRequest* request, nsISupports *aContext) {
    nsresult rv;

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    nsCOMPtr<nsIURI> uri;
    rv = channel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    channel->SetContentType(NS_LITERAL_CSTRING("text/html"));

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

    PRBool isScheme = PR_FALSE;
    if (NS_SUCCEEDED(uri->SchemeIs("ftp", &isScheme)) && isScheme) {

        // ftp urls don't always end in a /
        // make sure they do
        // but look out for /%2F as path
        nsCAutoString path;
        rv = uri->GetPath(path);
        if (NS_FAILED(rv)) return rv;
        if (baseUri.Last() != '/' && !path.EqualsIgnoreCase("/%2F")) {
            baseUri.Append('/');
            path.Append('/');
            uri->SetPath(path);
        }

        // strip out the password here, so it doesn't show in the page title
        // This is done by the 300: line generation in ftp, but we don't use
        // that - see above
        
        nsCAutoString pw;
        rv = uri->GetPassword(pw);
        if (NS_FAILED(rv)) return rv;
        if (!pw.IsEmpty()) {
             nsCOMPtr<nsIURI> newUri;
             rv = uri->Clone(getter_AddRefs(newUri));
             if (NS_FAILED(rv)) return rv;
             rv = newUri->SetPassword(EmptyCString());
             if (NS_FAILED(rv)) return rv;
             rv = newUri->GetAsciiSpec(titleUri);
             if (NS_FAILED(rv)) return rv;
             if (titleUri.Last() != '/' && !path.EqualsIgnoreCase("/%2F"))
                 titleUri.Append('/');
        }

        if (!path.Equals("//") && !path.EqualsIgnoreCase("/%2F")) {
            rv = uri->Resolve(NS_LITERAL_CSTRING(".."),parentStr);
            if (NS_FAILED(rv)) return rv;
        }
    } else if (NS_SUCCEEDED(uri->SchemeIs("file", &isScheme)) && isScheme) {
        nsCOMPtr<nsIFileURL> fileUrl = do_QueryInterface(uri);
        nsCOMPtr<nsIFile> file;
        rv = fileUrl->GetFile(getter_AddRefs(file));
        if (NS_FAILED(rv)) return rv;
        nsCOMPtr<nsILocalFile> lfile = do_QueryInterface(file, &rv);
        if (NS_FAILED(rv)) return rv;
        lfile->SetFollowLinks(PR_TRUE);
        
        nsCAutoString url;
        rv = net_GetURLSpecFromFile(file, url);
        if (NS_FAILED(rv)) return rv;
        baseUri.Assign(url);
        
        nsCOMPtr<nsIFile> parent;
        rv = file->GetParent(getter_AddRefs(parent));
        
        if (parent && NS_SUCCEEDED(rv)) {
            net_GetURLSpecFromFile(parent, url);
            if (NS_FAILED(rv)) return rv;
            parentStr.Assign(url);
        }

        // reset parser's charset to platform's default if this is file url
        nsCOMPtr<nsIPlatformCharset> platformCharset(do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv));
        NS_ENSURE_SUCCESS(rv, rv);
        nsCAutoString charset;
        rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName, charset);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = mParser->SetEncoding(charset.get());
        NS_ENSURE_SUCCESS(rv, rv);

    } else if (NS_SUCCEEDED(uri->SchemeIs("gopher", &isScheme)) && isScheme) {
        mExpectAbsLoc = PR_TRUE;
    }
    else {
        // default behavior for other protocols is to assume the channel's
        // URL references a directory ending in '/' -- fixup if necessary.
        nsCAutoString path;
        rv = uri->GetPath(path);
        if (NS_FAILED(rv)) return rv;
        if (baseUri.Last() != '/') {
            baseUri.Append('/');
            path.Append('/');
            uri->SetPath(path);
        }
        if (!path.Equals("/")) {
            rv = uri->Resolve(NS_LITERAL_CSTRING(".."), parentStr);
            if (NS_FAILED(rv)) return rv;
        }
    }

    nsString buffer;
    buffer.Assign(NS_LITERAL_STRING("<?xml version=\"1.0\" encoding=\""));
    
    // Get the encoding from the parser
    // XXX - this won't work for any encoding set via a 301: line in the
    // format - this output stuff would need to move to OnDataAvailable
    // for that. However, currently only file:// sends that, and file's HTML
    // index mode isn't enabled yet (bug 102812)

    nsXPIDLCString encoding;
    rv = mParser->GetEncoding(getter_Copies(encoding));
    if (NS_FAILED(rv)) return rv;

    AppendASCIItoUTF16(encoding, buffer);

    buffer.Append(NS_LITERAL_STRING("\"?>\n") +
                  NS_LITERAL_STRING("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" ") +
                  NS_LITERAL_STRING("\"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n"));

    // Anything but a gopher url needs to end in a /,
    // otherwise we end up linking to file:///foo/dirfile

    buffer.Append(NS_LITERAL_STRING("<html xmlns=\"http://www.w3.org/1999/xhtml\">\n<head><title>"));

    nsXPIDLString title;

    if (!mTextToSubURI) {
        mTextToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;
    }

    nsXPIDLString unEscapeSpec;
    rv = mTextToSubURI->UnEscapeAndConvert(encoding, titleUri.get(),
                                           getter_Copies(unEscapeSpec));
    if (NS_FAILED(rv)) return rv;
    
    nsXPIDLString htmlEscSpec;
    htmlEscSpec.Adopt(nsEscapeHTML2(unEscapeSpec.get(),
                                    unEscapeSpec.Length()));

    const PRUnichar* formatTitle[] = {
        htmlEscSpec.get()
    };

    rv = mBundle->FormatStringFromName(NS_LITERAL_STRING("DirTitle").get(),
                                       formatTitle,
                                       sizeof(formatTitle)/sizeof(PRUnichar*),
                                       getter_Copies(title));
    if (NS_FAILED(rv)) return rv;

    // we want to convert string bundle to NCR
    // to ensure they're shown in any charsets
    nsAutoString strNCR;
    ConvertNonAsciiToNCR(title, strNCR);
    buffer.Append(strNCR);

    buffer.Append(NS_LITERAL_STRING("</title><base href=\""));    
    AppendASCIItoUTF16(baseUri, buffer);
    buffer.Append(NS_LITERAL_STRING("\"/>\n"));

    buffer.Append(NS_LITERAL_STRING("<style type=\"text/css\">\n") +
                  NS_LITERAL_STRING("img { border: 0; padding: 0 2px; vertical-align: text-bottom; }\n") +
                  NS_LITERAL_STRING("td  { font-family: monospace; padding: 2px 3px; text-align: right; vertical-align: bottom; white-space: pre; }\n") +
                  NS_LITERAL_STRING("td:first-child { text-align: left; padding: 2px 10px 2px 3px; }\n") +
                  NS_LITERAL_STRING("table { border: 0; }\n") +
                  NS_LITERAL_STRING("a.symlink { font-style: italic; }\n") +
                  NS_LITERAL_STRING("</style>\n"));

    buffer.Append(NS_LITERAL_STRING("</head>\n<body>\n<h1>"));
    
    const PRUnichar* formatHeading[] = {
        htmlEscSpec.get()
    };

    rv = mBundle->FormatStringFromName(NS_LITERAL_STRING("DirTitle").get(),
                                       formatHeading,
                                       sizeof(formatHeading)/sizeof(PRUnichar*),
                                       getter_Copies(title));
    if (NS_FAILED(rv)) return rv;
    
    ConvertNonAsciiToNCR(title, strNCR);
    buffer.Append(strNCR);
    buffer.Append(NS_LITERAL_STRING("</h1>\n<hr/><table>\n"));

    //buffer.Append(NS_LITERAL_STRING("<tr><th>Name</th><th>Size</th><th>Last modified</th></tr>\n"));

    if (!parentStr.IsEmpty()) {
        nsXPIDLString parentText;
        rv = mBundle->GetStringFromName(NS_LITERAL_STRING("DirGoUp").get(),
                                        getter_Copies(parentText));
        if (NS_FAILED(rv)) return rv;
        
        ConvertNonAsciiToNCR(parentText, strNCR);
        buffer.Append(NS_LITERAL_STRING("<tr><td colspan=\"3\"><a href=\""));
        AppendASCIItoUTF16(parentStr, buffer);
        buffer.Append(NS_LITERAL_STRING("\">") +
                      strNCR +
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

    // set up unicode encoder
    if (!mUnicodeEncoder) {
      nsXPIDLCString encoding;
      rv = mParser->GetEncoding(getter_Copies(encoding));
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsICharsetConverterManager> charsetConverterManager;
        charsetConverterManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
        rv = charsetConverterManager->GetUnicodeEncoder(encoding.get(), 
                                                          getter_AddRefs(mUnicodeEncoder));
        if (NS_SUCCEEDED(rv))
            rv = mUnicodeEncoder->SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace, 
                                                       nsnull, (PRUnichar)'?');
      }
    }

    // convert the data with unicode encoder
    char *buffer = nsnull;
    PRInt32 dstLength;
    if (NS_SUCCEEDED(rv)) {
      PRInt32 unicharLength = aBuffer.Length();
      rv = mUnicodeEncoder->GetMaxLength(PromiseFlatString(aBuffer).get(), 
                                         unicharLength, &dstLength);
      if (NS_SUCCEEDED(rv)) {
        buffer = (char *) nsMemory::Alloc(dstLength);
        NS_ENSURE_TRUE(buffer, NS_ERROR_OUT_OF_MEMORY);

        rv = mUnicodeEncoder->Convert(PromiseFlatString(aBuffer).get(), &unicharLength, 
                                      buffer, &dstLength);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 finLen = 0;
          rv = mUnicodeEncoder->Finish(buffer + dstLength, &finLen);
          if (NS_SUCCEEDED(rv))
            dstLength += finLen;
        }
      }
    }

    // if conversion error then fallback to UTF-8
    if (NS_FAILED(rv)) {
      rv = NS_OK;
      if (buffer) {
        nsMemory::Free(buffer);
        buffer = nsnull;
      }
    }

    nsCOMPtr<nsIInputStream> inputData;
    if (buffer) {
      rv = NS_NewCStringInputStream(getter_AddRefs(inputData), Substring(buffer, buffer + dstLength));
      nsMemory::Free(buffer);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = mListener->OnDataAvailable(aRequest, aContext,
                                      inputData, 0, dstLength);
    }
    else {
      NS_ConvertUCS2toUTF8 utf8Buffer(aBuffer);
      rv = NS_NewCStringInputStream(getter_AddRefs(inputData), utf8Buffer);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = mListener->OnDataAvailable(aRequest, aContext,
                                      inputData, 0, utf8Buffer.Length());
    }
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
    nsresult rv;
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

    if (!mTextToSubURI) {
        mTextToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;
    }
    
    nsXPIDLCString encoding;
    rv = mParser->GetEncoding(getter_Copies(encoding));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLString unEscapeSpec;
    rv = mTextToSubURI->UnEscapeAndConvert(encoding, loc,
                                           getter_Copies(unEscapeSpec));
    if (NS_FAILED(rv)) return rv;
  
    // need to escape links
    nsCAutoString escapeBuf;

    NS_ConvertUCS2toUTF8 utf8UnEscapeSpec(unEscapeSpec);

    // now minimally re-escape the location...
    PRUint32 escFlags;
    // for some protocols, like gopher, we expect the location to be absolute.
    // if so, and if the location indeed appears to be a valid URI, then go
    // ahead and treat it like one.
    if (mExpectAbsLoc &&
        NS_SUCCEEDED(net_ExtractURLScheme(utf8UnEscapeSpec, nsnull, nsnull, nsnull))) {
        // escape as absolute 
        escFlags = esc_Forced | esc_OnlyASCII | esc_AlwaysCopy | esc_Minimal;
    }
    else {
        // escape as relative
        escFlags = esc_Forced | esc_OnlyASCII | esc_AlwaysCopy | esc_FileBaseName | esc_Colon;
    }
    NS_EscapeURL(utf8UnEscapeSpec.get(), utf8UnEscapeSpec.Length(), escFlags, escapeBuf);
  
    AppendUTF8toUTF16(escapeBuf, pushBuffer);
    
    pushBuffer.Append(NS_LITERAL_STRING("\"><img src=\""));

    switch (type) {
    case nsIDirIndex::TYPE_DIRECTORY:
    case nsIDirIndex::TYPE_SYMLINK:
        pushBuffer.Append(NS_LITERAL_STRING("resource://gre/res/html/gopher-menu.gif\" alt=\"Directory: "));
        break;
    case nsIDirIndex::TYPE_FILE:
    case nsIDirIndex::TYPE_UNKNOWN:
        pushBuffer.Append(NS_LITERAL_STRING("resource://gre/res/html/gopher-unknown.gif\" alt=\"File: "));
        break;
    }
    pushBuffer.Append(NS_LITERAL_STRING("\"/>"));

    nsXPIDLString tmp;
    aIndex->GetDescription(getter_Copies(tmp));
    PRUnichar* escaped = nsEscapeHTML2(tmp.get(), tmp.Length());
    pushBuffer.Append(escaped);
    nsMemory::Free(escaped);

    pushBuffer.Append(NS_LITERAL_STRING("</a></td>\n <td>"));

    PRInt64 size;
    aIndex->GetSize(&size);
    
    if (LL_NE(size, LL_INIT(0, -1)) &&
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
        nsAutoString strNCR;    // use NCR to show date in any doc charset
        mDateTime->FormatPRTime(nsnull,
                                kDateFormatShort,
                                kTimeFormatNone,
                                t,
                                formatted);
        ConvertNonAsciiToNCR(formatted, strNCR);
        pushBuffer.Append(strNCR);
        pushBuffer.Append(NS_LITERAL_STRING("</td>\n <td>"));
        mDateTime->FormatPRTime(nsnull,
                                kDateFormatNone,
                                kTimeFormatSeconds,
                                t,
                                formatted);
        ConvertNonAsciiToNCR(formatted, strNCR);
        pushBuffer.Append(strNCR);
    }

    pushBuffer.Append(NS_LITERAL_STRING("</td>\n</tr>\n"));

    // Split this up to avoid slow layout performance with large tables
    // - bug 85381
    if (++mRowCount > ROWS_PER_TABLE) {
        pushBuffer.Append(NS_LITERAL_STRING("</table>\n<table>\n"));
        mRowCount = 0;
    }
    
    return FormatInputStream(aRequest, aCtxt, pushBuffer);
}

NS_IMETHODIMP
nsIndexedToHTML::OnInformationAvailable(nsIRequest *aRequest,
                                        nsISupports *aCtxt,
                                        const nsAString& aInfo) {
    nsAutoString pushBuffer;
    PRUnichar* escaped = nsEscapeHTML2(PromiseFlatString(aInfo).get());
    if (!escaped)
        return NS_ERROR_OUT_OF_MEMORY;
    pushBuffer.Append(NS_LITERAL_STRING("<tr>\n <td>"));
    pushBuffer.Append(escaped);
    nsMemory::Free(escaped);
    pushBuffer.Append(NS_LITERAL_STRING("</td>\n <td></td>\n <td></td>\n <td></td>\n</tr>\n"));
    
    // Split this up to avoid slow layout performance with large tables
    // - bug 85381
    if (++mRowCount > ROWS_PER_TABLE) {
        pushBuffer.Append(NS_LITERAL_STRING("</table>\n<table>\n"));
        mRowCount = 0;
    }   
    return FormatInputStream(aRequest, aCtxt, pushBuffer);
}

void nsIndexedToHTML::FormatSizeString(PRInt64 inSize, nsString& outSizeString)
{
    nsInt64 size(inSize);
    outSizeString.Truncate();
    if (size > nsInt64(0)) {
        // round up to the nearest Kilobyte
        PRInt64  upperSize = (size + nsInt64(1023)) / nsInt64(1024);
        outSizeString.AppendInt(upperSize);
        outSizeString.Append(NS_LITERAL_STRING(" KB"));
    }
}

nsIndexedToHTML::nsIndexedToHTML() {
}

nsIndexedToHTML::~nsIndexedToHTML() {
}
