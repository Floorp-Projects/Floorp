/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include <stdio.h>
#include <stdlib.h>

#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsXPComCIID.h"
#include "nsIButton.h"
#include "nsICheckButton.h"
#include "nsILookAndFeel.h"
#include "nsIRadioButton.h"
#include "nsITextWidget.h"
#include "nsIScrollbar.h"
#include "nsGUIEvent.h"
#include "nsIEnumerator.h"
#include "nsIRenderingContext.h"
#include "nsFont.h"
#include "nsUnitConversion.h"
#include "nsColor.h"
#include "nsString.h"
#include "Scribble.h"
#include "nsIDeviceContext.h"
#include "nsTransform2D.h"
#include "nsGfxCIID.h"
#include "nsWidgetsCID.h"
#include "nsIAppShell.h"
#include "nsWidgetSupport.h"
#include "nsIImageManager.h"
#include "nsIImageRequest.h"
#include "nsIImageObserver.h"
#include "nsIImageGroup.h"
#include "net.h"

ScribbleApp scribbleData;

static nsIImageManager  *gImageManager = nsnull;
static nsIImageGroup    *gImageGroup = nsnull;
static nsIImageRequest  *gImageReq = nsnull;

#ifdef XP_PC
#define XPCOM_DLL "xpcom32.dll"
#define WIDGET_DLL "raptorwidget.dll"
#define GFXWIN_DLL "raptorgfxwin.dll"
#define TEXT_HEIGHT 25
#define FILE_URL_PREFIX "file://"
#endif

#ifdef XP_UNIX
#define XPCOM_DLL "libxpcom.so"
#define WIDGET_DLL "libwidgetgtk.so"
#define GFXWIN_DLL "libgfxgtk.so"
#define TEXT_HEIGHT 30
#define FILE_URL_PREFIX "file://"
#endif

#ifdef XP_MAC
#define XPCOM_DLL "XPCOM_DLL"
#define WIDGET_DLL "WIDGET_DLL"
#define GFXWIN_DLL "GFXWIN_DLL"
#define TEXT_HEIGHT 30
#define FILE_URL_PREFIX "file:///"
#endif

#define COLOR_FIELDS_X		50
#define COLOR_FIELDS_Y		350

static nsIImage		*gImage = nsnull;
static PRBool			gInstalledColorMap = PR_FALSE;

void  MyLoadImage(char *aFileName);

static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kCAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_IID(kCToolkitCID, NS_TOOLKIT_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kIWindowIID, NS_IWINDOW_IID);	//еее
static NS_DEFINE_IID(kIButtonIID, NS_IBUTTON_IID);
static NS_DEFINE_IID(kIScrollbarIID, NS_ISCROLLBAR_IID);
static NS_DEFINE_IID(kICheckButtonIID, NS_ICHECKBUTTON_IID);
static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kIRadioButtonIID, NS_IRADIOBUTTON_IID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);


//
// Main window events
//
nsEventStatus PR_CALLBACK HandleEventMain(nsGUIEvent *aEvent)
{ 
    
    //printf("aEvent->message %d on 0x%X\n", aEvent->message, aEvent->widget);

    nsEventStatus result = nsEventStatus_eConsumeNoDefault;
    switch(aEvent->message) {
        
        case NS_SIZE:
        {
            nsIEnumerator *enumer = aEvent->widget->GetChildren();
            if (enumer) {
                nsISupports *child;
                if (NS_SUCCEEDED(enumer->CurrentItem(&child))) {
                    nsIWidget *widget;
                    if (NS_OK == child->QueryInterface(kIWidgetIID, (void**)&widget)) {
                        widget->Resize(0, 0, 200, 
                          ((nsSizeEvent*)aEvent)->windowSize->height, PR_TRUE);
                        NS_IF_RELEASE(widget);
                    }
                    NS_IF_RELEASE(child);
                }

                NS_RELEASE(enumer);
                delete enumer;
            }

            scribbleData.drawPane->Resize(200, 
                               0, 
                               ((nsSizeEvent*)aEvent)->windowSize->width - 200,
                               ((nsSizeEvent*)aEvent)->windowSize->height,
                               PR_TRUE);

            break;
        }

        case NS_DESTROY:
            printf("Destroy Window...Release window\n");
            NS_IF_RELEASE(scribbleData.red);
            NS_IF_RELEASE(scribbleData.green);
            NS_IF_RELEASE(scribbleData.blue);
            NS_IF_RELEASE(scribbleData.scribble);
            NS_IF_RELEASE(scribbleData.lines);
           // NS_RELEASE(scribbleData.drawPane);
           // NS_RELEASE(scribbleData.mainWindow); 

            exit(0); // for now
            break;

        default:
            result = nsEventStatus_eIgnore;
    }

    return result;
}


