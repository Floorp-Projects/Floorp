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

//---- Factory Includes & Stuff -----// 
#include "nsIFactory.h" 
#include "nsRepository.h" 
#include "nsGfxCIID.h" 

#include "nsWidgetsCID.h" 
  

#include "nsIWidget.h"
#include "nsIButton.h"
#include "nsICheckButton.h"
#include "nsIRadioButton.h"
#include "nsIRadioGroup.h"
#include "nsIScrollbar.h"
#include "nsITextWidget.h"
#include "nsITextAreaWidget.h"
#include "nsGUIEvent.h"
#include "nsIEnumerator.h"
#include "nsString.h"
#include "nsRect.h"
#include "nsIRenderingContext.h"
#include "nsIListBox.h"
#include "nsIComboBox.h"
#include "nsIFileWidget.h"
#include "nsIDeviceContext.h"
#include "nsFont.h"
#include "nsRepository.h"
#include "nsWidgetsCID.h"
#include "nsITabWidget.h"

#include <stdio.h>

char *  gLogFileName   = "selftest.txt";
FILE *  gFD             = nsnull;
PRInt32 gOverallStatus   = 0;
PRInt32 gNonVisualStatus = 0;

nsIWidget     *window = NULL;
nsITextWidget *statusText = NULL;
nsITextWidget *textWidgetInstance = NULL;
nsITextWidget *passwordText  = NULL;

nsIListWidget *comboBox      = NULL;
nsIListWidget *listBox       = NULL;
nsIListBox    *gMultiListBox = NULL;

nsIWidget     *movingWidget  = NULL;
nsIScrollbar  *scrollbar     = NULL;
nsITabWidget  *tabWidget     = NULL;


char * gFailedMsg = NULL;



#define WIDGET_DLL "raptorwidget.dll"
#define GFXWIN_DLL "raptorgfxwin.dll"

#define DEBUG_MOUSE 0

#define NUM_COMBOBOX_ITEMS 8
#define kSetCaret        "Set Caret"
#define kGetCaret        "Get Caret"
#define kSetSelection    "Set Selection"
#define kRemoveSelection "Remove Selection"
#define kSetText         "Set Text"
#define kGetText         "Get Text"
#define kHideBtn         "Hide Btn"
#define kShowBtn         "Show Btn"
#define kBrowseBtn       "Browse..."

// class ids
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

static NS_DEFINE_IID(kCTabWidgetCID, NS_TABWIDGET_CID);


// interface ids
static NS_DEFINE_IID(kIWidgetIID,         NS_IWIDGET_IID);
static NS_DEFINE_IID(kIButtonIID,         NS_IBUTTON_IID);
static NS_DEFINE_IID(kIScrollbarIID,      NS_ISCROLLBAR_IID);
static NS_DEFINE_IID(kICheckButtonIID,    NS_ICHECKBUTTON_IID);
static NS_DEFINE_IID(kITextWidgetIID,     NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kITextAreaWidgetIID, NS_ITEXTAREAWIDGET_IID);
static NS_DEFINE_IID(kIRadioButtonIID,    NS_IRADIOBUTTON_IID);
static NS_DEFINE_IID(kIRadioGroupIID,     NS_IRADIOGROUP_IID);
static NS_DEFINE_IID(kIListBoxIID,        NS_ILISTBOX_IID);
static NS_DEFINE_IID(kIComboBoxIID,       NS_ICOMBOBOX_IID);
static NS_DEFINE_IID(kIFileWidgetIID,     NS_IFILEWIDGET_IID);
static NS_DEFINE_IID(kITabWidgetIID,      NS_ITABWIDGET_IID);


char * eval(PRInt32 aVal) {
  if (!aVal) {
    gOverallStatus++;
  }
  return aVal? "PASSED":"FAILED";
}

char * eval(PRUint32 aVal) {
  if (!aVal) {
    gOverallStatus++;
  }

  return aVal ? "PASSED":"FAILED";
}

char * eval(PRBool aVal) {
  if (!aVal) {
    gOverallStatus++;
  }
  return aVal ? "PASSED":"FAILED";
}

/**--------------------------------------------------------------------------------
  * Generic ListWidget Box Non-Visual Test
  *--------------------------------------------------------------------------------
 */
