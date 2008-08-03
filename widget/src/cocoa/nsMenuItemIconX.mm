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
#include "nsThreadUtils.h"
#include "nsToolkit.h"
#include "nsNetUtil.h"
#include "imgILoader.h"
#include "imgIRequest.h"
#include "gfxIImageFrame.h"
#include "nsIImage.h"
#include "nsMenuItemX.h"

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
    mIconRequest->Cancel(NS_BINDING_ABORTED);
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

  return LoadIcon(iconURI);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}


nsresult
nsMenuItemIconX::GetIconURI(nsIURI** aIconURI)
{
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

    nsCOMPtr<nsIDOMCSSStyleDeclaration> cssStyleDecl;
    nsAutoString empty;
    rv = domViewCSS->GetComputedStyle(domElement, empty,
                                      getter_AddRefs(cssStyleDecl));
    if (NS_FAILED(rv)) return rv;

    NS_NAMED_LITERAL_STRING(listStyleImage, "list-style-image");
    nsCOMPtr<nsIDOMCSSValue> cssValue;
    rv = cssStyleDecl->GetPropertyCSSValue(listStyleImage,
                                           getter_AddRefs(cssValue));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDOMCSSPrimitiveValue> primitiveValue =
     do_QueryInterface(cssValue);
    if (!primitiveValue) return NS_ERROR_FAILURE;

    PRUint16 primitiveType;
    rv = primitiveValue->GetPrimitiveType(&primitiveType);
    if (NS_FAILED(rv)) return rv;
    if (primitiveType != nsIDOMCSSPrimitiveValue::CSS_URI)
      return NS_ERROR_FAILURE;

    rv = primitiveValue->GetStringValue(imageURIString);
    if (NS_FAILED(rv)) return rv;
  }

  // If this menu item shouldn't have an icon, the string will be empty,
  // and NS_NewURI will fail.
  nsCOMPtr<nsIURI> iconURI;
  rv = NS_NewURI(getter_AddRefs(iconURI), imageURIString);
  if (NS_FAILED(rv)) return rv;

  *aIconURI = iconURI;
  NS_ADDREF(*aIconURI);
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
nsMenuItemIconX::FrameChanged(imgIContainer*  aContainer,
                             gfxIImageFrame* aFrame,
                             nsIntRect*      aDirtyRect)
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
  return NS_OK;
}


NS_IMETHODIMP
nsMenuItemIconX::OnStartFrame(imgIRequest* aRequest, gfxIImageFrame* aFrame)
{
  return NS_OK;
}


NS_IMETHODIMP
nsMenuItemIconX::OnDataAvailable(imgIRequest*     aRequest,
                                gfxIImageFrame*  aFrame,
                                const nsIntRect* aRect)
{
  return NS_OK;
}


NS_IMETHODIMP
nsMenuItemIconX::OnStopFrame(imgIRequest*    aRequest,
                            gfxIImageFrame* aFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (aRequest != mIconRequest) return NS_ERROR_FAILURE;

  // Only support one frame.
  if (mLoadedIcon)
    return NS_OK;

  nsCOMPtr<gfxIImageFrame> frame = aFrame;
  nsCOMPtr<nsIImage> image = do_GetInterface(frame);
  if (!image) return NS_ERROR_FAILURE;

  nsresult rv = image->LockImagePixels(PR_FALSE);
  if (NS_FAILED(rv))
    return rv;

  PRInt32 height = image->GetHeight();
  PRInt32 stride = image->GetLineStride();
  PRInt32 width = image->GetWidth();
  PRUint32 imageLength = ((stride * height) / 4);
  if ((stride % 4 != 0) || (height < 1) || (width < 1))
    return NS_ERROR_FAILURE;

  PRUint32* imageData = (PRUint32*)image->GetBits();

  PRUint32* reorderedData = (PRUint32*)malloc(height * stride);
  if (!reorderedData)
    return NS_ERROR_OUT_OF_MEMORY;

  // We have to reorder data to have alpha last because only Tiger can handle
  // alpha being first. Also the data must always be big endian (silly).
  
  for (PRUint32 i = 0; i < imageLength; i++) {
    PRUint32 pixel = imageData[i];
    reorderedData[i] = CFSwapInt32HostToBig((pixel << 8) | (pixel >> 24));
  }

  CGDataProviderRef provider = ::CGDataProviderCreateWithData(NULL, reorderedData, imageLength, PRAllocCGFree);
  if (!provider) {
    free(reorderedData);
    return NS_ERROR_FAILURE;
  }
  CGColorSpaceRef colorSpace = ::CGColorSpaceCreateDeviceRGB();
  CGImageRef cgImage = ::CGImageCreate(width, height, 8, 32, stride, colorSpace,
                                       kCGImageAlphaPremultipliedLast, provider,
                                       NULL, true, kCGRenderingIntentDefault);
  ::CGDataProviderRelease(provider);

  rv = image->UnlockImagePixels(PR_FALSE);
  if (NS_FAILED(rv)) {
    ::CGColorSpaceRelease(colorSpace);
    return rv;
  }

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
  mIconRequest->Cancel(NS_BINDING_ABORTED);
  mIconRequest = nsnull;
  return NS_OK;
}
