/* -*- Mode: C; tab-width: 8 -*- */
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

#include "secasn1.h"
#include "crmf.h"
#include "crmfi.h"

void
crmf_encoder_out(void *arg, const char *buf, unsigned long len,
		 int depth, SEC_ASN1EncodingPart data_kind)
{
    struct crmfEncoderOutput *output;

    output = (struct crmfEncoderOutput*) arg;
    output->fn (output->outputArg, buf, len);
}

SECStatus
cmmf_user_encode(void *src, CRMFEncoderOutputCallback inCallback, void *inArg,
		 const SEC_ASN1Template *inTemplate)
{
    struct crmfEncoderOutput output;

    PORT_Assert(src != NULL);
    if (src == NULL) {
        return SECFailure;
    }
    output.fn        = inCallback;
    output.outputArg = inArg;
    return SEC_ASN1Encode(src, inTemplate, crmf_encoder_out, &output);    
}

