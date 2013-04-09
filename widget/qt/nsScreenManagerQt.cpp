/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qdesktopwidget.h"
#include "qapplication.h"

#include "nsScreenManagerQt.h"
#include "nsScreenQt.h"

nsScreenManagerQt::nsScreenManagerQt()
{
    desktop = 0;
    screens = 0;
}

nsScreenManagerQt::~nsScreenManagerQt()
{
    delete [] screens;
}

// addref, release, QI
NS_IMPL_ISUPPORTS1(nsScreenManagerQt, nsIScreenManager)

void nsScreenManagerQt::init()
{
    if (desktop)
        return;

    desktop = QApplication::desktop();
    nScreens = desktop->numScreens();
    screens = new nsCOMPtr<nsIScreen>[nScreens];

    for (int i = 0; i < nScreens; ++i)
        screens[i] = new nsScreenQt(i);
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
    if (!desktop)
        init();

    QRect r(inLeft, inTop, inWidth, inHeight);
    int best = 0;
    int area = 0;
    for (int i = 0; i < nScreens; ++i) {
        const QRect& rect = desktop->screenGeometry(i);
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

    *aNumberOfScreens = desktop->numScreens();
    return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerQt::GetSystemDefaultScale(float *aDefaultScale)
{
    *aDefaultScale = 1.0f;
    return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerQt :: ScreenForNativeWidget (void *aWidget, nsIScreen **outScreen)
{
    // I don't know how to go from GtkWindow to nsIScreen, especially
    // given xinerama and stuff, so let's just do this
    QRect rect = static_cast<QWidget*>(aWidget)->geometry();
    return ScreenForRect(rect.x(), rect.y(), rect.width(), rect.height(), outScreen);
}
