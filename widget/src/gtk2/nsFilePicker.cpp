/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla GTK2 File Chooser.
 *
 * The Initial Developer of the Original Code is Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Aillon <caillon@redhat.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <gtk/gtk.h>

#include "nsIFileURL.h"
#include "nsIURI.h"
#include "nsIWidget.h"
#include "nsILocalFile.h"
#include "nsIStringBundle.h"

#include "nsArrayEnumerator.h"
#include "nsMemory.h"
#include "nsEnumeratorUtils.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "mozcontainer.h"

#include "prmem.h"
#include "prlink.h"

#include "nsFilePicker.h"
#include "nsAccessibilityHelper.h"

#ifdef MOZ_PLATFORM_MAEMO
#include <hildon-fm-2/hildon/hildon-file-chooser-dialog.h>
#endif

#define MAX_PREVIEW_SIZE 180

nsILocalFile *nsFilePicker::mPrevDisplayDirectory = nsnull;

// XXXdholbert -- this function is duplicated in nsPrintDialogGTK.cpp
// and needs to be unified in some generic utility class.
static GtkWindow *
get_gtk_window_for_nsiwidget(nsIWidget *widget)
{
  // Get native GdkWindow
  GdkWindow *gdk_win = GDK_WINDOW(widget->GetNativeData(NS_NATIVE_WIDGET));
  if (!gdk_win)
    return NULL;

  // Get the container
  gpointer user_data = NULL;
  gdk_window_get_user_data(gdk_win, &user_data);
  if (!user_data)
    return NULL;

  // Make sure its really a container
  MozContainer *parent_container = MOZ_CONTAINER(user_data);
  if (!parent_container)
    return NULL;

  // Get its toplevel
  return GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(parent_container)));
}

void
nsFilePicker::Shutdown()
{
  NS_IF_RELEASE(mPrevDisplayDirectory);
}

static GtkFileChooserAction
GetGtkFileChooserAction(PRInt16 aMode)
{
  GtkFileChooserAction action;

  switch (aMode) {
    case nsIFilePicker::modeSave:
    action = GTK_FILE_CHOOSER_ACTION_SAVE;
    break;

    case nsIFilePicker::modeGetFolder:
    action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
    break;

    case nsIFilePicker::modeOpen:
    case nsIFilePicker::modeOpenMultiple:
    action = GTK_FILE_CHOOSER_ACTION_OPEN;
    break;

    default:
    NS_WARNING("Unknown nsIFilePicker mode");
    action = GTK_FILE_CHOOSER_ACTION_OPEN;
    break;
  }

  return action;
}


static void
UpdateFilePreviewWidget(GtkFileChooser *file_chooser,
                        gpointer preview_widget_voidptr)
{
  GtkImage *preview_widget = GTK_IMAGE(preview_widget_voidptr);
  char *image_filename = gtk_file_chooser_get_preview_filename(file_chooser);

  if (!image_filename) {
    gtk_file_chooser_set_preview_widget_active(file_chooser, FALSE);
    return;
  }

  // We do this so GTK scales down images that are too big, but not scale up images that are too small
  GdkPixbuf *preview_pixbuf = gdk_pixbuf_new_from_file(image_filename, NULL);
  if (!preview_pixbuf) {
    g_free(image_filename);
    gtk_file_chooser_set_preview_widget_active(file_chooser, FALSE);
    return;
  }
  if (gdk_pixbuf_get_width(preview_pixbuf) > MAX_PREVIEW_SIZE || gdk_pixbuf_get_height(preview_pixbuf) > MAX_PREVIEW_SIZE) {
    g_object_unref(preview_pixbuf);
    preview_pixbuf = gdk_pixbuf_new_from_file_at_size(image_filename, MAX_PREVIEW_SIZE, MAX_PREVIEW_SIZE, NULL);
  }

  g_free(image_filename);
  if (!preview_pixbuf) {
    gtk_file_chooser_set_preview_widget_active(file_chooser, FALSE);
    return;
  }

  // This is the easiest way to do center alignment without worrying about containers
  // Minimum 3px padding each side (hence the 6) just to make things nice
  gint x_padding = (MAX_PREVIEW_SIZE + 6 - gdk_pixbuf_get_width(preview_pixbuf)) / 2;
  gtk_misc_set_padding(GTK_MISC(preview_widget), x_padding, 0);

  gtk_image_set_from_pixbuf(preview_widget, preview_pixbuf);
  g_object_unref(preview_pixbuf);
  gtk_file_chooser_set_preview_widget_active(file_chooser, TRUE);
}

