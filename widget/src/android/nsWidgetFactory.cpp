/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include "nsIGenericFactory.h"
#include "nsIModule.h"

#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"
#include "nsAppShell.h"
#include "nsToolkit.h"

#include "nsWindow.h"
#include "nsLookAndFeel.h"
#include "nsAppShellSingleton.h"
#include "nsScreenManagerAndroid.h"

#include "nsIdleServiceAndroid.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsToolkit)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLookAndFeel)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerAndroid)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsIdleServiceAndroid)


static const nsModuleComponentInfo components[] =
{
    { "Android Toolkit",
      NS_TOOLKIT_CID,
      "@mozilla.org/widget/toolkit/android;1",
      nsToolkitConstructor },
    { "Android AppShell",
      NS_APPSHELL_CID,
      "@mozilla.org/widget/appshell/android;1",
      nsAppShellConstructor },
    { "Android nsWindow",
      NS_WINDOW_CID,
      "@mozilla.org/widgets/window/android;1",
      nsWindowConstructor },
    { "Android Child nsWindow",
      NS_CHILD_CID,
      "@mozilla.org/widgets/child_window/android;1",
      nsWindowConstructor }, /* Note: same as Window! */
    { "Android Look And Feel",
      NS_LOOKANDFEEL_CID,
      "@mozilla.org/widget/lookandfeel/android;1",
      nsLookAndFeelConstructor },
    { "Android Screen Manager",
      NS_SCREENMANAGER_CID,
      "@mozilla.org/gfx/screenmanager;1",
      nsScreenManagerAndroidConstructor },
    { "Android Idle Service",
      NS_IDLE_SERVICE_CID,
      "@mozilla.org/widget/idleservice;1",
      nsIdleServiceAndroidConstructor }
};

static void
nsWidgetAndroidModuleDtor(nsIModule *aSelf)
{
    nsAppShellShutdown(aSelf);
}

NS_IMPL_NSGETMODULE_WITH_CTOR_DTOR(nsWidgetAndroidModule,
                                   components,
                                   nsAppShellInit,
                                   nsWidgetAndroidModuleDtor)

