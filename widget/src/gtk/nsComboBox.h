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

#ifndef nsComboBox_h__
#define nsComboBox_h__

#include "nsWidget.h"
#include "nsIComboBox.h"

/**
 * Native GTK+ Listbox wrapper
 */

class nsComboBox : public nsWidget,
                   public nsIListWidget,
                   public nsIComboBox
{

public:
  nsComboBox();
  virtual ~nsComboBox();

  NS_DECL_ISUPPORTS_INHERITED

  // nsIComboBox interface
  NS_IMETHOD      AddItemAt(nsString &aItem, PRInt32 aPosition);
  virtual PRInt32 FindItem(nsString &aItem, PRInt32 aStartPos);
  virtual PRInt32 GetItemCount();
  virtual PRBool  RemoveItemAt(PRInt32 aPosition);
  virtual PRBool  GetItemAt(nsString& anItem, PRInt32 aPosition);
  NS_IMETHOD      GetSelectedItem(nsString& aItem);
  virtual PRInt32 GetSelectedIndex();
  NS_IMETHOD      SelectItem(PRInt32 aPosition);
  NS_IMETHOD      Deselect() ;

  // nsIComboBox interface
  NS_IMETHOD      SetMultipleSelection(PRBool aMultipleSelections);
  PRInt32         GetSelectedCount();
  NS_IMETHOD      GetSelectedIndices(PRInt32 aIndices[], PRInt32 aSize);

  virtual void SetFontNative(GdkFont *aFont);

protected:
  NS_IMETHOD  CreateNative(GtkWidget *parentWindow);
  virtual void InitCallbacks(char * aName = nsnull);
  virtual void OnDestroySignal(GtkWidget* aGtkWidget);
  virtual void OnUnmapSignal(GtkWidget* aWidget);
  static gint UnmapSignal(GtkWidget* aGtkWidget, nsComboBox* aCombo);

  GtkWidget  *mCombo;  /* workaround for gtkcombo bug */
  GList *mItems;
  PRBool  mMultiSelect;
  int     mNumItems;
};

#endif // nsComboBox_h__