//
// Control pane events
//
nsEventStatus PR_CALLBACK HandleEventControlPane(nsGUIEvent *aEvent)
{
    //printf("aEvent->message %d on 0x%X\n", aEvent->message, aEvent->widget);

    switch(aEvent->message) {
        case NS_PAINT:
        {
            // paint the background
            nsIRenderingContext *drawCtx = ((nsPaintEvent*)aEvent)->renderingContext;
            drawCtx->SetColor(aEvent->widget->GetBackgroundColor());
            drawCtx->FillRect(*(((nsPaintEvent*)aEvent)->rect));

            // draw the line separating the two panes
            drawCtx->SetColor(aEvent->widget->GetForegroundColor());
            drawCtx->DrawLine(198, 0, 198, 800);
            drawCtx->DrawLine(199, 0, 199, 800);
            drawCtx->DrawLine(200, 0, 200, 800);

            // draw the colors text
            nsFont font("Times", NS_FONT_STYLE_NORMAL,
                         NS_FONT_VARIANT_NORMAL,
                         NS_FONT_WEIGHT_BOLD,
                         0,
                         12);
            drawCtx->SetFont(font);

            int y = COLOR_FIELDS_Y + TEXT_HEIGHT/2 - font.size/2;
            nsString red("Red");
            drawCtx->SetColor(NS_RGB(255, 0, 0));
            drawCtx->DrawString(red, 50, y);
            y += TEXT_HEIGHT+2;

            nsString green("Green");
            drawCtx->SetColor(NS_RGB(0, 255, 0));
            drawCtx->DrawString(green, 50, y);
            y += TEXT_HEIGHT+2;

            nsString blue("Blue");
            drawCtx->SetColor(NS_RGB(0, 0, 255));
            drawCtx->DrawString(blue, 50, y);
            y += TEXT_HEIGHT+2;

            return nsEventStatus_eConsumeNoDefault;
        }
    }

    return nsEventStatus_eIgnore;
}


//
// Graphic pane events
//
nsEventStatus PR_CALLBACK HandleEventGraphicPane(nsGUIEvent *aEvent)
{
    //printf("aEvent->message %d on 0x%X\n", aEvent->message, aEvent->widget);
    switch(aEvent->message) {

        case NS_PAINT:
        {
            nsIRenderingContext *drawCtx = ((nsPaintEvent*)aEvent)->renderingContext;
            drawCtx->SetColor(aEvent->widget->GetBackgroundColor());
            drawCtx->FillRect(*(((nsPaintEvent*)aEvent)->rect));

            return nsEventStatus_eConsumeNoDefault;
        }

        case NS_MOUSE_LEFT_BUTTON_DOWN:
            aEvent->widget->SetFocus();
            scribbleData.isDrawing = PR_TRUE;
            scribbleData.mousePos = ((nsGUIEvent*)aEvent)->point;
            return nsEventStatus_eConsumeNoDefault;

        case NS_MOUSE_LEFT_BUTTON_UP:
            scribbleData.isDrawing = PR_FALSE;
            return nsEventStatus_eConsumeNoDefault;

        case NS_MOUSE_MOVE:
        {
            if (scribbleData.isDrawing) {
            
                nsIRenderingContext *drawCtx = aEvent->widget->GetRenderingContext();
                drawCtx->SetColor(aEvent->widget->GetForegroundColor());
                drawCtx->DrawLine(scribbleData.mousePos.x, 
                                  scribbleData.mousePos.y,
                                  ((nsGUIEvent*)aEvent)->point.x,
                                  ((nsGUIEvent*)aEvent)->point.y);

                PRBool state;
                scribbleData.scribble->GetState(state);
                if (state)
                   scribbleData.mousePos = ((nsGUIEvent*)aEvent)->point;
  

                NS_RELEASE(drawCtx);
            }

            return nsEventStatus_eConsumeNoDefault;
        }

    }

    return nsEventStatus_eIgnore;
}


