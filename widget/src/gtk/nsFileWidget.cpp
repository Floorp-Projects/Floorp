/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
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
 */

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsIPlatformCharset.h"
#include "nsFileWidget.h"
#include "nsIToolkit.h"

NS_IMPL_ISUPPORTS1(nsFileWidget, nsIFileWidget)

// TODO: using static leaks, need to release at dll unload
static nsIUnicodeEncoder * gUnicodeEncoder = nsnull;

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

//-------------------------------------------------------------------------
//
// nsFileWidget constructor
//
//-------------------------------------------------------------------------
nsFileWidget::nsFileWidget() : nsIFileWidget()
{
  NS_INIT_REFCNT();
  mWidget = nsnull;
  mDisplayDirectory = nsnull;
  mFilterMenu = nsnull;
  mOptionMenu = nsnull;
  mNumberOfFilters = 0;
}

//-------------------------------------------------------------------------
//
// nsFileWidget destructor
//
//-------------------------------------------------------------------------
nsFileWidget::~nsFileWidget()
{
  if (mFilterMenu)
  {
    GtkWidget *menu_item;
    GList *list = g_list_first(GTK_MENU_SHELL(mFilterMenu)->children);

    for (;list; list = list->next)
    {
      menu_item = GTK_WIDGET(list->data);
      gchar *data = (gchar*)gtk_object_get_data(GTK_OBJECT(menu_item), "filters");
      
      if (data)
        nsCRT::free(data);
    }
  }

  gtk_widget_destroy(mWidget);
}


static void file_ok_clicked(GtkWidget *w, PRBool *ret)
{
  g_print("user hit ok\n");
#if 0
  struct stat buf;

  text = g_strdup(gtk_file_selection_get_filename(GTK_FILE_SELECTION(filesel)));
  g_strstrip(text);
  while ((text[strlen(text) - 1] == '/') && (strlen(text) != 1)) {
    text[strlen(text) - 1] = '\0';
  }
  if (stat(text, &buf) == 0) {
    if (S_ISDIR(buf.st_mode)) {               /* Selected directory -- don't close frequester */
      text2 = g_strdup_printf("%s/", text);
      gtk_file_selection_set_filename(GTK_FILE_SELECTION(filesel), text2);
      g_free(text2);
      return PR_FALSE;
    }
  }
  
  g_free(text);
#endif
  *ret = PR_TRUE;
  gtk_main_quit();
}

static void file_cancel_clicked(GtkWidget *w, PRBool *ret)
{
  g_print("user hit cancel\n");
  *ret = PR_FALSE;
  gtk_main_quit();
}

static gint file_delete_window(GtkWidget *w, gpointer data)
{
  printf("window closed\n");
  gtk_main_quit();
  return PR_TRUE;
}

static void file_destroy_dialog(GtkWidget *w, gpointer data)
{
  GtkFileSelection *fs = GTK_FILE_SELECTION(data);

  fs->fileop_dialog = NULL;
}

static void filter_item_activated(GtkWidget *w, gpointer data)
{
  //  nsFileWidget *f = (nsFileWidget*)data;
  gchar *foo = (gchar*)gtk_object_get_data(GTK_OBJECT(w), "filters");
  g_print("filter_item_activated(): %s\n", foo);
}

//-------------------------------------------------------------------------
//
// Show - Display the file dialog
//
//-------------------------------------------------------------------------
PRBool nsFileWidget::Show()
{
  PRBool ret = PR_FALSE;
  if (mWidget) {
    // make things shorter
    GtkFileSelection *fs = GTK_FILE_SELECTION(mWidget);

    nsresult rv;
    char *buff = nsnull;

    // convert from unicode to locale charset
    PRInt32 inLength = mDefault.Length();
    PRInt32 outLength;
    NS_ASSERTION(gUnicodeEncoder, "no unicode converter");
    rv = gUnicodeEncoder->GetMaxLength(mDefault.GetUnicode(), inLength, &outLength);
    if (NS_SUCCEEDED(rv)) {
      buff = new char[outLength+1];
      if (nsnull != buff) {
        rv = gUnicodeEncoder->Convert(mDefault.GetUnicode(), &inLength, buff, &outLength);
        if (NS_SUCCEEDED(rv)) {
          buff[outLength] = '\0';
          gtk_file_selection_set_filename(fs, (const gchar*) buff);
#if defined(debug_nhotta)
          printf("debug: file name is %s\n", buff);
#endif
          delete [] buff;
        }
        else
          gtk_file_selection_set_filename(fs, (const gchar*)nsAutoCString(mDefault));
      }
    }
    if (NS_FAILED(rv))
      gtk_file_selection_set_filename(fs, (const gchar*)nsAutoCString(mDefault));

    if (mNumberOfFilters != 0)
    {
      gtk_option_menu_set_menu(GTK_OPTION_MENU(mOptionMenu), mFilterMenu);
    }
    else
      gtk_widget_hide(mOptionMenu);

#if 0
    if (mDisplayDirectory)
      gtk_file_selection_complete(fs, "/");
#endif

    gtk_window_set_modal(GTK_WINDOW(mWidget), PR_TRUE);
    gtk_grab_add(mWidget);
    gtk_widget_show(mWidget);

    // handle close, destroy, etc on the dialog
    gtk_signal_connect(GTK_OBJECT(fs->ok_button), "clicked",
                       GTK_SIGNAL_FUNC(file_ok_clicked),
                       &ret);
    gtk_signal_connect(GTK_OBJECT(fs->cancel_button), "clicked",
                       GTK_SIGNAL_FUNC(file_cancel_clicked),
                       &ret);
    gtk_signal_connect(GTK_OBJECT(mWidget), "delete_event",
                       GTK_SIGNAL_FUNC(file_delete_window),
                       NULL);
    // start new loop.   ret is set in the above callbacks.
    gtk_main();
  }
  else {
    ret = PR_FALSE;
  }
  gtk_grab_remove(mWidget);
  return ret;
}