void listSelfTest(FILE * fd, char * aTitle, nsIListWidget * listBox) {
  fprintf(fd, "\n\n-----------------------------\n");
  fprintf(fd, "%s self test\n", aTitle);
  fprintf(fd, "-----------------------------\n\n");

  int inx = 4;
  char * item4 = "List Item 4";

  fprintf(fd, "\nTesting GetItemCount\n\tItem count should be [%d] is [%d] Test: [%s]\n", NUM_COMBOBOX_ITEMS, listBox->GetItemCount(),
    (NUM_COMBOBOX_ITEMS == (int)listBox->GetItemCount()?"PASSED":"FAILED")); 

  listBox->SelectItem(inx);
  nsAutoString buf;
  listBox->GetSelectedItem(buf);
  char * selStr = buf.ToNewCString();
  int    sel    = listBox->GetSelectedIndex();
  fprintf(fd, "\nTesting GetSelectedItem\n\tSelection should be %d item[%s] index[%d]  Test: [%s]\n", inx, selStr, sel,
         eval(inx == (int)sel)); 
  if (nsnull != selStr) delete selStr;

  listBox->GetItemAt(buf, 4);
  selStr = buf.ToNewCString();
  fprintf(fd, "\nTesting GetItemAt\n\tItem %d should be [%s] is [%s]  Test: [%s]\n", inx, item4, selStr, eval(strcmp(selStr, item4) == 0)); 
  if (nsnull != selStr) delete selStr;

  listBox->SelectItem(2);
  inx = listBox->GetSelectedIndex();
  fprintf(fd, "\nTesting SelectItem && GetSelectedIndex\n\t Selected Item should be [%d] is [%d]  Test: [%s]\n", 2, inx, eval(inx == 2)); 

  for (int i=0;i<(int)listBox->GetItemCount();i++) {
    nsAutoString buf;
    listBox->GetItemAt(buf, i);
    char * str = buf.ToNewCString();

    fprintf(fd, "Item %d [%s]\n", i, str);

    if (nsnull != str) delete str;
  }
  fprintf(fd, "Removing Item #4\n");
  listBox->RemoveItemAt(4);
  fprintf(fd, "\nTesting RemoveItemAt\n\tTest: [%s]\n", eval(-1 == (int)listBox->FindItem(nsString(item4), 0))); 

  for (i=0;i<(int)listBox->GetItemCount();i++) {
    nsAutoString buf;
    listBox->GetItemAt(buf, i);
    char * str = buf.ToNewCString();

    fprintf(fd, "Item %d [%s]\n", i, str);

    if (nsnull != str) delete str;
  }
  listBox->Deselect();
  fprintf(fd, "\nTesting Deselect\n\t Selected Item [%d]  Test:[%s]\n", (int)listBox->GetSelectedIndex(), (-1 == (int)listBox->GetSelectedIndex()?"PASSED":"FAILED")); 

}

/**--------------------------------------------------------------------------------
  * Generic ListWidget Box Non-Visual Test
  *--------------------------------------------------------------------------------
 */
void textShelfTest(FILE * fd, char * aTitle, nsITextWidget * aTextWidget) {
  fprintf(fd, "\n\n-----------------------------\n");
  fprintf(fd, "%s self test\n", aTitle);
  fprintf(fd, "-----------------------------\n\n");

  aTextWidget->SetText(nsString("1234567890"));
  PRUint32 start = 1;
  PRUint32 end   = 5;
  aTextWidget->SetSelection(start, end);

  PRUint32 start2 = 0;
  PRUint32 end2   = 0;
  aTextWidget->GetSelection(&start2, &end2);

  fprintf(fd, "Tested SetSelection and GetSelection Test %s\n", eval(start == start2 && end == end2));

  start = 5;
  aTextWidget->SetCaretPosition(start);

  start2 = aTextWidget->GetCaretPosition();

  fprintf(fd, "Tested SetCaretPosition and GetCaretPosition Test %s\n", eval(start == start2));
  aTextWidget->InsertText(nsString("xxx"),1,3);
  nsString str;
  aTextWidget->GetText(str,256);
  //fprintf(fd, "Tested InsertText Test %s\n", eval(start == start2));
  char * s = str.ToNewCString();
  fprintf(fd, "Tested InsertText Test %s\n", s);
  delete s;

}

/**--------------------------------------------------------------------------------
  * Generic MultiListWidget Box Non-Visual Test
  *--------------------------------------------------------------------------------
 */
void multiListSelfTest(FILE * fd, char * aTitle, nsIListWidget * listBox) {
  fprintf(fd, "\n\n-----------------------------\n");
  fprintf(fd, "%s self test\n", aTitle);
  fprintf(fd, "-----------------------------\n\n");

  nsIListBox * multi = (nsIListBox*)listBox;

  int inx = 4;
  char * item4 = "List Item 4";

  nsAutoString buf;
  char * selStr;

  multi->GetItemAt(buf, 4);
  selStr = buf.ToNewCString();
  fprintf(fd, "\nTesting GetItemAt\n\tItem %d should be [%s] is [%s]  Test: [%s]\n", inx, item4, selStr, 
          eval(strcmp(selStr, item4) == 0)); 
  if (nsnull != selStr) delete selStr;


  multi->Deselect();
  int count = multi->GetSelectedCount();
  fprintf(fd, "\nTesting Deselect\n\tCount %d Test: [%s]\n", count, eval(0 == count)); 

  PRInt32 inxs[] = {0,2,4};
  PRInt32 len = 3;

  for (int i=0;i<len;i++) {
    multi->SelectItem(inxs[i]);
  }

  int status = 1;
  count = multi->GetSelectedCount();
  fprintf(fd, "\nTesting GetSelectedCount\n\tCount [%d] should be [%d] Test: [%s]\n", count, len, 
          eval(len == count)); 

  if (count == len) {
    PRInt32 indices[256];
    multi->GetSelectedIndices(indices, 256);
    for (i=0;i<count;i++) {
      if (indices[i] != inxs[i]) {
        status = 0;
        break;
      }
    }
  } else {
    status = 0;
  }

  if (status == 1) {
    fprintf(fd, "\nTesting GetSelectedIndices\n\tTest: [%s]\n", eval(len == (int)multi->GetSelectedCount())); 
  }

  for (i=0;i<(int)multi->GetItemCount();i++) {
    nsAutoString buf;
    multi->GetItemAt(buf, i);
    char * str = buf.ToNewCString();

    fprintf(fd, "Item %d [%s]\n", i, str);

    if (nsnull != str) delete str;
  }
  fprintf(fd, "Removing Item #4\n");
  multi->RemoveItemAt(4);
  fprintf(fd, "\nTesting RemoveItemAt\n\tTest: [%s]\n", eval(-1 == (int)multi->FindItem(nsString(item4), 0))); 

  for (i=0;i<(int)multi->GetItemCount();i++) {
    nsAutoString buf;
    multi->GetItemAt(buf, i);
    char * str = buf.ToNewCString();

    fprintf(fd, "Item %d [%s]\n", i, str);

    if (nsnull != str) delete str;
  }

}

