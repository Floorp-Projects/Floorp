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
#include "nsISupportsUtils.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsStringStream.h"
#include "mozilla/storage.h"
#include "nsIPipe.h"
#include "Helpers.h"

using namespace mozilla;
using namespace mozilla::places;

////////////////////////////////////////////////////////////////////////////////
//// Global Functions

/**
 * Creates a channel to obtain the default favicon.
 */
static
nsresult
GetDefaultIcon(nsIChannel **aChannel)
{
  nsCOMPtr<nsIURI> defaultIconURI;
  nsresult rv = NS_NewURI(getter_AddRefs(defaultIconURI),
                          NS_LITERAL_CSTRING(FAVICON_DEFAULT_URL));
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_NewChannel(aChannel, defaultIconURI);
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
 * However, if an error occurs at any point, we do not set mReturnDefaultIcon to
 * false, so we will open up another channel to get the default favicon, and
 * pass that along to our output stream in HandleCompletion.  If anything
 * happens at that point, the world must be against us, so we return nothing.
 */
class faviconAsyncLoader : public AsyncStatementCallback
                         , public nsIRequestObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  faviconAsyncLoader(nsIChannel *aChannel, nsIOutputStream *aOutputStream) :
      mChannel(aChannel)
    , mOutputStream(aOutputStream)
    , mReturnDefaultIcon(true)
  {
    NS_ASSERTION(aChannel,
                 "Not providing a channel will result in crashes!");
    NS_ASSERTION(aOutputStream,
                 "Not providing an output stream will result in crashes!");
  }

  //////////////////////////////////////////////////////////////////////////////
  //// mozIStorageStatementCallback

  NS_IMETHOD HandleResult(mozIStorageResultSet *aResultSet)
  {
    // We will only get one row back in total, so we do not need to loop.
    nsCOMPtr<mozIStorageRow> row;
    nsresult rv = aResultSet->GetNextRow(getter_AddRefs(row));
    NS_ENSURE_SUCCESS(rv, rv);

    // We do not allow favicons without a MIME type, so we'll return the default
    // icon.
    nsAutoCString mimeType;
    (void)row->GetUTF8String(1, mimeType);
    NS_ENSURE_FALSE(mimeType.IsEmpty(), NS_OK);

    // Set our mimeType now that we know it.
    rv = mChannel->SetContentType(mimeType);
    NS_ENSURE_SUCCESS(rv, rv);

    // Obtain the binary blob that contains our favicon data.
    uint8_t *favicon;
    uint32_t size = 0;
    rv = row->GetBlob(0, &size, &favicon);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t totalWritten = 0;
    do {
      uint32_t bytesWritten;
      rv = mOutputStream->Write(
        &(reinterpret_cast<const char *>(favicon)[totalWritten]),
        size - totalWritten,
        &bytesWritten
      );
      if (NS_FAILED(rv) || !bytesWritten)
        break;
      totalWritten += bytesWritten;
    } while (size != totalWritten);
    NS_ASSERTION(NS_FAILED(rv) || size == totalWritten,
                 "Failed to write all of our data out to the stream!");

    // Free our favicon array.
    NS_Free(favicon);

    // Handle an error to write if it occurred, but only after we've freed our
    // favicon.
    NS_ENSURE_SUCCESS(rv, rv);

    // At this point, we should have written out all of our data to our stream.
    // HandleCompletion will close the output stream, so we are done here.
    mReturnDefaultIcon = false;
    return NS_OK;
  }

  NS_IMETHOD HandleCompletion(uint16_t aReason)
  {
    if (!mReturnDefaultIcon)
      return mOutputStream->Close();

    // We need to return our default icon, so we'll open up a new channel to get
    // that data, and push it to our output stream.  If at any point we get an
    // error, we can't do anything, so we'll just close our output stream.
    nsCOMPtr<nsIStreamListener> listener;
    nsresult rv = NS_NewSimpleStreamListener(getter_AddRefs(listener),
                                             mOutputStream, this);
    NS_ENSURE_SUCCESS(rv, mOutputStream->Close());

    nsCOMPtr<nsIChannel> newChannel;
    rv = GetDefaultIcon(getter_AddRefs(newChannel));
    NS_ENSURE_SUCCESS(rv, mOutputStream->Close());

    rv = newChannel->AsyncOpen(listener, nullptr);
    NS_ENSURE_SUCCESS(rv, mOutputStream->Close());

    return NS_OK;
  }

  //////////////////////////////////////////////////////////////////////////////
  //// nsIRequestObserver

  NS_IMETHOD OnStartRequest(nsIRequest *, nsISupports *)
  {
    return NS_OK;
  }

  NS_IMETHOD OnStopRequest(nsIRequest *, nsISupports *, nsresult aStatusCode)
  {
    // We always need to close our output stream, regardless of the status code.
    (void)mOutputStream->Close();

    // But, we'll warn about it not being successful if it wasn't.
    NS_WARN_IF_FALSE(NS_SUCCEEDED(aStatusCode),
                     "Got an error when trying to load our default favicon!");

    return NS_OK;
  }

private:
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIOutputStream> mOutputStream;
  bool mReturnDefaultIcon;
};

NS_IMPL_ISUPPORTS_INHERITED(
  faviconAsyncLoader,
  AsyncStatementCallback,
  nsIRequestObserver
)

} // anonymous namespace

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
nsAnnoProtocolHandler::NewChannel(nsIURI *aURI, nsIChannel **_retval)
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

  return NewFaviconChannel(aURI, annoURI, _retval);
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
                                         nsIChannel **_channel)
{
  // Create our pipe.  This will give us our input stream and output stream
  // that will be written to when we get data from the database.
  nsCOMPtr<nsIInputStream> inputStream;
  nsCOMPtr<nsIOutputStream> outputStream;
  nsresult rv = NS_NewPipe(getter_AddRefs(inputStream),
                           getter_AddRefs(outputStream),
                           MAX_FAVICON_SIZE, MAX_FAVICON_SIZE, true,
                           true);
  NS_ENSURE_SUCCESS(rv, GetDefaultIcon(_channel));

  // Create our channel.  We'll call SetContentType with the right type when
  // we know what it actually is.
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewInputStreamChannel(getter_AddRefs(channel), aURI, inputStream,
                                EmptyCString());
  NS_ENSURE_SUCCESS(rv, GetDefaultIcon(_channel));

  // Now we go ahead and get our data asynchronously for the favicon.
  nsCOMPtr<mozIStorageStatementCallback> callback =
    new faviconAsyncLoader(channel, outputStream);
  NS_ENSURE_TRUE(callback, GetDefaultIcon(_channel));
  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(faviconService, GetDefaultIcon(_channel));

  rv = faviconService->GetFaviconDataAsync(aAnnotationURI, callback);
  NS_ENSURE_SUCCESS(rv, GetDefaultIcon(_channel));

  channel.forget(_channel);
  return NS_OK;
}
