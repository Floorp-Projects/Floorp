/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef _SECSTUBN_H_
#define _SECSTUBN_H_

#include "xp.h"
#include "ntypes.h"
#include "net.h"

typedef struct _digests DIGESTS;
typedef struct _zig ZIG;
typedef struct _zigcontext ZIG_Context;
typedef struct _peheader PEHeader;

typedef struct SOBITEM_ {
    char *pathname;
    int type;
    size_t size;
    void *data;
} SOBITEM;

typedef struct FINGERZIG_ {
    size_t length;
    void *key;
    CERTCertificate *cert;
} FINGERZIG;

#define ZIG_CB_SIGNAL 1
#define ZIG_SIGN 10
#define ZIG_F_GUESS 0
#define ZIG_MF 2
#define ZIG_ERR_PNF 12

SEC_BEGIN_PROTOS

void
SECNAV_Posting(PRFileDesc *fd);

void
SECNAV_HTTPHead(PRFileDesc *fd);

void
SECNAV_RegisterNetlibMimeConverters();

char *
SECNAV_MungeString(const char *unmunged_string);

char *
SECNAV_UnMungeString(const char *munged_string);

PRBool
SECNAV_GenKeyFromChoice(void *proto_win, LO_Element *form,
			char *choiceString, char *challenge,
			char *typeString, char *pqgString,
			char **pValue, PRBool *pDone);

char **
SECNAV_GetKeyChoiceList(char *type, char *pqgString);

PRBool
SECNAV_SecurityDialog(MWContext *context, int state);

void
NET_InitCertLdapProtocol(void);
const char *

SECNAV_GetPolicyNameString(void);

int
SECNAV_InitConfigObject(void);

int
SECNAV_RunInitialSecConfig(void);

void
SECNAV_EarlyInit(void);

void
SECNAV_Init(void);

void
SECNAV_Shutdown(void);

void
SECNAV_SecurityAdvisor(void *proto_win, URL_Struct *url);

char *
SECNAV_MakeCertButtonString(CERTCertificate *cert);

int
SECNAV_SecURLData(char *which, NET_StreamClass *stream, MWContext *cx);

char *
SECNAV_SecURLContentType(char *which);

int
SECNAV_SecHandleSecurityAdvisorURL(MWContext *cx, const char *which);

void
SECNAV_HandleInternalSecURL(URL_Struct *url, MWContext *cx);

SECStatus
SECNAV_ComputeFortezzaProxyChallengeResponse(MWContext *context,
					     char *asciiChallenge,
					     char **signature_out,
					     char **clientRan_out,
					     char **certChain_out);

char *
SECNAV_PrettySecurityStatus(int level, unsigned char *status);

char *
SECNAV_SecurityVersion(PRBool longForm);

char *
SECNAV_SSLCapabilities(void);

unsigned char *
SECNAV_SSLSocketStatus(PRFileDesc *fd, int *return_security_level);

unsigned char *
SECNAV_CopySSLSocketStatus(unsigned char *status);

unsigned int
SECNAV_SSLSocketStatusLength(unsigned char *status);

char *
SECNAV_SSLSocketCertString(unsigned char *status);

PRBool
SECNAV_CompareCertsForRedirection(unsigned char *status1,
				  unsigned char *status2);

CERTCertificate *
SECNAV_CertFromSSLSocketStatus(unsigned char *status);


DIGESTS *PR_CALLBACK
SOB_calculate_digest(void XP_HUGE *data, long length);

int PR_CALLBACK
SOB_verify_digest(ZIG *siglist, const char *name, DIGESTS *dig);

void PR_CALLBACK
SOB_destroy (ZIG *zig);

char *
SOB_get_error (int status);

ZIG_Context *
SOB_find(ZIG *zig, char *pattern, int type);

int
SOB_find_next(ZIG_Context *ctx, SOBITEM **it);

void
SOB_find_end(ZIG_Context *ctx);

char *
SOB_get_url (ZIG *zig);

ZIG *
SOB_new (void);

int
SOB_set_callback (int type, ZIG *zig,
		  int (*fn) (int status, ZIG *zig, 
			     const char *metafile,
			     char *pathname, char *errortext));

int PR_CALLBACK
SOB_cert_attribute(int attrib, ZIG *zig, long keylen, void *key, 
		   void **result, unsigned long *length);

int PR_CALLBACK
SOB_stash_cert(ZIG *zig, long keylen, void *key);

int SOB_parse_manifest(char XP_HUGE *raw_manifest, long length, 
		       const char *path, const char *url, ZIG *zig);

void
SECNAV_signedAppletPrivileges(void *proto_win, char *javaPrin, 
			      char *javaTarget, char *risk, int isCert);

void
SECNAV_signedAppletPrivilegesOnMozillaThread(void *proto_win, char *javaPrin,
                                             char *javaTarget, char *risk, int isCert);

char *
SOB_JAR_list_certs (void);

int
SOB_JAR_validate_archive (char *filename);

void *
SOB_JAR_new_hash (int alg);

void *
SOB_JAR_hash (int alg, void *cookie, int length, void *data);

void *
SOB_JAR_end_hash (int alg, void *cookie);

int
SOB_JAR_sign_archive (char *nickname, char *password, char *sf, char *outsig);

int
SOB_set_context (ZIG *zig, MWContext *mw);

int
SOB_pass_archive(int format, char *filename, const char *url, ZIG *zig);

int
SOB_get_metainfo(ZIG *zig, char *name, char *header, void **info,
		 unsigned long *length);

int
SOB_verified_extract(ZIG *zig, char *path, char *outpath);

NET_StreamClass *
SECNAV_MakePreencryptedWriteStream(FO_Present_Types format_out, void *data,
				   URL_Struct *url, MWContext *window_id);

SEC_END_PROTOS

#endif /* _SECSTUBN_H_ */
