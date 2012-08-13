#ifndef mozilla_osfileutils_h__
#define mozilla_osfileutils_h__

#include "mozilla/Types.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"

extern "C" {

// Memory utilities

/**
 * As |NS_Free|, but exported.
 */
MOZ_EXPORT_API(void) osfile_ns_free(void* buf);

// Unicode utilities

/**
 * Duplicate a Unicode string, as per wpcpy/StrDupW.
 *
 * @param source A well-formed, nul-terminated, Unicode string.
 *
 * @return Either |NULL| if there was not enough memory to copy the
 * string, or a new string with the same contents as |source|.
 * Memory MUST be released with |osfile_ns_free|.
 */
MOZ_EXPORT_API(PRUnichar*) osfile_wstrdup(PRUnichar* source);

/**
 * Decode a nul-terminated C string into a Unicode string.
 *
 * @param aEncoding The encoding to use.
 * @param aSource The C string to decode.
 * @param aBytesToDecode The number of bytes to decode from |aSource|.
 *
 * @return null in case of error, otherwise a sequence of Unicode
 *   chars, representing |aSource|. This memory MUST be released with
 *   |NS_Free|/|osfile_ns_free|.
 */
MOZ_EXPORT_API(PRUnichar*) osfile_DecodeAll(
   const char* aEncoding,
   const char* aSource,
   const int32_t aBytesToDecode);

/**
 * Encode a complete Unicode string into a set of bytes.
 *
 * @param aEncoding The encoding to use.
 * @param aSource The Unicode string to encode. Must be nul-terminated.
 * @param aBytesWritten (out) The number of bytes encoded.
 *
 * @return null in case of error, otherwise a new buffer.  The
 * number of bytes actually allocated may be higher than
 * |aBytesWritten|.  The buffer MUST be released with
 * |NS_Free|/|osfile_ns_free|.
 */
MOZ_EXPORT_API(char*) osfile_EncodeAll(
   const char* aEncoding,
   const PRUnichar* aSource,
   int32_t* aBytesWritten);

} // extern "C"

#endif // mozilla_osfileutils_h__