/**--------------------------------------------------------------------------------
  *
 */
int createTestButton(nsIWidget * aWin, 
                     char * aTitle, 
                     int aX, 
                     int aY, 
                     int aWidth, 
                     EVENT_CALLBACK aHandleEventFunction) {
  nsIButton *button;
  nsRect rect(aX, aY, aWidth, 25);  
  NSRepository::CreateInstance(kCButtonCID, nsnull, kIButtonIID, (LPVOID*)&button);
  button->Create(window, rect, aHandleEventFunction, NULL);
  nsString label(aTitle);
  button->SetLabel(label);
  button->Show(PR_TRUE);
  NS_RELEASE(button);
  return aX + aWidth;
}

/**--------------------------------------------------------------------------------
  * List Test Handler
  *--------------------------------------------------------------------------------
 */
nsEventStatus PR_CALLBACK GenericListHandleEvent(nsGUIEvent *aEvent, char * aTitle, nsIListWidget * aListWidget)
{ 

  nsIButton * btn;
  if (aEvent->message != NS_MOUSE_LEFT_BUTTON_UP) {
    return nsEventStatus_eIgnore;
  }
  nsString str(aTitle);
  fprintf(gFD, "\nVisually tested nsIListWidget\n");

  if (NS_OK == aEvent->widget->QueryInterface(kIButtonIID, (void**)&btn)) {
    nsAutoString strBuf;
    btn->GetLabel(strBuf);
    char * title = strBuf.ToNewCString();
    //fprintf(gFD, "Title is [%s]\n", title);

    if (!strcmp(title, kSetSelection)) {
      aListWidget->SelectItem(2);
      fprintf(gFD, "\tTested SelectItem(2)\n");
      str.Append(" should show 'List Item 2'");
      statusText->SetText(str);
      gFailedMsg = "List::SelectItem";
    } else if (!strcmp(title, kRemoveSelection)) {
      PRInt32 inx = aListWidget->FindItem(nsString("List Item 2"), 0);
      
      if (inx > -1) {
        aListWidget->RemoveItemAt(inx);
      }
      fprintf(gFD, "\tTested FindItem(...)\n");
      str.Append(" should show empty");
      statusText->SetText(str);
      gFailedMsg = "List::RemoveItemAt && FindItem";
    }
    delete title;
    NS_RELEASE(btn);
  }
  return nsEventStatus_eIgnore;
}

/**--------------------------------------------------------------------------------
  * Combox Test Handler
  *--------------------------------------------------------------------------------
 */
nsEventStatus PR_CALLBACK ComboTestHandleEvent(nsGUIEvent *aEvent) 
{
  return GenericListHandleEvent(aEvent, "ComboBox", comboBox);
}


/**--------------------------------------------------------------------------------
  * ListBox Test Handler
  *--------------------------------------------------------------------------------
 */
nsEventStatus PR_CALLBACK ListBoxTestHandleEvent(nsGUIEvent *aEvent)
{
  return GenericListHandleEvent(aEvent, "ListBox", listBox);
}

/**--------------------------------------------------------------------------------
  * Multi-ListBox Test Handler
  *--------------------------------------------------------------------------------
 */
nsEventStatus PR_CALLBACK MultiListBoxTestHandleEvent(nsGUIEvent *aEvent)
{
  nsIButton * btn;
  if (aEvent->message != NS_MOUSE_LEFT_BUTTON_UP) {
    return nsEventStatus_eIgnore;
  }
  nsString str("Multi-ListBox");
  fprintf(gFD, "\nVisually tested Multi-List Box\n");

  if (NS_OK == aEvent->widget->QueryInterface(kIButtonIID, (void**)&btn)) {
    nsAutoString strBuf;
    btn->GetLabel(strBuf);
    char * title = strBuf.ToNewCString();
    //fprintf(gFD, "Title is [%s]\n", title);

    if (!strcmp(title, kSetSelection)) {
      gMultiListBox->Deselect();

      PRInt32 inxs[] = {0,2,4};
      PRInt32 len = 3;

      for (int i=0;i<len;i++) {
        gMultiListBox->SelectItem(inxs[i]);
      }
      fprintf(gFD, "\tTested SelectItem()\n");
      str.Append(" should show 'List Item 0,2,5'");
      statusText->SetText(str);
      gFailedMsg = "Multi-List::SelectItem";
    } else if (!strcmp(title, kRemoveSelection)) {
      PRInt32 inx = gMultiListBox->FindItem(nsString("List Item 2"), 0);
      
      if (inx > -1) {
        gMultiListBox->RemoveItemAt(inx);
      }
      fprintf(gFD, "\tTested FindItem(...)\n");
      str.Append(" should show 0,5");
      statusText->SetText(str);
      gFailedMsg = "Multi-List::FindItem && RemoveItemAt";
    }
    delete title;
    NS_RELEASE(btn);
  }
  return nsEventStatus_eIgnore;

}

/**--------------------------------------------------------------------------------
  * Checkbutton Test Handler
  *--------------------------------------------------------------------------------
 */
nsEventStatus PR_CALLBACK CheckButtonTestHandleEvent(nsGUIEvent *aEvent)
{
  nsICheckButton * chkBtn;
  if (aEvent->message != NS_MOUSE_LEFT_BUTTON_UP) {
    return nsEventStatus_eIgnore;
  }
  fprintf(gFD, "\nVisually tested CheckBox\n");

  if (NS_OK == aEvent->widget->QueryInterface(kICheckButtonIID, (void**)&chkBtn)) {
    fprintf(gFD, "\tGetState and SetState tested.\n");
    chkBtn->SetState((PRBool)!chkBtn->GetState());
    NS_RELEASE(chkBtn);
    gFailedMsg = "CheckButton::SetState & GetState";
  }
  return nsEventStatus_eIgnore;
}

