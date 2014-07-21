/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QGuiApplication>
#include <QScreen>

#include "nsScreenManagerQt.h"
#include "nsScreenQt.h"

nsScreenManagerQt::nsScreenManagerQt()
{
    mInitialized = false;
    desktop = 0;
    screens = 0;
}

nsScreenManagerQt::~nsScreenManagerQt()
{
    delete [] screens;
}

// addref, release, QI
NS_IMPL_ISUPPORTS(nsScreenManagerQt, nsIScreenManager)

void nsScreenManagerQt::init()
{
    if (mInitialized)
        return;

    nScreens = QGuiApplication::screens().size();
    screens = new nsCOMPtr<nsIScreen>[nScreens];

    for (int i = 0; i < nScreens; ++i)
        screens[i] = new nsScreenQt(i);
    mInitialized = true;
}

//
// ScreenForRect
//
// Returns the screen that contains the rectangle. If the rect overlaps
// multiple screens, it picks the screen with the greatest area of intersection.
//
// The coordinates are in pixels (not twips) and in screen coordinates.
//
NS_IMETHODIMP
nsScreenManagerQt::ScreenForRect(int32_t inLeft, int32_t inTop,
                                 int32_t inWidth, int32_t inHeight,
                                 nsIScreen **outScreen)
{
    if (!mInitialized)
        init();

    QRect r(inLeft, inTop, inWidth, inHeight);
    int best = 0;
    int area = 0;
    for (int i = 0; i < nScreens; ++i) {
        const QRect& rect = QGuiApplication::screens()[i]->geometry();
        QRect intersection = r&rect;
        int a = intersection.width()*intersection.height();
        if (a > area) {
            best = i;
            area = a;
        }
    }

    NS_IF_ADDREF(*outScreen = screens[best]);
    return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerQt::ScreenForId(uint32_t aId, nsIScreen** aOutScreen)
{
    if (!mInitialized) {
        init();
    }

    if (aId < nScreens) {
        NS_IF_ADDREF(*aOutScreen = screens[aId]);
        return NS_OK;
    }

    return NS_ERROR_FAILURE;
}

//
// GetPrimaryScreen
//
// The screen with the menubar/taskbar. This shouldn't be needed very
// often.
//
NS_IMETHODIMP
nsScreenManagerQt::GetPrimaryScreen(nsIScreen **aPrimaryScreen)
{
    if (!desktop)
        init();

    NS_IF_ADDREF(*aPrimaryScreen = screens[0]);
    return NS_OK;
}

//
// GetNumberOfScreens
//
// Returns how many physical screens are available.
//
NS_IMETHODIMP
nsScreenManagerQt::GetNumberOfScreens(uint32_t *aNumberOfScreens)
{
    if (!desktop)
        init();

    *aNumberOfScreens = nScreens;
    return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerQt::GetSystemDefaultScale(float *aDefaultScale)
{
    *aDefaultScale = 1.0f;
    return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerQt::ScreenForNativeWidget(void *aWidget, nsIScreen **outScreen)
{
    // I don't know how to go from GtkWindow to nsIScreen, especially
    // given xinerama and stuff, so let's just do this
    QRect rect(0, 0, 1, 1);
    return ScreenForRect(rect.x(), rect.y(), rect.width(), rect.height(), outScreen);
}
