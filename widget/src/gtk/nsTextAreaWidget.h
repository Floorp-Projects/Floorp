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

#ifndef nsTextAreaWidget_h__
#define nsTextAreaWidget_h__

#include "nsTextHelper.h"
#include "nsITextAreaWidget.h"

/**
 * Native GTK+ multi-line edit control wrapper.
 */

class nsTextAreaWidget : public nsTextHelper
{

public:
  nsTextAreaWidget();
  virtual ~nsTextAreaWidget();

  NS_DECL_ISUPPORTS_INHERITED

  virtual void SetFontNative(GdkFont *aFont);

protected:
  NS_METHOD CreateNative(GtkWidget *parentWindow);
};


#endif // nsTextAreaWidget_h__
