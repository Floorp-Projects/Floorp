/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is support for icons in native menu items on Mac OS X.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Mentovai <mark@moxienet.com> (Original Author)
 *  Josh Aas <josh@mozilla.com>
 *  Benjamin Frisch <bfrisch@gmail.com>
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

/*
 * Retrieves and displays icons in native menu items on Mac OS X.
 */

#include "nsMenuItemIconX.h"

#include "nsObjCExceptions.h"
#include "prmem.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsWidgetAtoms.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMViewCSS.h"
#include "nsIDOMElement.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMCSSValue.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsIDOMRect.h"
#include "nsThreadUtils.h"
#include "nsToolkit.h"
#include "nsNetUtil.h"
#include "imgILoader.h"
#include "imgIRequest.h"
#include "nsMenuItemX.h"
#include "gfxImageSurface.h"
#include "imgIContainer.h"

static const PRUint32 kIconWidth = 16;
static const PRUint32 kIconHeight = 16;
static const PRUint32 kIconBitsPerComponent = 8;
static const PRUint32 kIconComponents = 4;
static const PRUint32 kIconBitsPerPixel = kIconBitsPerComponent *
                                          kIconComponents;
static const PRUint32 kIconBytesPerRow = kIconWidth * kIconBitsPerPixel / 8;
static const PRUint32 kIconBytes = kIconBytesPerRow * kIconHeight;

static void
PRAllocCGFree(void* aInfo, const void* aData, size_t aSize) {
  free((void*)aData);
}

typedef nsresult (nsIDOMRect::*GetRectSideMethod)(nsIDOMCSSPrimitiveValue**);

NS_IMPL_ISUPPORTS2(nsMenuItemIconX, imgIContainerObserver, imgIDecoderObserver)

nsMenuItemIconX::nsMenuItemIconX(nsMenuObjectX* aMenuItem,
                                 nsIContent*    aContent,
                                 NSMenuItem*    aNativeMenuItem)
: mContent(aContent)
, mMenuObject(aMenuItem)
, mLoadedIcon(PR_FALSE)
, mSetIcon(PR_FALSE)
, mNativeMenuItem(aNativeMenuItem)
{
  //  printf("Creating icon for menu item %d, menu %d, native item is %d\n", aMenuItem, aMenu, aNativeMenuItem);
}

nsMenuItemIconX::~nsMenuItemIconX()
{
  if (mIconRequest)
    mIconRequest->CancelAndForgetObserver(NS_BINDING_ABORTED);
}

// Called from mMenuObjectX's destructor, to prevent us from outliving it
// (as might otherwise happen if calls to our imgIDecoderObserver methods
// are still outstanding).  mMenuObjectX owns our nNativeMenuItem.
void nsMenuItemIconX::Destroy()
{
  if (mIconRequest) {
    mIconRequest->CancelAndForgetObserver(NS_BINDING_ABORTED);
    mIconRequest = nsnull;
  }
  mMenuObject = nsnull;
  mNativeMenuItem = nil;
}

