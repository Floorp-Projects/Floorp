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

#include "nsLabel.h"

NS_IMPL_ADDREF(nsLabel)
NS_IMPL_RELEASE(nsLabel)

nsLabel::nsLabel() : nsWidget(), nsILabel()
{
  NS_INIT_REFCNT();
}

NS_METHOD nsLabel::PreCreateWidget(nsWidgetInitData *aInitData)
{
  return NS_OK;
}

nsLabel::~nsLabel()
{
}

nsresult nsLabel::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

  static NS_DEFINE_IID(kILabelIID, NS_ILABEL_IID);
  if (result == NS_NOINTERFACE && aIID.Equals(kILabelIID)) {
    *aInstancePtr = (void*) ((nsILabel*)this);
    NS_ADDREF_THIS();
    result = NS_OK;
  }
  
  return result;
}

NS_METHOD nsLabel::SetAlignment(nsLabelAlignment aAlignment)
{
  return NS_OK;
}

NS_METHOD nsLabel::SetLabel(const nsString& aText)
{
  return NS_OK;
}

NS_METHOD nsLabel::GetLabel(nsString& aBuffer)
{
  return NS_OK;
}

NS_METHOD nsLabel::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  return NS_OK;
}

NS_METHOD nsLabel::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  return NS_OK;
}