//
// Buttons events
//
nsEventStatus PR_CALLBACK HandleEventButton(nsGUIEvent *aEvent)
{
    //printf("aEvent->message %d on 0x%X\n", aEvent->message, aEvent->widget);

    switch(aEvent->message) {
        case NS_MOUSE_LEFT_BUTTON_UP:
            scribbleData.drawPane->Invalidate(PR_TRUE);
    }

    return nsEventStatus_eIgnore;
}

//
// Buttons events
//
nsEventStatus PR_CALLBACK HandleEventRadioButton(nsGUIEvent *aEvent)
{
    //printf("aEvent->message %d on 0x%X\n", aEvent->message, aEvent->widget);

    switch(aEvent->message) {
        case NS_MOUSE_LEFT_BUTTON_UP: {
            nsIWidget * win;
            if (NS_OK == scribbleData.lines->QueryInterface(kIWidgetIID, (void**)&win)) {
              if (win == aEvent->widget) {
                scribbleData.lines->SetState(PR_TRUE);
                scribbleData.scribble->SetState(PR_FALSE);
              } else {
                scribbleData.lines->SetState(PR_FALSE);
                scribbleData.scribble->SetState(PR_TRUE);
              }
            }
        }
    }

    return nsEventStatus_eIgnore;
}



//
// Handle events for the auto-mode (rectangles/circles)
//
nsEventStatus PR_CALLBACK HandleEventCheck(nsGUIEvent *aEvent)
{
    //printf("aEvent->message %d on 0x%X\n", aEvent->message, aEvent->widget);
    //printf("aEvent->message %d == %d on 0x%X\n", aEvent->message, NS_MOUSE_LEFT_BUTTON_UP, aEvent->widget);

    switch(aEvent->message) {
        case NS_MOUSE_LEFT_BUTTON_UP: 
        {
            nsICheckButton *option;

            if (NS_OK == aEvent->widget->QueryInterface(kICheckButtonIID, (void**)&option)) {
                // invert the two checkboxes state
                PRBool state;
                option->GetState(state);
                option->SetState((state) ? PR_FALSE : PR_TRUE);

                if (state == PR_FALSE) {
                    nsAutoString buf;
                    option->GetLabel(buf);

                    nsIRenderingContext *drawCtx = scribbleData.drawPane->GetRenderingContext();
                    
                    //
                    srand(aEvent->time);
                    if (drawCtx) {

                        // a sort of random rect
                        nsRect rect;
                        scribbleData.drawPane->GetBounds(rect);
#if 0
                        nscoord	x = rect.x;
                        nscoord	y = rect.y;
#else
                        nscoord	x = 0;
                        nscoord	y = 0;
#endif
                        nscoord width = rect.width;
                        nscoord height = rect.height;

                        nsString circles("Circles");
                        nsString rects("Rectangles");
                        nsString image("Image");
                        if (buf.Equals(circles)) {
                            for (int i = 0; i < 100; i++) {
                                drawCtx->SetColor((nscolor)rand());

                                rect.MoveTo(x+(rand() % width),y+( rand() % height));
                                rect.SizeTo(rand() % (width - rect.x), 
                                            rand() % (height - rect.y));
                                drawCtx->DrawEllipse(rect);
                            }
                        }
                        else if (buf.Equals(rects)) {
                            for (int i = 0; i < 100; i++) {
                                drawCtx->SetColor((nscolor)rand());

                                rect.MoveTo(x+(rand() % width+1),y+( rand() % height+1));
                                rect.SizeTo(rand() % (width - rect.x)+1, 
                                            rand() % (height - rect.y)+1);
                                drawCtx->DrawRect(rect);
                            }
                        }
                        else 
                        	if (buf.Equals(image)) 
	                        	{
#ifdef XP_MAC
                            char szFile[256] = "file:///Raptor/moz/mozilla/webshell/tests/viewer/samples/raptor.jpg";
#else
                            char szFile[256] = "S:\\mozilla\\dist\\WIN32_D.OBJ\\bin\\res\\samples\\raptor.jpg";
#endif

                            // put up an image
                            MyLoadImage(szFile);
	                        	}

                        NS_RELEASE(drawCtx);
                    }
                }
            }

        }
    }

    return nsEventStatus_eIgnore;
}


