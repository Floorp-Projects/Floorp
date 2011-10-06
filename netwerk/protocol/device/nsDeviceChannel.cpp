/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Camera.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brad Lassey <blassey@mozilla.com>
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

#include "plstr.h"
#include "nsComponentManagerUtils.h"
#include "nsDeviceChannel.h"
#include "nsDeviceCaptureProvider.h"
#include "mozilla/Preferences.h"

#ifdef ANDROID
#include "AndroidCaptureProvider.h"
#endif

// Copied from modules/libpr0n/decoders/icon/nsIconURI.cpp
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
                             PR_TRUE,
                             0,
                             -1)) {
    type.AssignLiteral("image/png");
    SetContentType(type);
    captureParams.captureAudio = PR_FALSE;
    captureParams.captureVideo = PR_TRUE;
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
    captureParams.bpp = 32;
#ifdef ANDROID
    capture = GetAndroidCaptureProvider();
#endif
  } else if (kNotFound != spec.Find(NS_LITERAL_CSTRING("type=video/x-raw-yuv"),
                                    PR_TRUE,
                                    0,
                                    -1)) {
    type.AssignLiteral("video/x-raw-yuv");
    SetContentType(type);
    captureParams.captureAudio = PR_FALSE;
    captureParams.captureVideo = PR_TRUE;
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
    captureParams.bpp = 32;
    captureParams.timeLimit = 0;
    captureParams.frameLimit = 60000;
#ifdef ANDROID
    // only enable if "device.camera.enabled" is true.
    if (mozilla::Preferences::GetBool("device.camera.enabled", false) == true)
      capture = GetAndroidCaptureProvider();
#endif
  } else {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (!capture)
    return NS_ERROR_FAILURE;

  return capture->Init(type, &captureParams, aStream);
}
