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


//---- Factory Includes & Stuff -----// 
#include "nsIFactory.h" 
#include "nsIComponentManager.h" 
#include "nsGfxCIID.h" 

#include "nsWidgetsCID.h" 
  

#include "nsIWidget.h"
#include "nsIButton.h"
#include "nsICheckButton.h"
#include "nsIRadioButton.h"
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
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"
#include "nsITabWidget.h"
#include "nsITooltipWidget.h"
#include "nsIAppShell.h"
#include "nsWidgetSupport.h"

#include <stdio.h>

#ifndef XP_MAC
extern "C" char *fe_GetConfigDir(void) {
  printf("XXX: return /tmp for fe_GetConfigDir\n");
  return strdup("/tmp");
}
#endif

char *  gLogFileName   = "selftest.txt";
FILE *  gFD             = nsnull;
PRInt32 gOverallStatus   = 0;
PRInt32 gNonVisualStatus = 0;

nsIWidget         *window = NULL;
nsITextWidget     *statusText = NULL;
nsITextAreaWidget *textWidgetInstance = NULL;
nsITextWidget     *passwordText  = NULL;

nsIComboBox   *comboBox      = NULL;
nsIListBox    *listBox       = NULL;
nsIListBox    *gMultiListBox = NULL;

nsIWidget     *movingWidget  = NULL;
nsIScrollbar  *scrollbar     = NULL;
nsITabWidget  *tabWidget     = NULL;
nsIButton     *toolTipButton1 = NULL;
nsIButton     *toolTipButton2 = NULL;

nsITooltipWidget *tooltipWindow = NULL;

nsIRadioButton * gRadioBtns[16];
int    gNumRadioBtns = 0;

char * gFailedMsg = NULL;


#ifdef XP_PC
#define WIDGET_DLL "raptorwidget.dll"
#define GFX_DLL "raptorgfxwin.dll"
#define TEXT_HEIGHT 25
#endif

#ifdef XP_UNIX
#ifndef WIDGET_DLL
#define WIDGET_DLL "libwidgetgtk.so"
#endif
#ifndef GFX_DLL
#define GFX_DLL "libgfxgtk.so"
#endif
#define TEXT_HEIGHT 30
#endif

#ifdef XP_MAC
#define WIDGET_DLL "WIDGET_DLL"
#define GFX_DLL "GFXWIN_DLL"
#define TEXT_HEIGHT 30
#endif

#define DEBUG_MOUSE 0

#define NUM_COMBOBOX_ITEMS 8
#define kSetCaret        "Set Caret"
#define kGetCaret        "Get Caret"
#define kSetSelection    "Set Selection"
#define kClearSelection  "Clear Sel."
#define kSelectAll       "Select All"
#define kRemoveSelection "Remove Selection"
#define kSetText         "Set Text"
#define kGetText         "Get Text"
#define kHideBtn         "Hide Btn"
#define kShowBtn         "Show Btn"
#define kBrowseBtn       "Browse..."
#define kSetSelectedIndices "Set 0,2,4"

#define kTooltip1_x 400
#define kTooltip1_y 100
#define kTooltip1_width 100
#define kTooltip1_height 100

#define kTooltip2_x 200
#define kTooltip2_y 300
#define kTooltip2_width 100
#define kTooltip2_height 100

// class ids
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

static NS_DEFINE_IID(kCTabWidgetCID, NS_TABWIDGET_CID);
static NS_DEFINE_IID(kCTooltipWidgetCID, NS_TOOLTIPWIDGET_CID);
static NS_DEFINE_IID(kCAppShellCID, NS_APPSHELL_CID);



// interface ids
static NS_DEFINE_IID(kIWindowIID, 				NS_IWINDOW_IID);
static NS_DEFINE_IID(kISupportsIID,       NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWidgetIID,         NS_IWIDGET_IID);
static NS_DEFINE_IID(kIButtonIID,         NS_IBUTTON_IID);
static NS_DEFINE_IID(kIScrollbarIID,      NS_ISCROLLBAR_IID);
static NS_DEFINE_IID(kICheckButtonIID,    NS_ICHECKBUTTON_IID);
static NS_DEFINE_IID(kITextWidgetIID,     NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kITextAreaWidgetIID, NS_ITEXTAREAWIDGET_IID);
static NS_DEFINE_IID(kIRadioButtonIID,    NS_IRADIOBUTTON_IID);
static NS_DEFINE_IID(kIListBoxIID,        NS_ILISTBOX_IID);
static NS_DEFINE_IID(kIListWidgetIID,     NS_ILISTWIDGET_IID);
static NS_DEFINE_IID(kIComboBoxIID,       NS_ICOMBOBOX_IID);
static NS_DEFINE_IID(kIFileWidgetIID,     NS_IFILEWIDGET_IID);
static NS_DEFINE_IID(kITabWidgetIID,      NS_ITABWIDGET_IID);
static NS_DEFINE_IID(kITooltipWidgetIID,  NS_ITOOLTIPWIDGET_IID);
static NS_DEFINE_IID(kIAppShellIID,       NS_IAPPSHELL_IID);


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

