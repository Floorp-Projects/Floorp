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

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "nsIButton.h"
#include "nsICheckButton.h"
#include "nsIRadioButton.h"
#include "nsIRadioGroup.h"
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

static NS_DEFINE_IID(kCAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kIButtonIID, NS_IBUTTON_IID);
static NS_DEFINE_IID(kIScrollbarIID, NS_ISCROLLBAR_IID);
static NS_DEFINE_IID(kICheckButtonIID, NS_ICHECKBUTTON_IID);
static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kIRadioButtonIID, NS_IRADIOBUTTON_IID);
static NS_DEFINE_IID(kIRadioGroupIID, NS_IRADIOGROUP_IID);


//
// Main window events
//
nsEventStatus PR_CALLBACK HandleEventMain(nsGUIEvent *aEvent)
{ 
    
    printf("aEvent->message %d on 0x%X\n", aEvent->message, aEvent->widget);

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
                        widget->Resize(0, 0, 200, ((nsSizeEvent*)aEvent)->windowSize->height - 26, PR_TRUE);
                        NS_RELEASE(widget);
                    }
                }

                NS_RELEASE(enumer);
                delete enumer;
            }

            scribbleData.drawPane->Resize(200, 
                                          0, 
                                          ((nsSizeEvent*)aEvent)->windowSize->width - 206,
                                          ((nsSizeEvent*)aEvent)->windowSize->height - 26,
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
            NS_RELEASE(scribbleData.group);
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
    printf("aEvent->message %d on 0x%X\n", aEvent->message, aEvent->widget);

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

            nsString red("Red");
            drawCtx->SetColor(NS_RGB(255, 0, 0));
            drawCtx->DrawString(red, 50, 351, 50);

            nsString green("Green");
            drawCtx->SetColor(NS_RGB(0, 255, 0));
            drawCtx->DrawString(green, 50, 372, 50);

            nsString blue("Blue");
            drawCtx->SetColor(NS_RGB(0, 0, 255));
            drawCtx->DrawString(blue, 50, 393, 50);

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
    printf("aEvent->message %d on 0x%X\n", aEvent->message, aEvent->widget);

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
                drawCtx->SetColor(aEvent->widget->GetForegroundColor());
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
    printf("aEvent->message %d on 0x%X\n", aEvent->message, aEvent->widget);

    switch(aEvent->message) {
        case NS_MOUSE_LEFT_BUTTON_UP:
            scribbleData.drawPane->Invalidate(PR_TRUE);
    }

    return nsEventStatus_eIgnore;
}



