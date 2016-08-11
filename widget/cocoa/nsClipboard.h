/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsClipboard_h_
#define nsClipboard_h_

#include "nsIClipboard.h"
#include "nsXPIDLString.h"
#include "mozilla/StaticPtr.h"

#import <Cocoa/Cocoa.h>

class nsITransferable;

class nsClipboard : public nsIClipboard
{

public:
  nsClipboard();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICLIPBOARD

  // On macOS, cache the transferable of the current selection (chrome/content)
  // in the parent process. This is needed for the services menu which
  // requires synchronous access to the current selection.
  static mozilla::StaticRefPtr<nsITransferable> sSelectionCache;

  // Helper methods, used also by nsDragService
  static NSDictionary* PasteboardDictFromTransferable(nsITransferable *aTransferable);
  static bool IsStringType(const nsCString& aMIMEType, NSString** aPasteboardType);
  static NSString* WrapHtmlForSystemPasteboard(NSString* aString);
  static nsresult TransferableFromPasteboard(nsITransferable *aTransferable, NSPasteboard *pboard);

protected:

  // impelement the native clipboard behavior
  NS_IMETHOD SetNativeClipboardData(int32_t aWhichClipboard);
  NS_IMETHOD GetNativeClipboardData(nsITransferable * aTransferable, int32_t aWhichClipboard);
  void ClearSelectionCache();
  void SetSelectionCache(nsITransferable* aTransferable);
  
private:
  virtual ~nsClipboard();
  int32_t mCachedClipboard;
  int32_t mChangeCount; // Set to the native change count after any modification of the clipboard.

  bool                        mIgnoreEmptyNotification;
  nsCOMPtr<nsIClipboardOwner> mClipboardOwner;
  nsCOMPtr<nsITransferable>   mTransferable;
};

#endif // nsClipboard_h_