/**--------------------------------------------------------------------------------
  * Failed Button Handler
  *--------------------------------------------------------------------------------
 */
nsEventStatus PR_CALLBACK FailedButtonHandleEvent(nsGUIEvent *aEvent)
{
  nsIButton * btn;
  if (aEvent->message != NS_MOUSE_LEFT_BUTTON_UP) {
    return nsEventStatus_eIgnore;
  }
  //fprintf(gFD, "\nVisually tested CheckBox\n");

  if (NS_OK == aEvent->widget->QueryInterface(kIButtonIID, (void**)&btn)) {
    gOverallStatus++;
    if (gFailedMsg == nsnull) {
      fprintf(gFD, "\n*** Something failed but the msg wan't set correctly in the code!\n");
    } else {
      fprintf(gFD, "Method [%s] FAILED!\n", gFailedMsg);
      gFailedMsg = nsnull;
    }

  }
  return nsEventStatus_eIgnore;
}

/**--------------------------------------------------------------------------------
  * Succeeded Button Handler
  *--------------------------------------------------------------------------------
 */
nsEventStatus PR_CALLBACK SucceededButtonHandleEvent(nsGUIEvent *aEvent)
{
  nsIButton * btn;
  if (aEvent->message != NS_MOUSE_LEFT_BUTTON_UP) {
    return nsEventStatus_eIgnore;
  }
  //fprintf(gFD, "\nVisually tested CheckBox\n");

  if (NS_OK == aEvent->widget->QueryInterface(kIButtonIID, (void**)&btn)) {
    if (gFailedMsg == nsnull) {
      fprintf(gFD, "\n*** Something Succeeded but the msg wan't set correctly in the code!\n");
    } else {
      fprintf(gFD, "Method [%s] SUCCEEDED!\n", gFailedMsg);
      gFailedMsg = nsnull;
    }

  }
  return nsEventStatus_eIgnore;
}

/**--------------------------------------------------------------------------------
  * TextWidget Test Handler
  *--------------------------------------------------------------------------------
 */
nsEventStatus PR_CALLBACK GenericTextTestHandleEvent(char           *aTitle, 
                                              nsGUIEvent     *aEvent, 
                                              nsITextWidget  *aTextWidget,
                                              const nsString &aTestStr,
                                              const nsString &aStrToShow)
{
  nsIButton * btn;
  if (aEvent->message != NS_MOUSE_LEFT_BUTTON_UP) {
    return nsEventStatus_eIgnore;
  }


  if (NS_OK == aEvent->widget->QueryInterface(kIButtonIID, (void**)&btn)) {
    nsAutoString strBuf;
    nsString     str;
    btn->GetLabel(strBuf);
    char * title = strBuf.ToNewCString();

    if (!strcmp(title, kSetText)) {
      aTextWidget->SetText(aTestStr);

      fprintf(gFD, "\nVisually Testing Text\n");

      str.Append(" should show[");
      str.Append(aStrToShow);
      str.Append("]");

      statusText->SetText(str);
      gFailedMsg = "nsITextWidget::SetText";
    } else if (!strcmp(title, kGetText)) {
      nsString getStr;
      aTextWidget->GetText(getStr, 256);
      
      fprintf(gFD, "\tTested GetText(...) \n");

      if (aTestStr.Equals(getStr)) {
        str.Append(" Test PASSED");
        fprintf(gFD, "Test PASSED");
      } else {
        str.Append(" Test FAILED");
        fprintf(gFD, "Test FAILED");
      }
      statusText->SetText(str);
      gFailedMsg = "nsITextWidget::GetText";
    } else if (!strcmp(title, kSetCaret)) {
      nsString getStr;
      aTextWidget->SetCaretPosition(1);
      fprintf(gFD, "\tTested SetCaretPosition(1) \n");
      str.Append(" should show caret in position 1");
      statusText->SetText(str);
      gFailedMsg = "nsITextWidget::SetCaretPosition";
    } else if (!strcmp(title, kGetCaret)) {
      nsString getStr;
      PRUint32 pos = aTextWidget->GetCaretPosition();

      fprintf(gFD, "Visually tested GetCaretPosition(1) \n");

      if (pos == 1) {
        str.Append(" Test PASSED");
        fprintf(gFD, "Test PASSED\n");
      } else {
        str.Append(" Test FAILED");
        fprintf(gFD, "Test FAILED\n");
      }
      statusText->SetText(str);
      gFailedMsg = "nsITextWidget::GetCaretPosition";
    } else if (!strcmp(title, kSetSelection)) {
      nsString getStr;
      aTextWidget->SetSelection(1,5);

      fprintf(gFD, "\tTested SetSelection(1,5) \n");

      str.Append(" should show selection from 1 to 5");
      statusText->SetText(str);
      gFailedMsg = "nsITextWidget::SetSelection";
    }
    delete title;
    NS_RELEASE(btn);
  }
  return nsEventStatus_eIgnore;
}


/**--------------------------------------------------------------------------------
  * Button Test Handler
  *--------------------------------------------------------------------------------
  */
