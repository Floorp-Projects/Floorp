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

#ifndef CK_H
#define CK_H

#ifdef DEBUG
static const char CK_CVS_ID[] = "@(#) $RCSfile: ck.h,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:43:09 $ $Name:  $";
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
