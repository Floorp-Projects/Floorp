/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChangeObserver_h_
#define nsChangeObserver_h_

class nsIContent;
class nsAtom;
namespace mozilla {
namespace dom {
class Document;
}
}  // namespace mozilla

#define NS_DECL_CHANGEOBSERVER                                            \
  void ObserveAttributeChanged(mozilla::dom::Document* aDocument,         \
                               nsIContent* aContent, nsAtom* aAttribute)  \
      override;                                                           \
  void ObserveContentRemoved(mozilla::dom::Document* aDocument,           \
                             nsIContent* aContainer, nsIContent* aChild,  \
                             nsIContent* aPreviousChild) override;        \
  void ObserveContentInserted(mozilla::dom::Document* aDocument,          \
                              nsIContent* aContainer, nsIContent* aChild) \
      override;

// Something that wants to be alerted to changes in attributes or changes in
// its corresponding content object.
//
// This interface is used by our menu code so we only have to have one
// nsIMutationObserver per menu subtree root (e.g. per menubar).
//
// Any class that implements this interface must take care to unregister itself
// on deletion.
//
// XXXmstange The methods below use nsIContent*. Eventually, the should be
// converted to use mozilla::dom::Element* instead.
class nsChangeObserver {
 public:
  // Called when the attribute aAttribute on the element aContent has changed.
  // Only if aContent is being observed by this nsChangeObserver.
  virtual void ObserveAttributeChanged(mozilla::dom::Document* aDocument,
                                       nsIContent* aContent,
                                       nsAtom* aAttribute) = 0;

  // Called when aChild has been removed from its parent aContainer.
  // aPreviousSibling is the old previous sibling of aChild.
  // aContainer is always the old parent node of aChild and of aPreviousSibling.
  // Only called if aContainer or aContainer's parent node are being observed
  // by this nsChangeObserver.
  // In other words: If you observe an element, ObserveContentRemoved is called
  // if that element's children and grandchildren are removed. NOT if the
  // observed element itself is removed.
  virtual void ObserveContentRemoved(mozilla::dom::Document* aDocument,
                                     nsIContent* aContainer, nsIContent* aChild,
                                     nsIContent* aPreviousSibling) = 0;

  // Called when aChild has been inserted into its new parent aContainer.
  // Only called if aContainer or aContainer's parent node are being observed
  // by this nsChangeObserver.
  // In other words: If you observe an element, ObserveContentInserted is called
  // if that element receives a new child or grandchild. NOT if the observed
  // element itself is inserted anywhere.
  virtual void ObserveContentInserted(mozilla::dom::Document* aDocument,
                                      nsIContent* aContainer,
                                      nsIContent* aChild) = 0;
};

#endif  // nsChangeObserver_h_
