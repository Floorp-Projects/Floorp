/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <qcolor.h>
#include <qcolormap.h>
#include <qrect.h>
#include <qdesktopwidget.h>
#include <qapplication.h>
#include <QTransform>

#include "nsScreenQt.h"
#include "nsXULAppAPI.h"

#ifdef MOZ_ENABLE_QTMOBILITY
#include "mozqorientationsensorfilter.h"
#endif

#ifdef MOZ_ENABLE_QMSYSTEM2
#include <qmdisplaystate.h>
using namespace mozilla;

const int DISPLAY_BLANK_TIMEOUT = 10800; /*3 * 60 * 60 seconds*/
const int DISPLAY_DIM_TIMEOUT = 10620; /*3 * 59 * 60 seconds*/

#endif

nsScreenQt::nsScreenQt(int aScreen)
    : mScreen(aScreen)
#ifdef MOZ_ENABLE_QMSYSTEM2
    , mDisplayState(nullptr)
#endif
{
    // nothing else to do. I guess we could cache a bunch of information
    // here, but we want to ask the device at runtime in case anything
    // has changed.
}

nsScreenQt::~nsScreenQt()
{
#ifdef MOZ_ENABLE_QMSYSTEM2
    delete mDisplayState;
    mDisplayState = nullptr;
#endif
}

NS_IMETHODIMP
nsScreenQt::GetRect(PRInt32 *outLeft,PRInt32 *outTop,
                    PRInt32 *outWidth,PRInt32 *outHeight)
{
    QRect r = QApplication::desktop()->screenGeometry(mScreen);
#ifdef MOZ_ENABLE_QTMOBILITY
    r = MozQOrientationSensorFilter::GetRotationTransform().mapRect(r);
    // just rotating gives us weird negative coordinates, but we want to return
    // sensible logical coordinates
    r.moveTo(0, 0);
#endif

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
    QRect r = QApplication::desktop()->screenGeometry(mScreen);
#ifdef MOZ_ENABLE_QTMOBILITY
    r = MozQOrientationSensorFilter::GetRotationTransform().mapRect(r);
#endif

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

#ifdef MOZ_ENABLE_QMSYSTEM2
void
nsScreenQt::ApplyMinimumBrightness(PRUint32 aType)
{
    // resets all we did before,
    // 1) there is no interface to get default values
    // 2) user might have changed system settings while fennec is running
    //    there is no notification about that.
    delete mDisplayState;
    mDisplayState = nullptr;

    if( aType == BRIGHTNESS_FULL) {
        mDisplayState = new MeeGo::QmDisplayState();

        // no way to keep display from blanking than setting a huge timeout
        // parameter is seconds. setting timeout to huge time this should work for 99.9% of our usecases
        mDisplayState->setDisplayBlankTimeout( DISPLAY_BLANK_TIMEOUT /*in seconds*/ );
        mDisplayState->setDisplayDimTimeout( DISPLAY_DIM_TIMEOUT /*in seconds*/ );
        mDisplayState->setDisplayBrightnessValue( mDisplayState->getMaxDisplayBrightnessValue() );
        mDisplayState->set(MeeGo::QmDisplayState::On);
     }
}
#endif
