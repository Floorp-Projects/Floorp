/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "nspr.h"
#include "nscore.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsICharsetConverterManager.h"
#include "nsILocalFile.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"

extern "C" {
#include "nlslayer.h"
}

static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_IID(kIStringBundleServiceIID, NS_ISTRINGBUNDLESERVICE_IID);
static NS_DEFINE_CID(kDateTimeCID, NS_DATETIMEFORMAT_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

static nsIUnicodeEncoder *encoderASCII = nsnull;

#define TEXT_BUNDLE	    "resource:/psmdata/ui/psm_text.properties"
#define UI_BUNDLE		"resource:/psmdata/ui/psm_ui.properties"
#define BIN_BUNDLE		"resource:/psmdata/ui/psm_bin.properties"
#define DOC_BUNDLE		"resource:/psmdata/ui/psm_doc.properties"

extern "C" {
nsIStringBundle* nlsCreateBundle(char* bundleURL);
char* nlsGetUTF8StringFromBundle(nsIStringBundle *bundle, const char *name);
nsIStringBundle * bundles[4] = {NULL, NULL, NULL, NULL};
}


extern "C" PRBool nlsInit()
{
	nsICharsetConverterManager *ccm = nsnull;
	nsAutoString charsetUTF8 = NS_ConvertASCIItoUCS2("UTF-8");
	nsAutoString charsetASCII = NS_ConvertASCIItoUCS2("ISO-8859-1");
	PRBool ret = PR_FALSE;
	nsresult res;

#if !defined(XP_MAC)
	res = NS_InitXPCOM(NULL, NULL);
	NS_ASSERTION( NS_SUCCEEDED(res), "NS_InitXPCOM failed" );
#endif

	// Create the bundles
	bundles[0] = nlsCreateBundle(TEXT_BUNDLE);
	bundles[1] = nlsCreateBundle(UI_BUNDLE);
	bundles[2] = nlsCreateBundle(BIN_BUNDLE);
	bundles[3] = nlsCreateBundle(DOC_BUNDLE);

	// Create a unicode encoder and decoder
	res = nsServiceManager::GetService(kCharsetConverterManagerCID,
										NS_GET_IID(nsICharsetConverterManager),
										(nsISupports**)&ccm);

	if (NS_FAILED(res) || (nsnull == ccm)) {
		goto loser;
	}

	res = ccm->GetUnicodeEncoder(&charsetASCII, &encoderASCII);
	if (NS_FAILED(res) || (nsnull == encoderASCII)) {
		goto loser;
	}

	ret = PR_TRUE;
	goto done;
loser:
	NS_IF_RELEASE(bundles[0]);
	NS_IF_RELEASE(bundles[1]);
	NS_IF_RELEASE(bundles[2]);
	NS_IF_RELEASE(bundles[3]);
	NS_IF_RELEASE(encoderASCII);
done:
	return ret;
}

extern "C" nsIStringBundle* nlsCreateBundle(char* bundleURL)
{
	nsresult ret;
	nsIStringBundleService *service = nsnull;
	nsIStringBundle* bundle = nsnull;

	// Get the string bundle service
	ret = nsServiceManager::GetService(kStringBundleServiceCID,
										kIStringBundleServiceIID,
										(nsISupports**)&service);
	if (NS_FAILED(ret)) {
		return NULL;
	}

	// Create the bundle
	ret = service->CreateBundle(bundleURL, &bundle);
	if (NS_FAILED(ret)) {
		return NULL;
	}

	NS_IF_RELEASE(service);

	return bundle;
}

extern "C" char* nlsGetUTF8StringFromBundle(nsIStringBundle *bundle, const char *name)
{
	nsresult ret;

	nsString key = NS_ConvertASCIItoUCS2(name);
	nsString value;
	PRUnichar * p = NULL;
	ret = bundle->GetStringFromName(key.GetUnicode(), &p);
	if (NS_FAILED(ret)) {
		return NULL;
	}
	value = p;

	// XXX This is a hack to get cr and lf chars in string. 
	// See bug# 21418
	value.ReplaceSubstring(NS_ConvertASCIItoUCS2("<psm:cr>"), NS_ConvertASCIItoUCS2("\r"));
	value.ReplaceSubstring(NS_ConvertASCIItoUCS2("<psm:lf>"), NS_ConvertASCIItoUCS2("\n"));
	return value.ToNewUTF8String();
}

extern "C" char* nlsGetUTF8String(const char *name)
{
	int i;
	char *value = NULL;

	for (i=0;i<4;i++) {
		value = nlsGetUTF8StringFromBundle(bundles[i], name);
		if (value) {
			break;
		}
	}
	return value;
}

extern "C" void * nlsNewDateFormat()
{
	nsIComponentManager *comMgr;
	nsIDateTimeFormat *dateTimeFormat = nsnull;
	nsresult rv;

	rv = NS_GetGlobalComponentManager(&comMgr);
	if (NS_FAILED(rv)) {
		return NULL;
	}
	rv = comMgr->CreateInstance(kDateTimeCID, nsnull, NS_GET_IID(nsIDateTimeFormat), (void**)&dateTimeFormat);
	if (NS_FAILED(rv)) {
		return NULL;
	}
	return dateTimeFormat;
}

extern "C" void nlsFreeDateFormat(void * p)
{
	nsIDateTimeFormat *dateTimeFormat = (nsIDateTimeFormat*)p;

	NS_IF_RELEASE(dateTimeFormat);
}

extern "C" char * nslPRTimeToUTF8String(void* p, PRInt64 t)
{
	nsIDateTimeFormat *dateTimeFormat = (nsIDateTimeFormat*)p;
	nsString dateTime;
	nsresult rv;

	rv = dateTimeFormat->FormatPRTime(nsnull, kDateFormatShort, kTimeFormatNoSeconds, PRTime(t), dateTime);
	if (NS_FAILED(rv)) {
		return nsnull;
	}

	return dateTime.ToNewUTF8String();
}

extern "C" PRBool nlsUnicodeToUTF8(unsigned char * inBuf, unsigned int inBufBytes,
							unsigned char * outBuf, unsigned int maxOutBufLen,
							unsigned int * outBufLen)
{
	const char *utf8;
	PRBool ret = PR_TRUE;
	
	utf8 = NS_ConvertUCS2toUTF8((PRUnichar*)inBuf, inBufBytes/2).get();
	*outBufLen = PL_strlen(utf8);
	if (*outBufLen+1 > maxOutBufLen) {
		ret = PR_FALSE;
		goto loser;
	}
	memcpy(outBuf, utf8, *outBufLen+1);
	
loser:
	return ret;
}

extern "C" PRBool nlsUTF8ToUnicode(unsigned char * inBuf, unsigned int inBufBytes,
							unsigned char * outBuf, unsigned int maxOutBufLen,
							unsigned int * outBufLen)
{
	PRBool ret = PR_TRUE;
	nsAutoString autoString = NS_ConvertUTF8toUCS2((const char*)inBuf);
	const PRUnichar *buffer;
	PRUint32 bufLen;
	unsigned int newLen;
	
	buffer = autoString.GetUnicode();
	bufLen = autoString.Length();
	newLen = (bufLen+1)*2;
	if (newLen > maxOutBufLen) {
		ret = PR_FALSE;
		goto loser;
	}
	memcpy(outBuf, (char*)buffer, newLen);
	*outBufLen = newLen;
loser:
	return ret;
}

extern "C" PRBool nlsUnicodeToASCII(unsigned char * inBuf, unsigned int inBufBytes,
							unsigned char * outBuf, unsigned int maxOutBufLen,
							unsigned int * outBufLen)
{
	PRBool ret = PR_FALSE;
	nsIUnicodeEncoder *enc = encoderASCII;
	PRInt32 dstLength;
	nsresult res;

	res = enc->GetMaxLength((const PRUnichar *)inBuf, inBufBytes, &dstLength);
	if (NS_FAILED(res) || (dstLength > maxOutBufLen)) {
		goto loser;
	}

	res = enc->Convert((const PRUnichar *)inBuf, (PRInt32*)&inBufBytes, (char*)outBuf, &dstLength);
	if (NS_FAILED(res)) {
		goto loser;
	}

	outBuf[dstLength] = '\0';
	*outBufLen = dstLength;
	ret = PR_TRUE;
loser:
	return ret;
}

extern "C" PRBool nlsASCIIToUnicode(unsigned char * inBuf, unsigned int inBufBytes,
							unsigned char * outBuf, unsigned int maxOutBufLen,
							unsigned int * outBufLen)
{
	nsAutoString autoString = NS_ConvertASCIItoUCS2((const char*)inBuf);
	PRUint32 bufLen;
	const PRUnichar *buffer;
	PRBool ret = PR_TRUE;
	unsigned int newLen;
	
	bufLen = autoString.Length();
	buffer = autoString.GetUnicode();
	newLen = (bufLen+1)*2;
	if (newLen > maxOutBufLen) {
		ret = PR_FALSE;
		goto loser;
	}
	memcpy(outBuf, buffer, newLen);
	*outBufLen = newLen;
loser:
	return ret;
}

extern "C" char *xpcomGetProcessDir(void)
{
    nsresult rv;
    char *retVal = NULL;
    
    nsCOMPtr<nsILocalFile> mozFile;

	NS_WITH_SERVICE(nsIProperties, directoryService, NS_DIRECTORY_SERVICE_CONTRACTID, &rv);									
    if (NS_FAILED(rv)) {
        return NULL;
    }    
    										
     directoryService->Get( NS_XPCOM_CURRENT_PROCESS_DIR,
                            NS_GET_IID(nsIFile), 
                            getter_AddRefs(mozFile));
    if (mozFile) {
        mozFile->GetPath(&retVal);
    }
    return retVal;
}

