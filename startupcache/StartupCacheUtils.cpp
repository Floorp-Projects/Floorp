/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsIStringStream.h"
#include "nsNetUtil.h"
#include "nsIFileURL.h"
#include "nsIJARURI.h"
#include "nsIResProtocolHandler.h"
#include "nsIChromeRegistry.h"
#include "nsAutoPtr.h"
#include "StartupCacheUtils.h"
#include "mozilla/scache/StartupCache.h"
#include "mozilla/Omnijar.h"

namespace mozilla {
namespace scache {

NS_EXPORT nsresult
NewObjectInputStreamFromBuffer(char* buffer, uint32_t len, 
                               nsIObjectInputStream** stream)
{
  nsCOMPtr<nsIStringInputStream> stringStream
    = do_CreateInstance("@mozilla.org/io/string-input-stream;1");
  nsCOMPtr<nsIObjectInputStream> objectInput 
    = do_CreateInstance("@mozilla.org/binaryinputstream;1");
  
  stringStream->AdoptData(buffer, len);
  objectInput->SetInputStream(stringStream);
  
  objectInput.forget(stream);
  return NS_OK;
}

NS_EXPORT nsresult
NewObjectOutputWrappedStorageStream(nsIObjectOutputStream **wrapperStream,
                                    nsIStorageStream** stream,
                                    bool wantDebugStream)
{
  nsCOMPtr<nsIStorageStream> storageStream;

  nsresult rv = NS_NewStorageStream(256, UINT32_MAX, getter_AddRefs(storageStream));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObjectOutputStream> objectOutput
    = do_CreateInstance("@mozilla.org/binaryoutputstream;1");
  nsCOMPtr<nsIOutputStream> outputStream
    = do_QueryInterface(storageStream);
  
  objectOutput->SetOutputStream(outputStream);
  
#ifdef DEBUG
  if (wantDebugStream) {
    // Wrap in debug stream to detect unsupported writes of 
    // multiply-referenced non-singleton objects
    StartupCache* sc = StartupCache::GetSingleton();
    NS_ENSURE_TRUE(sc, NS_ERROR_UNEXPECTED);
    nsCOMPtr<nsIObjectOutputStream> debugStream;
    sc->GetDebugObjectOutputStream(objectOutput, getter_AddRefs(debugStream));
    debugStream.forget(wrapperStream);
  } else {
    objectOutput.forget(wrapperStream);
  }
#else
  objectOutput.forget(wrapperStream);
#endif
  
  storageStream.forget(stream);
  return NS_OK;
}

NS_EXPORT nsresult
NewBufferFromStorageStream(nsIStorageStream *storageStream, 
                           char** buffer, uint32_t* len) 
{
  nsresult rv;
  nsCOMPtr<nsIInputStream> inputStream;
  rv = storageStream->NewInputStream(0, getter_AddRefs(inputStream));
  NS_ENSURE_SUCCESS(rv, rv);
  
  uint64_t avail64;
  rv = inputStream->Available(&avail64);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(avail64 <= UINT32_MAX, NS_ERROR_FILE_TOO_BIG);

  uint32_t avail = (uint32_t)avail64;
  nsAutoArrayPtr<char> temp (new char[avail]);
  uint32_t read;
  rv = inputStream->Read(temp, avail, &read);
  if (NS_SUCCEEDED(rv) && avail != read)
    rv = NS_ERROR_UNEXPECTED;
  
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  *len = avail;
  *buffer = temp.forget();
  return NS_OK;
}

static const char baseName[2][5] = { "gre/", "app/" };

static inline bool
canonicalizeBase(nsAutoCString &spec,
                 nsACString &out)
{
    nsAutoCString greBase, appBase;
    nsresult rv = mozilla::Omnijar::GetURIString(mozilla::Omnijar::GRE, greBase);
    if (NS_FAILED(rv) || !greBase.Length())
        return false;

    rv = mozilla::Omnijar::GetURIString(mozilla::Omnijar::APP, appBase);
    if (NS_FAILED(rv))
        return false;

    bool underGre = !greBase.Compare(spec.get(), false, greBase.Length());
    bool underApp = appBase.Length() &&
                    !appBase.Compare(spec.get(), false, appBase.Length());

    if (!underGre && !underApp)
        return false;

    /**
     * At this point, if both underGre and underApp are true, it can be one
     * of the two following cases:
     * - the GRE directory points to a subdirectory of the APP directory,
     *   meaning spec points under GRE.
     * - the APP directory points to a subdirectory of the GRE directory,
     *   meaning spec points under APP.
     * Checking the GRE and APP path length is enough to know in which case
     * we are.
     */
    if (underGre && underApp && greBase.Length() < appBase.Length())
        underGre = false;

    out.AppendLiteral("/resource/");
    out.Append(baseName[underGre ? mozilla::Omnijar::GRE : mozilla::Omnijar::APP]);
    out.Append(Substring(spec, underGre ? greBase.Length() : appBase.Length()));
    return true;
}

/**
 * PathifyURI transforms uris into useful zip paths
 * to make it easier to manipulate startup cache entries
 * using standard zip tools.
 * Transformations applied:
 *  * resource:// URIs are resolved to their corresponding file/jar URI to
 *    canonicalize resources URIs other than gre and app.
 *  * Paths under GRE or APP directory have their base path replaced with
 *    resource/gre or resource/app to avoid depending on install location.
 *  * jar:file:///path/to/file.jar!/sub/path urls are replaced with
 *    /path/to/file.jar/sub/path
 *
 *  The result is appended to the string passed in. Adding a prefix before
 *  calling is recommended to avoid colliding with other cache users.
 *
 * For example, in the js loader (string is prefixed with jsloader by caller):
 *  resource://gre/modules/XPCOMUtils.jsm or
 *  file://$GRE_DIR/modules/XPCOMUtils.jsm or
 *  jar:file://$GRE_DIR/omni.jar!/modules/XPCOMUtils.jsm becomes
 *     jsloader/resource/gre/modules/XPCOMUtils.jsm
 *  file://$PROFILE_DIR/extensions/{uuid}/components/component.js becomes
 *     jsloader/$PROFILE_DIR/extensions/%7Buuid%7D/components/component.js
 *  jar:file://$PROFILE_DIR/extensions/some.xpi!/components/component.js becomes
 *     jsloader/$PROFILE_DIR/extensions/some.xpi/components/component.js
 */
NS_EXPORT nsresult
PathifyURI(nsIURI *in, nsACString &out)
{
    bool equals;
    nsresult rv;
    nsCOMPtr<nsIURI> uri = in;
    nsAutoCString spec;

    // Resolve resource:// URIs. At the end of this if/else block, we
    // have both spec and uri variables identifying the same URI.
    if (NS_SUCCEEDED(in->SchemeIs("resource", &equals)) && equals) {
        nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIProtocolHandler> ph;
        rv = ioService->GetProtocolHandler("resource", getter_AddRefs(ph));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIResProtocolHandler> irph(do_QueryInterface(ph, &rv));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = irph->ResolveURI(in, spec);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = ioService->NewURI(spec, nullptr, nullptr, getter_AddRefs(uri));
        NS_ENSURE_SUCCESS(rv, rv);
    } else {
        if (NS_SUCCEEDED(in->SchemeIs("chrome", &equals)) && equals) {
            nsCOMPtr<nsIChromeRegistry> chromeReg =
                mozilla::services::GetChromeRegistryService();
            if (!chromeReg)
                return NS_ERROR_UNEXPECTED;

            rv = chromeReg->ConvertChromeURL(in, getter_AddRefs(uri));
            NS_ENSURE_SUCCESS(rv, rv);
        }

        rv = uri->GetSpec(spec);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    if (!canonicalizeBase(spec, out)) {
        if (NS_SUCCEEDED(uri->SchemeIs("file", &equals)) && equals) {
            nsCOMPtr<nsIFileURL> baseFileURL;
            baseFileURL = do_QueryInterface(uri, &rv);
            NS_ENSURE_SUCCESS(rv, rv);

            nsAutoCString path;
            rv = baseFileURL->GetPath(path);
            NS_ENSURE_SUCCESS(rv, rv);

            out.Append(path);
        } else if (NS_SUCCEEDED(uri->SchemeIs("jar", &equals)) && equals) {
            nsCOMPtr<nsIJARURI> jarURI = do_QueryInterface(uri, &rv);
            NS_ENSURE_SUCCESS(rv, rv);

            nsCOMPtr<nsIURI> jarFileURI;
            rv = jarURI->GetJARFile(getter_AddRefs(jarFileURI));
            NS_ENSURE_SUCCESS(rv, rv);

            rv = PathifyURI(jarFileURI, out);
            NS_ENSURE_SUCCESS(rv, rv);

            nsAutoCString path;
            rv = jarURI->GetJAREntry(path);
            NS_ENSURE_SUCCESS(rv, rv);
            out.Append('/');
            out.Append(path);
        } else { // Very unlikely
            nsAutoCString spec;
            rv = uri->GetSpec(spec);
            NS_ENSURE_SUCCESS(rv, rv);

            out.Append('/');
            out.Append(spec);
        }
    }
    return NS_OK;
}

} // namespace scache
} // namespace mozilla
