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

ScribbleApp scribbleData;

#ifdef XP_PC
#define WIDGET_DLL "raptorwidget.dll"
#define GFX_DLL "raptorgfxwin.dll"
#define TEXT_HEIGHT 25
#endif

#ifdef XP_UNIX
#define WIDGET_DLL "libwidgetunix.so"
#define GFX_DLL "libgfxunix.so"
#define TEXT_HEIGHT 30
#endif

#ifdef XP_MAC
#define WIDGET_DLL "WIDGET_DLL"
#define GFX_DLL "GFXWIN_DLL"
#define TEXT_HEIGHT 30
#endif


static NS_DEFINE_IID(kCAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
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
                nsISupports *next;
                next = enumer->Next();
                if (next) {
                    nsIWidget *widget;
                    if (NS_OK == next->QueryInterface(kIWidgetIID, (void**)&widget)) {
                        widget->Resize(0, 0, 200, 
                          ((nsSizeEvent*)aEvent)->windowSize->height, PR_TRUE);
                        NS_RELEASE(widget);
                    }
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
            NS_RELEASE(scribbleData.red);
            NS_RELEASE(scribbleData.green);
            NS_RELEASE(scribbleData.blue);
            NS_RELEASE(scribbleData.scribble);
            NS_RELEASE(scribbleData.lines);
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

    nsEventStatus result = nsEventStatus_eConsumeNoDefault;
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

            int y = 351;
            nsString red("Red");
            drawCtx->SetColor(NS_RGB(255, 0, 0));
            drawCtx->DrawString(red, 50, y, 200);
            y += TEXT_HEIGHT+2;

            nsString green("Green");
            drawCtx->SetColor(NS_RGB(0, 255, 0));
            drawCtx->DrawString(green, 50, y, 100);
            y += TEXT_HEIGHT+2;

            nsString blue("Blue");
            drawCtx->SetColor(NS_RGB(0, 0, 255));
            drawCtx->DrawString(blue, 50, y, 100);
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

    nsEventStatus result = nsEventStatus_eConsumeNoDefault;
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
                //drawCtx->SetColor(aEvent->widget->GetForegroundColor());
                drawCtx->SetColor(NS_RGB(255, 0, 0));
                drawCtx->DrawLine(scribbleData.mousePos.x, 
                                  scribbleData.mousePos.y,
                                  ((nsGUIEvent*)aEvent)->point.x,
                                  ((nsGUIEvent*)aEvent)->point.y);

                if (scribbleData.scribble->GetState())
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
                PRBool state = option->GetState();
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
                        nscoord	x = rect.x;
                        nscoord	y = rect.y;
                        nscoord width = rect.width;
                        nscoord height = rect.height;

                        nsString circles("Circles");
                        nsString rects("Rectangles");
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

                                rect.MoveTo(x+(rand() % width),y+( rand() % height));
                                rect.SizeTo(rand() % (width - rect.x), 
                                            rand() % (height - rect.y));
                                drawCtx->DrawRect(rect);
                            }
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
            if (NS_OK == aEvent->widget->QueryInterface(kITextWidgetIID, (void**)&text)) {
                if (text == scribbleData.red) {
                    scribbleData.red->GetText(buf, 0);
                    PRInt32 value, err;
                    value = buf.ToInteger(&err);
                    color = NS_RGB(value, NS_GET_G(color), NS_GET_B(color));
                }
                else if (text == scribbleData.green) {
                    scribbleData.green->GetText(buf, 0);
                    PRInt32 value, err;
                    value = buf.ToInteger(&err);
                    color = NS_RGB(NS_GET_R(color), value, NS_GET_B(color));
                }
                else if (text == scribbleData.blue) {
                    scribbleData.blue->GetText(buf, 0);
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

    // register graphics classes
    static NS_DEFINE_IID(kCRenderingContextIID, NS_RENDERING_CONTEXT_CID);
    static NS_DEFINE_IID(kCDeviceContextIID, NS_DEVICE_CONTEXT_CID);
    static NS_DEFINE_IID(kCFontMetricsIID, NS_FONT_METRICS_CID);
    static NS_DEFINE_IID(kCImageIID, NS_IMAGE_CID);

    nsRepository::RegisterFactory(kCRenderingContextIID, GFX_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kCDeviceContextIID, GFX_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kCFontMetricsIID, GFX_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kCImageIID, GFX_DLL, PR_FALSE, PR_FALSE);

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

    nsRepository::RegisterFactory(kCAppShellCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kCWindowCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kCChildCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kCButtonCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kCCheckButtonCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kCComboBoxCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kCFileWidgetCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kCListBoxCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kCRadioButtonCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kCHorzScrollbarCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kCVertScrollbarCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kCTextAreaCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kCTextFieldCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsRepository::RegisterFactory(kCLookAndFeelCID, WIDGET_DLL, PR_FALSE, PR_FALSE);

    //NS_InitToolkit(PR_GetCurrentThread());

    nsresult  res;

    static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
    static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

    res = nsRepository::CreateInstance(kDeviceContextCID, nsnull, kDeviceContextIID, (void **)&scribbleData.mContext);

    if (NS_OK == res)
      scribbleData.mContext->Init(nsnull);


     // Create an application shell
    nsIAppShell *appShell;
    nsRepository::CreateInstance(kCAppShellCID, nsnull, kIAppShellIID,
                                 (void**)&appShell);
    appShell->Create(argc, argv);
   


    //nsILookAndFeel *laf;
    //nsRepository::CreateInstance(kCLookAndFeelCID, nsnull, kILookAndFeelIID,(void**)&laf);

    
    //
    // create the main window
    //
    nsRepository::CreateInstance(kCWindowCID, nsnull, kIWidgetIID, 
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
    nsRepository::CreateInstance(kCChildCID, nsnull, kIWidgetIID, (void **)&controlPane);
    controlPane->Create(scribbleData.mainWindow, rect, HandleEventControlPane, NULL);
    //controlPane->SetBackgroundColor(laf->GetColor(nsLAF::WindowBackground));
    controlPane->SetBackgroundColor(NS_RGB(100,128,128));
    controlPane->Show(PR_TRUE);


    //
    // Add the scribble/lines section
    //

    // create the "Scribble" radio button
    rect.SetRect(50, 50, 100, 25);  

    nsRepository::CreateInstance(kCRadioButtonCID, nsnull, kIRadioButtonIID, (void **)&(scribbleData.scribble));
    scribbleData.scribble->Create(controlPane, rect, HandleEventRadioButton, NULL);
    nsString cbLabel("Scribble");
    scribbleData.scribble->SetLabel(cbLabel);
    scribbleData.scribble->SetState(PR_FALSE);
    //scribbleData.scribble->SetBackgroundColor(laf->GetColor(nsLAF::WindowBackground));
    scribbleData.scribble->Show(PR_TRUE);

    // create the "Lines" radio button
    rect.SetRect(50, 75, 100, 25);  

    nsRepository::CreateInstance(kCRadioButtonCID, nsnull, kIRadioButtonIID, (void **)&(scribbleData.lines));
    scribbleData.lines->Create(controlPane, rect, HandleEventRadioButton, NULL);
    nsString cbLabel1("Lines");
    scribbleData.lines->SetLabel(cbLabel1);
    scribbleData.lines->SetState(PR_TRUE);
    //scribbleData.lines->SetBackgroundColor(laf->GetColor(nsLAF::WindowBackground));
    scribbleData.lines->Show(PR_TRUE);

    // Add the circle/rectangle section

    // create the "Circles" check button
    nsICheckButton * checkButton;
    rect.SetRect(50, 200, 100, 25);  

    nsRepository::CreateInstance(kCCheckButtonCID, nsnull, kICheckButtonIID, (void **)&checkButton);
    checkButton->Create(controlPane, rect, HandleEventCheck, NULL);
    nsString cbLabel2("Circles");
    checkButton->SetLabel(cbLabel2);
    //checkButton->SetBackgroundColor(laf->GetColor(nsLAF::WindowBackground));
    checkButton->Show(PR_TRUE);
    NS_RELEASE(checkButton);

    // create the "Rectangles" check button
    rect.SetRect(50, 225, 100, 25);  

    nsRepository::CreateInstance(kCCheckButtonCID, nsnull, kICheckButtonIID, (void **)&checkButton);
    checkButton->Create(controlPane, rect, HandleEventCheck, NULL);
    nsString cbLabel3("Rectangles");
    checkButton->SetLabel(cbLabel3);
    //checkButton->SetBackgroundColor(laf->GetColor(nsLAF::WindowBackground));
    checkButton->Show(PR_TRUE);
    NS_RELEASE(checkButton);

    //
    // Add the color section
    //

    int y = 350;
    // create the "red" text widget
    rect.SetRect(100, y, 50, TEXT_HEIGHT);  

    nsRepository::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (void **)&(scribbleData.red));
    scribbleData.red->Create(controlPane, rect, HandleEventText, NULL);
    nsString initText("0");
    scribbleData.red->SetText(initText);
    scribbleData.red->SetBackgroundColor(NS_RGB(0, 0, 255));
    scribbleData.red->Show(PR_TRUE);
    y += rect.height +2;

    // create the "green" text widget
    rect.SetRect(100, y, 50, TEXT_HEIGHT);  

    nsRepository::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (void **)&(scribbleData.green));
    scribbleData.green->Create(controlPane, rect, HandleEventText, NULL);
    scribbleData.green->SetText(initText);
    scribbleData.green->SetBackgroundColor(NS_RGB(255, 0, 0));
    scribbleData.green->Show(PR_TRUE);
    y += rect.height +2;

    // create the "blue" text widget
    rect.SetRect(100, y, 50, TEXT_HEIGHT);  

    nsRepository::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (void **)&(scribbleData.blue));
    scribbleData.blue->Create(controlPane, rect, HandleEventText, NULL);
    scribbleData.blue->SetText(initText);
    scribbleData.blue->SetBackgroundColor(NS_RGB(0, 255, 0));
    scribbleData.blue->Show(PR_TRUE);
    y += rect.height +2;

    //
    // create a button  
    //
    nsIButton *button;
    rect.SetRect(50, 500, 100, 25);  
    nsRepository::CreateInstance(kCButtonCID, nsnull, kIButtonIID, (void **)&button);
    button->Create(controlPane, rect, HandleEventButton, NULL);
    nsString label("Clear");
    button->SetLabel(label);
    button->Show(PR_TRUE);
    NS_RELEASE(button);

    NS_RELEASE(controlPane); // the parent keeps a reference on this child

    //
    // create the draw pane
    //
    rect.SetRect(200, 0, 400, 700);  
    nsRepository::CreateInstance(kCChildCID, nsnull, kIWidgetIID, (void **)&scribbleData.drawPane);
    scribbleData.drawPane->Create(scribbleData.mainWindow, rect, HandleEventGraphicPane, NULL);
    //scribbleData.drawPane->SetBackgroundColor(laf->GetColor(nsLAF::WindowBackground));
    scribbleData.drawPane->SetBackgroundColor(NS_RGB(255,250,250));
    scribbleData.drawPane->Show(PR_TRUE);

    // show. We are in business...
    scribbleData.mainWindow->Show(PR_TRUE);

    //laf->Release();

    return(appShell->Run());
}