//
// Handle events for the text fields
//
nsEventStatus PR_CALLBACK HandleEventText(nsGUIEvent *aEvent)
{
    //if (aEvent->message != 300)
      //printf("HandleEventText message %d on 0x%X\n", aEvent->message, aEvent->widget);

    switch(aEvent->message) {
        case NS_LOSTFOCUS: 
        {
            nscolor color = scribbleData.drawPane->GetForegroundColor();
            nsAutoString buf;
            
            nsITextWidget *text;
            PRUint32  size;
            if (NS_OK == aEvent->widget->QueryInterface(kITextWidgetIID, (void**)&text)) {
                if (text == scribbleData.red) {
                    scribbleData.red->GetText(buf, 0, size);
                    PRInt32 value, err;
                    value = buf.ToInteger(&err);
                    color = NS_RGB(value, NS_GET_G(color), NS_GET_B(color));
                }
                else if (text == scribbleData.green) {
                    scribbleData.green->GetText(buf, 0, size);
                    PRInt32 value, err;
                    value = buf.ToInteger(&err);
                    color = NS_RGB(NS_GET_R(color), value, NS_GET_B(color));
                }
                else if (text == scribbleData.blue) {
                    scribbleData.blue->GetText(buf, 0, size);
                    PRInt32 value, err;
                    value = buf.ToInteger(&err);
                    color = NS_RGB(NS_GET_R(color), NS_GET_G(color), value);
                }

                NS_RELEASE(text);
            }

            scribbleData.drawPane->SetForegroundColor(color);

            return nsEventStatus_eIgnore;
        }
    }

    return nsEventStatus_eIgnore;
}