//
// Handle events for the auto-mode (rectangles/circles)
//
nsEventStatus PR_CALLBACK HandleEventCheck(nsGUIEvent *aEvent)
{
    printf("aEvent->message %d on 0x%X\n", aEvent->message, aEvent->widget);
    printf("aEvent->message %d == %d on 0x%X\n", aEvent->message, NS_MOUSE_LEFT_BUTTON_UP, aEvent->widget);

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
                        nscoord width = rect.width;
                        nscoord height = rect.height;

                        nsString circles("Circles");
                        nsString rects("Rectangles");
                        if (buf.Equals(circles)) {
                            for (int i = 0; i < 100; i++) {
                                drawCtx->SetColor((nscolor)rand());

                                rect.MoveTo(rand() % width, rand() % height);
                                rect.SizeTo(rand() % (width - rect.x), 
                                            rand() % (height - rect.y));
                                drawCtx->DrawEllipse(rect);
                            }
                        }
                        else if (buf.Equals(rects)) {
                            for (int i = 0; i < 100; i++) {
                                drawCtx->SetColor((nscolor)rand());

                                rect.MoveTo(rand() % width, rand() % height);
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
    printf("aEvent->message %d on 0x%X\n", aEvent->message, aEvent->widget);

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

#define WIDGET_DLL "raptorwidget.dll"
#define GFXWIN_DLL "raptorgfxwin.dll"

//
// Main application entry function
//
nsresult CreateApplication()
{
    scribbleData.isDrawing = PR_FALSE;

    // register graphics classes
    static NS_DEFINE_IID(kCRenderingContextIID, NS_RENDERING_CONTEXT_CID);
    static NS_DEFINE_IID(kCDeviceContextIID, NS_DEVICE_CONTEXT_CID);
    static NS_DEFINE_IID(kCFontMetricsIID, NS_FONT_METRICS_CID);
    static NS_DEFINE_IID(kCImageIID, NS_IMAGE_CID);

    NSRepository::RegisterFactory(kCRenderingContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCDeviceContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCFontMetricsIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCImageIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);

    // register widget classes
    static NS_DEFINE_IID(kCWindowCID, NS_WINDOW_CID);
    static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);
    static NS_DEFINE_IID(kCButtonCID, NS_BUTTON_CID);
    static NS_DEFINE_IID(kCCheckButtonCID, NS_CHECKBUTTON_CID);
    static NS_DEFINE_IID(kCComboBoxCID, NS_COMBOBOX_CID);
    static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
    static NS_DEFINE_IID(kCListBoxCID, NS_LISTBOX_CID);
    static NS_DEFINE_IID(kCRadioButtonCID, NS_RADIOBUTTON_CID);
    static NS_DEFINE_IID(kCRadioGroupCID, NS_RADIOGROUP_CID);
    static NS_DEFINE_IID(kCHorzScrollbarCID, NS_HORZSCROLLBAR_CID);
    static NS_DEFINE_IID(kCVertScrollbarCID, NS_VERTSCROLLBAR_CID);
    static NS_DEFINE_IID(kCTextAreaCID, NS_TEXTAREA_CID);
    static NS_DEFINE_IID(kCTextFieldCID, NS_TEXTFIELD_CID);

    NSRepository::RegisterFactory(kCAppShellCID, "raptorwidget.dll", PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCWindowCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCChildCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCButtonCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCCheckButtonCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCComboBoxCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCFileWidgetCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCListBoxCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCRadioButtonCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCRadioGroupCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCHorzScrollbarCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCVertScrollbarCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCTextAreaCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCTextFieldCID, WIDGET_DLL, PR_FALSE, PR_FALSE);

    //NS_InitToolkit(PR_GetCurrentThread());

    nsresult  res;

    static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
    static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

    res = NSRepository::CreateInstance(kDeviceContextCID, nsnull, kDeviceContextIID, (void **)&scribbleData.mContext);

    if (NS_OK == res)
      scribbleData.mContext->Init(nsnull);


     // Create an application shell
    nsIAppShell *appShell;
    NSRepository::CreateInstance(kCAppShellCID, nsnull, kIAppShellIID, (void**)&appShell);
    int     argc;
    char ** argv = nsnull;
    appShell->Create(&argc, argv);
   
    //
    // create the main window
    //
    NSRepository::CreateInstance(kCWindowCID, nsnull, kIWidgetIID, (LPVOID*)&(scribbleData.mainWindow));
    nsRect rect(100, 100, 600, 600);
    scribbleData.mainWindow->Create((nsIWidget*)NULL, rect, HandleEventMain, NULL);

    //
    // create the control pane
    //
    nsIWidget *controlPane;
    rect.SetRect(0, 0, 200, 580);  
    NSRepository::CreateInstance(kCChildCID, nsnull, kIWidgetIID, (LPVOID*)&controlPane);
    controlPane->Create(scribbleData.mainWindow, rect, HandleEventControlPane, NULL);

    //
    // Add the scribble/lines section
    //

    // create the "Scribble" check button
    rect.SetRect(50, 50, 100, 25);  

    NSRepository::CreateInstance(kCRadioButtonCID, nsnull, kIRadioButtonIID, (LPVOID*)&(scribbleData.scribble));
    scribbleData.scribble->Create(controlPane, rect, NULL, NULL);
    nsString cbLabel("Scribble");
    scribbleData.scribble->SetLabel(cbLabel);
    scribbleData.scribble->SetState(PR_FALSE);
    //scribbleData.scribble->SetBackgroundColor(NS_RGB(255, 255, 255));
    scribbleData.scribble->Show(PR_TRUE);

    // create the "Lines" check button
    rect.SetRect(50, 75, 100, 25);  

    NSRepository::CreateInstance(kCRadioButtonCID, nsnull, kIRadioButtonIID, (LPVOID*)&(scribbleData.lines));
    scribbleData.lines->Create(controlPane, rect, NULL, NULL);
    nsString cbLabel1("Lines");
    scribbleData.lines->SetLabel(cbLabel1);
    scribbleData.lines->SetState(PR_TRUE);
    //scribbleData.lines->SetBackgroundColor(NS_RGB(255, 255, 255));
    scribbleData.lines->Show(PR_TRUE);
    //
    // Add the circle/rectangle section
    //

    // create the "Circles" check button
    nsICheckButton * checkButton;
    rect.SetRect(50, 200, 100, 25);  

    NSRepository::CreateInstance(kCCheckButtonCID, nsnull, kICheckButtonIID, (LPVOID*)&checkButton);
    checkButton->Create(controlPane, rect, HandleEventCheck, NULL);
    nsString cbLabel2("Circles");
    checkButton->SetLabel(cbLabel2);
    checkButton->SetBackgroundColor(NS_RGB(255, 255, 255));
    checkButton->Show(PR_TRUE);
    NS_RELEASE(checkButton);

    // create the "Rectangles" check button
    rect.SetRect(50, 225, 100, 25);  

    NSRepository::CreateInstance(kCCheckButtonCID, nsnull, kICheckButtonIID, (LPVOID*)&checkButton);
    checkButton->Create(controlPane, rect, HandleEventCheck, NULL);
    nsString cbLabel3("Rectangles");
    checkButton->SetLabel(cbLabel3);
    checkButton->SetBackgroundColor(NS_RGB(255, 255, 255));
    checkButton->Show(PR_TRUE);
    NS_RELEASE(checkButton);

    //
    // Add the color section
    //

    // create the "red" text widget
    rect.SetRect(100, 350, 50, 22);  

    NSRepository::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (LPVOID*)&(scribbleData.red));
    scribbleData.red->Create(controlPane, rect, HandleEventText, NULL);
    nsString initText("0");
    scribbleData.red->SetText(initText);
    scribbleData.red->SetBackgroundColor(NS_RGB(0, 0, 255));
    scribbleData.red->Show(PR_TRUE);

    // create the "green" text widget
    rect.SetRect(100, 371, 50, 22);  

    NSRepository::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (LPVOID*)&(scribbleData.green));
    scribbleData.green->Create(controlPane, rect, HandleEventText, NULL);
    scribbleData.green->SetText(initText);
    scribbleData.green->SetBackgroundColor(NS_RGB(255, 0, 0));
    scribbleData.green->Show(PR_TRUE);

    // create the "blue" text widget
    rect.SetRect(100, 392, 50, 22);  

    NSRepository::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (LPVOID*)&(scribbleData.blue));
    scribbleData.blue->Create(controlPane, rect, HandleEventText, NULL);
    scribbleData.blue->SetText(initText);
    scribbleData.blue->SetBackgroundColor(NS_RGB(0, 255, 0));
    scribbleData.blue->Show(PR_TRUE);

    //
    // create a button  
    //
    nsIButton *button;
    rect.SetRect(50, 500, 100, 25);  
    NSRepository::CreateInstance(kCButtonCID, nsnull, kIButtonIID, (LPVOID*)&button);
    button->Create(controlPane, rect, HandleEventButton, NULL);
    nsString label("Clear");
    button->SetLabel(label);
    button->Show(PR_TRUE);
    NS_RELEASE(button);

    NS_RELEASE(controlPane); // the parent keeps a reference on this child

    //
    // create the draw pane
    //
    rect.SetRect(200, 0, 400, 580);  
    NSRepository::CreateInstance(kCChildCID, nsnull, kIWidgetIID, (LPVOID*)&scribbleData.drawPane);
    scribbleData.drawPane->Create(scribbleData.mainWindow, rect, HandleEventGraphicPane, NULL);

    //
    // show. We are in business...
    //
    scribbleData.mainWindow->Show(PR_TRUE);

    return(appShell->Run());
}

