/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsBaseDragService_h__
#define nsBaseDragService_h__

#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsITransferable.h"
#include "nsISupportsArray.h"
#include "nsIDOMDocument.h"
#include "nsCOMPtr.h"

class nsIDOMNode;
class nsIFrame;
class nsIPresContext;


/**
 * XP DragService wrapper base class
 */

class nsBaseDragService : public nsIDragService, public nsIDragSession
{

public:
  nsBaseDragService();
  virtual ~nsBaseDragService();

  //nsISupports
  NS_DECL_ISUPPORTS
  
  //nsIDragSession and nsIDragService
  NS_DECL_NSIDRAGSERVICE
  NS_DECL_NSIDRAGSESSION

protected:

  virtual void GetFrameFromNode ( nsIDOMNode* inNode, nsIFrame** outFrame,
                                   nsIPresContext** outContext ) ;

  nsCOMPtr<nsISupportsArray> mTransArray;
  PRBool             mCanDrop;
  PRBool             mDoingDrag;
  nsSize             mTargetSize;
  PRUint32           mDragAction;  
  nsCOMPtr<nsIDragTracker> mCurrentlyTracking;
  nsCOMPtr<nsIDOMDocument> mSourceDocument;       // the document at the drag source. will be null
                                                  //  if it came from outside the app.

};

#endif // nsBaseDragService_h__