//-------------------------------------------------------------------------
//
// Show - Display the replace dialog
//
//-------------------------------------------------------------------------
PRBool nsFileWidget::AskReplace()
{
  PRBool  theReplace = PR_FALSE;
  GtkWidget *dialog;
  GtkWidget *vbox;
  GtkWidget *replace;
  GtkWidget *cancel;
  GtkWidget *label;
  GtkFileSelection *fs = GTK_FILE_SELECTION(mWidget);
  gchar *filename;
  gchar *buf;
 
  if( fs->fileop_dialog )
    return( PR_FALSE );

  filename = gtk_entry_get_text( GTK_ENTRY(fs->selection_entry));

  fs->fileop_file = filename;

  fs->fileop_dialog = dialog = gtk_dialog_new ();
  gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
              (GtkSignalFunc) file_destroy_dialog,
              (gpointer) fs);

  gtk_window_set_title (GTK_WINDOW (dialog), "Replace?");
  gtk_window_set_position( GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
  
  if( GTK_WINDOW(fs)->modal )
      gtk_window_set_modal( GTK_WINDOW(dialog), TRUE);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox,
                     FALSE, FALSE, 0);
  gtk_widget_show(vbox);

  buf = g_strconcat( "Replace file \"", filename, "\" ?", NULL);
  label = gtk_label_new(buf);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
  gtk_widget_show(label);
  g_free(buf);

  // Add the replace button to window
  replace = gtk_button_new_with_label( "Replace" );
  gtk_signal_connect( GTK_OBJECT( replace ), "clicked",
                      GTK_SIGNAL_FUNC(file_ok_clicked),
                      &theReplace);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),
                     replace, TRUE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS(replace, GTK_CAN_DEFAULT);
  gtk_widget_show(replace);

  // Add the cancel button to the window
  cancel = gtk_button_new_with_label( "Cancel" );
  gtk_signal_connect( GTK_OBJECT( cancel ), "clicked",
                      GTK_SIGNAL_FUNC(file_cancel_clicked),
                      &theReplace);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),
                     cancel, TRUE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS(cancel, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(cancel);
  gtk_widget_show(cancel);

  gtk_widget_show(dialog);

  gtk_main();
  return (PRBool)theReplace;
}

//-------------------------------------------------------------------------
//
// Set the list of filters
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsFileWidget::SetFilterList(PRUint32 aNumberOfFilters,
                                          const nsString aTitles[],
                                          const nsString aFilters[])
{
  GtkWidget *menu_item;

  mNumberOfFilters  = aNumberOfFilters;
  mTitles           = aTitles;
  mFilters          = aFilters;

  mFilterMenu = gtk_menu_new();

  for(unsigned int i=0; i < aNumberOfFilters; i++)
  {
    // we need *.{htm, html, xul, etc}
    char *foo = aTitles[i].ToNewCString();
    char *filters = aFilters[i].ToNewCString();
    printf("%20s %s\n", foo, filters);

    menu_item = gtk_menu_item_new_with_label(nsAutoCString(aTitles[i]));

    gtk_object_set_data(GTK_OBJECT(menu_item), "filters", filters);

    gtk_signal_connect(GTK_OBJECT(menu_item),
                       "activate",
                       GTK_SIGNAL_FUNC(filter_item_activated),
                       this);

    gtk_menu_append(GTK_MENU(mFilterMenu), menu_item);
    gtk_widget_show(menu_item);

    nsCRT::free(foo);
  }

  return NS_OK;
}

