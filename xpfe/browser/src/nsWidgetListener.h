/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsWidgetListener_h__
#define nsWidgetListener_h__

#include "nsIWidgetController.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMFormListener.h"
#include "nsIDOMLoadListener.h"
#include "nsIDOMElement.h"



class nsWidgetListener : public nsIWidgetController,
                         public nsIDOMMouseListener,
                         public nsIDOMKeyListener,
                         public nsIDOMFormListener,
                         public nsIDOMLoadListener
{
public:
  nsWidgetListener();

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIDOMMouseListener interface
  virtual nsresult HandleEvent (nsIDOMEvent* aEvent);
  virtual nsresult MouseDown    (nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseUp      (nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseClick   (nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseOver    (nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseOut     (nsIDOMEvent* aMouseEvent);

  // nsIDOMKeyListener interface
  virtual nsresult KeyDown (nsIDOMEvent* aKeyEvent);
  virtual nsresult KeyUp   (nsIDOMEvent* aKeyEvent);
  virtual nsresult KeyPress(nsIDOMEvent* aKeyEvent);

  // nsIDOMFormListener interface
  virtual nsresult Submit(nsIDOMEvent* aFormEvent);
  virtual nsresult Reset (nsIDOMEvent* aFormEvent);
  virtual nsresult Change(nsIDOMEvent* aFormEvent);

  // nsIDOMLoadListener interface
  virtual nsresult Load  (nsIDOMEvent* aLoadEvent);
  virtual nsresult Unload(nsIDOMEvent* aLoadEvent);
  virtual nsresult Abort (nsIDOMEvent* aLoadEvent);
  virtual nsresult Error (nsIDOMEvent* aLoadEvent);

protected:
  virtual ~nsWidgetListener();

  nsresult AddEventListener(nsISupports* aNode, const nsIID& aInterfaceIID);
};

#endif /* nsWidgetListener_h__ */