static nsCAutoString
MakeCaseInsensitiveShellGlob(const char* aPattern) {
  // aPattern is UTF8
  nsCAutoString result;
  unsigned int len = strlen(aPattern);

  for (unsigned int i = 0; i < len; i++) {
    if (!g_ascii_isalpha(aPattern[i])) {
      // non-ASCII characters will also trigger this path, so unicode
      // is safely handled albeit case-sensitively
      result.Append(aPattern[i]);
      continue;
    }

    // add the lowercase and uppercase version of a character to a bracket
    // match, so it matches either the lowercase or uppercase char.
    result.Append('[');
    result.Append(g_ascii_tolower(aPattern[i]));
    result.Append(g_ascii_toupper(aPattern[i]));
    result.Append(']');

  }

  return result;
}

NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

nsFilePicker::nsFilePicker()
  : mMode(nsIFilePicker::modeOpen),
    mSelectedType(0)
{
}

nsFilePicker::~nsFilePicker()
{
}

void
ReadMultipleFiles(gpointer filename, gpointer array)
{
  nsCOMPtr<nsILocalFile> localfile;
  nsresult rv = NS_NewNativeLocalFile(nsDependentCString(static_cast<char*>(filename)),
                                      PR_FALSE,
                                      getter_AddRefs(localfile));
  if (NS_SUCCEEDED(rv)) {
    nsCOMArray<nsILocalFile>& files = *static_cast<nsCOMArray<nsILocalFile>*>(array);
    files.AppendObject(localfile);
  }

  g_free(filename);
}

void
nsFilePicker::ReadValuesFromFileChooser(GtkWidget *file_chooser)
{
  mFiles.Clear();

  if (mMode == nsIFilePicker::modeOpenMultiple) {
    mFileURL.Truncate();

    GSList *list = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(file_chooser));
    g_slist_foreach(list, ReadMultipleFiles, static_cast<gpointer>(&mFiles));
    g_slist_free(list);
  } else {
    gchar *filename = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(file_chooser));
    mFileURL.Assign(filename);
    g_free(filename);
  }

  GtkFileFilter *filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(file_chooser));
  GSList *filter_list = gtk_file_chooser_list_filters(GTK_FILE_CHOOSER(file_chooser));

  mSelectedType = static_cast<PRInt16>(g_slist_index(filter_list, filter));
  g_slist_free(filter_list);

  // Remember last used directory.
  nsCOMPtr<nsILocalFile> file;
  GetFile(getter_AddRefs(file));
  if (file) {
    nsCOMPtr<nsIFile> dir;
    file->GetParent(getter_AddRefs(dir));
    nsCOMPtr<nsILocalFile> localDir(do_QueryInterface(dir));
    if (localDir) {
      localDir.swap(mPrevDisplayDirectory);
    }
  }
}

void
nsFilePicker::InitNative(nsIWidget *aParent,
                         const nsAString& aTitle,
                         PRInt16 aMode)
{
  mParentWidget = aParent;
  mTitle.Assign(aTitle);
  mMode = aMode;
}

NS_IMETHODIMP
nsFilePicker::AppendFilters(PRInt32 aFilterMask)
{
  mAllowURLs = !!(aFilterMask & filterAllowURLs);
  return nsBaseFilePicker::AppendFilters(aFilterMask);
}

