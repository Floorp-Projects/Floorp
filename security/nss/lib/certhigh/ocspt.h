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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
 * Public header for exported OCSP types.
 *
 * $Id: ocspt.h,v 1.4 2004/04/25 15:03:03 gerv%gerv.net Exp $
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

#endif /* _OCSPT_H_ */