//
// Main application entry function
//
nsresult CreateApplication(int * argc, char ** argv)
{
    scribbleData.isDrawing = PR_FALSE;

    // register xpcom classes
    nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);

    // register graphics classes
    static NS_DEFINE_IID(kCRenderingContextIID, NS_RENDERING_CONTEXT_CID);
    static NS_DEFINE_IID(kCDeviceContextIID, NS_DEVICE_CONTEXT_CID);
    static NS_DEFINE_IID(kCFontMetricsIID, NS_FONT_METRICS_CID);
    static NS_DEFINE_IID(kCImageIID, NS_IMAGE_CID);

    nsComponentManager::RegisterComponent(kCRenderingContextIID, NULL, NULL, GFXWIN_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kCDeviceContextIID, NULL, NULL, GFXWIN_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kCFontMetricsIID, NULL, NULL, GFXWIN_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kCImageIID, NULL, NULL, GFXWIN_DLL, PR_FALSE, PR_FALSE);

    // register widget classes
    static NS_DEFINE_IID(kCWindowCID, NS_WINDOW_CID);
    static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);
    static NS_DEFINE_IID(kCButtonCID, NS_BUTTON_CID);
    static NS_DEFINE_IID(kCCheckButtonCID, NS_CHECKBUTTON_CID);
    static NS_DEFINE_IID(kCComboBoxCID, NS_COMBOBOX_CID);
    static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
    static NS_DEFINE_IID(kCListBoxCID, NS_LISTBOX_CID);
    static NS_DEFINE_IID(kCRadioButtonCID, NS_RADIOBUTTON_CID);
    static NS_DEFINE_IID(kCHorzScrollbarCID, NS_HORZSCROLLBAR_CID);
    static NS_DEFINE_IID(kCVertScrollbarCID, NS_VERTSCROLLBAR_CID);
    static NS_DEFINE_IID(kCTextAreaCID, NS_TEXTAREA_CID);
    static NS_DEFINE_IID(kCTextFieldCID, NS_TEXTFIELD_CID);
    static NS_DEFINE_IID(kCLookAndFeelCID, NS_LOOKANDFEEL_CID);

    nsComponentManager::RegisterComponent(kCAppShellCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
 		nsComponentManager::RegisterComponent(kCToolkitCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kCWindowCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kCChildCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kCButtonCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kCCheckButtonCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kCComboBoxCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kCFileWidgetCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kCListBoxCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kCRadioButtonCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kCHorzScrollbarCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kCVertScrollbarCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kCTextAreaCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kCTextFieldCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kCLookAndFeelCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);

    //NS_InitToolkit(PR_GetCurrentThread());

    nsresult  res;

    static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
    static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

    // Create the Event Queue for the UI thread...
    res = nsServiceManager::GetService(kEventQueueServiceCID,
                                       kIEventQueueServiceIID,
                                       (nsISupports **)&scribbleData.mEventQService);

    if (NS_OK != res) {
        NS_ASSERTION(PR_FALSE, "Could not obtain the event queue service");
        return res;
    }

    printf("Going to create the event queue\n");
    res = scribbleData.mEventQService->CreateThreadEventQueue();
    if (NS_OK != res) {
        NS_ASSERTION(PR_FALSE, "Could not create the event queue for the thread");
	return res;
    }

     // Create an application shell
    nsIAppShell *appShell = nsnull;
    nsComponentManager::CreateInstance(kCAppShellCID, nsnull, kIAppShellIID,
                                 (void**)&appShell);
    appShell->Create(argc, argv);

    res = nsComponentManager::CreateInstance(kDeviceContextCID, nsnull, kDeviceContextIID, (void **)&scribbleData.mContext);

    if (NS_OK == res)
      scribbleData.mContext->Init(nsnull);


    //nsILookAndFeel *laf;
    //nsComponentManager::CreateInstance(kCLookAndFeelCID, nsnull, kILookAndFeelIID,(void**)&laf);

    //
    // create the main window
    //
    nsComponentManager::CreateInstance(kCWindowCID, nsnull, kIWindowIID,
                                 (void **)&(scribbleData.mainWindow));
    nsRect rect(100, 100, 600, 700);
    scribbleData.mainWindow->Create((nsIWidget*)NULL, 
                                    rect, 
                                    HandleEventMain, 
                                    NULL,
                                    appShell);
    //scribbleData.mainWindow->SetBackgroundColor(laf->GetColor(nsLAF::WindowBackground));
    scribbleData.mainWindow->SetBackgroundColor(NS_RGB(255,255,255));
    scribbleData.mainWindow->SetTitle("Scribble");
    //
    // create the control pane
    //
    nsIWidget *controlPane;
    rect.SetRect(0, 0, 200, 700);  
    nsComponentManager::CreateInstance(kCChildCID, nsnull, kIWidgetIID, (void **)&controlPane);
    controlPane->Create(scribbleData.mainWindow, rect, HandleEventControlPane, NULL);
    //controlPane->SetBackgroundColor(laf->GetColor(nsLAF::WindowBackground));
    controlPane->SetBackgroundColor(NS_RGB(100,128,128));
    controlPane->Show(PR_TRUE);

    //
    // Add the scribble/lines section
    //

    // create the "Scribble" radio button
    rect.SetRect(50, 50, 100, 25);  
    nsComponentManager::CreateInstance(kCRadioButtonCID, nsnull, kIRadioButtonIID, (void **)&(scribbleData.scribble));
    NS_CreateRadioButton(controlPane,scribbleData.scribble,rect,HandleEventRadioButton);
    scribbleData.scribble->SetLabel("Scribble");
    scribbleData.scribble->SetState(PR_FALSE);
    //scribbleData.scribble->SetBackgroundColor(laf->GetColor(nsLAF::WindowBackground));

    // create the "Lines" radio button
    rect.SetRect(50, 75, 100, 25);  

    nsComponentManager::CreateInstance(kCRadioButtonCID, nsnull, kIRadioButtonIID, (void **)&(scribbleData.lines));
    NS_CreateRadioButton(controlPane,scribbleData.lines,rect,HandleEventRadioButton);
    scribbleData.lines->SetLabel("Lines");
    scribbleData.lines->SetState(PR_TRUE);
    //scribbleData.lines->SetBackgroundColor(laf->GetColor(nsLAF::WindowBackground));
    // Add the circle/rectangle section

    // create the "Circles" check button
    nsICheckButton * checkButton;
    rect.SetRect(50, 200, 100, 25);  

    nsIWidget* widget = nsnull;
    nsComponentManager::CreateInstance(kCCheckButtonCID, nsnull, kICheckButtonIID, (void **)&checkButton);
    if (NS_OK == checkButton->QueryInterface(kIWidgetIID,(void **)&widget))
    {
      widget->Create(controlPane, rect, HandleEventCheck, NULL);
      nsString cbLabel2("Circles");
      checkButton->SetLabel(cbLabel2);
      //checkButton->SetBackgroundColor(laf->GetColor(nsLAF::WindowBackground));
      widget->Show(PR_TRUE);
      NS_RELEASE(widget);
    }
    NS_RELEASE(checkButton);

    // create the "Rectangles" check button
    rect.SetRect(50, 225, 100, 25);  

    nsComponentManager::CreateInstance(kCCheckButtonCID, nsnull, kICheckButtonIID, (void **)&checkButton);
    NS_CreateCheckButton(controlPane,checkButton,rect,HandleEventCheck);
    if (NS_OK == checkButton->QueryInterface(kIWidgetIID,(void**)&widget))
    {
      // widget->SetBackgroundColor(laf->GetColor(nsLAF::WindowBackground));
      checkButton->SetLabel("Rectangles");
      NS_RELEASE(widget);
    }
    NS_RELEASE(checkButton);

    // create the "Images" check button
    rect.SetRect(50, 250, 100, 25);  

    nsComponentManager::CreateInstance(kCCheckButtonCID, nsnull, kICheckButtonIID, (void **)&checkButton);
    if (NS_OK == checkButton->QueryInterface(kIWidgetIID,(void **)&widget))
    {
      widget->Create(controlPane, rect, HandleEventCheck, NULL);
      nsString cbLabel4("Image");
      checkButton->SetLabel(cbLabel4);
      //checkButton->SetBackgroundColor(laf->GetColor(nsLAF::WindowBackground));
      widget->Show(PR_TRUE);
      NS_RELEASE(widget);
    }
    NS_RELEASE(checkButton);

    //
    // Add the color section
    //

    int y = COLOR_FIELDS_Y;
    // create the "red" text widget
    rect.SetRect(100, y, 50, TEXT_HEIGHT);  

    PRUint32 size;
    nsString initText("0");
    nsComponentManager::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (void **)&(scribbleData.red));
    if (NS_OK == scribbleData.red->QueryInterface(kIWidgetIID,(void **)&widget))
    {
      widget->Create(controlPane, rect, HandleEventText, NULL);
      scribbleData.red->SetText(initText,size);
      widget->SetBackgroundColor(NS_RGB(0, 0, 255));
      widget->Show(PR_TRUE);
    }
    y += rect.height +2;

    // create the "green" text widget
    rect.SetRect(100, y, 50, TEXT_HEIGHT);  

    nsComponentManager::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (void **)&(scribbleData.green));
    if (NS_OK == scribbleData.green->QueryInterface(kIWidgetIID,(void **)&widget))
    {
      widget->Create(controlPane, rect, HandleEventText, NULL);
      scribbleData.green->SetText(initText,size);
      widget->SetBackgroundColor(NS_RGB(255, 0, 0));
      widget->Show(PR_TRUE);
    }
    y += rect.height +2;

    // create the "blue" text widget
    rect.SetRect(100, y, 50, TEXT_HEIGHT);  

    nsComponentManager::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (void **)&(scribbleData.blue));
    if (NS_OK == scribbleData.blue->QueryInterface(kIWidgetIID,(void **)&widget))
    {
      widget->Create(controlPane, rect, HandleEventText, NULL);
      scribbleData.blue->SetText(initText,size);
      widget->SetBackgroundColor(NS_RGB(0, 255, 0));
      widget->Show(PR_TRUE);
    }
    y += rect.height +2;

    //
    // create a button  
    //
    nsIButton *button;
    rect.SetRect(50, 500, 100, 25);  
    nsComponentManager::CreateInstance(kCButtonCID, nsnull, kIButtonIID, (void **)&button);
    NS_CreateButton(controlPane,button,rect,HandleEventButton);
    button->SetLabel("Clear");

    NS_RELEASE(button);
    NS_RELEASE(controlPane); // the parent keeps a reference on this child

    //
    // create the draw pane
    //
    rect.SetRect(200, 0, 400, 700);  
    nsComponentManager::CreateInstance(kCChildCID, nsnull, kIWidgetIID, (void **)&scribbleData.drawPane);
    scribbleData.drawPane->Create(scribbleData.mainWindow, rect, HandleEventGraphicPane, NULL);
    //scribbleData.drawPane->SetBackgroundColor(laf->GetColor(nsLAF::WindowBackground));
    scribbleData.drawPane->SetBackgroundColor(NS_RGB(255,250,250));
    scribbleData.drawPane->Show(PR_TRUE);

    // show. We are in business...
    scribbleData.mainWindow->Show(PR_TRUE);

    //laf->Release();

    polllistener *plistener = new polllistener;
    appShell->SetDispatchListener(plistener);

    return(appShell->Run());
}

