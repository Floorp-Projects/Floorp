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
 *   Christopher Blizzzard <blizzard@mozilla.org>
 *   Stuart Parmenter <pavlov@netscape.com>
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
#include "nsFontRetrieverService.h"
#include "nsDragService.h"
#include "nsFileSpecWithUIImpl.h"
#include "nsScrollbar.h"
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
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontRetrieverService)
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
  { "Gtk nsWindow",
    NS_WINDOW_CID,
    "@mozilla.org/widgets/window/gtk;1",
    nsWindowConstructor },
  { "Gtk Child nsWindow",
    NS_CHILD_CID,
    "@mozilla.org/widgets/child_window/gtk;1",
    ChildWindowConstructor },
  { "Gtk Button",
    NS_BUTTON_CID,
    "@mozilla.org/widgets/button/gtk;1",
    nsButtonConstructor },
  { "Gtk Check Button",
    NS_CHECKBUTTON_CID,
    "@mozilla.org/widgets/checkbutton/gtk;1",
    nsCheckButtonConstructor },
  { "Gtk File Widget",
    NS_FILEWIDGET_CID,
    "@mozilla.org/widgets/filewidget/gtk;1",
    nsFileWidgetConstructor },
  { "Gtk Horiz Scrollbar",
    NS_HORZSCROLLBAR_CID,
    "@mozilla.org/widgets/horizscroll/gtk;1",
    nsHorizScrollbarConstructor },
  { "Gtk Vert Scrollbar",
    NS_VERTSCROLLBAR_CID,
    "@mozilla.org/widgets/vertscroll/gtk;1",
    nsVertScrollbarConstructor },
  { "Gtk Text Widget",
    NS_TEXTFIELD_CID,
    "@mozilla.org/widgets/textwidget/gtk;1",
    nsTextWidgetConstructor },
  { "Gtk AppShell",
    NS_APPSHELL_CID,
    "@mozilla.org/widget/appshell/gtk;1",
    nsAppShellConstructor },
  { "Gtk Toolkit",
    NS_TOOLKIT_CID,
    "@mozilla.org/widget/toolkit/gtk;1",
    nsToolkitConstructor },
  { "Gtk Look And Feel",
    NS_LOOKANDFEEL_CID,
    "@mozilla.org/widget/lookandfeel/gtk;1",
    nsLookAndFeelConstructor },
  { "Gtk Label",
    NS_LABEL_CID,
    "@mozilla.org/widget/label/gtk;1",
    nsLabelConstructor },
  { "Gtk Sound",
    NS_SOUND_CID,
    //    "@mozilla.org/widget/sound/gtk;1"
    "@mozilla.org/sound;1",
    nsSoundConstructor },
  { "Transferrable",
    NS_TRANSFERABLE_CID,
    //    "@mozilla.org/widget/transferrable/gtk;1",
    "@mozilla.org/widget/transferable;1",
    nsTransferableConstructor },
  { "Gtk Clipboard",
    NS_CLIPBOARD_CID,
    //    "@mozilla.org/widget/clipboard/gtk;1",
    "@mozilla.org/widget/clipboard;1",
    nsClipboardConstructor },
  { "XIF Format Converter",
    NS_XIFFORMATCONVERTER_CID,
    "@mozilla.org/widget/xifformatconverter/gtk;1",
    nsXIFFormatConverterConstructor },
  { "Gtk Font Retriever Service",
    NS_FONTRETRIEVERSERVICE_CID,
    "@mozilla.org/widget/fontretrieverservice/gtk;1",
    nsFontRetrieverServiceConstructor },
  { "Gtk Drag Service",
    NS_DRAGSERVICE_CID,
    //    "@mozilla.org/widget/dragservice/gtk;1",
    "@mozilla.org/widget/dragservice;1",
    nsDragServiceConstructor },
  { "File Spec with UI",
    NS_FILESPECWITHUI_CID,
    //    "@mozilla.org/widget/filespecwithui/gtk;1",
    "@mozilla.org/filespecwithui;1",
    nsFileSpecWithUIImplConstructor }
};

NS_IMPL_NSGETMODULE("nsWidgetGTKModule", components)