nsEventStatus PR_CALLBACK ButtonTestHandleEvent(nsGUIEvent *aEvent)
{

  nsIButton * btn;
  if (aEvent->message != NS_MOUSE_LEFT_BUTTON_UP) {
    return nsEventStatus_eIgnore;
  }

  if (NS_OK == aEvent->widget->QueryInterface(kIButtonIID, (void**)&btn)) {
    nsAutoString strBuf;
    nsString     str("Tesing ");
    btn->GetLabel(strBuf);
    char * title = strBuf.ToNewCString();

    if (!strcmp(title, kHideBtn)) {
      movingWidget->Show(PR_FALSE);
      str.Append("nsIWidget::Show(FALSE)");
      statusText->SetText(str);
      gFailedMsg = "nsIWidget::Show(FALSE)";
    } else if (!strcmp(title, kShowBtn)) {
      movingWidget->Show(PR_TRUE);
      str.Append("nsIWidget::Show(TRUE)");
      statusText->SetText(str);
      gFailedMsg = "nsIWidget::Show(TRUE)";
    }

    delete title;
    NS_RELEASE(btn);
    
  }
  return nsEventStatus_eIgnore;
}

/**--------------------------------------------------------------------------------
  * TextWidget Test Handler
  *--------------------------------------------------------------------------------
 */
nsEventStatus PR_CALLBACK PasswordTextTestHandleEvent(nsGUIEvent *aEvent)
{
  return GenericTextTestHandleEvent("Password", aEvent, passwordText, nsString("123"), nsString("***"));
}

/**--------------------------------------------------------------------------------
  * Dumps the size of the main window and all child windows
  *--------------------------------------------------------------------------------
  */

void DumpRects()
{
  nsRect rect;
  // print the main window position
  window->GetBounds(rect);
  printf("Bounds(%d, %d, %d, %d)\n", rect.x, rect.y, rect.width, rect.height);

  // print all children's position
  nsIEnumerator *enumerator = window->GetChildren();
  nsISupports * widget;
  while (widget = enumerator->Next()) {
      nsIWidget *child;
      if (NS_OK == widget->QueryInterface(kIWidgetIID, (void**)&child)) {
          //
          child->GetBounds(rect);
          printf("Bounds(%d, %d, %d, %d)\n", rect.x, rect.y, rect.width, rect.height);
          NS_RELEASE(child);
      }
      NS_RELEASE(widget);
  }

  NS_RELEASE(enumerator);
  delete enumerator;
}


/**--------------------------------------------------------------------------------
 * Main Handler
 *--------------------------------------------------------------------------------
 */
nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent)
{ 
	//  printf("aEvent->message %d\n", aEvent->message);
    nsEventStatus result = nsEventStatus_eIgnore;
    switch(aEvent->message) {

        case NS_MOVE:
            char str[256];
            sprintf(str, "Moved window to %d,%d", aEvent->point.x, aEvent->point.y);
            statusText->SetText(str);
            break;

        case NS_MOUSE_ENTER:
            if (DEBUG_MOUSE) printf("NS_MOUSE_ENTER 0x%X\n", aEvent->widget);
            break;

        case NS_MOUSE_EXIT:
            if (DEBUG_MOUSE) printf("NS_MOUSE_EXIT 0x%X\n", aEvent->widget);
            break;

        case NS_MOUSE_MOVE:
            if (DEBUG_MOUSE) printf("NS_MOUSE_MOVE\n");
            break;
        
        case NS_MOUSE_LEFT_BUTTON_UP:
            if (DEBUG_MOUSE) printf("NS_MOUSE_LEFT_BUTTON_UP\n");
            break;
        

        case NS_MOUSE_LEFT_BUTTON_DOWN:
            if (DEBUG_MOUSE) printf("NS_MOUSE_LEFT_BUTTON_DOWN\n");
            break;

        case NS_CREATE:
            printf("Window Created\n");
            break;
        
        case NS_PAINT: 
              // paint the background
            if (aEvent->widget == window) {
                nsIRenderingContext *drawCtx = ((nsPaintEvent*)aEvent)->renderingContext;
                drawCtx->SetColor(aEvent->widget->GetBackgroundColor());
                drawCtx->FillRect(*(((nsPaintEvent*)aEvent)->rect));

                return nsEventStatus_eIgnore;
            }
            break;
        
        case NS_DESTROY:
            printf("Destroy Window...Release window\n");
            fprintf(gFD, "\n\n--------------------------------------------------\n");
            fprintf(gFD, "Summary of non-visual tests, Number of Failures %d\n", gNonVisualStatus);
            fprintf(gFD, "Summary of visual tests,     Number of Failures %d\n", gOverallStatus-gNonVisualStatus);
            fprintf(gFD, "Summary of all tests,        Number of Failures %d\n", gOverallStatus);
            fprintf(gFD, "--------------------------------------------------\n");
            fflush(gFD);
            fclose(gFD);

			      exit(0); // for now

            break;

       
        case NS_SCROLLBAR_POS:
        case NS_SCROLLBAR_PAGE_NEXT:
        case NS_SCROLLBAR_PAGE_PREV:
        case NS_SCROLLBAR_LINE_NEXT:
        case NS_SCROLLBAR_LINE_PREV:
            if (nsnull != movingWidget) {
              PRUint32  pos = scrollbar->GetPosition();
              movingWidget->Move(10, pos);
            }
            break;

        case NS_KEY_UP: {
            nsKeyEvent * ke = (nsKeyEvent*)aEvent;
            char str[256];
            sprintf(str, "Key Event Key Code[%d] Key [%c] Shift [%s] Control [%s] Alt [%s]",
              ke->keyCode, ke->keyCode, 
              (ke->isShift?"Pressed":"Released"),
              (ke->isControl?"Pressed":"Released"),
              (ke->isAlt?"Pressed":"Released"));
            printf("%s\n", str);
            statusText->SetText(nsString(str));
            }
            break;


        default:
            result = nsEventStatus_eIgnore;
    }
    //printf("result: %d = %d\n", result, PR_FALSE);

    return result;
}



