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
 * The Initial Developer of the Original Code is Christopher
 * Blizzard.  Portions created by Christopher Blizzard are
 * Copyright (C) 2000 Christopher Blizzard. All Rights Reserved.
 *
 * Contributor(s): 
 *   John C. Griggs <johng@corel.com>
 *
 */

#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"

#include "nsWindow.h"
#include "nsAppShell.h"
#include "nsToolkit.h"
#include "nsLookAndFeel.h"
#include "nsTransferable.h"
#include "nsClipboard.h"
#include "nsHTMLFormatConverter.h"
#include "nsDragService.h"
#include "nsFileSpecWithUIImpl.h"
#include "nsScrollbar.h"

#ifdef IBMBIDI
#include "nsBidiKeyboard.h"
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(ChildWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAppShell)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsToolkit)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLookAndFeel)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDragService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFileSpecWithUIImpl)
#ifdef IBMBIDI
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidiKeyboard)
#endif

static nsresult nsHorizScrollbarConstructor(nsISupports *aOuter,REFNSIID aIID,
                                            void **aResult)
{
  nsresult rv;
  nsISupports *inst = nsnull;

  if (NULL == aResult) {
    rv = NS_ERROR_NULL_POINTER;
    return rv;
  }
  *aResult = NULL;
  if (NULL != aOuter) {
    rv = NS_ERROR_NO_AGGREGATION;
    return rv;
  }
  inst = (nsISupports*)(nsBaseWidget*)(nsWidget*)new nsScrollbar(PR_FALSE);
  if (inst == NULL) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID,aResult);
  NS_RELEASE(inst);

  return rv;
}

static nsresult nsVertScrollbarConstructor(nsISupports *aOuter,REFNSIID aIID,
                                           void **aResult)
{
  nsresult rv;
  nsISupports *inst = nsnull;

  if (NULL == aResult) {
    rv = NS_ERROR_NULL_POINTER;
    return rv;
  }
  *aResult = NULL;
  if (NULL != aOuter) {
    rv = NS_ERROR_NO_AGGREGATION;
    return rv;
  }
  inst = (nsISupports*)(nsBaseWidget*)(nsWidget*)new nsScrollbar(PR_TRUE);
  if (inst == NULL) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return rv;
}  

static nsModuleComponentInfo components[] =
{
  { "Qt nsWindow",
    NS_WINDOW_CID,
    "@mozilla.org/widgets/window/qt;1",
    nsWindowConstructor },
  { "Qt Child nsWindow",
    NS_CHILD_CID,
    "@mozilla.org/widgets/child_window/qt;1",
    ChildWindowConstructor },
  { "Qt Horiz Scrollbar",
    NS_HORZSCROLLBAR_CID,
    "@mozilla.org/widgets/horizscroll/qt;1",
    nsHorizScrollbarConstructor },
  { "Qt Vert Scrollbar",
    NS_VERTSCROLLBAR_CID,
    "@mozilla.org/widgets/vertscroll/qt;1",
    nsVertScrollbarConstructor },
  { "Qt AppShell",
    NS_APPSHELL_CID,
    "@mozilla.org/widget/appshell/qt;1",
    nsAppShellConstructor },
  { "Qt Toolkit",
    NS_TOOLKIT_CID,
    "@mozilla.org/widget/toolkit/qt;1",
    nsToolkitConstructor },
  { "Qt Look And Feel",
    NS_LOOKANDFEEL_CID,
    "@mozilla.org/widget/lookandfeel/qt;1",
    nsLookAndFeelConstructor },
  { "Transferrable",
    NS_TRANSFERABLE_CID,
    "@mozilla.org/widget/transferable;1",
    nsTransferableConstructor },
  { "Qt Clipboard",
    NS_CLIPBOARD_CID,
    "@mozilla.org/widget/clipboard;1",
    nsClipboardConstructor },
  { "HTML Format Converter",
    NS_HTMLFORMATCONVERTER_CID,
    "@mozilla.org/widget/htmlformatconverter/qt;1",
    nsHTMLFormatConverterConstructor },
  { "Qt Drag Service",
    NS_DRAGSERVICE_CID,
    "@mozilla.org/widget/dragservice;1",
    nsDragServiceConstructor },
#ifdef IBMBIDI
    { "Qt Bidi Keyboard",
    NS_BIDIKEYBOARD_CID,
    "@mozilla.org/widget/bidikeyboard;1",
    nsBidiKeyboardConstructor },
#endif // IBMBIDI
  { "File Spec with UI",
    NS_FILESPECWITHUI_CID,
    "@mozilla.org/filespecwithui;1",
    nsFileSpecWithUIImplConstructor }
};

NS_IMPL_NSGETMODULE("nsWidgetQTModule",components)
