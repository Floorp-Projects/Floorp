/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson           <waterson@netscape.com>
 *   Robert John Churchill    <rjc@netscape.com>
 *   Pierre Phaneuf           <pp@ludusdesign.com>
 *   Bradley Baetz            <bbaetz@cs.mcgill.ca>
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

/* This parsing code originally lived in xpfe/components/directory/ - bbaetz */

#include "prprf.h"

#include "nsDirIndexParser.h"
#include "nsReadableUtils.h"
#include "nsDirIndex.h"
#include "nsEscape.h"
#include "nsIServiceManager.h"
#include "nsIInputStream.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsCRT.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefLocalizedString.h"

NS_IMPL_THREADSAFE_ISUPPORTS3(nsDirIndexParser,
                              nsIRequestObserver,
                              nsIStreamListener,
                              nsIDirIndexParser)

nsDirIndexParser::nsDirIndexParser() {
}

nsresult
nsDirIndexParser::Init() {
  mLineStart = 0;
  mHasDescription = PR_FALSE;
  mFormat = nsnull;

  // get default charset to be used for directory listings (fallback to
  // ISO-8859-1 if pref is unavailable).
  NS_NAMED_LITERAL_CSTRING(kFallbackEncoding, "ISO-8859-1");
  nsXPIDLString defCharset;
  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefs) {
    nsCOMPtr<nsIPrefLocalizedString> prefVal;
    prefs->GetComplexValue("intl.charset.default",
                           NS_GET_IID(nsIPrefLocalizedString),
                           getter_AddRefs(prefVal));
    if (prefVal)
      prefVal->ToString(getter_Copies(defCharset));
  }
  if (!defCharset.IsEmpty())
    LossyCopyUTF16toASCII(defCharset, mEncoding); // charset labels are always ASCII
  else
    mEncoding.Assign(kFallbackEncoding);
 
  nsresult rv;
  // XXX not threadsafe
  if (gRefCntParser++ == 0)
    rv = CallGetService(NS_ITEXTTOSUBURI_CONTRACTID, &gTextToSubURI);
  else
    rv = NS_OK;

  return rv;
}

nsDirIndexParser::~nsDirIndexParser() {
  delete[] mFormat;
  // XXX not threadsafe
  if (--gRefCntParser == 0) {
    NS_IF_RELEASE(gTextToSubURI);
  }
}