#if 0
char * eval(PRBool aVal) {
  if (!aVal) {
    gOverallStatus++;
  }
  return aVal ? "PASSED":"FAILED";
}
#endif

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
  fflush(fd);

  fprintf(fd, "\nTesting SelectItem value is [%d]\n", inx); fflush(fd);
  listBox->SelectItem(inx);
  nsAutoString buf;
  listBox->GetSelectedItem(buf);
  char * selStr = buf.ToNewCString();
  fprintf(fd, "\nTesting GetSelectedItem\n");
  fprintf(fd, "\tSelection should be [%s] is [%s]  Test: [%s]\n", 
          item4, selStr, eval(!strcmp(item4, selStr))); fflush(fd);
  if (nsnull != selStr) delete[] selStr;

  int    sel    = listBox->GetSelectedIndex();
  fprintf(fd, "\nTesting GetSelectedIndex\n");fflush(fd);
  fprintf(fd, "\tSelection should be [%d] is [%d]  Test: [%s]\n", 
          inx, sel, eval(inx == (int)sel)); fflush(fd);

  nsString item4Str(item4);
  sel = listBox->FindItem(item4Str, 0);
  fprintf(fd, "\nTesting FindItem\n");fflush(fd);
  fprintf(fd, "\tItem index should be [%d] index is [%d] Test: [%s]\n", 
          inx, sel, eval(inx == (int)sel)); fflush(fd);
  listBox->GetItemAt(buf, 4);
  selStr = buf.ToNewCString();
  fprintf(fd, "\nTesting GetItemAt\n\tItem %d should be [%s] is [%s]  Test: [%s]\n", inx, item4, selStr, eval(strcmp(selStr, item4) == 0)); fflush(fd);
  if (nsnull != selStr) delete[] selStr;

  listBox->SelectItem(2);
  inx = listBox->GetSelectedIndex();
  fprintf(fd, "\nTesting SelectItem && GetSelectedIndex\n\t Selected Item should be [%d] is [%d]  Test: [%s]\n", 2, inx, eval(inx == 2)); fflush(fd);

  int i;
  for (i=0;i<(int)listBox->GetItemCount();i++) {
    nsAutoString buf;
    listBox->GetItemAt(buf, i);
    char * str = buf.ToNewCString();

    fprintf(fd, "Item %d [%s]\n", i, str);fflush(fd);

    if (nsnull != str) delete[] str;
  }
  fprintf(fd, "Removing Item #4\n");fflush(fd);
  listBox->RemoveItemAt(4);
  
  nsString item4string(item4);
  fprintf(fd, "\nTesting RemoveItemAt\n\tTest: [%s]\n", eval(-1 == (int)listBox->FindItem(item4string, 0))); fflush(fd);

  for (i=0;i<(int)listBox->GetItemCount();i++) {
    nsAutoString buf;
    listBox->GetItemAt(buf, i);
    char * str = buf.ToNewCString();

    fprintf(fd, "Item %d [%s]\n", i, str);fflush(fd);

    if (nsnull != str) delete[] str;
  }
  listBox->Deselect();
  fprintf(fd, "\nTesting Deselect\n\t Selected Item [%d]  Test:[%s]\n", (int)listBox->GetSelectedIndex(), (-1 == (int)listBox->GetSelectedIndex()?"PASSED":"FAILED")); fflush(fd);

}

/**--------------------------------------------------------------------------------
  * Generic Text Box Non-Visual Test
  *--------------------------------------------------------------------------------
 */
void textSelfTest(FILE * fd, char * aTitle, nsITextWidget * aTextWidget) {
  fprintf(fd, "\n\n-----------------------------\n");
  fprintf(fd, "%s self test\n", aTitle);
  fprintf(fd, "-----------------------------\n\n");

  PRUint32 actualSize;
  aTextWidget->SetText(nsString("1234567890"),actualSize);
  PRUint32 start = 1;
  PRUint32 end   = 5;
  aTextWidget->SetSelection(start, end);

  PRUint32 start2 = 0;
  PRUint32 end2   = 0;
  aTextWidget->GetSelection(&start2, &end2);

  fprintf(fd, "Tested SetSelection and GetSelection Test Should be [%d,%d] is [%d,%d] [%s]\n", start,end, start2,end2,eval(start == start2 && end == end2));

  start = 5;
  aTextWidget->SetCaretPosition(start);

  aTextWidget->GetCaretPosition(start2);

  fprintf(fd, "Tested SetCaretPosition and GetCaretPosition Test [%s]\n", eval(start == start2));
  aTextWidget->InsertText(nsString("xxx"),1,3,actualSize);
  nsString str;
  aTextWidget->GetText(str,256,actualSize);
  char * s = str.ToNewCString();
  char * s2 = "1xxx234567890";
  fprintf(fd, "Tested InsertText Test [%s] is [%s] [%s]\n", s2, s, eval(!strcmp(s2, s)));
  fprintf(fd, "Tested InsertText Test [%s]\n", s);
  delete[] s;

}

/**--------------------------------------------------------------------------------
  * Generic MultiListWidget Box Non-Visual Test
  *--------------------------------------------------------------------------------
 */
void multiListSelfTest(FILE * fd, char * aTitle, nsIListBox * listBox) {
  fprintf(fd, "\n\n-----------------------------\n");
  fprintf(fd, "%s self test\n", aTitle);
  fprintf(fd, "-----------------------------\n\n");fflush(fd);

  nsIListBox * multi = (nsIListBox*)listBox;

  int inx = 4;
  char * item4 = "Multi List Item 4";

  nsAutoString buf;
  char * selStr;

  multi->GetItemAt(buf, 4);
  selStr = buf.ToNewCString();
  fprintf(fd, "\nTesting GetItemAt\n\tItem %d should be [%s] is [%s]  Test: [%s]\n", inx, item4, selStr, 
          eval(strcmp(selStr, item4) == 0)); fflush(fd);
  if (nsnull != selStr) delete[] selStr;


  multi->Deselect();
  int count = multi->GetSelectedCount();
  fprintf(fd, "\nTesting Deselect\n\tCount %d Test: [%s]\n", count, eval(0 == count)); fflush(fd);

  PRInt32 inxs[] = {0,2,4};
  PRInt32 len = 3;

  int i;
  /*for (i=0;i<len;i++) {
    multi->SelectItem(inxs[i]);
  }*/
  multi->SetSelectedIndices(inxs, 3);
  fprintf(fd, "\nTesting selecting items 0,2,4\n");fflush(fd);

  /*char * item0 = "Multi List Item 0";
  nsString selItem;
  multi->GetSelectedItem(selItem);
  selStr = selItem.ToNewCString();
  fprintf(fd, "\nTesting GetSelectedItem\n\t is [%s] should be [%s] Test: [%s]\n", 
    selStr, item0,  eval(!strcmp(selStr, item0))); fflush(fd);
  if (nsnull != selStr) delete[] selStr;*/ 

  int status = 1;
  count = multi->GetSelectedCount();
  fprintf(fd, "\nTesting GetSelectedCount\n\tCount [%d] should be [%d] Test: [%s]\n", count, len, 
          eval(len == count)); fflush(fd);

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
    fprintf(fd, "\nTesting GetSelectedIndices\n\tTest: [%s]\n", eval(len == (int)multi->GetSelectedCount())); fflush(fd);
  }

  for (i=0;i<(int)multi->GetItemCount();i++) {
    nsAutoString buf;
    multi->GetItemAt(buf, i);
    char * str = buf.ToNewCString();

    fprintf(fd, "Item %d [%s]\n", i, str);fflush(fd);

    if (nsnull != str) delete[] str;
  }
  fprintf(fd, "Removing Item #4\n");fflush(fd);
  multi->RemoveItemAt(4);

  nsString item4string(item4);
  fprintf(fd, "\nTesting RemoveItemAt\n\tTest: [%s]\n", eval(-1 == (int)multi->FindItem(item4string, (PRInt32)0))); fflush(fd);

  for (i=0;i<(int)multi->GetItemCount();i++) {
    nsAutoString buf;
    multi->GetItemAt(buf, i);
    char * str = buf.ToNewCString();

    fprintf(fd, "Item %d [%s]\n", i, str);fflush(fd);

    if (nsnull != str) delete[] str;
  }
  fprintf(fd, "Done with Mulitple List Box\n");
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
  nsComponentManager::CreateInstance(kCButtonCID, nsnull, kIButtonIID, (void **) &button);
  NS_CreateButton(aWin,button,rect,aHandleEventFunction);
  button->SetLabel(aTitle);
  return aX + aWidth;
}

