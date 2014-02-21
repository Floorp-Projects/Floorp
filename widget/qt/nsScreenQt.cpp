/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QColor>
#include <QRect>
#include <QGuiApplication>
#include <QTransform>
#include <QScreen>

#include "nsScreenQt.h"
#include "nsXULAppAPI.h"

nsScreenQt::nsScreenQt(int aScreen)
    : mScreen(aScreen)
{
    // nothing else to do. I guess we could cache a bunch of information
    // here, but we want to ask the device at runtime in case anything
    // has changed.
}

nsScreenQt::~nsScreenQt()
{
}

NS_IMETHODIMP
nsScreenQt::GetRect(int32_t *outLeft,int32_t *outTop,
                    int32_t *outWidth,int32_t *outHeight)
{
    QRect r = QGuiApplication::screens()[mScreen]->geometry();

    *outTop = r.x();
    *outLeft = r.y();
    *outWidth = r.width();
    *outHeight = r.height();

    return NS_OK;
}

NS_IMETHODIMP
nsScreenQt::GetAvailRect(int32_t *outLeft,int32_t *outTop,
                         int32_t *outWidth,int32_t *outHeight)
{
    QRect r = QGuiApplication::screens()[mScreen]->geometry();

    *outTop = r.x();
    *outLeft = r.y();
    *outWidth = r.width();
    *outHeight = r.height();

    return NS_OK;
}

NS_IMETHODIMP
nsScreenQt::GetPixelDepth(int32_t *aPixelDepth)
{
    // #############
    *aPixelDepth = QGuiApplication::primaryScreen()->depth();
    return NS_OK;
}

NS_IMETHODIMP
nsScreenQt::GetColorDepth(int32_t *aColorDepth)
{
    // ###############
    return GetPixelDepth(aColorDepth);
}
