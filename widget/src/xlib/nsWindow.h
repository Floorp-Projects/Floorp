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
 *   Peter Hartshorn <peter@igelaus.com.au>
 *   Ken Faulkner <faulkner@igelaus.com.au>
 *   B.J. Rossiter <bj@igelaus.com.au>
 *   Tony Tsui <tony@igelaus.com.au>
 */

#ifndef nsWindow_h__
#define nsWindow_h__

#include "nsWidget.h"
#include <X11/cursorfont.h>

#include "nsString.h"

class nsListItem {
public:
  nsListItem() {}
  nsListItem(void *aData, nsListItem *aPrev);
  ~nsListItem() {}

  void *getData() { return data; }
  nsListItem *getNext() { return next; }
  void setNext(nsListItem *aNext) { next = aNext; }
  nsListItem *getPrev() { return prev; };
  void setPrev(nsListItem *aPrev) { prev = aPrev; }

private:
  void *data;
  nsListItem *next;
  nsListItem *prev;
};

class nsList {
public:
  nsList();
  ~nsList();
  nsListItem *getHead() { return head; }
  void add(void *aData);
  void remove(void *aData);
  void reset();

private:
  nsListItem *head;
  nsListItem *tail;
};


class nsWindow : public nsWidget
{
 public:
  nsWindow();
  ~nsWindow();
  static void      UpdateIdle (void *data);
  NS_IMETHOD CaptureRollupEvents(nsIRollupListener *aListener, 
                                 PRBool aDoCapture, 
                                 PRBool aConsumeRollupEvent);
  NS_IMETHOD Invalidate(PRBool aIsSynchronous);
  NS_IMETHOD Invalidate(const nsRect & aRect, PRBool aIsSynchronous);
  NS_IMETHOD           InvalidateRegion(const nsIRegion* aRegion, PRBool aIsSynchronous);
  NS_IMETHOD Update();
  NS_IMETHOD Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
  NS_IMETHOD ScrollWidgets(PRInt32 aDx, PRInt32 aDy);
  NS_IMETHOD ScrollRect(nsRect &aSrcRect, PRInt32 aDx, PRInt32 aDy);

  NS_IMETHOD SetTitle(const nsString& aTitle);

  NS_IMETHOD Resize(PRInt32 aWidth,
                    PRInt32 aHeight,
                    PRBool   aRepaint);
  NS_IMETHOD Resize(PRInt32 aX,
                    PRInt32 aY,
                    PRInt32 aWidth,
                    PRInt32 aHeight,
                    PRBool   aRepaint);


  NS_IMETHOD SetFocus(void);
  virtual  PRBool OnExpose(nsPaintEvent &event);
  NS_IMETHOD GetAttention(void);
  
protected:
  virtual void DestroyNative(void);
  virtual void DestroyNativeChildren(void);

  virtual long GetEventMask();

  // Keyboard and Pointer Grabbing
  void NativeGrab(PRBool aGrab);

  void                 QueueDraw();
  void                 UnqueueDraw();
  PRBool mIsUpdating;
  PRBool mBlockFocusEvents;

  static PRBool    sIsGrabbing;
  static nsWindow  *sGrabWindow;

#if 0
  virtual void CreateNative(Window aParent, nsRect aRect);
#endif
};

class ChildWindow : public nsWindow
{
 public:
  ChildWindow();
  virtual PRInt32 IsChild() { return PR_TRUE; };
};

#endif