/**--------------------------------------------------------------------------------
  *
 */
nsIButton* createSimpleButton(nsIWidget * aWin, 
                     char * aTitle, 
                     int aX, 
                     int aY, 
                     int aWidth, 
                     EVENT_CALLBACK aHandleEventFunction) {
  nsIButton *button;
  nsRect rect(aX, aY, aWidth, 25);  
  nsComponentManager::CreateInstance(kCButtonCID, nsnull, kIButtonIID, (void**)&button);
  NS_CreateButton(aWin,button,rect,aHandleEventFunction);
  button->SetLabel(aTitle);
  return button;
}


/**--------------------------------------------------------------------------------
  *
 */
nsITooltipWidget* createTooltipWindow(nsIWidget * aWin, 
                     char * aTitle, 
                     int aX, 
                     int aY, 
                     int aWidth, 
                     EVENT_CALLBACK aHandleEventFunction) {
  nsITooltipWidget *tooltip;
  nsRect rect(aX, aY, aWidth, 40);  
  nsComponentManager::CreateInstance(kCTooltipWidgetCID, nsnull, kITooltipWidgetIID, (void**)&tooltip);
  NS_CreateTooltipWidget((nsISupports*)nsnull,tooltip,rect,aHandleEventFunction);
  
  nsIWidget* toolTipWidget;
  if (NS_OK == tooltip->QueryInterface(kIWidgetIID,(void**)&toolTipWidget))
  {
    nsIButton *toolTipButton = createSimpleButton(toolTipWidget, "tooltip",5, 5, 80, 0);
    NS_RELEASE(toolTipWidget);
  }
  return tooltip;
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
  PRUint32 actualSize;

  if (NS_OK == aEvent->widget->QueryInterface(kIButtonIID, (void**)&btn)) {
    nsAutoString strBuf;
    btn->GetLabel(strBuf);
    char * title = strBuf.ToNewCString();
    //fprintf(gFD, "Title is [%s]\n", title);

    if (!strcmp(title, kSetSelection)) {
      aListWidget->SelectItem(2);
      fprintf(gFD, "\tTested SelectItem(2)\n");
      str.Append(" should show 'List Item 2'");
      statusText->SetText(str,actualSize);
      gFailedMsg = "List::SelectItem";
    } else if (!strcmp(title, kRemoveSelection)) {
      nsString item2("List Item 2");
      PRInt32 inx = aListWidget->FindItem(item2, 0);

      printf("aListWidget->FindItem(item2, 0) %d\n", inx);

      if (inx > -1) {
        aListWidget->RemoveItemAt(inx);
      }
      fprintf(gFD, "\tTested FindItem(...)\n");
      str.Append(" should show empty");
      statusText->SetText(str,actualSize);
      gFailedMsg = "List::RemoveItemAt && FindItem";
    }
    delete[] title;
    //NS_RELEASE(btn);
  }
  return nsEventStatus_eIgnore;
}

/**--------------------------------------------------------------------------------
  * Combox Test Handler
  *--------------------------------------------------------------------------------
 */
nsEventStatus PR_CALLBACK ComboTestHandleEvent(nsGUIEvent *aEvent) 
{
  nsEventStatus   result = nsEventStatus_eIgnore;
  nsIListWidget*  widget = nsnull;
  if (comboBox != nsnull && NS_OK == comboBox->QueryInterface(kIListWidgetIID,(void**)&widget))
  {
    result = GenericListHandleEvent(aEvent, "ComboBox", widget);
    NS_RELEASE(comboBox);
  }
  return result;
}


/**--------------------------------------------------------------------------------
  * ListBox Test Handler
  *--------------------------------------------------------------------------------
 */
nsEventStatus PR_CALLBACK ListBoxTestHandleEvent(nsGUIEvent *aEvent)
{
  nsEventStatus   result = nsEventStatus_eIgnore;
  nsIListWidget*  widget = nsnull;
  if (listBox != nsnull && NS_OK == listBox->QueryInterface(kIListWidgetIID,(void**)&widget))
  {
    result = GenericListHandleEvent(aEvent, "ListBox", widget);
    NS_RELEASE(listBox);
  }
  return result;
}

/**--------------------------------------------------------------------------------
  * Multi-ListBox Test Handler
  *--------------------------------------------------------------------------------
 */