NS_IMETHODIMP
nsFilePicker::AppendFilter(const nsAString& aTitle, const nsAString& aFilter)
{
  if (aFilter.EqualsLiteral("..apps")) {
    // No platform specific thing we can do here, really....
    return NS_OK;
  }

  nsCAutoString filter, name;
  CopyUTF16toUTF8(aFilter, filter);
  CopyUTF16toUTF8(aTitle, name);

  mFilters.AppendElement(filter);
  mFilterNames.AppendElement(name);

  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::SetDefaultString(const nsAString& aString)
{
  mDefault = aString;

  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::GetDefaultString(nsAString& aString)
{
  // Per API...
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFilePicker::SetDefaultExtension(const nsAString& aExtension)
{
  mDefaultExtension = aExtension;

  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::GetDefaultExtension(nsAString& aExtension)
{
  aExtension = mDefaultExtension;

  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::GetFilterIndex(PRInt32 *aFilterIndex)
{
  *aFilterIndex = mSelectedType;

  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::SetFilterIndex(PRInt32 aFilterIndex)
{
  mSelectedType = aFilterIndex;

  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::GetFile(nsILocalFile **aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);

  *aFile = nsnull;
  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetFileURL(getter_AddRefs(uri));
  if (!uri)
    return rv;

  nsCOMPtr<nsIFileURL> fileURL(do_QueryInterface(uri, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> file;
  rv = fileURL->GetFile(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(file, aFile);
}

NS_IMETHODIMP
nsFilePicker::GetFileURL(nsIURI **aFileURL)
{
  *aFileURL = nsnull;
  return NS_NewURI(aFileURL, mFileURL);
}

NS_IMETHODIMP
nsFilePicker::GetFiles(nsISimpleEnumerator **aFiles)
{
  NS_ENSURE_ARG_POINTER(aFiles);

  if (mMode == nsIFilePicker::modeOpenMultiple) {
    return NS_NewArrayEnumerator(aFiles, mFiles);
  }

  return NS_ERROR_FAILURE;
}

PRBool
confirm_overwrite_file(GtkWidget *parent, nsILocalFile* file)
{
  nsCOMPtr<nsIStringBundleService> sbs = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv = sbs->CreateBundle("chrome://global/locale/filepicker.properties",
                                  getter_AddRefs(bundle));
  if (NS_FAILED(rv)) {
    return PR_FALSE;
  }

  nsAutoString leafName;
  file->GetLeafName(leafName);
  const PRUnichar *formatStrings[] =
  {
    leafName.get()
  };

  nsXPIDLString title, message;
  bundle->GetStringFromName(NS_LITERAL_STRING("confirmTitle").get(),
                            getter_Copies(title));
  bundle->FormatStringFromName(NS_LITERAL_STRING("confirmFileReplacing").get(),
                               formatStrings, NS_ARRAY_LENGTH(formatStrings),
                               getter_Copies(message));

  GtkWindow *parent_window = GTK_WINDOW(parent);
  GtkWidget *dialog;
  
  dialog = gtk_message_dialog_new(parent_window,
                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_QUESTION,
                                  GTK_BUTTONS_YES_NO,
                                  "%s", NS_ConvertUTF16toUTF8(message).get());
  gtk_window_set_title(GTK_WINDOW(dialog), NS_ConvertUTF16toUTF8(title).get());
  if (parent_window && parent_window->group) {
    gtk_window_group_add_window(parent_window->group, GTK_WINDOW(dialog));
  }

  PRBool result = (RunDialog(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES);

  gtk_widget_destroy(dialog);

  return result;
}

NS_IMETHODIMP
nsFilePicker::Show(PRInt16 *aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  nsXPIDLCString title;
  title.Adopt(ToNewUTF8String(mTitle));

  GtkWindow *parent_widget = get_gtk_window_for_nsiwidget(mParentWidget);

  GtkFileChooserAction action = GetGtkFileChooserAction(mMode);
  const gchar *accept_button = (action == GTK_FILE_CHOOSER_ACTION_SAVE)
                               ? GTK_STOCK_SAVE : GTK_STOCK_OPEN;
#ifdef MOZ_PLATFORM_MAEMO
  GtkWidget *file_chooser =
    hildon_file_chooser_dialog_new_with_properties(parent_widget,
                                                   "action", action,
                                                   "open-button-text", accept_button,
                                                   NULL);
  gtk_window_set_title(GTK_WINDOW(file_chooser), title);
#else
  GtkWidget *file_chooser =
      gtk_file_chooser_dialog_new(title, parent_widget, action,
                                  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                  accept_button, GTK_RESPONSE_ACCEPT,
                                  NULL);
  gtk_dialog_set_alternative_button_order(GTK_DIALOG(file_chooser),
                                          GTK_RESPONSE_ACCEPT,
                                          GTK_RESPONSE_CANCEL,
                                          -1);
  if (mAllowURLs) {
    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(file_chooser), FALSE);
  }
#endif

  if (action == GTK_FILE_CHOOSER_ACTION_OPEN || action == GTK_FILE_CHOOSER_ACTION_SAVE) {
    GtkWidget *img_preview = gtk_image_new();
    gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(file_chooser), img_preview);
    g_signal_connect(file_chooser, "update-preview", G_CALLBACK(UpdateFilePreviewWidget), img_preview);
  }

  if (parent_widget && parent_widget->group) {
    gtk_window_group_add_window(parent_widget->group, GTK_WINDOW(file_chooser));
  }

  NS_ConvertUTF16toUTF8 defaultName(mDefault);
  switch (mMode) {
    case nsIFilePicker::modeOpenMultiple:
      gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(file_chooser), TRUE);
      break;
    case nsIFilePicker::modeSave:
      gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(file_chooser),
                                        defaultName.get());
      break;
  }

  nsCOMPtr<nsIFile> defaultPath;
  if (mDisplayDirectory) {
    mDisplayDirectory->Clone(getter_AddRefs(defaultPath));
  } else if (mPrevDisplayDirectory) {
    mPrevDisplayDirectory->Clone(getter_AddRefs(defaultPath));
  }

  if (defaultPath) {
    if (!defaultName.IsEmpty() && mMode != nsIFilePicker::modeSave) {
      // Try to select the intended file. Even if it doesn't exist, GTK still switches
      // directories.
      defaultPath->AppendNative(defaultName);
      nsCAutoString path;
      defaultPath->GetNativePath(path);
      gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_chooser), path.get());
    } else {
      nsCAutoString directory;
      defaultPath->GetNativePath(directory);
      gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(file_chooser),
                                          directory.get());
    }
  }

  gtk_dialog_set_default_response(GTK_DIALOG(file_chooser), GTK_RESPONSE_ACCEPT);

  PRInt32 count = mFilters.Length();
  for (PRInt32 i = 0; i < count; ++i) {
    // This is fun... the GTK file picker does not accept a list of filters
    // so we need to split out each string, and add it manually.

    char **patterns = g_strsplit(mFilters[i].get(), ";", -1);
    if (!patterns) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    GtkFileFilter *filter = gtk_file_filter_new();
    for (int j = 0; patterns[j] != NULL; ++j) {
      nsCAutoString caseInsensitiveFilter = MakeCaseInsensitiveShellGlob(g_strstrip(patterns[j]));
      gtk_file_filter_add_pattern(filter, caseInsensitiveFilter.get());
    }

    g_strfreev(patterns);

    if (!mFilterNames[i].IsEmpty()) {
      // If we have a name for our filter, let's use that.
      const char *filter_name = mFilterNames[i].get();
      gtk_file_filter_set_name(filter, filter_name);
    } else {
      // If we don't have a name, let's just use the filter pattern.
      const char *filter_pattern = mFilters[i].get();
      gtk_file_filter_set_name(filter, filter_pattern);
    }

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_chooser), filter);

    // Set the initially selected filter
    if (mSelectedType == i) {
      gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(file_chooser), filter);
    }
  }

  gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(file_chooser), PR_TRUE);
  gint response = RunDialog(GTK_DIALOG(file_chooser));

  switch (response) {
    case GTK_RESPONSE_OK:
    case GTK_RESPONSE_ACCEPT:
    ReadValuesFromFileChooser(file_chooser);
    *aReturn = nsIFilePicker::returnOK;
    if (mMode == nsIFilePicker::modeSave) {
      nsCOMPtr<nsILocalFile> file;
      GetFile(getter_AddRefs(file));
      if (file) {
        PRBool exists = PR_FALSE;
        file->Exists(&exists);
        if (exists)
          *aReturn = nsIFilePicker::returnReplace;
      }
    }
    break;

    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_CLOSE:
    case GTK_RESPONSE_DELETE_EVENT:
    *aReturn = nsIFilePicker::returnCancel;
    break;

    default:
    NS_WARNING("Unexpected response");
    *aReturn = nsIFilePicker::returnCancel;
    break;
  }

  gtk_widget_destroy(file_chooser);

  return NS_OK;
}
