/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Hartshorn <peter@igelaus.com.au>
 *   Ken Faulkner <faulkner@igelaus.com.au>
 *   B.J. Rossiter <bj@igelaus.com.au>
 *   Tony Tsui <tony@igelaus.com.au>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  NS_DECL_ISUPPORTS_INHERITED

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
  NS_IMETHOD Show(PRBool aShow);

  NS_IMETHOD Resize(PRInt32 aWidth,
                    PRInt32 aHeight,
                    PRBool   aRepaint);
  NS_IMETHOD Resize(PRInt32 aX,
                    PRInt32 aY,
                    PRInt32 aWidth,
                    PRInt32 aHeight,
                    PRBool   aRepaint);


  NS_IMETHOD SetFocus(PRBool aRaise);
  virtual  PRBool OnExpose(nsPaintEvent &event);
  NS_IMETHOD GetAttention(void);
  
protected:
  virtual long GetEventMask();

  // Keyboard and Pointer Grabbing
  void NativeGrab(PRBool aGrab);

  void                 QueueDraw();
  void                 UnqueueDraw();
  PRBool mIsUpdating;
  PRBool mBlockFocusEvents;
  PRBool mIsTooSmall;

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