nsEventStatus PR_CALLBACK HandleFileButtonEvent(nsGUIEvent *aEvent)
{
  switch(aEvent->message) {
           
    case NS_MOUSE_LEFT_BUTTON_UP:
      // create a FileWidget
      //
      nsIFileWidget *fileWidget;

      nsString title("FileWidget Title <here> mode = save");
      NSRepository::CreateInstance(kCFileWidgetCID, nsnull, kIFileWidgetIID, (LPVOID*)&fileWidget);
  
      nsString titles[] = {"all files","html","executables" };
      nsString filters[] = {"*.*", "*.html", "*.exe" };
      fileWidget->SetFilterList(3, titles, filters);
      fileWidget->Create(nsnull,
                         title,
                         eMode_save,
                         nsnull,
                         nsnull);
  
      PRUint32 result = fileWidget->Show();
      if (result) {
        nsString file;
        fileWidget->GetFile(file);
        printf("file widget contents %s\n", file.ToNewCString());
        statusText->SetText(file.ToNewCString());
      }
      else
        statusText->SetText("Cancel selected");


      NS_RELEASE(fileWidget);
    break;
  }

  return(nsEventStatus_eConsumeDoDefault);
}

/*--------------------------------------------------------------------------------
 * Tab change handler
 *--------------------------------------------------------------------------------
 */

nsEventStatus PR_CALLBACK HandleTabEvent(nsGUIEvent *aEvent)
{
  switch(aEvent->message) {
           
    case NS_TABCHANGE:
      PRUint32 tab = tabWidget->GetSelectedTab();
      char buf[256];
      sprintf(buf, "Selected tab %d", tab);
      statusText->SetText(buf);
    break;
  }

  return(nsEventStatus_eConsumeDoDefault);
}