NS_IMETHODIMP
nsDirIndexParser::SetListener(nsIDirIndexListener* aListener) {
  mListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndexParser::GetListener(nsIDirIndexListener** aListener) {
  NS_IF_ADDREF(*aListener = mListener.get());
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndexParser::GetComment(char** aComment) {
  *aComment = ToNewCString(mComment);

  if (!*aComment)
    return NS_ERROR_OUT_OF_MEMORY;
  
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndexParser::SetEncoding(const char* aEncoding) {
  mEncoding.Assign(aEncoding);
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndexParser::GetEncoding(char** aEncoding) {
  *aEncoding = ToNewCString(mEncoding);

  if (!*aEncoding)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsDirIndexParser::OnStartRequest(nsIRequest* aRequest, nsISupports* aCtxt) {
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndexParser::OnStopRequest(nsIRequest *aRequest, nsISupports *aCtxt,
                                nsresult aStatusCode) {
  // Finish up
  if (mBuf.Length() > (PRUint32) mLineStart) {
    ProcessData(aRequest, aCtxt);
  }

  return NS_OK;
}

nsDirIndexParser::Field
nsDirIndexParser::gFieldTable[] = {
  { "Filename", FIELD_FILENAME },
  { "Description", FIELD_DESCRIPTION },
  { "Content-Length", FIELD_CONTENTLENGTH },
  { "Last-Modified", FIELD_LASTMODIFIED },
  { "Content-Type", FIELD_CONTENTTYPE },
  { "File-Type", FIELD_FILETYPE },
  { nsnull, FIELD_UNKNOWN }
};

nsrefcnt nsDirIndexParser::gRefCntParser = 0;
nsITextToSubURI *nsDirIndexParser::gTextToSubURI;

nsresult
nsDirIndexParser::ParseFormat(const char* aFormatStr) {
  // Parse a "200" format line, and remember the fields and their
  // ordering in mFormat. Multiple 200 lines stomp on each other.

  delete[] mFormat;

  // Lets find out how many elements we have.
  // easier to do this then realloc
  const char* pos = aFormatStr;
  int num = 0;
  do {
    while (*pos && nsCRT::IsAsciiSpace(PRUnichar(*pos)))
      ++pos;
    
    ++num;

    if (! *pos)
      break;

    while (*pos && !nsCRT::IsAsciiSpace(PRUnichar(*pos)))
      ++pos;

  } while (*pos);

  mFormat = new int[num+1];
  mFormat[num] = -1;
  
  int formatNum=0;
  do {
    while (*aFormatStr && nsCRT::IsAsciiSpace(PRUnichar(*aFormatStr)))
      ++aFormatStr;
    
    if (! *aFormatStr)
      break;

    nsCAutoString name;
    PRInt32     len = 0;
    while (aFormatStr[len] && !nsCRT::IsAsciiSpace(PRUnichar(aFormatStr[len])))
      ++len;
    name.SetCapacity(len + 1);
    name.Append(aFormatStr, len);
    aFormatStr += len;
    
    // Okay, we're gonna monkey with the nsStr. Bold!
    name.SetLength(nsUnescapeCount(name.BeginWriting()));

    // All tokens are case-insensitive - http://www.mozilla.org/projects/netlib/dirindexformat.html
    if (name.LowerCaseEqualsLiteral("description"))
      mHasDescription = PR_TRUE;
    
    for (Field* i = gFieldTable; i->mName; ++i) {
      if (name.EqualsIgnoreCase(i->mName)) {
        mFormat[formatNum] = i->mType;
        ++formatNum;
        break;
      }
    }

  } while (*aFormatStr);
  
  return NS_OK;
}

nsresult
nsDirIndexParser::ParseData(nsIDirIndex *aIdx, char* aDataStr) {
  // Parse a "201" data line, using the field ordering specified in
  // mFormat.

  if (!mFormat) {
    // Ignore if we haven't seen a format yet.
    return NS_OK;
  }

  nsresult rv = NS_OK;

  nsCAutoString filename;

  for (PRInt32 i = 0; mFormat[i] != -1; ++i) {
    // If we've exhausted the data before we run out of fields, just
    // bail.
    if (! *aDataStr)
      break;

    while (*aDataStr && nsCRT::IsAsciiSpace(*aDataStr))
      ++aDataStr;

    char    *value = aDataStr;

    if (*aDataStr == '"' || *aDataStr == '\'') {
      // it's a quoted string. snarf everything up to the next quote character
      const char quotechar = *(aDataStr++);
      ++value;
      while (*aDataStr && *aDataStr != quotechar)
        ++aDataStr;
      *aDataStr++ = '\0';

      if (! aDataStr) {
        NS_WARNING("quoted value not terminated");
      }
    } else {
      // it's unquoted. snarf until we see whitespace.
      value = aDataStr;
      while (*aDataStr && (!nsCRT::IsAsciiSpace(*aDataStr)))
        ++aDataStr;
      *aDataStr++ = '\0';
    }

    fieldType t = fieldType(mFormat[i]);
    switch (t) {
    case FIELD_FILENAME: {
      // don't unescape at this point, so that UnEscapeAndConvert() can
      filename = value;
      
      PRBool  success = PR_FALSE;
      
      nsAutoString entryuri;
      
      if (gTextToSubURI) {
        PRUnichar   *result = nsnull;
        if (NS_SUCCEEDED(rv = gTextToSubURI->UnEscapeAndConvert(mEncoding.get(), filename.get(),
                                                                &result)) && (result)) {
          if (*result) {
            aIdx->SetLocation(filename.get());
            if (!mHasDescription)
              aIdx->SetDescription(result);
            success = PR_TRUE;
          }
          NS_Free(result);
        } else {
          NS_WARNING("UnEscapeAndConvert error");
        }
      }
      
      if (success == PR_FALSE) {
        // if unsuccessfully at charset conversion, then
        // just fallback to unescape'ing in-place
        // XXX - this shouldn't be using UTF8, should it?
        // when can we fail to get the service, anyway? - bbaetz
        aIdx->SetLocation(filename.get());
        if (!mHasDescription) {
          aIdx->SetDescription(NS_ConvertUTF8toUTF16(value).get());
        }
      }
    }
      break;
    case FIELD_DESCRIPTION:
      nsUnescape(value);
      aIdx->SetDescription(NS_ConvertUTF8toUTF16(value).get());
      break;
    case FIELD_CONTENTLENGTH:
      {
        PRInt64 len;
        PRInt32 status = PR_sscanf(value, "%lld", &len);
        if (status == 1)
          aIdx->SetSize(len);
        else
          aIdx->SetSize(LL_MAXUINT); // LL_MAXUINT means unknown
      }
      break;
    case FIELD_LASTMODIFIED:
      {
        PRTime tm;
        nsUnescape(value);
        if (PR_ParseTimeString(value, PR_FALSE, &tm) == PR_SUCCESS) {
          aIdx->SetLastModified(tm);
        }
      }
      break;
    case FIELD_CONTENTTYPE:
      aIdx->SetContentType(value);
      break;
    case FIELD_FILETYPE:
      // unescape in-place
      nsUnescape(value);
      if (!nsCRT::strcasecmp(value, "directory")) {
        aIdx->SetType(nsIDirIndex::TYPE_DIRECTORY);
      } else if (!nsCRT::strcasecmp(value, "file")) {
        aIdx->SetType(nsIDirIndex::TYPE_FILE);
      } else if (!nsCRT::strcasecmp(value, "symbolic-link")) {
        aIdx->SetType(nsIDirIndex::TYPE_SYMLINK);
      } else {
        aIdx->SetType(nsIDirIndex::TYPE_UNKNOWN);
      }
      break;
    case FIELD_UNKNOWN:
      // ignore
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDirIndexParser::OnDataAvailable(nsIRequest *aRequest, nsISupports *aCtxt,
                                  nsIInputStream *aStream,
                                  PRUint32 aSourceOffset,
                                  PRUint32 aCount) {
  if (aCount < 1)
    return NS_OK;
  
  PRInt32 len = mBuf.Length();
  
  // Ensure that our mBuf has capacity to hold the data we're about to
  // read.
  if (!EnsureStringLength(mBuf, len + aCount))
    return NS_ERROR_OUT_OF_MEMORY;

  // Now read the data into our buffer.
  nsresult rv;
  PRUint32 count;
  rv = aStream->Read(mBuf.BeginWriting() + len, aCount, &count);
  if (NS_FAILED(rv)) return rv;

  // Set the string's length according to the amount of data we've read.
  // Note: we know this to work on nsCString. This isn't guaranteed to
  //       work on other strings.
  mBuf.SetLength(len + count);

  return ProcessData(aRequest, aCtxt);
}

nsresult
nsDirIndexParser::ProcessData(nsIRequest *aRequest, nsISupports *aCtxt) {
  if (!mListener)
    return NS_ERROR_FAILURE;
  
  PRInt32     numItems = 0;
  
  while(PR_TRUE) {
    ++numItems;
    
    PRInt32             eol = mBuf.FindCharInSet("\n\r", mLineStart);
    if (eol < 0)        break;
    mBuf.SetCharAt(PRUnichar('\0'), eol);
    
    const char  *line = mBuf.get() + mLineStart;
    
    PRInt32 lineLen = eol - mLineStart;
    mLineStart = eol + 1;
    
    if (lineLen >= 4) {
      nsresult  rv;
      const char        *buf = line;
      
      if (buf[0] == '1') {
        if (buf[1] == '0') {
          if (buf[2] == '0' && buf[3] == ':') {
            // 100. Human-readable comment line. Ignore
          } else if (buf[2] == '1' && buf[3] == ':') {
            // 101. Human-readable information line.
            mComment.Append(buf + 4);

            char    *value = ((char *)buf) + 4;
            nsUnescape(value);
            mListener->OnInformationAvailable(aRequest, aCtxt, NS_ConvertUTF8toUTF16(value));

          } else if (buf[2] == '2' && buf[3] == ':') {
            // 102. Human-readable information line, HTML.
            mComment.Append(buf + 4);
          }
        }
      } else if (buf[0] == '2') {
        if (buf[1] == '0') {
          if (buf[2] == '0' && buf[3] == ':') {
            // 200. Define field names
            rv = ParseFormat(buf + 4);
            if (NS_FAILED(rv)) {
              return rv;
            }
          } else if (buf[2] == '1' && buf[3] == ':') {
            // 201. Field data
            nsCOMPtr<nsIDirIndex> idx = do_CreateInstance("@mozilla.org/dirIndex;1",&rv);
            if (NS_FAILED(rv))
              return rv;
            
            rv = ParseData(idx, ((char *)buf) + 4);
            if (NS_FAILED(rv)) {
              return rv;
            }

            mListener->OnIndexAvailable(aRequest, aCtxt, idx);
          }
        }
      } else if (buf[0] == '3') {
        if (buf[1] == '0') {
          if (buf[2] == '0' && buf[3] == ':') {
            // 300. Self-referring URL
          } else if (buf[2] == '1' && buf[3] == ':') {
            // 301. OUR EXTENSION - encoding
            int i = 4;
            while (buf[i] && nsCRT::IsAsciiSpace(buf[i]))
              ++i;
            
            if (buf[i])
              SetEncoding(buf+i);
          }
        }
      }
    }
  }
  
  return NS_OK;
}
