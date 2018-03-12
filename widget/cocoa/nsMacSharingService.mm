/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "nsMacSharingService.h"
#include "nsCocoaUtils.h"
#include "mozilla/MacStringHelpers.h"

NS_IMPL_ISUPPORTS(nsMacSharingService, nsIMacSharingService)

static NSString*
NSImageToBase64(const NSImage* aImage)
{
  [aImage lockFocus];
  NSBitmapImageRep* bitmapRep = [[NSBitmapImageRep alloc] initWithFocusedViewRect:NSMakeRect(0, 0, aImage.size.width, aImage.size.height)];
  [aImage unlockFocus];
  NSData* imageData = [bitmapRep representationUsingType:NSPNGFileType properties:@{}];
  [bitmapRep release];
  NSString* base64Encoded = [imageData base64EncodedStringWithOptions:0];
  return [NSString stringWithFormat: @"data:image/png;base64,%@", base64Encoded];
}

static void
SetStrAttribute(JSContext* aCx,
                JS::Rooted<JSObject*>& aObj,
                const char* aKey, NSString* aVal)
{
  nsAutoString strVal;
  mozilla::CopyCocoaStringToXPCOMString(aVal, strVal);
  JS::Rooted<JSString*> title(aCx, JS_NewUCStringCopyZ(aCx, strVal.get()));
  JS::Rooted<JS::Value> attVal(aCx, JS::StringValue(title));
  JS_SetProperty(aCx, aObj, aKey, attVal);
}

nsresult
nsMacSharingService::GetSharingProviders(const nsAString& aUrlToShare,
                                         JSContext* aCx,
                                         JS::MutableHandleValue aResult)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  JS::Rooted<JSObject*> array(aCx, JS_NewArrayObject(aCx, 0));
  NSURL* url = [NSURL URLWithString:nsCocoaUtils::ToNSString(aUrlToShare)];

  NSArray* sharingService = [NSSharingService
                             sharingServicesForItems:[NSArray arrayWithObject:url]];
  int32_t serviceCount = 0;

  for (NSSharingService *currentService in sharingService) {

    JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));

    SetStrAttribute(aCx, obj, "title", currentService.title);
    SetStrAttribute(aCx, obj, "menuItemTitle", currentService.menuItemTitle);
    SetStrAttribute(aCx, obj, "image", NSImageToBase64(currentService.image));

    JS::Rooted<JS::Value> element(aCx, JS::ObjectValue(*obj));
    JS_SetElement(aCx, array, serviceCount++, element);
  }

  aResult.setObject(*array);

  return NS_OK;
  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsMacSharingService::ShareUrl(const nsAString& aShareTitle,
                              const nsAString& aUrlToShare)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSString* titleString = nsCocoaUtils::ToNSString(aShareTitle);
  NSURL* url = [NSURL URLWithString:nsCocoaUtils::ToNSString(aUrlToShare)];

  NSArray* sharingService = [NSSharingService
                             sharingServicesForItems:[NSArray arrayWithObject:url]];

  for (NSSharingService *currentService in sharingService) {
    if ([currentService.title isEqualToString:titleString]) {
      [currentService performWithItems:@[url]];
      break;
    }
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
