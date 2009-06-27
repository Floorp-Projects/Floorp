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
 * The Original Code is HTML Parser C++ Translator code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
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
 
#include "nsICharsetConverterManager.h"
#include "nsServiceManagerUtils.h"
#include "nsICharsetAlias.h"
#include "nsEncoderDecoderUtils.h"
#include "nsTraceRefcnt.h"

static NS_DEFINE_CID(kCharsetAliasCID, NS_CHARSETALIAS_CID);

nsHtml5MetaScanner::nsHtml5MetaScanner()
 : readable(nsnull),
   metaState(NS_HTML5META_SCANNER_NO),
   contentIndex(-1),
   charsetIndex(-1),
   stateSave(NS_HTML5META_SCANNER_DATA),
   strBufLen(0),
   strBuf(jArray<PRUnichar,PRInt32>(36))
{
  MOZ_COUNT_CTOR(nsHtml5MetaScanner);
}

nsHtml5MetaScanner::~nsHtml5MetaScanner()
{
  MOZ_COUNT_DTOR(nsHtml5MetaScanner);
  strBuf.release();
}

void
nsHtml5MetaScanner::sniff(nsHtml5ByteReadable* bytes, nsIUnicodeDecoder** decoder, nsACString& charset)
{
  readable = bytes;
  stateLoop(stateSave);
  readable = nsnull;
  if (mUnicodeDecoder) {
    mUnicodeDecoder.forget(decoder);
    charset.Assign(mCharset);
  }
}

PRBool
nsHtml5MetaScanner::tryCharset(nsString* charset)
{
  nsresult res = NS_OK;
  nsCOMPtr<nsICharsetConverterManager> convManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &res);
  if (NS_FAILED(res)) {
    NS_ERROR("Could not get CharsetConverterManager service.");
    return PR_FALSE;
  }
  nsCAutoString encoding;
  CopyUTF16toUTF8(*charset, encoding);
  // XXX spec says only UTF-16
  if (encoding.LowerCaseEqualsASCII("utf-16") ||
      encoding.LowerCaseEqualsASCII("utf-16be") ||
      encoding.LowerCaseEqualsASCII("utf-16le") ||
      encoding.LowerCaseEqualsASCII("utf-32") ||
      encoding.LowerCaseEqualsASCII("utf-32be") ||
      encoding.LowerCaseEqualsASCII("utf-32le")) {
    mCharset.Assign("utf-8");
    res = convManager->GetUnicodeDecoderRaw(mCharset.get(), getter_AddRefs(mUnicodeDecoder));
    if (NS_FAILED(res)) {
      NS_ERROR("Could not get decoder for UTF-8.");
      return PR_FALSE;
    }
    return PR_TRUE;
  }
  nsCAutoString preferred;
  nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID, &res));
  if (NS_FAILED(res)) {
    NS_ERROR("Could not get CharsetAlias service.");
    return PR_FALSE;
  }
  res = calias->GetPreferred(encoding, preferred);
  if (NS_FAILED(res)) {
    return PR_FALSE;
  }
  if (preferred.LowerCaseEqualsASCII("utf-16") ||
      preferred.LowerCaseEqualsASCII("utf-16be") ||
      preferred.LowerCaseEqualsASCII("utf-16le") ||
      preferred.LowerCaseEqualsASCII("utf-32") ||
      preferred.LowerCaseEqualsASCII("utf-32be") ||
      preferred.LowerCaseEqualsASCII("utf-32le") ||
      preferred.LowerCaseEqualsASCII("utf-7") ||
      preferred.LowerCaseEqualsASCII("jis_x0212-1990") ||
      preferred.LowerCaseEqualsASCII("x-jis0208") ||
      preferred.LowerCaseEqualsASCII("x-imap4-modified-utf7") ||
      preferred.LowerCaseEqualsASCII("x-user-defined")) {
    return PR_FALSE;
  }
  res = convManager->GetUnicodeDecoderRaw(preferred.get(), getter_AddRefs(mUnicodeDecoder));
  if (res == NS_ERROR_UCONV_NOCONV) {
    return PR_FALSE;
  } else if (NS_FAILED(res)) {
    NS_ERROR("Getting an encoding decoder failed in a bad way.");
    mUnicodeDecoder = nsnull;
    return PR_FALSE;
  } else {
    NS_ASSERTION(mUnicodeDecoder, "Getter nsresult and object don't match.");
    mCharset.Assign(preferred);
    return PR_TRUE;
  }
}