nsresult
nsMenuItemIconX::SetupIcon()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Still don't have one, then something is wrong, get out of here.
  if (!mNativeMenuItem) {
    NS_ERROR("No native menu item\n");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> iconURI;
  nsresult rv = GetIconURI(getter_AddRefs(iconURI));
  if (NS_FAILED(rv)) {
    // There is no icon for this menu item. An icon might have been set
    // earlier.  Clear it.
    [mNativeMenuItem setImage:nil];

    return NS_OK;
  }

  rv = LoadIcon(iconURI);
  if (NS_FAILED(rv)) {
    // There is no icon for this menu item, as an error occured while loading it.
    // An icon might have been set earlier or the place holder icon may have
    // been set.  Clear it.
    [mNativeMenuItem setImage:nil];
  }
  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

static PRInt32
GetDOMRectSide(nsIDOMRect* aRect, GetRectSideMethod aMethod)
{
  nsCOMPtr<nsIDOMCSSPrimitiveValue> dimensionValue;
  (aRect->*aMethod)(getter_AddRefs(dimensionValue));
  if (!dimensionValue)
    return -1;

  PRUint16 primitiveType;
  nsresult rv = dimensionValue->GetPrimitiveType(&primitiveType);
  if (NS_FAILED(rv) || primitiveType != nsIDOMCSSPrimitiveValue::CSS_PX)
    return -1;

  float dimension = 0;
  rv = dimensionValue->GetFloatValue(nsIDOMCSSPrimitiveValue::CSS_PX,
                                     &dimension);
  if (NS_FAILED(rv))
    return -1;

  return NSToIntRound(dimension);
}

nsresult
nsMenuItemIconX::GetIconURI(nsIURI** aIconURI)
{
  if (!mMenuObject)
    return NS_ERROR_FAILURE;

  // Mac native menu items support having both a checkmark and an icon
  // simultaneously, but this is unheard of in the cross-platform toolkit,
  // seemingly because the win32 theme is unable to cope with both at once.
  // The downside is that it's possible to get a menu item marked with a
  // native checkmark and a checkmark for an icon.  Head off that possibility
  // by pretending that no icon exists if this is a checkable menu item.
  if (mMenuObject->MenuObjectType() == eMenuItemObjectType) {
    nsMenuItemX* menuItem = static_cast<nsMenuItemX*>(mMenuObject);
    if (menuItem->GetMenuItemType() != eRegularMenuItemType)
      return NS_ERROR_FAILURE;
  }

  if (!mContent)
    return NS_ERROR_FAILURE;

  // First, look at the content node's "image" attribute.
  nsAutoString imageURIString;
  PRBool hasImageAttr = mContent->GetAttr(kNameSpaceID_None,
                                          nsWidgetAtoms::image,
                                          imageURIString);

  nsresult rv;
  nsCOMPtr<nsIDOMCSSValue> cssValue;
  nsCOMPtr<nsIDOMCSSStyleDeclaration> cssStyleDecl;
  nsCOMPtr<nsIDOMCSSPrimitiveValue> primitiveValue;
  PRUint16 primitiveType;
  if (!hasImageAttr) {
    // If the content node has no "image" attribute, get the
    // "list-style-image" property from CSS.
    nsCOMPtr<nsIDOMDocumentView> domDocumentView =
     do_QueryInterface(mContent->GetDocument());
    if (!domDocumentView) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMAbstractView> domAbstractView;
    rv = domDocumentView->GetDefaultView(getter_AddRefs(domAbstractView));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDOMViewCSS> domViewCSS = do_QueryInterface(domAbstractView);
    if (!domViewCSS) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(mContent);
    if (!domElement) return NS_ERROR_FAILURE;

    nsAutoString empty;
    rv = domViewCSS->GetComputedStyle(domElement, empty,
                                      getter_AddRefs(cssStyleDecl));
    if (NS_FAILED(rv)) return rv;

    NS_NAMED_LITERAL_STRING(listStyleImage, "list-style-image");
    rv = cssStyleDecl->GetPropertyCSSValue(listStyleImage,
                                           getter_AddRefs(cssValue));
    if (NS_FAILED(rv)) return rv;

    primitiveValue = do_QueryInterface(cssValue);
    if (!primitiveValue) return NS_ERROR_FAILURE;

    rv = primitiveValue->GetPrimitiveType(&primitiveType);
    if (NS_FAILED(rv)) return rv;
    if (primitiveType != nsIDOMCSSPrimitiveValue::CSS_URI)
      return NS_ERROR_FAILURE;

    rv = primitiveValue->GetStringValue(imageURIString);
    if (NS_FAILED(rv)) return rv;
  }

  // Empty the mImageRegionRect initially as the image region CSS could
  // have been changed and now have an error or have been removed since the
  // last GetIconURI call.
  mImageRegionRect.Empty();

  // If this menu item shouldn't have an icon, the string will be empty,
  // and NS_NewURI will fail.
  nsCOMPtr<nsIURI> iconURI;
  rv = NS_NewURI(getter_AddRefs(iconURI), imageURIString);
  if (NS_FAILED(rv)) return rv;

  *aIconURI = iconURI;
  NS_ADDREF(*aIconURI);

  if (!hasImageAttr) {
    // Check if the icon has a specified image region so that it can be
    // cropped appropriately before being displayed.
    NS_NAMED_LITERAL_STRING(imageRegion, "-moz-image-region");
    rv = cssStyleDecl->GetPropertyCSSValue(imageRegion,
                                           getter_AddRefs(cssValue));
    // Just return NS_OK if there if there is a failure due to no
    // moz-image region specified so the whole icon will be drawn anyway.
    if (NS_FAILED(rv)) return NS_OK;

    primitiveValue = do_QueryInterface(cssValue);
    if (!primitiveValue) return NS_OK;

    rv = primitiveValue->GetPrimitiveType(&primitiveType);
    if (NS_FAILED(rv)) return NS_OK;
    if (primitiveType != nsIDOMCSSPrimitiveValue::CSS_RECT)
      return NS_OK;

    nsCOMPtr<nsIDOMRect> imageRegionRect;
    rv = primitiveValue->GetRectValue(getter_AddRefs(imageRegionRect));
    if (NS_FAILED(rv)) return NS_OK;

    if (imageRegionRect) {
      // Return NS_ERROR_FAILURE if the image region is invalid so the image
      // is not drawn, and behavior is similar to XUL menus.
      PRInt32 bottom = GetDOMRectSide(imageRegionRect, &nsIDOMRect::GetBottom);
      PRInt32 right = GetDOMRectSide(imageRegionRect, &nsIDOMRect::GetRight);
      PRInt32 top = GetDOMRectSide(imageRegionRect, &nsIDOMRect::GetTop);
      PRInt32 left = GetDOMRectSide(imageRegionRect, &nsIDOMRect::GetLeft);

      if (top < 0 || left < 0 || bottom <= top || right <= left)
        return NS_ERROR_FAILURE;

      mImageRegionRect.SetRect(left, top, right - left, bottom - top);
    }
  }

  return NS_OK;
}

nsresult
nsMenuItemIconX::LoadIcon(nsIURI* aIconURI)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (mIconRequest) {
    // Another icon request is already in flight.  Kill it.
    mIconRequest->Cancel(NS_BINDING_ABORTED);
    mIconRequest = nsnull;
  }

  mLoadedIcon = PR_FALSE;

  if (!mContent) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> document = mContent->GetOwnerDoc();
  if (!document) return NS_ERROR_FAILURE;

  nsCOMPtr<nsILoadGroup> loadGroup = document->GetDocumentLoadGroup();
  if (!loadGroup) return NS_ERROR_FAILURE;

  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<imgILoader> loader = do_GetService("@mozilla.org/image/loader;1",
                                              &rv);
  if (NS_FAILED(rv)) return rv;

  if (!mSetIcon) {
    // Set a completely transparent 16x16 image as the icon on this menu item
    // as a placeholder.  This keeps the menu item text displayed in the same
    // position that it will be displayed when the real icon is loaded, and
    // prevents it from jumping around or looking misaligned.

    static PRBool sInitializedPlaceholder;
    static NSImage* sPlaceholderIconImage;
    if (!sInitializedPlaceholder) {
      sInitializedPlaceholder = PR_TRUE;

      // Note that we only create the one and reuse it forever, so this is not a leak.
      sPlaceholderIconImage = [[NSImage alloc] initWithSize:NSMakeSize(kIconWidth, kIconHeight)];
    }

    if (!sPlaceholderIconImage) return NS_ERROR_FAILURE;

    if (mNativeMenuItem)
      [mNativeMenuItem setImage:sPlaceholderIconImage];
  }

  rv = loader->LoadImage(aIconURI, nsnull, nsnull, loadGroup, this,
                         nsnull, nsIRequest::LOAD_NORMAL, nsnull,
                         nsnull, getter_AddRefs(mIconRequest));
  if (NS_FAILED(rv)) return rv;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

//
// imgIContainerObserver
//

NS_IMETHODIMP
nsMenuItemIconX::FrameChanged(imgIContainer* aContainer,
                              nsIntRect*     aDirtyRect)
{
  return NS_OK;
}

//
// imgIDecoderObserver
//

NS_IMETHODIMP
nsMenuItemIconX::OnStartRequest(imgIRequest* aRequest)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuItemIconX::OnStartDecode(imgIRequest* aRequest)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuItemIconX::OnStartContainer(imgIRequest*   aRequest,
                                  imgIContainer* aContainer)
{
  // Request a decode
  NS_ABORT_IF_FALSE(aContainer, "who sent the notification then?");
  aContainer->RequestDecode();

  return NS_OK;
}

NS_IMETHODIMP
nsMenuItemIconX::OnStartFrame(imgIRequest* aRequest, PRUint32 aFrame)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuItemIconX::OnDataAvailable(imgIRequest*     aRequest,
                                 PRBool           aCurrentFrame,
                                 const nsIntRect* aRect)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuItemIconX::OnStopFrame(imgIRequest*    aRequest,
                             PRUint32        aFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (aRequest != mIconRequest)
    return NS_ERROR_FAILURE;

  // Only support one frame.
  if (mLoadedIcon)
    return NS_OK;

  if (!mNativeMenuItem) return NS_ERROR_FAILURE;

  nsCOMPtr<imgIContainer> imageContainer;
  aRequest->GetImage(getter_AddRefs(imageContainer));
  if (!imageContainer) {
    [mNativeMenuItem setImage:nil];
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<gfxImageSurface> image;
  nsresult rv = imageContainer->CopyFrame(imgIContainer::FRAME_CURRENT,
                                          imgIContainer::FLAG_NONE,
                                          getter_AddRefs(image));
  if (NS_FAILED(rv) || !image) {
    [mNativeMenuItem setImage:nil];
    return NS_ERROR_FAILURE;
  }

  PRInt32 origHeight = image->Height();
  PRInt32 origStride = image->Stride();
  PRInt32 origWidth = image->Width();
  if ((origStride % 4 != 0) || (origHeight < 1) || (origWidth < 1)) {
    [mNativeMenuItem setImage:nil];
    return NS_ERROR_FAILURE;
  }

  PRUint32* imageData = (PRUint32*)image->Data();

  // If the image region is invalid, don't draw the image to almost match
  // the behavior of other platforms.
  if (!mImageRegionRect.IsEmpty() &&
      (mImageRegionRect.XMost() > origWidth ||
       mImageRegionRect.YMost() > origHeight)) {
    [mNativeMenuItem setImage:nil];
    return NS_ERROR_FAILURE;
  }

  if (mImageRegionRect.IsEmpty()) {
    mImageRegionRect.SetRect(0, 0, origWidth, origHeight);
  }

  PRInt32 newStride = mImageRegionRect.width * sizeof(PRUint32);
  PRInt32 imageLength = mImageRegionRect.height * mImageRegionRect.width;

  PRUint32* reorderedData = (PRUint32*)malloc(imageLength * sizeof(PRUint32));
  if (!reorderedData) {
    [mNativeMenuItem setImage:nil];
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // We have to clip the data to the image region and reorder the data to have
  // alpha last because only Tiger can handle alpha being first. Also the data
  // must always be big endian (silly).
  for (PRInt32 y = 0; y < mImageRegionRect.height; y++) {
    PRInt32 srcLine = (mImageRegionRect.y + y) * (origStride/4);
    PRInt32 dstLine = y * mImageRegionRect.width;
    for (PRInt32 x = 0; x < mImageRegionRect.width; x++) {
      PRUint32 pixel = imageData[srcLine + x + mImageRegionRect.x];
      reorderedData[dstLine + x] =
        CFSwapInt32HostToBig((pixel << 8) | (pixel >> 24));
    }
  }

  CGDataProviderRef provider = ::CGDataProviderCreateWithData(NULL, reorderedData, imageLength, PRAllocCGFree);
  if (!provider) {
    free(reorderedData);
    [mNativeMenuItem setImage:nil];
    return NS_ERROR_FAILURE;
  }
  CGColorSpaceRef colorSpace = ::CGColorSpaceCreateDeviceRGB();
  CGImageRef cgImage = ::CGImageCreate(mImageRegionRect.width,
                                       mImageRegionRect.height, 8, 32, newStride,
                                       colorSpace, kCGImageAlphaPremultipliedLast,
                                       provider, NULL, true,
                                       kCGRenderingIntentDefault);
  ::CGDataProviderRelease(provider);

  // The image may not be the right size for a menu icon (16x16).
  // Create a new CGImage for the menu item.
  PRUint8* bitmap = (PRUint8*)malloc(kIconBytes);

  CGImageAlphaInfo alphaInfo = ::CGImageGetAlphaInfo(cgImage);

  CGContextRef bitmapContext;
  bitmapContext = ::CGBitmapContextCreate(bitmap, kIconWidth, kIconHeight,
                                          kIconBitsPerComponent,
                                          kIconBytesPerRow,
                                          colorSpace,
                                          alphaInfo);
  if (!bitmapContext) {
    ::CGImageRelease(cgImage);
    free(bitmap);
    ::CGColorSpaceRelease(colorSpace);
    return NS_ERROR_FAILURE;
  }

  CGRect iconRect = ::CGRectMake(0, 0, kIconWidth, kIconHeight);
  ::CGContextClearRect(bitmapContext, iconRect);
  ::CGContextDrawImage(bitmapContext, iconRect, cgImage);
  ::CGImageRelease(cgImage);
  ::CGContextRelease(bitmapContext);

  provider = ::CGDataProviderCreateWithData(NULL, bitmap, kIconBytes,
                                            PRAllocCGFree);
  if (!provider) {
    free(bitmap);
    ::CGColorSpaceRelease(colorSpace);
    return NS_ERROR_FAILURE;
  }

  CGImageRef iconImage =
   ::CGImageCreate(kIconWidth, kIconHeight, kIconBitsPerComponent,
                   kIconBitsPerPixel, kIconBytesPerRow, colorSpace, alphaInfo,
                   provider, NULL, TRUE, kCGRenderingIntentDefault);
  ::CGColorSpaceRelease(colorSpace);
  ::CGDataProviderRelease(provider);
  if (!iconImage) return NS_ERROR_FAILURE;

  NSRect imageRect = NSMakeRect(0.0, 0.0, 0.0, 0.0);
  CGContextRef imageContext = nil;
 
  // Get the image dimensions.
  imageRect.size.width = kIconWidth;
  imageRect.size.height = kIconHeight;
 
  // Create a new image to receive the Quartz image data.
  NSImage* newImage = [[NSImage alloc] initWithSize:imageRect.size];

  [newImage lockFocus];
 
  // Get the Quartz context and draw.
  imageContext = (CGContextRef)[[NSGraphicsContext currentContext]
                                graphicsPort];
  CGContextDrawImage(imageContext, *(CGRect*)&imageRect, iconImage);
  [newImage unlockFocus];

  if (!mNativeMenuItem) return NS_ERROR_FAILURE;

  [mNativeMenuItem setImage:newImage];
  
  [newImage release];
  ::CGImageRelease(iconImage);

  mLoadedIcon = PR_TRUE;
  mSetIcon = PR_TRUE;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsMenuItemIconX::OnStopContainer(imgIRequest*   aRequest,
                                imgIContainer* aContainer)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuItemIconX::OnStopDecode(imgIRequest*     aRequest,
                             nsresult         status,
                             const PRUnichar* statusArg)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMenuItemIconX::OnStopRequest(imgIRequest* aRequest,
                              PRBool       aIsLastPart)
{
  NS_ASSERTION(mIconRequest, "NULL mIconRequest!  Multiple calls to OnStopRequest()?");
  if (mIconRequest) {
    mIconRequest->Cancel(NS_BINDING_ABORTED);
    mIconRequest = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMenuItemIconX::OnDiscard(imgIRequest* aRequest)
{
  return NS_OK;
}
