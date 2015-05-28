#ifndef mozilla_psm__CryptoUtil_h
#define mozilla_psm__CryptoUtil_h

#include <mozilla/RefPtr.h>
#include "mozilla/Types.h"

#ifdef MOZILLA_INTERNAL_API
#define MOZ_CRYPTO_API(x) MOZ_EXPORT_API(x)
#else
#define MOZ_CRYPTO_API(x) MOZ_IMPORT_API(x)
#endif

#endif // mozilla_psm__CryptoUtil_h