/**--------------------------------------------------------------------------------
  *
 */
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, 
    int nCmdShow) 
{
    // Open global test log file
    gFD = fopen(gLogFileName, "w");
    if (gFD == nsnull) {
      fprintf(stderr, "Couldn't open file[%s]\n", gLogFileName);
      exit(1);
    }

    // register widget classes
    NSRepository::RegisterFactory(kCWindowCID, "raptorwidget.dll", PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCChildCID, "raptorwidget.dll", PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCButtonCID, "raptorwidget.dll", PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCCheckButtonCID, "raptorwidget.dll", PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCComboBoxCID, "raptorwidget.dll", PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCFileWidgetCID, "raptorwidget.dll", PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCListBoxCID, "raptorwidget.dll", PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCRadioButtonCID, "raptorwidget.dll", PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCRadioGroupCID, "raptorwidget.dll", PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCHorzScrollbarCID, "raptorwidget.dll", PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCVertScrollbarCID, "raptorwidget.dll", PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCTextAreaCID, "raptorwidget.dll", PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCTextFieldCID, "raptorwidget.dll", PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCTabWidgetCID, "raptorwidget.dll", PR_FALSE, PR_FALSE);

    static NS_DEFINE_IID(kCRenderingContextIID, NS_RENDERING_CONTEXT_CID); 
    static NS_DEFINE_IID(kCDeviceContextIID, NS_DEVICE_CONTEXT_CID); 
    static NS_DEFINE_IID(kCFontMetricsIID, NS_FONT_METRICS_CID); 
    static NS_DEFINE_IID(kCImageIID, NS_IMAGE_CID); 


    NSRepository::RegisterFactory(kCRenderingContextIID, "raptorgfxwin.dll", PR_FALSE, PR_FALSE); 
    NSRepository::RegisterFactory(kCDeviceContextIID, "raptorgfxwin.dll", PR_FALSE, PR_FALSE); 
    NSRepository::RegisterFactory(kCFontMetricsIID, "raptorgfxwin.dll", PR_FALSE, PR_FALSE); 
    NSRepository::RegisterFactory(kCImageIID, "raptorgfxwin.dll", PR_FALSE, PR_FALSE); 

    nsIDeviceContext* deviceContext = 0;

    // Create a device context for the widgets
    nsresult  res;

    static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
    static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

    res = NSRepository::CreateInstance(kDeviceContextCID, nsnull, kDeviceContextIID, (void **)&deviceContext);

    if (NS_OK == res)
    {
      deviceContext->Init();
      NS_ADDREF(deviceContext);
    }

    //
    // create the main window
    //
    NSRepository::CreateInstance(kCWindowCID, nsnull, kIWidgetIID, (LPVOID*)&window);
    nsRect rect(100, 100, 600, 700);
   
    window->Create((nsIWidget*)NULL, rect, HandleEvent, NULL);
    window->SetTitle("TOP-LEVEL window");

    //
    // create a child
    //
    nsIWidget *child;
    //
    // create another child
    //

    int x = 10;
    int y = 10;
    rect.SetRect(x, y, 200, 100);  
    NSRepository::CreateInstance(kCChildCID, nsnull, kIWidgetIID, (LPVOID*)&child);
      
   
    child->SetBorderStyle(eBorderStyle_dialog);
    child->Create(window, rect, HandleEvent, NULL);
    //child->SetBackgroundColor(NS_RGB(255, 255, 0));
    child->SetForegroundColor(NS_RGB(255, 0, 0));
    NS_RELEASE(child); // the parent keeps a reference on this child

    y += rect.height + 5;

   
    //
    // create a button
    //
    nsIButton *button;
    rect.SetRect(x, y, 60, 25);  
    NSRepository::CreateInstance(kCButtonCID, nsnull, kIButtonIID, (LPVOID*)&button);
    button->Create(window, rect, HandleEvent, NULL);
    nsString label("Slider");
    button->SetLabel(label);

    nsAutoString strBuf;
    button->GetLabel(strBuf);
    button->Show(PR_TRUE);

    movingWidget = button;

    x = createTestButton(window, kHideBtn, x+180, y, 75, ButtonTestHandleEvent);
    x = createTestButton(window, kShowBtn, x+5,   y, 75, ButtonTestHandleEvent);
    x = 10;
    y += rect.height + 5;

    // Create browse button
    x = createTestButton(window, kBrowseBtn, x,   y, 75, HandleFileButtonEvent);
    x = 10;
    y += rect.height + 5;

    //
    // create a check button
    //
    nsICheckButton * checkButton;
    rect.SetRect(x, y, 100, 25);  

    NSRepository::CreateInstance(kCCheckButtonCID, nsnull, kICheckButtonIID, (LPVOID*)&checkButton);
    checkButton->Create(window, rect, CheckButtonTestHandleEvent, NULL);
    nsString cbLabel("CheckButton");
    checkButton->SetLabel(cbLabel);
    NS_RELEASE(checkButton);
    y += rect.height + 5;

    //
    // create a text widget
    //

    nsITextWidget * textWidget;
    rect.SetRect(x, y, 100, 25);  

    NSRepository::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (LPVOID*)&textWidget);
    textWidget->Create(window, rect, HandleEvent, deviceContext);


    nsFont font("Times", NS_FONT_STYLE_NORMAL,
                         NS_FONT_VARIANT_NORMAL,
                         NS_FONT_WEIGHT_BOLD,
                         0,
                         12);

    textWidget->SetFont(font);
    nsString initialText("0123456789");
    textWidget->SetText(initialText);
    textWidget->SetMaxTextLength(12);
    textWidget->SelectAll();

    NS_RELEASE(textWidget); 
    y += rect.height + 5;

     //
    // create a text password widget
    //

    nsITextWidget * ptextWidget;
    rect.SetRect(x, y, 100, 25);  
    NSRepository::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (LPVOID*)&ptextWidget);
    ptextWidget->SetPassword(PR_TRUE);
    ptextWidget->Create(window, rect, HandleEvent, NULL);
    nsString pinitialText("password text");
    ptextWidget->SetText(pinitialText);
    passwordText = ptextWidget;

    textShelfTest(gFD, "Password Text", passwordText);

    x = createTestButton(window, kSetCaret, x+180, y, 75, PasswordTextTestHandleEvent);
    x = createTestButton(window, kGetCaret, x+5,   y, 75, PasswordTextTestHandleEvent);
    x = createTestButton(window, kSetText,  x+5,   y, 75, PasswordTextTestHandleEvent);
    x = createTestButton(window, kGetText,  x+5,   y, 75, PasswordTextTestHandleEvent);
    x = createTestButton(window, kSetSelection, x+5,   y, 100, PasswordTextTestHandleEvent);
    x = 10;
    y += rect.height + 5;

    //
    // create a readonly text widget
    //

    nsITextWidget * rtextWidget;
    rect.SetRect(x, y, 100, 25);  
    NSRepository::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (LPVOID*)&rtextWidget);
    rtextWidget->SetReadOnly(PR_TRUE);
    rtextWidget->Create(window, rect, HandleEvent, NULL);
    nsString rinitialText("This is readonly");
    rtextWidget->SetText(rinitialText);
    NS_RELEASE(rtextWidget); 
    y += rect.height + 5;

    //
    // create a text area widget
    //

    nsITextAreaWidget * textAreaWidget;
    rect.SetRect(x, y, 150, 100);  
    NSRepository::CreateInstance(kCTextAreaCID, nsnull, kITextAreaWidgetIID, (LPVOID*)&textAreaWidget);
    textAreaWidget->Create(window, rect, HandleEvent, NULL);
    nsString textAreaInitialText("Text Area Widget");
    textWidgetInstance = textAreaWidget;
    textAreaWidget->SetText(textAreaInitialText);
    NS_RELEASE(textAreaWidget); 
    y += rect.height + 5;

    x += rect.width + 5;

    //
    // create a scrollbar
    //
    rect.SetRect(x, 10, 25, 300);  
    NSRepository::CreateInstance(kCVertScrollbarCID, nsnull, kIScrollbarIID, (LPVOID*)&scrollbar);
    scrollbar->Create(window, rect, HandleEvent, NULL);
    scrollbar->SetMaxRange(300);
    scrollbar->SetThumbSize(50);
    scrollbar->SetPosition(100);
 
    //
    // create a Status Text
    //

    rect.SetRect(x+25, 10, 350, 25);  
    NSRepository::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (LPVOID*)&statusText);
    statusText->Create(window, rect, HandleEvent, deviceContext);
  
    //
    // create a Failed Button
    //
    rect.SetRect(x+25, 38, 60, 25);  
    NSRepository::CreateInstance(kCButtonCID, nsnull, kIButtonIID, (LPVOID*)&button);
    button->Create(window, rect, FailedButtonHandleEvent, NULL);
    nsString failedLabel("Failed");
    button->SetLabel(failedLabel);
    button->Show(PR_TRUE);
    NS_RELEASE(button);
    y += rect.height + 5;

    //
    // create a Succeeded Button
    //
    rect.SetRect(x+100, 38, 80, 25);  
    NSRepository::CreateInstance(kCButtonCID, nsnull, kIButtonIID, (LPVOID*)&button);
    button->Create(window, rect, SucceededButtonHandleEvent, NULL);
    nsString succeededLabel("Succeeded");
    button->SetLabel(succeededLabel);
    button->Show(PR_TRUE);
    NS_RELEASE(button);
    y += rect.height + 5;

    x = 10; 
    

    //
    // create a listbox widget
    //

    rect.SetRect(x, y, 150, 100);  
    NSRepository::CreateInstance(kCListBoxCID, nsnull, kIListBoxIID, (LPVOID*)&listBox);
    listBox->Create(window, rect, HandleEvent, NULL);
    char str[256];
    for (int i=0;i<NUM_COMBOBOX_ITEMS;i++) {
      sprintf(str, "%s %d", "List Item", i);
      nsString listStr1(str);
      listBox->AddItemAt(listStr1, i);
    }
    listSelfTest(gFD, "ListBox", listBox);

    x = createTestButton(window, kSetSelection,    x+150, y, 125, ListBoxTestHandleEvent);
    x = createTestButton(window, kRemoveSelection, x+5,   y, 125, ListBoxTestHandleEvent);
    x = 10;
    y += rect.height + 5;


    //
    // create a multi-selection listbox widget
    //

    rect.SetRect(x, y, 150, 100);  
    NSRepository::CreateInstance(kCListBoxCID, nsnull, kIListBoxIID, (LPVOID*)&gMultiListBox);
      // Notice the extra agrument PR_TRUE below which indicates that
      // the list widget is multi-select
    gMultiListBox->SetMultipleSelection(PR_TRUE);
    gMultiListBox->Create(window, rect, HandleEvent, NULL);
    for (i=0;i<NUM_COMBOBOX_ITEMS;i++) {
      sprintf(str, "%s %d", "List Item", i);
      nsString listStr1(str);
      gMultiListBox->AddItemAt(listStr1, i);
    }
    multiListSelfTest(gFD, "Multi-ListBox", gMultiListBox);

    x = createTestButton(window, kSetSelection,    x+150, y, 125, MultiListBoxTestHandleEvent);
    x = createTestButton(window, kRemoveSelection, x+5,   y, 125, MultiListBoxTestHandleEvent);

    y += rect.height + 5;
    x = 10;

    //
    // create a tab widget
    //

    rect.SetRect(x, y, 300, 50);  
    NSRepository::CreateInstance(kCTabWidgetCID, nsnull, kITabWidgetIID, (LPVOID*)&tabWidget);
    tabWidget->Create(window, rect, HandleTabEvent, NULL);
    nsString tabs[] = {"low", "medium", "high" };
   
    tabWidget->SetTabs(3, tabs);
    tabWidget->Show(PR_TRUE);
    y += rect.height + 5;
    x = 10;

    //
    // create a Radio Group
    //
    nsIRadioGroup * radioGroup;
    NSRepository::CreateInstance(kCRadioGroupCID, nsnull, kIRadioGroupIID, (LPVOID*)&radioGroup);
    radioGroup->SetName(nsString("Group1"));

    //
    // create a Radio button
    //
    nsIRadioButton * radioButton;
    rect.SetRect(x, y, 120, 25);  

    NSRepository::CreateInstance(kCRadioButtonCID, nsnull, kIRadioButtonIID, (LPVOID*)&radioButton);
    radioButton->SetRadioGroup(radioGroup);
    radioButton->Create(window, rect, HandleEvent, NULL);
    nsString rbLabel("RadioButton1");
    radioButton->SetLabel(rbLabel);
    radioButton->Show(PR_TRUE);
    NS_RELEASE(radioButton);
    y += rect.height + 5;

    //
    // create a Radio button
    //
    rect.SetRect(x, y, 120, 25);  

    NSRepository::CreateInstance(kCRadioButtonCID, nsnull, kIRadioButtonIID, (LPVOID*)&radioButton);
    radioButton->SetRadioGroup(radioGroup);
    radioButton->Create(window, rect, HandleEvent, NULL);
    nsString rbLabel2("RadioButton2");
    radioButton->SetLabel(rbLabel2);
    radioButton->Show(PR_TRUE);
    NS_RELEASE(radioButton);
    y += rect.height + 5;

    // [TODO] implement a test here to see if the radio group exists
    // radioGroup = NULL;

    //window->SetBackgroundColor(NS_RGB(0,255,0));

    //
    // create a ComboBox
    //
    rect.SetRect(x, y, 120, 100);  

    NSRepository::CreateInstance(kCComboBoxCID, nsnull, kIComboBoxIID, (LPVOID*)&comboBox);
    comboBox->Create(window, rect, HandleEvent, NULL);
    for (i=0;i<NUM_COMBOBOX_ITEMS;i++) {
      sprintf(str, "%s %d", "List Item", i);
      nsString listStr1(str);
      comboBox->AddItemAt(listStr1, i);
    }
    listSelfTest(gFD, "ComboBox", comboBox);
    NS_RELEASE(comboBox);

    x = createTestButton(window, kSetSelection,    x+125, y, 125, ComboTestHandleEvent);
    x = createTestButton(window, kRemoveSelection, x+5,   y, 125, ComboTestHandleEvent);

    x = 10;
    y += 30;

   
    nsString status("The non-visual tests: ");
    status.Append(eval(gOverallStatus));
    gNonVisualStatus = gOverallStatus;

    statusText->SetText(status);

    // show
    window->Show(PR_TRUE);

    window->SetCursor(eCursor_hyperlink);

    // Process messages
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}

void main(int argc, char **argv)
{
    WinMain(GetModuleHandle(NULL), NULL, 0, SW_SHOW);
}

