/* -*- Mode: C; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "crmf.h"
#include "crmfi.h"

SECStatus 
CRMF_EncodeCertReqMsg(CRMFCertReqMsg            *inCertReqMsg,
		      CRMFEncoderOutputCallback  fn,
		      void                      *arg)
{
    struct crmfEncoderOutput output;

    output.fn        = fn;
    output.outputArg = arg;
    return SEC_ASN1Encode(inCertReqMsg,CRMFCertReqMsgTemplate, 
			  crmf_encoder_out, &output);
    
}


SECStatus
CRMF_EncodeCertRequest(CRMFCertRequest           *inCertReq,
		       CRMFEncoderOutputCallback  fn,
		       void                      *arg)
{
    struct crmfEncoderOutput output;

    output.fn        = fn;
    output.outputArg = arg;
    return SEC_ASN1Encode(inCertReq, CRMFCertRequestTemplate, 
			  crmf_encoder_out, &output);
}

SECStatus
CRMF_EncodeCertReqMessages(CRMFCertReqMsg              **inCertReqMsgs,
			   CRMFEncoderOutputCallback     fn,
			   void                         *arg)
{
    struct crmfEncoderOutput output;
    CRMFCertReqMessages msgs;
    
    output.fn        = fn;
    output.outputArg = arg;
    msgs.messages = inCertReqMsgs;
    return SEC_ASN1Encode(&msgs, CRMFCertReqMessagesTemplate,
			  crmf_encoder_out, &output);
}




