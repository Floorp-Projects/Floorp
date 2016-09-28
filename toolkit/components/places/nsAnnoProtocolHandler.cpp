//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Implementation of moz-anno: URLs for accessing favicons.  The urls are sent
 * to the favicon service.  If the favicon service doesn't have the
 * data, a stream containing the default favicon will be returned.
 *
 * The reference to annotations ("moz-anno") is a leftover from previous
 * iterations of this component. As of now the moz-anno protocol is independent
 * of annotations.
 */

#include "nsAnnoProtocolHandler.h"
#include "nsFaviconService.h"
#include "nsIChannel.h"
#include "nsIInputStreamChannel.h"
#include "nsILoadGroup.h"
#include "nsIStandardURL.h"
#include "nsIStringStream.h"
#include "nsIInputStream.h"
#include "nsISupportsUtils.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIOutputStream.h"
#include "nsInputStreamPump.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsStringStream.h"
#include "SimpleChannel.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/storage.h"
#include "Helpers.h"
#include "FaviconHelpers.h"

using namespace mozilla;
using namespace mozilla::places;

////////////////////////////////////////////////////////////////////////////////
//// Global Functions

/**
 * Creates a channel to obtain the default favicon.
 */
static
nsresult
GetDefaultIcon(nsILoadInfo *aLoadInfo, nsIChannel **aChannel)
{
  nsCOMPtr<nsIURI> defaultIconURI;
  nsresult rv = NS_NewURI(getter_AddRefs(defaultIconURI),
                          NS_LITERAL_CSTRING(FAVICON_DEFAULT_URL));
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_NewChannelInternal(aChannel, defaultIconURI, aLoadInfo);
}

////////////////////////////////////////////////////////////////////////////////
//// faviconAsyncLoader

namespace {

/**
 * An instance of this class is passed to the favicon service as the callback
 * for getting favicon data from the database.  We'll get this data back in
 * HandleResult, and on HandleCompletion, we'll close our output stream which
 * will close the original channel for the favicon request.
 *
 * However, if an error occurs at any point and we don't have mData, we will
 * just fallback to the default favicon.  If anything happens at that point, the
 * world must be against us, so we can do nothing.
 */
class faviconAsyncLoader : public AsyncStatementCallback
{
public:
  faviconAsyncLoader(nsIChannel *aChannel, nsIStreamListener *aListener)
    : mChannel(aChannel)
    , mListener(aListener)
  {
    MOZ_ASSERT(aChannel, "Not providing a channel will result in crashes!");
    MOZ_ASSERT(aListener, "Not providing a stream listener will result in crashes!");
    // Set the default content type.
    Unused << mChannel->SetContentType(NS_LITERAL_CSTRING(PNG_MIME_TYPE));
  }

  //////////////////////////////////////////////////////////////////////////////
  //// mozIStorageStatementCallback

