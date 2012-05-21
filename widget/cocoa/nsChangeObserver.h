/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChangeObserver_h_
#define nsChangeObserver_h_

class nsIContent;
class nsIDocument;
class nsIAtom;

#define NS_DECL_CHANGEOBSERVER \
void ObserveAttributeChanged(nsIDocument *aDocument, nsIContent *aContent, nsIAtom *aAttribute); \
void ObserveContentRemoved(nsIDocument *aDocument, nsIContent *aChild, PRInt32 aIndexInContainer); \
void ObserveContentInserted(nsIDocument *aDocument, nsIContent* aContainer, nsIContent *aChild);

// Something that wants to be alerted to changes in attributes or changes in
// its corresponding content object.
//
// This interface is used by our menu code so we only have to have one
// nsIDocumentObserver.
//
// Any class that implements this interface must take care to unregister itself
// on deletion.
class nsChangeObserver
{
public:
  // XXX use dom::Element
  virtual void ObserveAttributeChanged(nsIDocument* aDocument,
                                       nsIContent* aContent,
                                       nsIAtom* aAttribute)=0;

  virtual void ObserveContentRemoved(nsIDocument* aDocument,
                                     nsIContent* aChild, 
                                     PRInt32 aIndexInContainer)=0;

  virtual void ObserveContentInserted(nsIDocument* aDocument,
                                      nsIContent* aContainer,
                                      nsIContent* aChild)=0;
};

#endif // nsChangeObserver_h_
