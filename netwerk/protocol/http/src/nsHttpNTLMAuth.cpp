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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#include <windows.h>
#include <stdlib.h>

#define SECURITY_WIN32 1
#include <security.h>
#include <rpc.h>

#include "nsHttp.h"
#include "nsHttpNTLMAuth.h"
#include "plbase64.h"
#include "nsCRT.h"
#include "nsString.h"

static HINSTANCE              gLib = NULL;
static PSecurityFunctionTable gFT = NULL;
static ULONG                  gMaxTokenLen = 0;

#ifdef DEBUG
#define CASE_(_x) case _x: return # _x;
static const char *MapErrorCode(int rc)
{
    switch (rc) {
    CASE_(SEC_E_OK)
    CASE_(SEC_I_CONTINUE_NEEDED)
    CASE_(SEC_I_COMPLETE_NEEDED)
    CASE_(SEC_I_COMPLETE_AND_CONTINUE)
    CASE_(SEC_E_INCOMPLETE_MESSAGE)
    CASE_(SEC_I_INCOMPLETE_CREDENTIALS)
    CASE_(SEC_E_INVALID_HANDLE)
    CASE_(SEC_E_TARGET_UNKNOWN)
    CASE_(SEC_E_LOGON_DENIED)
    CASE_(SEC_E_INTERNAL_ERROR)
    CASE_(SEC_E_NO_CREDENTIALS)
    CASE_(SEC_E_NO_AUTHENTICATING_AUTHORITY)
    CASE_(SEC_E_INSUFFICIENT_MEMORY)
    CASE_(SEC_E_INVALID_TOKEN)
    }
    return "<unknown>";
}
#else
#define MapErrorCode(_rc) ""
#endif

//-----------------------------------------------------------------------------

#include "nsICryptoFIPSInfo.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

