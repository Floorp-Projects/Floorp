/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code mozilla.org code.
 *
 * The Initial Developer of the Original Code Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2001 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsScrollbar.h"

#include <gtk/gtkscrollbar.h>

nsScrollbar::nsScrollbar(PRBool aIsVertical)
{
  mScrollbar = nsnull;
}

nsScrollbar::~nsScrollbar()
{
}

NS_IMPL_ADDREF_INHERITED(nsScrollbar, nsCommonWidget)
NS_IMPL_RELEASE_INHERITED(nsScrollbar, nsCommonWidget)
NS_IMPL_QUERY_INTERFACE2(nsScrollbar, nsIScrollbar, nsIWidget)

NS_IMETHODIMP
nsScrollbar::SetMaxRange(PRUint32 aEndRange)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScrollbar::GetMaxRange(PRUint32& aMaxRange)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsScrollbar::SetPosition(PRUint32 aPos)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsScrollbar::GetPosition(PRUint32& aPos)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsScrollbar::SetThumbSize(PRUint32 aSize)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsScrollbar::GetThumbSize(PRUint32& aSize)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsScrollbar::SetLineIncrement(PRUint32 aSize)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsScrollbar::GetLineIncrement(PRUint32& aSize)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsScrollbar::SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
			 PRUint32 aPosition, PRUint32 aLineIncrement)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

