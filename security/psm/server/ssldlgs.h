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
#ifndef __SSM_SSLDLGS_H__
#define __SSM_SSLDLGS_H__

#include "textgen.h"
#include "minihttp.h"
#include "sslconn.h"

#include "certt.h"


/*
 * Function: SSMStatus SSM_ServerCertKeywordHandler()
 * Purpose: keyword handler for "_server_cert_info"
 *          performs substitution for cert info data
 *          syntax: {_server_cert_info cert_format,wrapper}
 *                  cert_format: either simple_cert_format (0) or
 *                               pretty_cert_format (1)
 * Arguments and return values:
 * - cx: SSMTextGenContext to be manipulated
 * - returns: SSM_SUCCESS if successful; error code otherwise
 *
 */
SSMStatus SSM_ServerCertKeywordHandler(SSMTextGenContext* cx);

/*
 * Function: SSMStatus SSM_FormatCert()
 * Purpose: creates a UnicodeString that is used in expanding the keyword
 *          "_server_cert_info" from the given cert
 *          this function provides info on hostname, issuer name, and validity
 *          period.  Any UI dialog that uses a subset of this info can use
 *          this function.
 * Arguments and return values
 * - cert: server cert to be manipulated
 * - fmt:  message format (shouldn't be NULL)
 * - result: resulting expanded string (shouldn't be NULL)
 *
 * Note: note that this is very similar to a _Print method.  In fact,
 *	 this could be turned into SSMResourceCert_Print() method w/o too many
 *	 changes.
 */
SSMStatus SSM_FormatCert(CERTCertificate* cert, char* fmt, 
			 char** result);
/*
 * Function: SSMStatus SSM_HTTPBadClientAuthButtonHandler()
 * Purpose: command handler for "BCAButton" (button handling
 *          for bad client auth dialogs)
 * Arguments and return values:
 * - req: request object to be manipulated
 * - returns: SSM_SUCCESS if successful; error code otherwise
 *
 * Note: we use this command handler instead of the default handler
 *       to block the SSL threads while this dialog is up.
 */
SSMStatus SSM_HTTPBadClientAuthButtonHandler(HTTPRequest* req);

/*
 * Function: SSMStatus SSM_SSLMakeClientAuthDialog()
 * Purpose: generates the client auth cert selection dialog and receives
 *          a chosen cert nickname from the user
 * Arguments and return values:
 * - conn: SSL connection object to be manipulated; m_UIInfo.chosenNickname
 *         will be populated as a result of the dialog
 * - returns: SSM_SUCCESS if successful; otherwise error code
 *
 * Note: (design issue) how do we handle "cancel" event?  Do we display the
 *       "no cert" dialog and exit, or we just exit?  Probably we just exit.
 */
SSMStatus SSM_SSLMakeClientAuthDialog(SSMSSLDataConnection* conn);

/*
 * Function: SSMStatus SSM_ClientAuthCertListKeywordHandler()
 * Purpose: formats the client cert list according to the given format
 *          for the cert selection dialog
 *          syntax: {_client_auth_certList prefix,wrapper,suffix}
 *                  where wrapper is for the individual cert
 * Arguments and return values:
 * - cx: text context to be manipulated
 * - returns: SSM_SUCCESS if successful; error code otherwise
 *
 * Note:
 */
SSMStatus SSM_ClientAuthCertListKeywordHandler(SSMTextGenContext* cx);

/*
 * Function: SSMStatus SSM_HTTPClientAuthButtonHandler()
 * Purpose: handles the user input for buttons in cert selection dialog
 * URL: "CAButton?baseRef=windowclose_doclose_js&target={0}&chosen=0&do_ok={text_ok}" or "CAButton?...&target={0}&do_cancel={text_cancel}", etc.
 */
SSMStatus SSM_HTTPClientAuthButtonHandler(HTTPRequest* req);

SECStatus SSM_SSLMakeCertExpiredDialog(CERTCertificate* cert,
				       SSMSSLDataConnection* conn);
SECStatus SSM_SSLMakeCertBadDomainDialog(CERTCertificate* cert,
					 SSMSSLDataConnection* conn);
SSMStatus SSM_ClientAuthCertSelectionButtonHandler(HTTPRequest* req);
SSMStatus SSM_ServerAuthFailureButtonHandler(HTTPRequest* req);
SSMStatus SSM_ServerAuthUnknownIssuerButtonHandler(HTTPRequest* req);
SSMStatus SSM_CurrentTimeKeywordHandler(SSMTextGenContext* cx);

/*
 * Function: SSMStatus SSM_ServerAuthDomainKeywordHandler()
 * Purpose: formats the domain name mismatch warning string for server auth
 *          syntax: {_server_cert_domain_info bad_domain_wrapper}
 * Arguments and return values
 * - cx: text context to be manipulated
 * - returns: SSM_SUCCESS if successful; error code otherwise
 *
 * Note: I wrote a separate keyword handler although there is a generic
 *       server cert info keyword handler; this is the only place where
 *       one needs the URL hostname and it turns out to be simpler and more
 *       efficient to write a separate handler for this purpose
 */
SSMStatus SSM_ServerAuthDomainNameKeywordHandler(SSMTextGenContext* cx);

/*
 * Function: SSMStatus SSM_VerifyServerCertKeywordHandler()
 * Purpose: handles the nickname substitution for keyword 
 *          "_verify_server_cert"
 * Arguments and return values:
 * - cx: text context to be manipulated
 * - returns: SSM_SUCCESS if successful; error code otherwise
 *
 * Note: since this only comes from the server auth failure dialog, we know
 *       beforehand that the cert is bad (thus no need really to verify the
 *       cert).  This might change if we want to print out the nature of the
 *       error.  Also, if we want to consolidate this view cert window
 *       handling (it's not clear how we can do that because we have different
 *       certs and different targets for different cases), this function
 *       may have to be modified.
 */
SSMStatus SSM_VerifyServerCertKeywordHandler(SSMTextGenContext* cx);

/*
 * Function: SSMStatus SSM_PrettyFormatCert()
 * Purpose: formats the cert info for "View Security Certificate" dialogs
 *          this function provides info on issuer name, serial number, 
 *          validity period, finger print, and comment.  Any dialog that 
 *          needs a subset of the parameters can use this function to format
 *          the cert
 * Arguments and return values:
 * - cert: cert to be presented
 * - fmt: MessageFormat object
 * - result: UnicodeString to be returned as a result of the operation
 * - returns: SSM_SUCCESS if successful; error code otherwise
 *
 * Note: this provides no information on whether the cert is valid or not
 *       lot of the code is borrowed from CERT_HTMLInfo() from NSS
 *       we also access lots of internal cert fields but can't help it...
 */
SSMStatus SSM_PrettyFormatCert(CERTCertificate* cert, char* fmt,
			       char** result, PRBool addIssuerLink);

SECStatus SSM_SSLMakeUnknownIssuerDialog(CERTCertificate* cert,
					 SSMSSLDataConnection* conn);
SSMStatus SSM_HTTPUnknownIssuerStep1ButtonHandler(HTTPRequest* req);

#endif /* __SSM_SSLDLGS_H__ */