NS_IMETHODIMP  nsFileWidget::GetFile(nsFileSpec& aFile)
{
  if (mWidget) {
    gchar *fn = gtk_file_selection_get_filename(GTK_FILE_SELECTION(mWidget));
    if (fn && strcmp(fn,"")) {
      aFile = fn; // Put the filename into the nsFileSpec instance.
    } else {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
NS_IMETHODIMP  nsFileWidget::GetSelectedType(PRInt16& theType)
{
  theType = mSelectedType;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------
NS_IMETHODIMP  nsFileWidget::SetDefaultString(const nsString& aString)
{
  mDefault = aString;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the display directory
//
//-------------------------------------------------------------------------
NS_IMETHODIMP  nsFileWidget::SetDisplayDirectory(const nsFileSpec& aDirectory)
{
  mDisplayDirectory = aDirectory;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the display directory
//
//-------------------------------------------------------------------------
NS_IMETHODIMP  nsFileWidget::GetDisplayDirectory(nsFileSpec& aDirectory)
{
  aDirectory = mDisplayDirectory;
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_IMETHODIMP nsFileWidget::Create(nsIWidget *aParent,
                                   const nsString& aTitle,
                                   nsFileDlgMode aMode,
                                   nsIDeviceContext *aContext,
                                   nsIAppShell *aAppShell,
                                   nsIToolkit *aToolkit,
                                   void *aInitData)
{
  nsresult rv = NS_OK;
  
  mMode = aMode;
  mTitle.SetLength(0);
  mTitle.Append(aTitle);

  mWidget = gtk_file_selection_new((const gchar *)nsAutoCString(aTitle));
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "destroy",
                     GTK_SIGNAL_FUNC(DestroySignal),
                     this);

  gtk_button_box_set_layout(GTK_BUTTON_BOX(GTK_FILE_SELECTION(mWidget)->button_area), GTK_BUTTONBOX_SPREAD);

  mOptionMenu = gtk_option_menu_new();

  gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(mWidget)->main_vbox), mOptionMenu, PR_FALSE, PR_FALSE, 0);
  gtk_widget_show(mOptionMenu);


  // Hide the file column for the folder case.
  if (aMode == eMode_getfolder) {
    gtk_widget_hide((GTK_FILE_SELECTION(mWidget)->file_list)->parent);
  }

  // Create an unicode converter for file system charset
  if (nsnull == gUnicodeEncoder) {
      nsAutoString localeCharset;

      nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
      if (NS_SUCCEEDED(rv)) {
        rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName, localeCharset);
      }

      if (NS_FAILED(rv)) {
        NS_ASSERTION(0, "error getting locale charset, using ISO-8859-1");
        localeCharset.AssignWithConversion("ISO-8859-1");
        rv = NS_OK;
      }

      // get an unicode converter
      NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &rv); 
      if (NS_SUCCEEDED(rv)) {
        rv = ccm->GetUnicodeEncoder(&localeCharset, &gUnicodeEncoder);
      }
  }

  return rv;
}

gint
nsFileWidget::DestroySignal(GtkWidget *  aGtkWidget,
                            nsFileWidget* aWidget)
{
  aWidget->OnDestroySignal(aGtkWidget);
  return TRUE;
}

void
nsFileWidget::OnDestroySignal(GtkWidget* aGtkWidget)
{
  if (aGtkWidget == mWidget) {
    mWidget = nsnull;
  }
}

nsFileDlgResults nsFileWidget::GetFile(nsIWidget *aParent,
                                       const nsString &promptString,
                                       nsFileSpec &theFileSpec)
{
	Create(aParent, promptString, eMode_load, nsnull, nsnull);
	if (Show() == PR_TRUE)
	{
		GetFile(theFileSpec);
		return nsFileDlgResults_OK;
	}

  return nsFileDlgResults_Cancel;
}

nsFileDlgResults nsFileWidget::GetFolder(nsIWidget *aParent,
                                         const nsString &promptString,
                                         nsFileSpec &theFileSpec)
{
	Create(aParent, promptString, eMode_getfolder, nsnull, nsnull);
	if (Show() == PR_TRUE)
	{
		GetFile(theFileSpec);
		return nsFileDlgResults_OK;
	}

  return nsFileDlgResults_Cancel;
}

nsFileDlgResults nsFileWidget::PutFile(nsIWidget *aParent,
                                       const nsString &promptString,
                                       nsFileSpec &theFileSpec)
{
  nsFileDlgResults theResult = nsFileDlgResults_Cancel;
  
  Create(aParent, promptString, eMode_save, nsnull, nsnull);
  if (Show() == PR_TRUE) {
    GetFile(theFileSpec);

    if (theFileSpec.IsDirectory()) {
      // if the user hits ok with nothing in the filename field it returns the directory.
      theResult = nsFileDlgResults_Cancel;
    } else if (theFileSpec.Exists()) {
      PRBool result = AskReplace();
      theResult = result ? nsFileDlgResults_Replace : nsFileDlgResults_Cancel;
      // Ask for replace dialog
    } else {
      theResult = nsFileDlgResults_OK;
    }
  }
  return theResult; 
}
