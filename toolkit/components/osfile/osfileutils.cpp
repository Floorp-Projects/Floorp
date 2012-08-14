
#include "mozilla/Scoped.h"

#include "osfileutils.h"
#include "nsICharsetConverterManager.h"
#include "nsServiceManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsCRTGlue.h"

// Utilities for handling errors
namespace {

#if defined(XP_WIN)
#include <Windows.h>

/**
 * Set the OS-specific error to inform the OS that
 * the last operation failed because it is not supported.
 */
void error_not_supported() {
  SetLastError(ERROR_NOT_SUPPORTED);
}

/**
 * Set the OS-specific error to inform the OS that
 * the last operation failed because of an invalid
 * argument.
 */
void error_invalid_argument() {
  SetLastError(ERROR_INVALID_DATA);
}

/**
 * Set the OS-specific error to inform the OS that
 * the last operation failed because of insufficient
 * memory.
 */
void error_no_memory() {
  SetLastError(ERROR_NOT_ENOUGH_MEMORY);
}

#else

#include "errno.h"

/**
 * Set the OS-specific error to inform the OS that
 * the last operation failed because it is not supported.
 */
void error_not_supported() {
  errno = ENOTSUP;
}

/**
 * Set the OS-specific error to inform the OS that
 * the last operation failed because of an invalid
 * argument.
 */
void error_invalid_argument() {
  errno = EINVAL;
}

/**
 * Set the OS-specific error to inform the OS that
 * the last operation failed because of insufficient
 * memory.
 */
void error_no_memory() {
  errno = ENOMEM;
}

#endif // defined(XP_WIN)

}


extern "C" {

// Memory utilities

MOZ_EXPORT_API(void) osfile_ns_free(void* buf) {
  NS_Free(buf);
}

// Unicode utilities

MOZ_EXPORT_API(PRUnichar*) osfile_wstrdup(PRUnichar* string) {
  return NS_strdup(string);
}

MOZ_EXPORT_API(PRUnichar*) osfile_DecodeAll(
   const char* aEncoding,
   const char* aSource,
   const int32_t aBytesToDecode)
{
  if (!aEncoding || !aSource) {
    error_invalid_argument();
    return nullptr;
  }

  nsresult rv;
  nsCOMPtr<nsICharsetConverterManager> manager =
    do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    error_not_supported();
    return nullptr;
  }

  nsCOMPtr<nsIUnicodeDecoder> decoder;
  rv = manager->GetUnicodeDecoder(aEncoding, getter_AddRefs(decoder));
  if (NS_FAILED(rv)) {
    error_invalid_argument();
    return nullptr;
  }

  // Compute an upper bound to the number of chars, allocate buffer

  int32_t srcBytes = aBytesToDecode;
  int32_t upperBoundChars = 0;
  rv = decoder->GetMaxLength(aSource, srcBytes, &upperBoundChars);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  int32_t bufSize = (upperBoundChars + 1) * sizeof (PRUnichar);

  mozilla::ScopedFreePtr<PRUnichar> dest((PRUnichar*)NS_Alloc(bufSize));
  if (dest.get() == nullptr) {
    error_no_memory();
    return nullptr;
  }

  // Convert, add trailing \0

  rv = decoder->Convert(aSource, &srcBytes, dest.rwget(), &upperBoundChars);
  if (NS_FAILED(rv)) {
    error_invalid_argument();
    return nullptr;
  }

  dest.rwget()[upperBoundChars] = '\0';

  return dest.forget();
}

MOZ_EXPORT_API(char*) osfile_EncodeAll(
   const char* aEncoding,
   const PRUnichar* aSource,
   int32_t* aBytesProduced)
{
  if (!aEncoding || !aSource || !aBytesProduced) {
    error_invalid_argument();
    return nullptr;
  }

  nsresult rv;
  nsCOMPtr<nsICharsetConverterManager> manager =
    do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    error_not_supported();
    return nullptr;
  }

  nsCOMPtr<nsIUnicodeEncoder> encoder;
  rv = manager->GetUnicodeEncoder(aEncoding, getter_AddRefs(encoder));
  if (NS_FAILED(rv)) {
    error_invalid_argument();
    return nullptr;
  }

  int32_t srcChars = NS_strlen(aSource);

  int32_t upperBoundBytes = 0;
  rv = encoder->GetMaxLength(aSource, srcChars, &upperBoundBytes);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  printf_stderr("Encoding %d chars into at up to %d bytes\n", srcChars, upperBoundBytes);
  int32_t bufSize = upperBoundBytes;
  mozilla::ScopedFreePtr<char> dest((char*)NS_Alloc(bufSize));

  if (dest.get() == nullptr) {
    error_no_memory();
    return nullptr;
  }

  rv = encoder->Convert(aSource, &srcChars, dest.rwget(), &upperBoundBytes);
  if (NS_FAILED(rv)) {
    error_invalid_argument();
    return nullptr;
  }

  *aBytesProduced = upperBoundBytes;
  return dest.forget();
}

} // extern "C"
