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
    Optimized ASN.1 DER decoder

    differences with SEC_ASN1DecodeItem :
    - only supports DER . BER is not supported
    - does not support streaming
    - much faster. A 10x improvement was measured on large CRLs
    - much more memory efficient. The peak usage has been measured to be 1/5th
      on large CRLs
    - requires an arena to be passed in for the few cases where a memory
      allocation is needed
    - allows SEC_ASN1_SKIP and SEC_ASN1_OPTIONAL together in a template, with
      the additional requirement of a data type
    - allows SEC_ASN1_SKIP and SEC_ASN1_SAVE to work at the same time. This
      is useful to get a handle to a component we are interested in for later
      decoding, such as CRL entries
    - allows SEC_ASN1_SAVE and SEC_ASN1_OPTIONAL to be used together, again
      with the additional requirement of a data type
    - SEC_ASN1_SKIP_REST is supported in subtemplates
    - only tested on CRL templates - use with caution
    - the target decoded structure has pointers into the DER input, rather
      than copies. This avoids a very large number of unnecessary memory
      allocations.

*/

#ifndef __QUICKDER__
#define __QUICKDER__

#include "plarena.h"

#include "seccomon.h"
#include "secasn1t.h"

SEC_BEGIN_PROTOS

extern SECStatus SEC_QuickDERDecodeItem(PRArenaPool* arena, void* dest,
                     const SEC_ASN1Template* templateEntry,
                     SECItem* src);

SEC_END_PROTOS

#endif /* __QUICKDER__ */