// test for FIPS mode.  if set, then NTLM cannot be supported.  if PSM is not
// present, assume FIPS mode to be consistent with DIGEST auth.  i.e., if user
// did not install PSM, then we should assume that the user doesn't want any
// crypto activity.
static PRBool
IsNTLMDisabled()
{
    nsresult rv;
    nsCOMPtr<nsICryptoFIPSInfo> fipsInfo =
            do_GetService(NS_CRYPTO_FIPSINFO_SERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return PR_TRUE;

    PRBool result;
    rv = fipsInfo->GetIsFIPSModeActive(&result);
    if (NS_FAILED(rv)) return PR_TRUE;

    return result;
}

//-----------------------------------------------------------------------------

// IE6 will automatically send out the user's default identity when challenged.
// this is rather risky given NTLM's use of the MD4 hash and the fact that any
// webserver can issue a NTLM challenge.  this macro defines whether or not
// mozilla will automatically send out the user's default identity.
#define TRY_DEFAULT_LOGON_AUTOMATICALLY PR_FALSE

class nsNTLMSessionState : public nsISupports
{
public:
    NS_DECL_ISUPPORTS

    nsNTLMSessionState()
        : mDefaultLogonFailed(!TRY_DEFAULT_LOGON_AUTOMATICALLY)
        {}
    virtual ~nsNTLMSessionState() {}

    PRBool mDefaultLogonFailed;
};

NS_IMPL_ISUPPORTS0(nsNTLMSessionState)

//-----------------------------------------------------------------------------

class nsNTLMContinuationState : public nsISupports
{
public:
    NS_DECL_ISUPPORTS

    virtual ~nsNTLMContinuationState()
    {
        (gFT->DeleteSecurityContext)(&mCtx);
#ifdef __MINGW32__
        (gFT->FreeCredentialsHandle)(&mCred);
#else
        (gFT->FreeCredentialHandle)(&mCred);
#endif
    }

    CredHandle mCred;
    CtxtHandle mCtx;
};

NS_IMPL_ISUPPORTS0(nsNTLMContinuationState)

//-----------------------------------------------------------------------------

nsHttpNTLMAuth::nsHttpNTLMAuth()
{
}

nsHttpNTLMAuth::~nsHttpNTLMAuth()
{
    if (gLib) {
        FreeLibrary(gLib);
        gLib = NULL;
    }
}

nsresult
nsHttpNTLMAuth::Init()
{
    if (IsNTLMDisabled())
        return NS_ERROR_FAILURE;

    PSecurityFunctionTable (*initFun)(void);

    gLib = LoadLibrary("secur32.dll");
    if (!gLib) {
        NS_ERROR("failed to load secur32.dll");
        return NS_ERROR_UNEXPECTED;
    }

    initFun = (PSecurityFunctionTable (*)(void)) GetProcAddress(gLib, "InitSecurityInterfaceA");
    if (!initFun) {
        NS_ERROR("failed to locate DLL entry point");
        return NS_ERROR_UNEXPECTED;
    }

    gFT = initFun();
    if (!gFT) {
        NS_ERROR("no function table");
        return NS_ERROR_UNEXPECTED;
    }

    PSecPkgInfo pkgInfo;
    int rc = (gFT->QuerySecurityPackageInfo)("NTLM", &pkgInfo);
    if (rc != SEC_E_OK) {
        NS_ERROR("NTLM security package not found");
        return NS_ERROR_UNEXPECTED;
    }
    gMaxTokenLen = pkgInfo->cbMaxToken;

    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsHttpNTLMAuth, nsIHttpAuthenticator)

NS_IMETHODIMP
nsHttpNTLMAuth::ChallengeReceived(nsIHttpChannel *channel,
                                  const char     *challenge,
                                  PRBool          isProxyAuth,
                                  nsISupports   **sessionState,
                                  nsISupports   **continuationState,
                                  PRBool         *identityInvalid)
{
    LOG(("nsHttpNTLMAuth::ChallengeReceived\n"));

    *identityInvalid = PR_FALSE;
    // only request new identity if challenge is exactly "NTLM" and if we
    // have determined that the default logon is not accepted.
    if (PL_strcasecmp(challenge, "NTLM") == 0) {
        nsNTLMSessionState *session = (nsNTLMSessionState *) *sessionState;
        if (session) {
            // existance of continuation state implies default logon was
            // attempted and must have failed.
            if (*continuationState)
                session->mDefaultLogonFailed = PR_TRUE;
        }
        else {
            session = new nsNTLMSessionState();
            if (!session)
                return NS_ERROR_OUT_OF_MEMORY;
            NS_ADDREF(*sessionState = session);
        }
        *identityInvalid = session->mDefaultLogonFailed; 
    }
    return NS_OK;
}

NS_IMETHODIMP
nsHttpNTLMAuth::GenerateCredentials(nsIHttpChannel  *httpChannel,
                                    const char      *challenge,
                                    PRBool           isProxyAuth,
                                    const PRUnichar *domain,
                                    const PRUnichar *user,
                                    const PRUnichar *pass,
                                    nsISupports    **sessionState,
                                    nsISupports    **continuationState,
                                    char           **creds)

{
    LOG(("nsHttpNTLMAuth::GenerateCredentials\n"));

    // NOTE: the FIPS setting can change dynamically, so we need to check it
    // here as well.  this isn't a very costly check, and moreover we aren't
    // going to execute this code that frequently since NTLM performs the
    // handshake once per connection.
    if (IsNTLMDisabled())
        return NS_ERROR_FAILURE;

    NS_ENSURE_TRUE(gFT, NS_ERROR_NOT_INITIALIZED);

    *creds = NULL;

    nsNTLMSessionState *session = (nsNTLMSessionState *) *sessionState;
    NS_ASSERTION(session, "ChallengeReceived not called");

    nsNTLMContinuationState *state = (nsNTLMContinuationState *) *continuationState;
    CtxtHandle *ctxIn;
    SecBufferDesc obd, ibd;
    SecBuffer ob, ib;
    PRBool ibSet;
    int rc;

    // initial challenge
    if (PL_strcasecmp(challenge, "NTLM") == 0) {
        SEC_WINNT_AUTH_IDENTITY_W ident, *pIdent = NULL;
        TimeStamp useBefore;
        PRUnichar *buf = nsnull;

        if (user && pass && domain) {
            ident.User           = (PRUnichar *) user;
            ident.UserLength     = nsCRT::strlen(user);
            ident.Domain         = (PRUnichar *) domain;
            ident.DomainLength   = nsCRT::strlen(domain);
            ident.Password       = (PRUnichar *) pass;
            ident.PasswordLength = nsCRT::strlen(pass);
            ident.Flags          = SEC_WINNT_AUTH_IDENTITY_UNICODE;
            pIdent = &ident;

#ifdef DEBUG_darinf
            LOG(("  using supplied logon [domain=%s user=%s pass=%s]\n",
                 NS_LossyConvertUCS2toASCII(domain).get(),
                 NS_LossyConvertUCS2toASCII(user).get(),
                 NS_LossyConvertUCS2toASCII(pass).get()));
#endif
        }
        else
            LOG(("  using default windows logon\n"));

        // clear out any old state (in case user identity was wrong)
        if (state) {
            NS_RELEASE(state);
            *continuationState = nsnull;
            if (session)
                session->mDefaultLogonFailed = PR_TRUE;
        }
        state = new nsNTLMContinuationState(); // no addref yet

        rc = (gFT->AcquireCredentialsHandle)(NULL,                 // pszPrincipal
                                             "NTLM",               // pszPackage
                                             SECPKG_CRED_OUTBOUND, // fCredentialUse
                                             NULL,                 // pvLogonID
                                             pIdent,               // pAuthData
                                             NULL,                 // pGetKeyFn
                                             NULL,                 // pvGetKeyArgument
                                             &state->mCred,        // phCredential
                                             &useBefore);          // ptsExpiry
        LOG(("  AcquireCredentialsHandle returned [rc=%d]\n", rc));

        CRTFREEIF(buf);

        ctxIn = NULL;
        ibSet = PR_FALSE;
    }
    else {
        NS_ENSURE_TRUE(state, NS_ERROR_UNEXPECTED);

        // decode challenge; skip past "NTLM " to the start of the base64
        // encoded data.
        int len = strlen(challenge);
        if (len < 6)
            return NS_ERROR_UNEXPECTED; // bogus challenge
        challenge += 5;
        len -= 5;

        // decode into the input secbuffer
        ib.BufferType = SECBUFFER_TOKEN;
        ib.cbBuffer = (len * 3)/4;      // sufficient size (see plbase64.h)
        ib.pvBuffer = malloc(ib.cbBuffer);
        if (!ib.pvBuffer)
            return NS_ERROR_OUT_OF_MEMORY;

        if (PL_Base64Decode(challenge, len, (char *) ib.pvBuffer) == NULL) {
            free(ib.pvBuffer);
            return NS_ERROR_UNEXPECTED; // improper base64 encoding
        }
        ctxIn = &state->mCtx;
        ibSet = PR_TRUE;
    }

    DWORD ctxReq = ISC_REQ_REPLAY_DETECT |
                   ISC_REQ_SEQUENCE_DETECT |
                   ISC_REQ_CONFIDENTIALITY |
                   ISC_REQ_DELEGATE;
    DWORD ctxAttr;

    if (ibSet) {
        ibd.ulVersion = SECBUFFER_VERSION;
        ibd.cBuffers = 1;
        ibd.pBuffers = &ib;
    }

    obd.ulVersion = SECBUFFER_VERSION;
    obd.cBuffers = 1;
    obd.pBuffers = &ob; // just one buffer
    ob.BufferType = SECBUFFER_TOKEN; // preping a token here
    ob.cbBuffer = gMaxTokenLen;
    ob.pvBuffer = calloc(ob.cbBuffer, 1);

    rc = (gFT->InitializeSecurityContext)(&state->mCred,        // phCredential
                                          ctxIn,                // phContext
                                          NULL,                 // pszTargetName
                                          ctxReq,               // fContextReq
                                          0,                    // Reserved1
                                          SECURITY_NATIVE_DREP, // TargetDataRep
                                          ibSet ? &ibd : NULL,  // pInput
                                          0,                    // Reserved2
                                          &state->mCtx,         // phNewContext
                                          &obd,                 // pOutput
                                          &ctxAttr,             // pfContextAttr
                                          NULL);                // ptsExpiry
    LOG(("  InitializeSecurityContext returned [rc=%d:%s]\n", rc, MapErrorCode(rc)));
    
    if (rc == SEC_I_CONTINUE_NEEDED || rc == SEC_E_OK) {
        // base64 encode data in output buffer and prepend "NTLM "
        int credsLen = 5 + ((ob.cbBuffer + 2)/3)*4;
        *creds = (char *) malloc(credsLen + 1);
        if (*creds) {
            memcpy(*creds, "NTLM ", 5);
            PL_Base64Encode((char *) ob.pvBuffer, ob.cbBuffer, *creds + 5);
            (*creds)[credsLen] = '\0'; // null terminate
        }
    }

    if (ibSet)
        free(ib.pvBuffer);
    free(ob.pvBuffer);

    if (!*creds) {
        // destroy newly allocated state
        if (state && !*continuationState)
            delete state;
        return NS_ERROR_FAILURE;
    }

    // save newly allocated state
    if (state && !*continuationState)
        NS_ADDREF(*continuationState = state);

    return NS_OK;
}

NS_IMETHODIMP
nsHttpNTLMAuth::GetAuthFlags(PRUint32 *flags)
{
    *flags = CONNECTION_BASED | IDENTITY_INCLUDES_DOMAIN;
    return NS_OK;
}
