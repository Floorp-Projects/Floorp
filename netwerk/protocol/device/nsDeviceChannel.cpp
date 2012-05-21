/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plstr.h"
#include "nsComponentManagerUtils.h"
#include "nsDeviceChannel.h"
#include "nsDeviceCaptureProvider.h"
#include "mozilla/Preferences.h"

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidCaptureProvider.h"
#endif

#ifdef MOZ_WIDGET_GONK
#include "GonkCaptureProvider.h"
#endif

using namespace mozilla;

// Copied from image/decoders/icon/nsIconURI.cpp
// takes a string like ?size=32&contentType=text/html and returns a new string
// containing just the attribute values. i.e you could pass in this string with
// an attribute name of "size=", this will return 32
// Assumption: attribute pairs are separated by &
void extractAttributeValue(const char* searchString, const char* attributeName, nsCString& result)
{
  result.Truncate();

  if (!searchString || !attributeName)
    return;

  PRUint32 attributeNameSize = strlen(attributeName);
  const char *startOfAttribute = PL_strcasestr(searchString, attributeName);
  if (!startOfAttribute ||
      !( *(startOfAttribute-1) == '?' || *(startOfAttribute-1) == '&') )
    return;

  startOfAttribute += attributeNameSize; // Skip the attributeName
  if (!*startOfAttribute)
    return;

  const char *endOfAttribute = strchr(startOfAttribute, '&');
  if (endOfAttribute) {
    result.Assign(Substring(startOfAttribute, endOfAttribute));
  } else {
    result.Assign(startOfAttribute);
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(nsDeviceChannel,
                             nsBaseChannel,
                             nsIChannel)

// nsDeviceChannel methods
nsDeviceChannel::nsDeviceChannel()
{
  SetContentType(NS_LITERAL_CSTRING("image/png"));
}

nsDeviceChannel::~nsDeviceChannel() 
{
}

nsresult
nsDeviceChannel::Init(nsIURI* aUri)
{
  nsBaseChannel::Init();
  nsBaseChannel::SetURI(aUri);
  return NS_OK;
}

nsresult
nsDeviceChannel::OpenContentStream(bool aAsync,
                                   nsIInputStream** aStream,
                                   nsIChannel** aChannel)
{
  if (!aAsync)
    return NS_ERROR_NOT_IMPLEMENTED;

  nsCOMPtr<nsIURI> uri = nsBaseChannel::URI();
  *aStream = nsnull;
  *aChannel = nsnull;
  NS_NAMED_LITERAL_CSTRING(width, "width=");
  NS_NAMED_LITERAL_CSTRING(height, "height=");

  nsCAutoString spec;
  uri->GetSpec(spec);

  nsCAutoString type;

  nsRefPtr<nsDeviceCaptureProvider> capture;
  nsCaptureParams captureParams;
  captureParams.camera = 0;
  if (kNotFound != spec.Find(NS_LITERAL_CSTRING("type=image/png"),
                             true,
                             0,
                             -1)) {
    type.AssignLiteral("image/png");
    SetContentType(type);
    captureParams.captureAudio = false;
    captureParams.captureVideo = true;
    captureParams.timeLimit = 0;
    captureParams.frameLimit = 1;
    nsCAutoString buffer;
    extractAttributeValue(spec.get(), "width=", buffer);
    nsresult err;
    captureParams.width = buffer.ToInteger(&err);
    if (!captureParams.width)
      captureParams.width = 640;
    extractAttributeValue(spec.get(), "height=", buffer);
    captureParams.height = buffer.ToInteger(&err);
    if (!captureParams.height)
      captureParams.height = 480;
    extractAttributeValue(spec.get(), "camera=", buffer);
    captureParams.camera = buffer.ToInteger(&err);
    captureParams.bpp = 32;
#ifdef MOZ_WIDGET_ANDROID
    capture = GetAndroidCaptureProvider();
#endif
#ifdef MOZ_WIDGET_GONK
    capture = GetGonkCaptureProvider();
#endif
  } else if (kNotFound != spec.Find(NS_LITERAL_CSTRING("type=video/x-raw-yuv"),
                                    true,
                                    0,
                                    -1)) {
    type.AssignLiteral("video/x-raw-yuv");
    SetContentType(type);
    captureParams.captureAudio = false;
    captureParams.captureVideo = true;
    nsCAutoString buffer;
    extractAttributeValue(spec.get(), "width=", buffer);
    nsresult err;
    captureParams.width = buffer.ToInteger(&err);
    if (!captureParams.width)
      captureParams.width = 640;
    extractAttributeValue(spec.get(), "height=", buffer);
    captureParams.height = buffer.ToInteger(&err);
    if (!captureParams.height)
      captureParams.height = 480;
    extractAttributeValue(spec.get(), "camera=", buffer);
    captureParams.camera = buffer.ToInteger(&err);
    captureParams.bpp = 32;
    captureParams.timeLimit = 0;
    captureParams.frameLimit = 60000;
#ifdef MOZ_WIDGET_ANDROID
    // only enable if "device.camera.enabled" is true.
    if (Preferences::GetBool("device.camera.enabled", false) == true)
      capture = GetAndroidCaptureProvider();
#endif
#ifdef MOZ_WIDGET_GONK
    if (Preferences::GetBool("device.camera.enabled", false) == true)
      capture = GetGonkCaptureProvider();
#endif
  } else {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (!capture)
    return NS_ERROR_FAILURE;

  return capture->Init(type, &captureParams, aStream);
}
