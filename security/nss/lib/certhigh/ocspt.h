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

/*
 * Public header for exported OCSP types.
 *
 * $Id: ocspt.h,v 1.2 2002/07/03 00:02:34 javi%netscape.com Exp $
 */

#ifndef _OCSPT_H_
#define _OCSPT_H_

/*
 * The following are all opaque types.  If someone needs to get at
 * a field within, then we need to fix the API.  Try very hard not
 * make the type available to them.
 */
typedef struct CERTOCSPRequestStr CERTOCSPRequest;
typedef struct CERTOCSPResponseStr CERTOCSPResponse;

/*
 * XXX I think only those first two above should need to be exported,
 * but until I know for certain I am leaving the rest of these here, too.
 */
typedef struct CERTOCSPCertIDStr CERTOCSPCertID;
typedef struct CERTOCSPCertStatusStr CERTOCSPCertStatus;
typedef struct CERTOCSPSingleResponseStr CERTOCSPSingleResponse;

/*
 * Making these types public so that it is possible for 3rpd party
 * apps to parse and look at the fields of an OCSP response.
 */

/*
 * This describes the value of the responseStatus field in an OCSPResponse.
 * The corresponding ASN.1 definition is:
 *
 * OCSPResponseStatus   ::=     ENUMERATED {
 *      successful              (0),    --Response has valid confirmations
 *      malformedRequest        (1),    --Illegal confirmation request
 *      internalError           (2),    --Internal error in issuer
 *      tryLater                (3),    --Try again later
 *                                      --(4) is not used
 *      sigRequired             (5),    --Must sign the request
 *      unauthorized            (6),    --Request unauthorized
 * }
 */
typedef enum {
    OCSPResponse_successful = 0,
    OCSPResponse_malformedRequest = 1,
    OCSPResponse_internalError = 2,
    OCSPResponse_tryLater = 3,
    OCSPResponse_unused = 4,
    OCSPResponse_sigRequired = 5,
    OCSPResponse_unauthorized = 6,
    OCSPResponse_other                  /* unknown/unrecognized value */
} OCSPResponseStatus;
#endif /* _OCSPT_H_ */
