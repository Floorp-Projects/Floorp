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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
 *   John C. Griggs <johng@corel.com>
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

#include "nsScreenQt.h"

#include <qcolor.h>
#include <qcolormap.h>
#include <qrect.h>
#include <qdesktopwidget.h>
#include <qapplication.h>

nsScreenQt::nsScreenQt(int aScreen)
    : mScreen(aScreen)
{
    // nothing else to do. I guess we could cache a bunch of information
    // here, but we want to ask the device at runtime in case anything
    // has changed.
}

nsScreenQt::~nsScreenQt()
{
    // nothing to see here.
}

// addref, release, QI
NS_IMPL_ISUPPORTS1(nsScreenQt, nsIScreen)

NS_IMETHODIMP
nsScreenQt::GetRect(PRInt32 *outLeft,PRInt32 *outTop,
                    PRInt32 *outWidth,PRInt32 *outHeight)
{
    QRect r = QApplication::desktop()->screenGeometry(mScreen);
    *outTop = r.x();
    *outLeft = r.y();
    *outWidth = r.width();
    *outHeight = r.height();

    return NS_OK;
}

NS_IMETHODIMP
nsScreenQt::GetAvailRect(PRInt32 *outLeft,PRInt32 *outTop,
                         PRInt32 *outWidth,PRInt32 *outHeight)
{
    QRect r = QApplication::desktop()->availableGeometry(mScreen);
    *outTop = r.x();
    *outLeft = r.y();
    *outWidth = r.width();
    *outHeight = r.height();

    return NS_OK;
}

NS_IMETHODIMP
nsScreenQt::GetPixelDepth(PRInt32 *aPixelDepth)
{
    // #############
    *aPixelDepth = (PRInt32)QColormap::instance().depth();
    return NS_OK;
}

NS_IMETHODIMP
nsScreenQt::GetColorDepth(PRInt32 *aColorDepth)
{
    // ###############
    return GetPixelDepth(aColorDepth);
}