//=================================================================

class MyObserver : public nsIImageRequestObserver
{
public:
	MyObserver();
	virtual ~MyObserver();
	
	NS_DECL_ISUPPORTS
	
	virtual void	Notify(nsIImageRequest *aImageRequest,nsIImage *aImage,nsImageNotification aNotificationType,
											PRInt32 aParam1,PRInt32 aParam2,void *aParam3);
											
	virtual void NotifyError(nsIImageRequest *aImageRequest,nsImageError aErrorType);
	
};

//=================================================================

MyObserver::MyObserver()
{
}

//=================================================================

MyObserver::~MyObserver()
{

}

//=================================================================

static NS_DEFINE_IID(kIImageObserverIID,NS_IIMAGEREQUESTOBSERVER_IID);
NS_IMPL_ISUPPORTS(MyObserver,kIImageObserverIID)

void
MyObserver::Notify(nsIImageRequest *aImageRequest,
									 nsIImage *aImage,
									 nsImageNotification aNotificationType,
									 PRInt32 aParam1,PRInt32 aParam2,void *aParam3)
{

	switch (aNotificationType) 
	  { 
	   case nsImageNotification_kDimensions: 
	    { 
	    char buffer[40]; 
	    sprintf(buffer, "Image:%d x %d", aParam1, aParam2); 
	    } 
	   break; 

	   case nsImageNotification_kPixmapUpdate: 
	   case nsImageNotification_kImageComplete: 
	   case nsImageNotification_kFrameComplete: 
	    { 
	    if (gImage == nsnull && aImage) 
	      { 
	      gImage = aImage; 
	      NS_ADDREF(aImage); 
	      } 

	    if (!gInstalledColorMap && gImage) 
	      { 
	      nsColorMap *cmap = gImage->GetColorMap(); 

	      if (cmap != nsnull && cmap->NumColors > 0) 
	        { 
	        //gWindow->SetColorMap(cmap); 
	        } 
	      gInstalledColorMap = PR_TRUE; 
	      } 

      nsIRenderingContext *drawCtx = scribbleData.drawPane->GetRenderingContext();

	    if (gImage) 
	      { 
        nscoord	x = 0;
        nscoord	y = 0;

				scribbleData.drawPane->ConvertToDeviceCoordinates(x,y);
	      drawCtx->DrawImage(gImage,x, y, gImage->GetWidth(), gImage->GetHeight()); 
	      } 
	   } 
	   break; 
	   default: /* should never get here? */
	   break;
	} 
} 