nsEventStatus PR_CALLBACK MultiListBoxTestHandleEvent(nsGUIEvent *aEvent)
{
  nsIButton * btn;
  if (gMultiListBox == nsnull || aEvent->message != NS_MOUSE_LEFT_BUTTON_UP) {
    return nsEventStatus_eIgnore;
  }
  nsString str("Multi-ListBox");
  fprintf(gFD, "\nVisually tested Multi-List Box\n");

  if (NS_OK == aEvent->widget->QueryInterface(kIButtonIID, (void**)&btn)) {
    nsAutoString strBuf;
    btn->GetLabel(strBuf);
    char * title = strBuf.ToNewCString();
    //fprintf(gFD, "Title is [%s]\n", title);

    PRUint32 actualSize;
 
    if (!strcmp(title, kSetSelection)) {
      gMultiListBox->Deselect();

      PRInt32 inxs[] = {0,2,4};
      PRInt32 len = 3;

      int i;
      for (i=0;i<len;i++) {
        gMultiListBox->SelectItem(2);//inxs[i]);
      }
      fprintf(gFD, "\tTested SelectItem()\n");
      str.Append(" should show 'List Item 0,2,5'");
      statusText->SetText(str,actualSize);
      gFailedMsg = "Multi-List::SelectItem";
    } else if (!strcmp(title, kSetSelectedIndices)) {
      PRInt32 inxs[] = {0,2,4};
      PRInt32 len = 3;
      gMultiListBox->SetSelectedIndices(inxs, 3);
    } else if (!strcmp(title, kRemoveSelection)) {
      nsString item2("Multi List Item 2");
      PRInt32 inx = gMultiListBox->FindItem(item2, 0);

      if (inx > -1) {
        gMultiListBox->RemoveItemAt(inx);
      }
      fprintf(gFD, "\tTested FindItem(...)\n");
      str.Append(" should show 0,5");
      statusText->SetText(str,actualSize);
      gFailedMsg = "Multi-List::FindItem && RemoveItemAt";
    }
    delete[] title;
    //NS_RELEASE(btn);
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
    PRBool state = PR_FALSE;
    chkBtn->GetState(state);
    chkBtn->SetState((PRBool)!state);
    //NS_RELEASE(chkBtn);
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
    
    PRUint32 actualSize;
    if (!strcmp(title, kSetText)) {
      aTextWidget->SetText(aTestStr,actualSize);

      fprintf(gFD, "\nVisually Testing Text\n");

      str.Append(" should show[");
      str.Append(aStrToShow);
      str.Append("]");

      statusText->SetText(str,actualSize);
      gFailedMsg = "nsITextWidget::SetText";
    } else if (!strcmp(title, kGetText)) {
      nsString getStr;
      aTextWidget->GetText(getStr, 256,actualSize);
      
      fprintf(gFD, "\tTested GetText(...) \n");

      if (aTestStr.Equals(getStr)) {
        str.Append(" Test PASSED");
        fprintf(gFD, "Test PASSED");
      } else {
        str.Append(" Test FAILED");
        fprintf(gFD, "Test FAILED");
      }
      statusText->SetText(str,actualSize);
      gFailedMsg = "nsITextWidget::GetText";
    } else if (!strcmp(title, kSetCaret)) {
      nsString getStr;
      aTextWidget->SetCaretPosition(1);
      fprintf(gFD, "\tTested SetCaretPosition(1) \n");
      str.Append(" should show caret in position 1");
      statusText->SetText(str,actualSize);
      gFailedMsg = "nsITextWidget::SetCaretPosition";
    } else if (!strcmp(title, kGetCaret)) {
      nsString getStr;
      PRUint32 pos = 0;
      aTextWidget->GetCaretPosition(pos);

      fprintf(gFD, "Visually tested GetCaretPosition(1) \n");

      if (pos == 1) {
        str.Append(" Test PASSED");
        fprintf(gFD, "Test PASSED\n");
      } else {
        str.Append(" Test FAILED");
        fprintf(gFD, "Test FAILED\n");
      }
      statusText->SetText(str,actualSize);
      gFailedMsg = "nsITextWidget::GetCaretPosition";
    } else if (!strcmp(title, kClearSelection)) {
      aTextWidget->SetSelection(0,0);
      //aTextWidget->SetCaretPosition(0);
      str.Append(" selection should be cleared");
      statusText->SetText(str,actualSize);
    } else if (!strcmp(title, kSelectAll)) {
      aTextWidget->SelectAll();
      str.Append(" Everything should be selected");
      statusText->SetText(str,actualSize);
    } else if (!strcmp(title, kSetSelection)) {
      nsString getStr;
      aTextWidget->SetSelection(1,5);

      fprintf(gFD, "\tTested SetSelection(1,5) \n");

      str.Append(" should show selection from 1 to 5");
      statusText->SetText(str,actualSize);
      gFailedMsg = "nsITextWidget::SetSelection";
    }
    delete[] title;
    //NS_RELEASE(btn);
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

  PRUint32 actualSize;
  if (NS_OK == aEvent->widget->QueryInterface(kIButtonIID, (void**)&btn)) {
    nsAutoString strBuf;
    nsString     str("Tesing ");
    btn->GetLabel(strBuf);
    char * title = strBuf.ToNewCString();

    if (!strcmp(title, kHideBtn)) {
      movingWidget->Show(PR_FALSE);
      str.Append("nsIWidget::Show(FALSE)");
      statusText->SetText(str,actualSize);
      gFailedMsg = "nsIWidget::Show(FALSE)";
    } else if (!strcmp(title, kShowBtn)) {
      movingWidget->Show(PR_TRUE);
      str.Append("nsIWidget::Show(TRUE)");
      statusText->SetText(str,actualSize);
      gFailedMsg = "nsIWidget::Show(TRUE)";
    }

    delete[] title;
    //NS_RELEASE(btn);
    
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
#ifdef XP_PC
  nsRect rect;
  // print the main window position
  window->GetBounds(rect);
  printf("Bounds(%d, %d, %d, %d)\n", rect.x, rect.y, rect.width, rect.height);

  // print all children's position
  nsIEnumerator *enumerator = window->GetChildren();
  nsISupports * widget;
  do {
      if (!NS_SUCCEEDED(enumerator->CurrentItem(&widget))) {
        return;
      }
      nsIWidget *child;
      if (NS_OK == widget->QueryInterface(kIWidgetIID, (void**)&child)) {
          //
          child->GetBounds(rect);
          printf("Bounds(%d, %d, %d, %d)\n", rect.x, rect.y, rect.width, rect.height);
          NS_RELEASE(child);
      }
      NS_RELEASE(widget);
  }
  while (NS_SUCCEEDED(enumerator->Next()));

  NS_RELEASE(enumerator);
  delete enumerator;
#endif
}


/**--------------------------------------------------------------------------------
 * Main Handler
 *--------------------------------------------------------------------------------
 */
nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent)
{ 
	//  printf("aEvent->message %d\n", aEvent->message);
    nsEventStatus result = nsEventStatus_eIgnore;
   PRUint32 actualSize;
   switch(aEvent->message) {

        case NS_SHOW_TOOLTIP:
          {
            char buf[256];
            nsTooltipEvent* tEvent = (nsTooltipEvent*)aEvent;
            sprintf(buf,"Show tooltip %d", tEvent->tipIndex);
            statusText->SetText(buf,actualSize);
            nsRect oldPos;
            oldPos.x = aEvent->point.x;
            oldPos.y = aEvent->point.y;
            nsRect newPos;
            window->WidgetToScreen(oldPos, newPos);
            NS_MoveWidget(tooltipWindow,newPos.x + 5, newPos.y + 5);
            NS_ShowWidget(tooltipWindow,PR_TRUE);
          }
          break;

        case NS_HIDE_TOOLTIP:
          statusText->SetText("Hide tooltip",actualSize);
          NS_ShowWidget(tooltipWindow,PR_FALSE);
          break;
        
        case NS_MOVE:
            char str[256];
            sprintf(str, "Moved window to %d,%d", aEvent->point.x, aEvent->point.y);
            statusText->SetText(str,actualSize);
            break;

        case NS_MOUSE_LEFT_DOUBLECLICK:
            statusText->SetText("Left button double click",actualSize);
            break;

        case NS_MOUSE_RIGHT_DOUBLECLICK:
            statusText->SetText("Right button double click",actualSize);
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
        
        case NS_MOUSE_LEFT_BUTTON_UP: {
            if (DEBUG_MOUSE) printf("NS_MOUSE_LEFT_BUTTON_UP\n");
            int i = 0;
            for (i=0;i<gNumRadioBtns;i++) {
              nsIWidget * win;
              if (NS_OK == gRadioBtns[i]->QueryInterface(kIWidgetIID, (void**)&win)) {
                printf("%d  0x%x  0x%x\n", i, win, aEvent->widget);
                if (win == aEvent->widget) {
                  gRadioBtns[i]->SetState(PR_TRUE);
                } else {
                  gRadioBtns[i]->SetState(PR_FALSE);
                }
              }
            }
            } break;
        

        case NS_MOUSE_LEFT_BUTTON_DOWN:
            if (DEBUG_MOUSE) printf("NS_MOUSE_LEFT_BUTTON_DOWN\n");
            break;

        case NS_CREATE:
            printf("Window Created\n");
            break;
        
        case NS_PAINT: 
#ifndef XP_UNIX
              // paint the background
            if (aEvent->widget == window) {
                nsIRenderingContext *drawCtx = ((nsPaintEvent*)aEvent)->renderingContext;
                drawCtx->SetColor(aEvent->widget->GetBackgroundColor());
                drawCtx->FillRect(*(((nsPaintEvent*)aEvent)->rect));

                return nsEventStatus_eIgnore;
            }
#endif
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
              PRUint32  pos = 0;
              scrollbar->GetPosition(pos);
              NS_MoveWidget(movingWidget,10,pos);
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
            statusText->SetText(nsString(str),actualSize);
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
  PRUint32 actualSize;
  switch(aEvent->message) {
           
    case NS_MOUSE_LEFT_BUTTON_UP:
      // create a FileWidget
      //
      nsIFileWidget *fileWidget;

      nsString title("FileWidget Title <here> mode = save");
      nsComponentManager::CreateInstance(kCFileWidgetCID, nsnull, kIFileWidgetIID, (void**)&fileWidget);
  
      nsString titles[] = {"all files","html","executables" };
      nsString filters[] = {"*.*", "*.html", "*.exe" };
      fileWidget->SetFilterList(3, titles, filters);
      fileWidget->Create(window,
                         title,
                         eMode_save);
  
      PRUint32 result = fileWidget->Show();
      if (result) {
        nsString file;
        fileWidget->GetFile(file);
        char* filestr = file.ToNewCString();
        printf("file widget contents %s\n", filestr);
        statusText->SetText(filestr,actualSize);
        delete[] filestr;
      }
      else
        statusText->SetText("Cancel selected",actualSize);


      //NS_RELEASE(fileWidget);
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
  PRUint32 actualSize;
  switch(aEvent->message) {
           
    case NS_TABCHANGE:
      PRUint32 tab = 0;
      tabWidget->GetSelectedTab(tab);
      char buf[256];
      sprintf(buf, "Selected tab %d", tab);
      statusText->SetText(buf,actualSize);
    break;
  }

  return(nsEventStatus_eConsumeDoDefault);
}


void SetTooltipPos(int pos, nsIWidget *aWidget, nsIButton *aButton1, nsIButton *aButton2)
{
  switch(pos) {
    case 1: {
      nsRect* tips1[2];
      tips1[0] = new nsRect(
                        kTooltip1_x,
                        kTooltip1_y,
                        kTooltip1_width / 2,
                        kTooltip1_height);
      tips1[1] = new nsRect(
                        kTooltip1_x + (kTooltip1_width / 2),
                        kTooltip1_y,
                        kTooltip1_width / 2,
                        kTooltip1_height);
      aWidget->SetTooltips(2, tips1);
      NS_MoveWidget(aButton1,kTooltip1_x, kTooltip1_y);
      NS_MoveWidget(aButton2,kTooltip1_x + kTooltip1_width, kTooltip1_y + kTooltip1_height);
      delete tips1[0];
      delete tips1[1];
            }
      break;
    case 2: {
      nsRect* tipsDummy[1];

        // Test updating the tooltips by initialy giving a tooltip
        // location that is 100 pixels to the left, then changing
        // it to the correct position.
      tipsDummy[0] = new nsRect(kTooltip2_x - 100,kTooltip2_y,
                      kTooltip2_width,kTooltip2_height);
     
      aWidget->SetTooltips(1, tipsDummy);

      nsRect* tips2[1];
      tips2[0] = new nsRect(kTooltip2_x,kTooltip2_y,
                      kTooltip2_width,kTooltip2_height);

      aWidget->UpdateTooltips(tips2); // Put it in the correct position

      NS_MoveWidget(aButton1,kTooltip2_x, kTooltip2_y);
      NS_MoveWidget(aButton2,kTooltip2_x + kTooltip2_width, kTooltip2_y + kTooltip2_height);

      delete tips2[0];
      delete tipsDummy[0];
      }
      break;
    }
}


nsEventStatus MoveTooltip(int aPos, nsGUIEvent *aEvent)
{
 switch(aEvent->message) {
           
    case NS_MOUSE_LEFT_BUTTON_DOWN:
      SetTooltipPos(aPos, window, toolTipButton1, toolTipButton2);
    break;
  }
 
  return(nsEventStatus_eConsumeDoDefault);
}

nsEventStatus PR_CALLBACK TooltipPos1(nsGUIEvent *aEvent)
{
  return(MoveTooltip(1, aEvent));
}

nsEventStatus PR_CALLBACK TooltipPos2(nsGUIEvent *aEvent)
{
  return(MoveTooltip(2, aEvent));
}

/*----------------------------------------------------------------------------
 * DoSelfTests
 *---------------------------------------------------------------------------*/
nsEventStatus PR_CALLBACK DoSelfTests(nsGUIEvent *aEvent)
{
  if (aEvent->message != NS_MOUSE_LEFT_BUTTON_UP) {
    return nsEventStatus_eIgnore;
  }

  textSelfTest(gFD, "Password Text", passwordText);
  
	if (listBox)
	{
	  nsIListWidget* widget;
	  if (NS_OK == listBox->QueryInterface(kIListWidgetIID,(void**)&widget))
	  {
	    listSelfTest(gFD, "ListBox", widget);
	    NS_RELEASE(widget);
	  }
  }

	if (comboBox)
	{
	  nsIListWidget* widget;
	  if (NS_OK == comboBox->QueryInterface(kIListWidgetIID,(void**)&widget))
	  {
	    listSelfTest(gFD, "ComboBox", widget);
	    NS_RELEASE(widget);
	  }
  }

	if (gMultiListBox)
	{
	  multiListSelfTest(gFD, "Multi-ListBox", gMultiListBox);
	}
  return nsEventStatus_eIgnore;

}

/**--------------------------------------------------------------------------------
 *
 */
nsresult WidgetTest(int *argc, char **argv)
{
    char str[256];
    int i;

    // Open global test log file
    gFD = fopen(gLogFileName, "w");
    if (gFD == nsnull) {
      fprintf(stderr, "Couldn't open file[%s]\n", gLogFileName);
      exit(1);
    }

    
    // register widget classes
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
    nsComponentManager::RegisterComponent(kCTabWidgetCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kCTooltipWidgetCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
    nsComponentManager::RegisterComponent(kCAppShellCID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
    static NS_DEFINE_IID(kCRenderingContextIID, NS_RENDERING_CONTEXT_CID); 
    static NS_DEFINE_IID(kCDeviceContextIID, NS_DEVICE_CONTEXT_CID); 
    static NS_DEFINE_IID(kCFontMetricsIID, NS_FONT_METRICS_CID); 
    static NS_DEFINE_IID(kCImageIID, NS_IMAGE_CID); 


    nsComponentManager::RegisterComponent(kCRenderingContextIID, NULL, NULL, GFX_DLL, PR_FALSE, PR_FALSE); 
    nsComponentManager::RegisterComponent(kCDeviceContextIID, NULL, NULL, GFX_DLL, PR_FALSE, PR_FALSE); 
    nsComponentManager::RegisterComponent(kCFontMetricsIID, NULL, NULL, GFX_DLL, PR_FALSE, PR_FALSE); 
    nsComponentManager::RegisterComponent(kCImageIID, NULL, NULL, GFX_DLL, PR_FALSE, PR_FALSE); 

      // Create a application shell
    nsIAppShell *appShell;
    nsComponentManager::CreateInstance(kCAppShellCID, nsnull, kIAppShellIID, (void**)&appShell);
    if (appShell != nsnull) {
      fputs("Created AppShell\n", stderr);
      appShell->Create(argc, argv);
    } else {
      printf("AppShell is null!\n");
    }

    nsIDeviceContext* deviceContext = 0;

    // Create a device context for the widgets
    nsresult  res;

    static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
    static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

    //
    // create the main window
    //
    nsComponentManager::CreateInstance(kCWindowCID, nsnull, kIWindowIID, (void**)&window);
    nsRect rect(100, 100, 600, 700);
    window->Create((nsIWidget*) nsnull, rect, HandleEvent, 
                   (nsIDeviceContext *) nsnull,
                   appShell);
    window->SetTitle("TOP-LEVEL window");
    window->Show(PR_TRUE);
    window->SetBackgroundColor(NS_RGB(196, 196, 196));

    //
    // Create Device Context based on main window
    //
    res = nsComponentManager::CreateInstance(kDeviceContextCID, nsnull, kDeviceContextIID, (void **)&deviceContext);

    if (NS_OK == res)
    {
      deviceContext->Init(window->GetNativeData(NS_NATIVE_WIDGET));
      NS_ADDREF(deviceContext);
    }

#ifdef XP_PC
    tooltipWindow = createTooltipWindow(window, "INSERT <tooltip> here", 0, 0, 150, 0);
    NS_ShowWidget(tooltipWindow,PR_FALSE);
    toolTipButton1 = createSimpleButton(window, "Tooltip \\/\\/",400, 100, 100, 0);
    toolTipButton2 = createSimpleButton(window, "Tooltip /\\/\\",500, 200, 100, 0);
    createTestButton(window, "Move Tooltip pos 1", 450, 150, 130, TooltipPos1);
    createTestButton(window, "Move Tooltip pos 2", 450, 175, 130, TooltipPos2);
    SetTooltipPos(1, window, toolTipButton1, toolTipButton2);
#endif

    //
    // create a child
    //
    nsIWidget *child;
    //
    // create another child
    //

    int x = 5;
    int y = 10;
    rect.SetRect(x, y, 100, 100);  

    nsComponentManager::CreateInstance(kCChildCID, nsnull, kIWidgetIID, (void**)&child);
      
#if 0 
    child->SetBorderStyle(eBorderStyle_dialog);
    child->Create(window, rect, HandleEvent, NULL);
    //child->SetBackgroundColor(NS_RGB(255, 255, 0));
    child->SetForegroundColor(NS_RGB(255, 0, 0));
    child->SetBackgroundColor(NS_RGB(255, 128, 64));
#endif  

    //NS_RELEASE(child); // the parent keeps a reference on this child

    y += rect.height + 5;

   
    //
    // create a button
    //
    nsIButton *button;
    rect.SetRect(x, y, 60, 25);  
    nsComponentManager::CreateInstance(kCButtonCID, nsnull, kIButtonIID, (void**)&button);
    NS_CreateButton(window,button,rect,HandleEvent);
    nsString label("Slider");
    button->SetLabel(label);

    nsAutoString strBuf;
    button->GetLabel(strBuf);

    button->QueryInterface(kIWidgetIID,(void**)&movingWidget);
    y += rect.height + 5;

    x = 5;
    x = createTestButton(window, kHideBtn, x, y, 75, ButtonTestHandleEvent) + 5;
    x = createTestButton(window, kShowBtn, x, y, 75, ButtonTestHandleEvent);
    x = 5;
    y += rect.height + 5;

    // Create browse button
    x = createTestButton(window, kBrowseBtn, x,   y, 75, HandleFileButtonEvent);
    x = 5;
    y += rect.height + 5;

    //
    // create a check button
    //
    nsICheckButton * checkButton;
    rect.SetRect(x, y, 100, 25);  

    nsComponentManager::CreateInstance(kCCheckButtonCID, nsnull, kICheckButtonIID, (void**)&checkButton);
    NS_CreateCheckButton(window,checkButton,rect,CheckButtonTestHandleEvent);
    nsString cbLabel("CheckButton");
    checkButton->SetLabel(cbLabel);
    y += rect.height + 5;

    //
    // create a text widget
    //

    nsIWidget*      widget = nsnull;
    nsITextWidget*  textWidget = nsnull;
    rect.SetRect(x, y, 100, TEXT_HEIGHT);  

    PRUint32 actualSize;
    nsFont font("Times", NS_FONT_STYLE_NORMAL,
                         NS_FONT_VARIANT_NORMAL,
                         NS_FONT_WEIGHT_BOLD,
                         0,
                         12);

    nsComponentManager::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (void**)&textWidget);
    NS_CreateTextWidget(window,textWidget,rect,HandleEvent);

    nsString initialText("0123456789");
    textWidget->SetText(initialText,actualSize);
    textWidget->SetMaxTextLength(12);
    textWidget->SelectAll();

    //NS_RELEASE(textWidget); 
    y += rect.height + 5;

     //
    // create a text password widget
    //

    nsITextWidget * ptextWidget;
    rect.SetRect(x, y, 100, TEXT_HEIGHT);  
    nsComponentManager::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (void**)&ptextWidget);
    NS_CreateTextWidget(window, ptextWidget, rect, HandleEvent);

    
    nsString pinitialText("password text");
    ptextWidget->SetText(pinitialText,actualSize);
    passwordText = ptextWidget;
   
    ptextWidget->SetPassword(PR_TRUE);

    x = x+180;
    int saveX = x;

    x = createTestButton(window, kSetSelection, saveX,y, 100, PasswordTextTestHandleEvent);
    x = createTestButton(window, kClearSelection, x+5,y, 100, PasswordTextTestHandleEvent);
    x = createTestButton(window, kSelectAll,      x+5,y, 100, PasswordTextTestHandleEvent);

    // Next Row of texting Buttons
    x = saveX;
    y += 30;
    x = createTestButton(window, kSetCaret, x+5,y, 75, PasswordTextTestHandleEvent);
    x = createTestButton(window, kGetCaret, x+5,y, 75, PasswordTextTestHandleEvent);
    x = createTestButton(window, kSetText,  x+5,y, 75, PasswordTextTestHandleEvent);
    x = createTestButton(window, kGetText,  x+5,y, 75, PasswordTextTestHandleEvent);

    x = 5;
    //y += rect.height + 5;

    //
    // create a readonly text widget
    //

    nsITextWidget * rtextWidget;
    rect.SetRect(x, y, 100, TEXT_HEIGHT);  
    nsComponentManager::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (void**)&rtextWidget);
    NS_CreateTextWidget(window, rtextWidget, rect, HandleEvent);

    PRBool old;

    nsString rinitialText("This is readonly");
    rtextWidget->SetText(rinitialText,actualSize);
    rtextWidget->SetReadOnly(PR_TRUE,old);
 
    //NS_RELEASE(rtextWidget); 
    y += rect.height + 5;

    //
    // create a text area widget
    //

    nsITextAreaWidget * textAreaWidget;
    rect.SetRect(x, y, 150, 100);  
    nsComponentManager::CreateInstance(kCTextAreaCID, nsnull, kITextAreaWidgetIID, (void**)&textAreaWidget);
    NS_CreateTextAreaWidget(window,textAreaWidget,rect,HandleEvent);
    nsString textAreaInitialText("Text Area Widget");
    textWidgetInstance = textAreaWidget;
    textAreaWidget->SetText(textAreaInitialText,actualSize);
    //NS_RELEASE(textAreaWidget); 
    //y += rect.height + 5;
    x += rect.width + 5;

    // Save these for later
    int saveY = y;
    saveX = x;

    //
    // create a scrollbar
    //
    rect.SetRect(x, 10, 25, 300);  
    nsComponentManager::CreateInstance(kCVertScrollbarCID, nsnull, kIScrollbarIID, (void**)&scrollbar);
    NS_CreateScrollBar(window,scrollbar,rect,HandleEvent);
    scrollbar->SetMaxRange(300);
    scrollbar->SetThumbSize(50);
    scrollbar->SetPosition(100);
    x += rect.width + 5;
 
    //
    // create a Status Text
    //
    y = 10;
    rect.SetRect(x, y, 350, TEXT_HEIGHT);  
    nsComponentManager::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (void**)&statusText);
    statusText->QueryInterface(kIWidgetIID,(void**)&widget);
    widget->Create(window, rect, HandleEvent, deviceContext);
    widget->Show(PR_TRUE);
    y += rect.height + 5;

    //
    // create a Failed Button
    //
    rect.SetRect(x, y, 100, 25);  
    nsComponentManager::CreateInstance(kCButtonCID, nsnull, kIButtonIID, (void**)&button);
    NS_CreateButton(window,button,rect,FailedButtonHandleEvent);
    nsString failedLabel("Failed");
    button->SetLabel(failedLabel);

    rect.SetRect(x, y+30, 150, 25);  
    nsComponentManager::CreateInstance(kCButtonCID, nsnull, kIButtonIID, (void**)&button);
    NS_CreateButton(window,button,rect,DoSelfTests);
    nsString selfTestLabel("Perform Self Tests");
    button->SetLabel(selfTestLabel);
    x += rect.width + 5;

    //
    // create a Succeeded Button
    //
    rect.SetRect(x, y, 100, 25);  
    nsComponentManager::CreateInstance(kCButtonCID, nsnull, kIButtonIID, (void**)&button);
    NS_CreateButton(window,button,rect,SucceededButtonHandleEvent);
    nsString succeededLabel("Succeeded");
    button->SetLabel(succeededLabel);
    
    //
    // create a listbox widget
    //
    y = saveY;
    x = saveX;
    rect.SetRect(x, y, 150, 100);  
    nsComponentManager::CreateInstance(kCListBoxCID, nsnull, kIListBoxIID, (void**)&listBox);
    if (listBox)
    {
	    NS_CreateListBox(window,listBox,rect,HandleEvent);
	    for (i=0;i<NUM_COMBOBOX_ITEMS;i++) {
	      sprintf(str, "%s %d", "List Item", i);
	      nsString listStr1(str);
	      listBox->AddItemAt(listStr1, i);
	    }
    }

    x += rect.width+5;
    x = createTestButton(window, kSetSelection,    x, y, 125, ListBoxTestHandleEvent) + 5;
    x = createTestButton(window, kRemoveSelection, x,   y, 125, ListBoxTestHandleEvent);
    x = 5;
    y += rect.height + 5;

    //
    // create a multi-selection listbox widget
    //
    rect.SetRect(x, y, 150, 100);  
    nsComponentManager::CreateInstance(kCListBoxCID, nsnull, kIListBoxIID, (void**)&gMultiListBox);
    if (gMultiListBox)
    {
	      // Notice the extra agrument PR_TRUE below which indicates that
	      // the list widget is multi-select
	    gMultiListBox->SetMultipleSelection(PR_TRUE);
	    NS_CreateListBox(window,gMultiListBox,rect,HandleEvent);
	    for (i=0;i<NUM_COMBOBOX_ITEMS;i++) {
	      sprintf(str, "%s %d", "Multi List Item", i);
	      nsString listStr1(str);
	      gMultiListBox->AddItemAt(listStr1, i);
	    }
    }

    x = createTestButton(window, kSetSelection,    x+150, y, 125, MultiListBoxTestHandleEvent);
    x = createTestButton(window, kRemoveSelection, x+5,   y, 125, MultiListBoxTestHandleEvent);
    x = createTestButton(window, kSetSelectedIndices, x+5,   y, 125, MultiListBoxTestHandleEvent);

    y += rect.height + 5;
    x = 5;

    //
    // create a tab widget
    //
    rect.SetRect(300, 500, 200, 50);  
    nsComponentManager::CreateInstance(kCTabWidgetCID, nsnull, kITabWidgetIID, (void**)&tabWidget);
    if (tabWidget)
    {
	    NS_CreateTabWidget(window,tabWidget,rect,HandleTabEvent);
	    nsString tabs[] = {"low", "medium", "high" };
	    tabWidget->SetTabs(3, tabs);
    }

    //
    // create a Radio button
    //
    nsIRadioButton * radioButton;
    rect.SetRect(x, y, 120, 25);  

    nsComponentManager::CreateInstance(kCRadioButtonCID, nsnull, kIRadioButtonIID, (void**)&radioButton);
    NS_CreateRadioButton(window,radioButton,rect,HandleEvent);
    nsString rbLabel("RadioButton1");
    radioButton->SetLabel(rbLabel);
    gRadioBtns[gNumRadioBtns++] = radioButton;
    y += rect.height + 5;

    //
    // create a Radio button
    //
    rect.SetRect(x, y, 120, 25);  

    nsComponentManager::CreateInstance(kCRadioButtonCID, nsnull, kIRadioButtonIID, (void**)&radioButton);
    NS_CreateRadioButton(window,radioButton,rect,HandleEvent);
    nsString rbLabel2("RadioButton2");
    radioButton->SetLabel(rbLabel2);
    gRadioBtns[gNumRadioBtns++] = radioButton;
    y += rect.height + 5;

    //window->SetBackgroundColor(NS_RGB(0,255,0));

    //
    // create a ComboBox
    //
    rect.SetRect(x, y, 120, 100);  

    nsComponentManager::CreateInstance(kCComboBoxCID, nsnull, kIComboBoxIID, (void**)&comboBox);
    if (comboBox)
    {
	    NS_CreateComboBox(window,comboBox,rect,HandleEvent);
	    for (i=0;i<NUM_COMBOBOX_ITEMS;i++) {
	      sprintf(str, "%s %d", "List Item", i);
	      nsString listStr1(str);
	      comboBox->AddItemAt(listStr1, i);
	    }
    }

    x = createTestButton(window, kSetSelection,    x+125, y, 125, ComboTestHandleEvent);
    x = createTestButton(window, kRemoveSelection, x+5,   y, 125, ComboTestHandleEvent);

    x = 5;
    y += 30;

   
    nsString status("The non-visual tests: ");
    status.Append( (gOverallStatus  ? "PASSED":"FAILED"));

    gNonVisualStatus = gOverallStatus;

    statusText->SetText(status,actualSize);

    // show
    window->Show(PR_TRUE);
    window->SetCursor(eCursor_hyperlink);

    return(appShell->Run());
}


