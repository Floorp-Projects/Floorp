/* vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/storage.h"
#include "mozilla/storage/Variant.h"
#include "mozilla/mozalloc.h"
#include "nsString.h"
#include "SQLFunctions.h"
#include "nsUTF8Utils.h"
#include "plbase64.h"
#include "prio.h"
#if defined(XP_OS2)
#include "nsIRandomGenerator.h"
#endif

// The length of guids that are used by the download manager
#define GUID_LENGTH 12

namespace mozilla {
namespace downloads {

// Keep this file in sync with the GUID-related code in toolkit/places/SQLFunctions.cpp
// and toolkit/places/Helpers.cpp!

////////////////////////////////////////////////////////////////////////////////
//// GUID Creation Function

//////////////////////////////////////////////////////////////////////////////
//// GenerateGUIDFunction

/* static */
nsresult
GenerateGUIDFunction::create(mozIStorageConnection *aDBConn)
{
#if defined(XP_OS2)
  // We need this service to be initialized on the main thread because it is
  // not threadsafe.  We are about to use it asynchronously, so initialize it
  // now.
  nsCOMPtr<nsIRandomGenerator> rg =
    do_GetService("@mozilla.org/security/random-generator;1");
  NS_ENSURE_STATE(rg);
#endif

  nsRefPtr<GenerateGUIDFunction> function = new GenerateGUIDFunction();
  nsresult rv = aDBConn->CreateFunction(
    NS_LITERAL_CSTRING("generate_guid"), 0, function
  );
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMPL_ISUPPORTS1(
    GenerateGUIDFunction,
    mozIStorageFunction
)

static
nsresult
Base64urlEncode(const uint8_t* aBytes,
                uint32_t aNumBytes,
                nsCString& _result)
{
  // SetLength does not set aside space for null termination.  PL_Base64Encode
  // will not null terminate, however, nsCStrings must be null terminated.  As a
  // result, we set the capacity to be one greater than what we need, and the
  // length to our desired length.
  uint32_t length = (aNumBytes + 2) / 3 * 4; // +2 due to integer math.
  NS_ENSURE_TRUE(_result.SetCapacity(length + 1, mozilla::fallible_t()),
                 NS_ERROR_OUT_OF_MEMORY);
  _result.SetLength(length);
  (void)PL_Base64Encode(reinterpret_cast<const char*>(aBytes), aNumBytes,
                        _result.BeginWriting());

  // base64url encoding is defined in RFC 4648.  It replaces the last two
  // alphabet characters of base64 encoding with '-' and '_' respectively.
  _result.ReplaceChar('+', '-');
  _result.ReplaceChar('/', '_');
  return NS_OK;
}

#ifdef XP_WIN
// Included here because windows.h conflicts with the use of mozIStorageError
// above.
#include <windows.h>
#include <wincrypt.h>
#endif

static
nsresult
GenerateRandomBytes(uint32_t aSize,
                    uint8_t* _buffer)
{
  // On Windows, we'll use its built-in cryptographic API.
#if defined(XP_WIN)
  HCRYPTPROV cryptoProvider;
  BOOL rc = CryptAcquireContext(&cryptoProvider, 0, 0, PROV_RSA_FULL,
                                CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
  if (rc) {
    rc = CryptGenRandom(cryptoProvider, aSize, _buffer);
    (void)CryptReleaseContext(cryptoProvider, 0);
  }
  return rc ? NS_OK : NS_ERROR_FAILURE;

  // On Unix, we'll just read in from /dev/urandom.
#elif defined(XP_UNIX)
  NS_ENSURE_ARG_MAX(aSize, INT32_MAX);
  PRFileDesc* urandom = PR_Open("/dev/urandom", PR_RDONLY, 0);
  nsresult rv = NS_ERROR_FAILURE;
  if (urandom) {
    int32_t bytesRead = PR_Read(urandom, _buffer, aSize);
    if (bytesRead == static_cast<int32_t>(aSize)) {
      rv = NS_OK;
    }
    (void)PR_Close(urandom);
  }
  return rv;
#elif defined(XP_OS2)
  nsCOMPtr<nsIRandomGenerator> rg =
      do_GetService("@mozilla.org/security/random-generator;1");
  NS_ENSURE_STATE(rg);

  uint8_t* temp;
  nsresult rv = rg->GenerateRandomBytes(aSize, &temp);
  NS_ENSURE_SUCCESS(rv, rv);
  memcpy(_buffer, temp, aSize);
  NS_Free(temp);
  return NS_OK;
#endif
}

nsresult
GenerateGUID(nsCString& _guid)
{
  _guid.Truncate();

  // Request raw random bytes and base64url encode them.  For each set of three
  // bytes, we get one character.
  const uint32_t kRequiredBytesLength =
    static_cast<uint32_t>(GUID_LENGTH / 4 * 3);

  uint8_t buffer[kRequiredBytesLength];
  nsresult rv = GenerateRandomBytes(kRequiredBytesLength, buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = Base64urlEncode(buffer, kRequiredBytesLength, _guid);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(_guid.Length() == GUID_LENGTH, "GUID is not the right size!");
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////
//// mozIStorageFunction

NS_IMETHODIMP
GenerateGUIDFunction::OnFunctionCall(mozIStorageValueArray *aArguments,
                                     nsIVariant **_result)
{
  nsAutoCString guid;
  nsresult rv = GenerateGUID(guid);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*_result = new mozilla::storage::UTF8TextVariant(guid));
  return NS_OK;
}

} // namespace mozilla
} // namespace downloads