//------------------------------------------------------------ 

void 
MyObserver::NotifyError(nsIImageRequest *aImageRequest, 
                        nsImageError aErrorType) 
{ 
  //::MessageBox(NULL, "Image loading error!",class1Name, MB_OK); 
} 
  
//------------------------------------------------------------

void
MyInterrupt()
{
  if (gImageGroup) 
    {
    gImageGroup->Interrupt();
    }
}

//------------------------------------------------------------


void
MyLoadImage(char *aFileName)
{
char fileURL[256];
#ifdef XP_PC
char *str;
#endif

    MyInterrupt();

   if (gImageReq) 
    {
    NS_RELEASE(gImageReq);
    gImageReq = NULL;
    }

   if (gImage) 
      {
      NS_RELEASE(gImage);
      gImage = NULL;
      }

    if (gImageGroup == NULL) 
      {
      nsIDeviceContext *deviceCtx = scribbleData.mContext;
      if (NS_NewImageGroup(&gImageGroup) != NS_OK || gImageGroup->Init(deviceCtx, nsnull) != NS_OK) 
        {
        NS_RELEASE(deviceCtx);
        return;
        }
      NS_RELEASE(deviceCtx);
      }

#ifdef XP_MAC
    strcpy(fileURL, aFileName);
#else
    strcpy(fileURL, FILE_URL_PREFIX);
    strcpy(fileURL + strlen(FILE_URL_PREFIX), aFileName);
#endif

#ifdef XP_PC
    str = fileURL;
    while ((str = strchr(str, '\\')) != NULL)
        *str = '/';
#endif

    nscolor white;

    MyObserver *observer = new MyObserver();
    NS_ColorNameToRGB("white", &white);
    gImageReq = gImageGroup->GetImage(fileURL,observer,&white, 0, 0, 0);
}


void
polllistener::AfterDispatch()
{
#ifndef XP_MAC
  NET_PollSockets();
#endif
}
