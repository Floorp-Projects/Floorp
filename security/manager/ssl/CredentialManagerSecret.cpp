/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CredentialManagerSecret.h"

#include <windows.h>
#include <wincred.h>

#include "mozilla/Logging.h"
#include "mozilla/SyncRunnable.h"

// This is the implementation of CredentialManagerSecretSecret, an instantiation
// of OSKeyStore for Windows. It uses the system credential manager, hence the
// name.

using namespace mozilla;

LazyLogModule gCredentialManagerSecretLog("credentialmanagersecret");
struct ScopedDelete {
  void operator()(CREDENTIALA* cred) { CredFree(cred); }
};

template <class T>
struct ScopedMaybeDelete {
  void operator()(T* ptr) {
    if (ptr) {
      ScopedDelete del;
      del(ptr);
    }
  }
};
typedef std::unique_ptr<CREDENTIALA, ScopedMaybeDelete<CREDENTIALA>>
    ScopedCREDENTIALA;

CredentialManagerSecret::CredentialManagerSecret() {}

CredentialManagerSecret::~CredentialManagerSecret() {}

nsresult CredentialManagerSecret::StoreSecret(const nsACString& aSecret,
                                              const nsACString& aLabel) {
  if (aSecret.Length() > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
    // Windows doesn't allow blobs larger than CRED_MAX_CREDENTIAL_BLOB_SIZE
    // bytes.
    MOZ_LOG(gCredentialManagerSecretLog, LogLevel::Debug,
            ("StoreSecret secret must not be larger than 512 bytes (got %zd)",
             aSecret.Length()));
    return NS_ERROR_FAILURE;
  }
  CREDENTIALA cred = {0};
  cred.Type = CRED_TYPE_GENERIC;
  const nsCString& label = PromiseFlatCString(aLabel);
  cred.TargetName = const_cast<LPSTR>(label.get());
  cred.CredentialBlobSize = aSecret.Length();
  const nsCString& secret = PromiseFlatCString(aSecret);
  cred.CredentialBlob = (LPBYTE)secret.get();
  cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
  cred.UserName = const_cast<char*>("");  // -Wwritable-strings

  // https://docs.microsoft.com/en-us/windows/desktop/api/wincred/nf-wincred-credwritea
  BOOL ok = CredWriteA(&cred, 0);
  if (!ok) {
    MOZ_LOG(gCredentialManagerSecretLog, LogLevel::Debug,
            ("CredWriteW failed %lu", GetLastError()));
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult CredentialManagerSecret::DeleteSecret(const nsACString& aLabel) {
  // https://docs.microsoft.com/en-us/windows/desktop/api/wincred/nf-wincred-creddeletea
  const nsCString& label = PromiseFlatCString(aLabel);
  BOOL ok = CredDeleteA(label.get(), CRED_TYPE_GENERIC, 0);
  int error = GetLastError();
  if (!ok && error != ERROR_NOT_FOUND) {
    MOZ_LOG(gCredentialManagerSecretLog, LogLevel::Debug,
            ("CredDeleteA failed %d", error));
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult CredentialManagerSecret::RetrieveSecret(
    const nsACString& aLabel,
    /* out */ nsACString& aSecret) {
  aSecret.Truncate();
  PCREDENTIALA pcred_raw = nullptr;
  const nsCString& label = PromiseFlatCString(aLabel);
  // https://docs.microsoft.com/en-us/windows/desktop/api/wincred/nf-wincred-credreada
  BOOL ok = CredReadA(label.get(), CRED_TYPE_GENERIC, 0, &pcred_raw);
  ScopedCREDENTIALA pcred(pcred_raw);
  if (!ok) {
    MOZ_LOG(gCredentialManagerSecretLog, LogLevel::Debug,
            ("CredReadA failed %lu", GetLastError()));
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(pcred);
  aSecret.Assign(reinterpret_cast<const char*>(pcred->CredentialBlob),
                 pcred->CredentialBlobSize);
  return NS_OK;
}
