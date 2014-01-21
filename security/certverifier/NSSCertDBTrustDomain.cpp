/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NSSCertDBTrustDomain.h"

#include <stdint.h>

#include "insanity/ScopedPtr.h"
#include "cert.h"
#include "nss.h"
#include "ocsp.h"
#include "prerror.h"
#include "prprf.h"
#include "secerr.h"
#include "secmod.h"

using namespace insanity::pkix;

namespace mozilla { namespace psm {

const char BUILTIN_ROOTS_MODULE_DEFAULT_NAME[] = "Builtin Roots Module";

namespace {

inline void PORT_Free_string(char* str) { PORT_Free(str); }

typedef ScopedPtr<SECMODModule, SECMOD_DestroyModule> ScopedSECMODModule;

static char*
nss_addEscape(const char* string, char quote)
{
    char* newString = 0;
    int escapes = 0, size = 0;
    const char* src;
    char* dest;

    for (src=string; *src ; src++) {
        if ((*src == quote) || (*src == '\\')) {
          escapes++;
        }
        size++;
    }

    newString = (char*)PORT_ZAlloc(escapes+size+1);
    if (!newString) {
        return nullptr;
    }

    for (src=string, dest=newString; *src; src++,dest++) {
        if ((*src == quote) || (*src == '\\')) {
            *dest++ = '\\';
        }
        *dest = *src;
    }

    return newString;
}

} // unnamed namespace

SECStatus
InitializeNSS(const char* dir, bool readOnly)
{
  // The NSS_INIT_NOROOTINIT flag turns off the loading of the root certs
  // module by NSS_Initialize because we will load it in InstallLoadableRoots
  // later.  It also allows us to work around a bug in the system NSS in
  // Ubuntu 8.04, which loads any nonexistent "<configdir>/libnssckbi.so" as
  // "/usr/lib/nss/libnssckbi.so".
  uint32_t flags = NSS_INIT_NOROOTINIT | NSS_INIT_OPTIMIZESPACE;
  if (readOnly) {
    flags |= NSS_INIT_READONLY;
  }
  return ::NSS_Initialize(dir, "", "", SECMOD_DB, flags);
}

SECStatus
LoadLoadableRoots(/*optional*/ const char* dir, const char* modNameUTF8)
{
  PR_ASSERT(modNameUTF8);

  if (!modNameUTF8) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

  ScopedPtr<char, PR_FreeLibraryName> fullLibraryPath(
    PR_GetLibraryName(dir, "nssckbi"));
  if (!fullLibraryPath) {
    return SECFailure;
  }

  ScopedPtr<char, PORT_Free_string> escaped_fullLibraryPath(
    nss_addEscape(fullLibraryPath.get(), '\"'));
  if (!escaped_fullLibraryPath) {
    return SECFailure;
  }

  // If a module exists with the same name, delete it.
  int modType;
  SECMOD_DeleteModule(modNameUTF8, &modType);

  ScopedPtr<char, PR_smprintf_free> pkcs11ModuleSpec(
    PR_smprintf("name=\"%s\" library=\"%s\"", modNameUTF8,
                escaped_fullLibraryPath.get()));
  if (!pkcs11ModuleSpec) {
    return SECFailure;
  }

  ScopedSECMODModule rootsModule(SECMOD_LoadUserModule(pkcs11ModuleSpec.get(),
                                                       nullptr, false));
  if (!rootsModule) {
    return SECFailure;
  }

  if (!rootsModule->loaded) {
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  return SECSuccess;
}

void
UnloadLoadableRoots(const char* modNameUTF8)
{
  PR_ASSERT(modNameUTF8);
  ScopedSECMODModule rootsModule(SECMOD_FindModule(modNameUTF8));

  if (rootsModule) {
    SECMOD_UnloadUserModule(rootsModule.get());
  }
}

void
SetClassicOCSPBehavior(CertVerifier::ocsp_download_config enabled,
                       CertVerifier::ocsp_strict_config strict,
                       CertVerifier::ocsp_get_config get)
{
  CERT_DisableOCSPDefaultResponder(CERT_GetDefaultCertDB());
  if (enabled == CertVerifier::ocsp_off) {
    CERT_DisableOCSPChecking(CERT_GetDefaultCertDB());
  } else {
    CERT_EnableOCSPChecking(CERT_GetDefaultCertDB());
  }

  SEC_OcspFailureMode failureMode = strict == CertVerifier::ocsp_strict
                                  ? ocspMode_FailureIsVerificationFailure
                                  : ocspMode_FailureIsNotAVerificationFailure;
  (void) CERT_SetOCSPFailureMode(failureMode);

  CERT_ForcePostMethodForOCSP(get != CertVerifier::ocsp_get_enabled);

  int OCSPTimeoutSeconds = 3;
  if (strict == CertVerifier::ocsp_strict) {
    OCSPTimeoutSeconds = 10;
  }
  CERT_SetOCSPTimeout(OCSPTimeoutSeconds);
}

} } // namespace mozilla::psm
