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
 * The Original Code is Startup Cache.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2009-2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Benedict Hsieh <bhsieh@mozilla.com>
 *  Taras Glek <tglek@mozilla.com>
 *  Mike Hommey <mh@glandium.org>
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

#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsIStringStream.h"
#include "nsNetUtil.h"
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
NewObjectInputStreamFromBuffer(char* buffer, PRUint32 len, 
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

  nsresult rv = NS_NewStorageStream(256, PR_UINT32_MAX, getter_AddRefs(storageStream));
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
                           char** buffer, PRUint32* len) 
{
  nsresult rv;
  nsCOMPtr<nsIInputStream> inputStream;
  rv = storageStream->NewInputStream(0, getter_AddRefs(inputStream));
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRUint32 avail, read;
  rv = inputStream->Available(&avail);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsAutoArrayPtr<char> temp (new char[avail]);
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
canonicalizeBase(nsCAutoString &spec,
                 nsACString &out,
                 mozilla::Omnijar::Type aType)
{
    nsCAutoString base;
    nsresult rv = mozilla::Omnijar::GetURIString(aType, base);

    if (NS_FAILED(rv) || !base.Length())
        return PR_FALSE;

    if (base.Compare(spec.get(), PR_FALSE, base.Length()))
        return PR_FALSE;

    out.Append("/resource/");
    out.Append(baseName[aType]);
    out.Append(Substring(spec, base.Length()));
    return PR_TRUE;
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
    nsCAutoString spec;

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

        rv = ioService->NewURI(spec, nsnull, nsnull, getter_AddRefs(uri));
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

    if (!canonicalizeBase(spec, out, mozilla::Omnijar::GRE) &&
        !canonicalizeBase(spec, out, mozilla::Omnijar::APP)) {
        if (NS_SUCCEEDED(uri->SchemeIs("file", &equals)) && equals) {
            nsCOMPtr<nsIFileURL> baseFileURL;
            baseFileURL = do_QueryInterface(uri, &rv);
            NS_ENSURE_SUCCESS(rv, rv);

            nsCAutoString path;
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

            nsCAutoString path;
            rv = jarURI->GetJAREntry(path);
            NS_ENSURE_SUCCESS(rv, rv);
            out.Append("/");
            out.Append(path);
        } else { // Very unlikely
            nsCAutoString spec;
            rv = uri->GetSpec(spec);
            NS_ENSURE_SUCCESS(rv, rv);

            out.Append("/");
            out.Append(spec);
        }
    }
    return NS_OK;
}

}
}