  NS_IMETHOD HandleResult(mozIStorageResultSet *aResultSet) override
  {
    nsCOMPtr<mozIStorageRow> row;
    while (NS_SUCCEEDED(aResultSet->GetNextRow(getter_AddRefs(row))) && row) {
      // TODO: For now just return the biggest icon, that is the first one.
      // Later this should allow to return a specific size.
      if (!mData.IsEmpty()) {
        return NS_OK;
      }

      int32_t width;
      nsresult rv = row->GetInt32(1, &width);
      NS_ENSURE_SUCCESS(rv, rv);

      // Eventually override the default mimeType for svg.
      if (width == UINT16_MAX) {
        rv = mChannel->SetContentType(NS_LITERAL_CSTRING(SVG_MIME_TYPE));
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Obtain the binary blob that contains our favicon data.
      uint8_t *data;
      uint32_t dataLen;
      rv = row->GetBlob(0, &dataLen, &data);
      NS_ENSURE_SUCCESS(rv, rv);
      mData.Adopt(TO_CHARBUFFER(data), dataLen);
    }

    return NS_OK;
  }

  NS_IMETHOD HandleCompletion(uint16_t aReason) override
  {
    MOZ_DIAGNOSTIC_ASSERT(mListener);
    NS_ENSURE_TRUE(mListener, NS_ERROR_UNEXPECTED);

    nsresult rv;
    // Ensure we'll break possible cycles with the listener.
    auto cleanup = MakeScopeExit([&] () {
      mListener = nullptr;
    });

    if (!mData.IsEmpty()) {
      nsCOMPtr<nsIInputStream> stream;
      rv = NS_NewCStringInputStream(getter_AddRefs(stream), mData);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      if (NS_SUCCEEDED(rv)) {
        RefPtr<nsInputStreamPump> pump;
        rv = nsInputStreamPump::Create(getter_AddRefs(pump), stream, -1, -1, 0, 0,
                                      true);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
        if (NS_SUCCEEDED(rv)) {
          return pump->AsyncRead(mListener, nullptr);
        }
      }
    }

    // Fallback to the default favicon.
    // we should pass the loadInfo of the original channel along
    // to the new channel. Note that mChannel can not be null,
    // constructor checks that.
    nsCOMPtr<nsILoadInfo> loadInfo = mChannel->GetLoadInfo();
    nsCOMPtr<nsIChannel> newChannel;
    rv = GetDefaultIcon(loadInfo, getter_AddRefs(newChannel));
    if (NS_FAILED(rv)) {
      mListener->OnStartRequest(mChannel, nullptr);
      mListener->OnStopRequest(mChannel, nullptr, rv);
      return rv;
    }
    return newChannel->AsyncOpen2(mListener);
  }

protected:
  virtual ~faviconAsyncLoader() {}

private:
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIStreamListener> mListener;
  nsCString mData;
};

} // namespace

////////////////////////////////////////////////////////////////////////////////
//// nsAnnoProtocolHandler

NS_IMPL_ISUPPORTS(nsAnnoProtocolHandler, nsIProtocolHandler)

// nsAnnoProtocolHandler::GetScheme

NS_IMETHODIMP
nsAnnoProtocolHandler::GetScheme(nsACString& aScheme)
{
  aScheme.AssignLiteral("moz-anno");
  return NS_OK;
}


// nsAnnoProtocolHandler::GetDefaultPort
//
//    There is no default port for annotation URLs

NS_IMETHODIMP
nsAnnoProtocolHandler::GetDefaultPort(int32_t *aDefaultPort)
{
  *aDefaultPort = -1;
  return NS_OK;
}


// nsAnnoProtocolHandler::GetProtocolFlags

NS_IMETHODIMP
nsAnnoProtocolHandler::GetProtocolFlags(uint32_t *aProtocolFlags)
{
  *aProtocolFlags = (URI_NORELATIVE | URI_NOAUTH | URI_DANGEROUS_TO_LOAD |
                     URI_IS_LOCAL_RESOURCE);
  return NS_OK;
}


// nsAnnoProtocolHandler::NewURI

NS_IMETHODIMP
nsAnnoProtocolHandler::NewURI(const nsACString& aSpec,
                              const char *aOriginCharset,
                              nsIURI *aBaseURI, nsIURI **_retval)
{
  nsCOMPtr <nsIURI> uri = do_CreateInstance(NS_SIMPLEURI_CONTRACTID);
  if (!uri)
    return NS_ERROR_OUT_OF_MEMORY;
  nsresult rv = uri->SetSpec(aSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = nullptr;
  uri.swap(*_retval);
  return NS_OK;
}


// nsAnnoProtocolHandler::NewChannel
//

NS_IMETHODIMP
nsAnnoProtocolHandler::NewChannel2(nsIURI* aURI,
                                   nsILoadInfo* aLoadInfo,
                                   nsIChannel** _retval)
{
  NS_ENSURE_ARG_POINTER(aURI);

  // annotation info
  nsCOMPtr<nsIURI> annoURI;
  nsAutoCString annoName;
  nsresult rv = ParseAnnoURI(aURI, getter_AddRefs(annoURI), annoName);
  NS_ENSURE_SUCCESS(rv, rv);

  // Only favicon annotation are supported.
  if (!annoName.EqualsLiteral(FAVICON_ANNOTATION_NAME))
    return NS_ERROR_INVALID_ARG;

  return NewFaviconChannel(aURI, annoURI, aLoadInfo, _retval);
}

NS_IMETHODIMP
nsAnnoProtocolHandler::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
  return NewChannel2(aURI, nullptr, _retval);
}


// nsAnnoProtocolHandler::AllowPort
//
//    Don't override any bans on bad ports.

NS_IMETHODIMP
nsAnnoProtocolHandler::AllowPort(int32_t port, const char *scheme,
                                 bool *_retval)
{
  *_retval = false;
  return NS_OK;
}


// nsAnnoProtocolHandler::ParseAnnoURI
//
//    Splits an annotation URL into its URI and name parts

nsresult
nsAnnoProtocolHandler::ParseAnnoURI(nsIURI* aURI,
                                    nsIURI** aResultURI, nsCString& aName)
{
  nsresult rv;
  nsAutoCString path;
  rv = aURI->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t firstColon = path.FindChar(':');
  if (firstColon <= 0)
    return NS_ERROR_MALFORMED_URI;

  rv = NS_NewURI(aResultURI, Substring(path, firstColon + 1));
  NS_ENSURE_SUCCESS(rv, rv);

  aName = Substring(path, 0, firstColon);
  return NS_OK;
}

nsresult
nsAnnoProtocolHandler::NewFaviconChannel(nsIURI *aURI, nsIURI *aAnnotationURI,
                                         nsILoadInfo* aLoadInfo, nsIChannel **_channel)
{
  // Create our channel.  We'll call SetContentType with the right type when
  // we know what it actually is.
  nsCOMPtr<nsIChannel> channel = NS_NewSimpleChannel(
    aURI, aLoadInfo, aAnnotationURI,
    [] (nsIStreamListener* listener, nsIChannel* channel, nsIURI* annotationURI) {
      auto fallback = [&] () -> RequestOrReason {
        nsCOMPtr<nsIChannel> chan;
        nsCOMPtr<nsILoadInfo> loadInfo = channel->GetLoadInfo();
        nsresult rv = GetDefaultIcon(loadInfo, getter_AddRefs(chan));
        NS_ENSURE_SUCCESS(rv, Err(rv));

        rv = chan->AsyncOpen2(listener);
        NS_ENSURE_SUCCESS(rv, Err(rv));

        return RequestOrReason(chan.forget());
      };

      // Now we go ahead and get our data asynchronously for the favicon.
      nsCOMPtr<mozIStorageStatementCallback> callback =
        new faviconAsyncLoader(channel, listener);

      nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
      // Any failures fallback to the default icon channel.
      if (!callback || !faviconService)
        return fallback();

      nsresult rv = faviconService->GetFaviconDataAsync(annotationURI, callback);
      if (NS_FAILED(rv))
        return fallback();

      return RequestOrReason(nullptr);
    });
  NS_ENSURE_TRUE(channel, NS_ERROR_OUT_OF_MEMORY);

  channel.forget(_channel);
  return NS_OK;
}
