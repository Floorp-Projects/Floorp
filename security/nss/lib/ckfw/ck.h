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

#ifndef CK_H
#define CK_H

#ifdef DEBUG
static const char CK_CVS_ID[] = "@(#) $RCSfile: ck.h,v $ $Revision: 1.3 $ $Date: 2005/01/20 02:25:45 $";
#endif /* DEBUG */

/*
 * ck.h
 *
 * This header file consolidates all header files needed by the source
 * files implementing the NSS Cryptoki Framework.  This makes managing
 * the source files a bit easier.
 */

/* Types */

#ifndef NSSBASET_H
#include "nssbaset.h"
#endif /* NSSBASET_H */

#ifndef NSSCKT_H
#include "nssckt.h"
#endif /* NSSCKT_H */

#ifndef NSSCKFT_H
#include "nssckft.h"
#endif /* NSSCKFT_H */

#ifndef NSSCKEPV_H
#include "nssckepv.h"
#endif /* NSSCKEPV_H */

#ifndef NSSCKFWT_H
#include "nssckfwt.h"
#endif /* NSSCKFWT_H */

#ifndef NSSCKMDT_H
#include "nssckmdt.h"
#endif /* NSSCKMDT_H */

#ifndef CKT_H
#include "ckt.h"
#endif /* CKT_H */

#ifndef CKFWTM_H
#include "ckfwtm.h"
#endif /* CKFWTM_H */

/* Prototypes */

#ifndef NSSBASE_H
#include "nssbase.h"
#endif /* NSSBASE_H */

#ifndef NSSCKG_H
#include "nssckg.h"
#endif /* NSSCKG_H */

#ifndef NSSCKFW_H
#include "nssckfw.h"
#endif /* NSSCKFW_H */

#ifndef NSSCKFWC_H
#include "nssckfwc.h"
#endif /* NSSCKFWC_H */

#ifndef CKFW_H
#include "ckfw.h"
#endif /* CKFW_H */

#ifndef CKFWM_H
#include "ckfwm.h"
#endif /* CKFWM_H */

#ifndef CKMD_H
#include "ckmd.h"
#endif /* CKMD_H */

/* NSS-private */

/* nss_ZNEW and the like.  We might want to publish the memory APIs.. */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

#endif /* CK_H */
