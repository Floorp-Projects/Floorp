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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Peter Hartshorn <peter@igelaus.com.au>
 *   Ken Faulkner <faulkner@igelaus.com.au>
 */

#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"

#include "nsWindow.h"
#include "nsButton.h"
#include "nsCheckButton.h"
#include "nsFileWidget.h"
#include "nsTextWidget.h"
#include "nsAppShell.h"
#include "nsToolkit.h"
#include "nsLookAndFeel.h"
#include "nsLabel.h"
#include "nsTransferable.h"
#include "nsClipboard.h"
#include "nsXIFFormatConverter.h"
//#include "nsFontRetrieverService.h"
#include "nsDragService.h"
#include "nsFileSpecWithUIImpl.h"
#include "nsScrollBar.h"
#include "nsSound.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(ChildWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsButton)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCheckButton)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFileWidget)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTextWidget)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAppShell)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsToolkit)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLookAndFeel)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLabel)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXIFFormatConverter)
//NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontRetrieverService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDragService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFileSpecWithUIImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSound)

static nsresult nsHorizScrollbarConstructor (nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;
  nsISupports *inst = nsnull;

  if ( NULL == aResult )
  {
    rv = NS_ERROR_NULL_POINTER;
    return rv;
  }
  *aResult = NULL;
  if (NULL != aOuter)
  {
    rv = NS_ERROR_NO_AGGREGATION;
    return rv;
  }
  
  inst = (nsISupports *)(nsBaseWidget *)(nsWidget *)new nsScrollbar(PR_FALSE);
  if (inst == NULL)
  {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return rv;
}

static nsresult nsVertScrollbarConstructor (nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;
  nsISupports *inst = nsnull;

  if ( NULL == aResult )
  {
    rv = NS_ERROR_NULL_POINTER;
    return rv;
  }
  *aResult = NULL;
  if (NULL != aOuter)
  {
    rv = NS_ERROR_NO_AGGREGATION;
    return rv;
  }
  
  inst = (nsISupports *)(nsBaseWidget *)(nsWidget *)new nsScrollbar(PR_TRUE);
  if (inst == NULL)
  {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return rv;
}

static nsModuleComponentInfo components[] =
{
  { "Xlib nsWindow",
    NS_WINDOW_CID,
    "@mozilla.org/widgets/window/xlib;1",
    nsWindowConstructor },
  { "Xlib Child nsWindow",
    NS_CHILD_CID,
    "@mozilla.org/widgets/child_window/xlib;1",
    ChildWindowConstructor },
  { "Xlib Button",
    NS_BUTTON_CID,
    "@mozilla.org/widgets/button/xlib;1",
    nsButtonConstructor },
  { "Xlib Check Button",
    NS_CHECKBUTTON_CID,
    "@mozilla.org/widgets/checkbutton/xlib;1",
    nsCheckButtonConstructor },
  { "Xlib File Widget",
    NS_FILEWIDGET_CID,
    "@mozilla.org/widgets/filewidget/xlib;1",
    nsFileWidgetConstructor },
  { "Xlib Horiz Scrollbar",
    NS_HORZSCROLLBAR_CID,
    "@mozilla.org/widgets/horizscroll/xlib;1",
    nsHorizScrollbarConstructor },
  { "Xlib Vert Scrollbar",
    NS_VERTSCROLLBAR_CID,
    "@mozilla.org/widgets/vertscroll/xlib;1",
    nsVertScrollbarConstructor },
  { "Xlib Text Widget",
    NS_TEXTFIELD_CID,
    "@mozilla.org/widgets/textwidget/xlib;1",
    nsTextWidgetConstructor },
  { "Xlib AppShell",
    NS_APPSHELL_CID,
    "@mozilla.org/widget/appshell/xlib;1",
    nsAppShellConstructor },
  { "Xlib Toolkit",
    NS_TOOLKIT_CID,
    "@mozilla.org/widget/toolkit/xlib;1",
    nsToolkitConstructor },
  { "Xlib Look And Feel",
    NS_LOOKANDFEEL_CID,
    "@mozilla.org/widget/lookandfeel/xlib;1",
    nsLookAndFeelConstructor },
  { "Xlib Label",
    NS_LABEL_CID,
    "@mozilla.org/widget/label/xlib;1",
    nsLabelConstructor },
  { "Xlib Sound",
    NS_SOUND_CID,
      "@mozilla.org/widget/sound/xlib;1",
    //"@mozilla.org/sound;1",
    nsSoundConstructor },
  { "Transferrable",
    NS_TRANSFERABLE_CID,
    //    "@mozilla.org/widget/transferrable/xlib;1",
    "@mozilla.org/widget/transferable;1",
    nsTransferableConstructor },
  { "Xlib Clipboard",
    NS_CLIPBOARD_CID,
    //    "@mozilla.org/widget/clipboard/xlib;1",
    "@mozilla.org/widget/clipboard;1",
    nsClipboardConstructor },
  { "XIF Format Converter",
    NS_XIFFORMATCONVERTER_CID,
    "@mozilla.org/widget/xifformatconverter/xlib;1",
    nsXIFFormatConverterConstructor },
  //{ "Xlib Font Retriever Service",
    //NS_FONTRETRIEVERSERVICE_CID,
    //"@mozilla.org/widget/fontretrieverservice/xlib;1",
    //nsFontRetrieverServiceConstructor },
  { "Xlib Drag Service",
    NS_DRAGSERVICE_CID,
    //    "@mozilla.org/widget/dragservice/xlib;1",
    "@mozilla.org/widget/dragservice;1",
    nsDragServiceConstructor },
  { "File Spec with UI",
    NS_FILESPECWITHUI_CID,
    //    "@mozilla.org/widget/filespecwithui/xlib;1",
    "@mozilla.org/filespecwithui;1",
    nsFileSpecWithUIImplConstructor }
};

NS_IMPL_NSGETMODULE("nsWidgetXLIBModule", components)
