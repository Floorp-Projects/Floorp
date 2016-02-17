/* -*- Mode: C; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

