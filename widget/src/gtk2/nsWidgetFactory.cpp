/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#include "nsIGenericFactory.h"
#include "nsWidgetsCID.h"
#include "nsWindow.h"
#include "nsAppShell.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAppShell)

static nsModuleComponentInfo components[] =
{
  { "Gtk2 Window",
    NS_WINDOW_CID,
    "@mozilla.org/widget/window/gtk2;1",
    nsWindowConstructor },
  { "Gtk2 AppShell",
    NS_APPSHELL_CID,
    "@mozilla.org/widget/appshell/gtk2;1",
    nsAppShellConstructor },
};

PR_STATIC_CALLBACK(void)
nsWidgetGtk2ModuleDtor(nsIModule *aSelf)
{
}

NS_IMPL_NSGETMODULE_WITH_DTOR(nsWidgetGtk2Module,
			      components,
			      nsWidgetGtk2ModuleDtor)
