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

#include <gtk/gtkwindow.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkstock.h>

#include "nsIFileURL.h"
#include "nsIURI.h"
#include "nsIWidget.h"
#include "nsILocalFile.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsMemory.h"
#include "nsEnumeratorUtils.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"

#include "prmem.h"
#include "prlink.h"

#include "nsFilePicker.h"

/* from nsFilePicker.js */
#define XULFILEPICKER_CID \
  { 0x54ae32f8, 0x1dd2, 0x11b2, \
    { 0xa2, 0x09, 0xdf, 0x7c, 0x50, 0x53, 0x70, 0xf8} }
static NS_DEFINE_CID(kXULFilePickerCID, XULFILEPICKER_CID);

// XXX total ass.  We should really impose a build-time requirement on gtk2.4
// and then check at run-time whether the user is actually running gtk2.4.
// We should then decide to load the component (or not) from the component mgr.
// We don't really have a mechanism to do that, though....

typedef struct GtkFileChooser;
typedef struct GtkFileFilter;

/* Copied from gtkfilechooser.h */
typedef enum
{
  GTK_FILE_CHOOSER_ACTION_OPEN,
  GTK_FILE_CHOOSER_ACTION_SAVE,
  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
  GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER
} GtkFileChooserAction;


typedef gchar* (*_gtk_file_chooser_get_filename_fn)(GtkFileChooser *chooser);
typedef GtkWidget* (*_gtk_file_chooser_dialog_new_fn)(const gchar *title,
                                                      GtkWindow *parent,
                                                      GtkFileChooserAction action,
                                                      const gchar *first_button_text,
                                                      gint first_button_response,
                                                      const gchar *second_button_text,
                                                      gint second_button_response,
                                                      const gchar *third_button_text /* NULL */);
typedef void (*_gtk_file_chooser_set_select_multiple_fn)(GtkFileChooser* chooser, gboolean truth);
typedef void (*_gtk_file_chooser_set_current_name_fn)(GtkFileChooser* chooser, const gchar* name);
typedef void (*_gtk_file_chooser_add_filter_fn)(GtkFileChooser* chooser, GtkFileFilter* filter);
typedef GtkFileFilter* (*_gtk_file_filter_new_fn)();
typedef void (*_gtk_file_filter_add_pattern_fn)(GtkFileFilter* filter, const gchar* pattern);
typedef void (*_gtk_file_filter_set_name_fn)(GtkFileFilter* filter, const gchar* name);

#define DECL_FUNC_PTR(func) static _##func##_fn _##func

DECL_FUNC_PTR(gtk_file_chooser_get_filename);
DECL_FUNC_PTR(gtk_file_chooser_dialog_new);
DECL_FUNC_PTR(gtk_file_chooser_set_select_multiple);
DECL_FUNC_PTR(gtk_file_chooser_set_current_name);
DECL_FUNC_PTR(gtk_file_chooser_add_filter);
DECL_FUNC_PTR(gtk_file_filter_new);
DECL_FUNC_PTR(gtk_file_filter_add_pattern);
DECL_FUNC_PTR(gtk_file_filter_set_name);


static PRLibrary *
LoadVersionedLibrary(const char* libName, const char* libVersion)
{
  char *platformLibName = PR_GetLibraryName(nsnull, libName);
  nsCAutoString versionLibName(platformLibName);
  versionLibName.Append(libVersion);
  PR_FreeLibraryName(platformLibName);
  return PR_LoadLibrary(versionLibName.get());
}

static nsresult
InitGTK24()
{
  static PRLibrary *libgtk24 = nsnull;

  #define GET_LIBGTK_FUNC(func) \
    PR_BEGIN_MACRO \
    _##func = (_##func##_fn) PR_FindFunctionSymbol(libgtk24, #func); \
    if (!_##func) { \
      NS_WARNING("Can't load ##func##"); \
      return NS_ERROR_NOT_AVAILABLE; \
    } \
    PR_END_MACRO

  // This is easier
  PRFuncPtr func = PR_FindFunctionSymbolAndLibrary("gtk_file_chooser_get_filename",
                                                   &libgtk24);
  if (libgtk24) {
    _gtk_file_chooser_get_filename = (_gtk_file_chooser_get_filename_fn)func;
  } else {
    // XXX hmm, this seems to fail when gtk 2.4 is already loaded...
    libgtk24 = LoadVersionedLibrary("gtk-2", ".4");
    if (!libgtk24) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    GET_LIBGTK_FUNC(gtk_file_chooser_get_filename);
  }

  GET_LIBGTK_FUNC(gtk_file_chooser_dialog_new);
  GET_LIBGTK_FUNC(gtk_file_chooser_set_select_multiple);
  GET_LIBGTK_FUNC(gtk_file_chooser_set_current_name);
  GET_LIBGTK_FUNC(gtk_file_chooser_add_filter);
  GET_LIBGTK_FUNC(gtk_file_filter_new);
  GET_LIBGTK_FUNC(gtk_file_filter_add_pattern);
  GET_LIBGTK_FUNC(gtk_file_filter_set_name);

  // Woot.
  return NS_OK;
}

// Ok, lib loading crap is done... the real code starts here:

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

    default:
    NS_WARNING("Unknown nsIFilePicker mode");
    // fall through

    case nsIFilePicker::modeOpen:
    case nsIFilePicker::modeOpenMultiple:
    action = GTK_FILE_CHOOSER_ACTION_OPEN;
    break;
  }

  return action;
}

nsString nsFilePicker::mLastUsedUnicodeDirectory;

//NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

NS_IMPL_ADDREF(nsFilePicker)
NS_IMPL_RELEASE(nsFilePicker)

NS_INTERFACE_MAP_BEGIN(nsFilePicker)
  if (mXULPicker) {
    return mXULPicker->QueryInterface(aIID, aInstancePtr);
  }
  NS_INTERFACE_MAP_ENTRY(nsIFilePicker)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIFilePicker)
NS_INTERFACE_MAP_END

nsFilePicker::nsFilePicker()
  : mMode(nsIFilePicker::modeOpen),
    mSelectedType(0)
{
}


nsFilePicker::~nsFilePicker()
{
}

void
nsFilePicker::ReadValuesFromFileChooser(GtkWidget *file_chooser)
{
  // Grab the filename
  gchar *filename = _gtk_file_chooser_get_filename ((GtkFileChooser*) (file_chooser));
  mFile.Assign(filename);
  g_free(filename);

  // Remember last used directory.
  nsCOMPtr<nsILocalFile> file;
  GetFile(getter_AddRefs(file));
  if (file) {
    nsCOMPtr<nsIFile> dir;
    file->GetParent(getter_AddRefs(dir));
    nsCOMPtr<nsILocalFile> localDir(do_QueryInterface(dir));
    if (localDir) {
      mDisplayDirectory = localDir;
    }
  }
}

NS_IMETHODIMP
nsFilePicker::Init(nsIDOMWindow *aParent, const nsAString &aTitle, PRInt16 aMode)
{
  // The GtkFileChooser API was first exposed in GTK+ 2.4.  Make sure we have it,
  // else fall back to the XUL picker.
  nsresult rv = InitGTK24();
  if (NS_FAILED(rv)) {
    NS_WARNING("Couldn't load GTK+ 2.4 -- falling back to XUL file chooser.");
    mXULPicker = do_CreateInstance(kXULFilePickerCID);
    if (!mXULPicker) {
      NS_WARNING("Couldn't load XUL file chooser.  Running in circles!");
      return NS_ERROR_NOT_AVAILABLE;
    }

    return mXULPicker->Init(aParent, aTitle, aMode);
  }

  nsIWidget *parent = DOMWindowToWidget(aParent);
  NS_ENSURE_TRUE(parent, NS_ERROR_FAILURE);

  InitNative(parent, aTitle, aMode);

  return NS_OK;
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
  if (mXULPicker) {
    return mXULPicker->AppendFilters(aFilterMask);
  }

  return nsBaseFilePicker::AppendFilters(aFilterMask);
}

NS_IMETHODIMP
nsFilePicker::AppendFilter(const nsAString& aTitle, const nsAString& aFilter)
{
  if (mXULPicker) {
    return mXULPicker->AppendFilter(aTitle, aFilter);
  }

  if (aFilter.Equals(NS_LITERAL_STRING("..apps"))) {
    // No platform specific thing we can do here, really....
    return NS_OK;
  }

  nsCAutoString filter, name;
  CopyUTF16toUTF8(aFilter, filter);
  CopyUTF16toUTF8(aTitle, name);

  mFilters.AppendCString(filter);
  mFilterNames.AppendCString(name);

  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::SetDefaultString(const nsAString& aString)
{
  if (mXULPicker) {
    return mXULPicker->SetDefaultString(aString);
  }

  mDefault = aString;

  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::GetDefaultString(nsAString& aString)
{
  if (mXULPicker) {
    return mXULPicker->GetDefaultString(aString);
  }

  // Per API...
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFilePicker::SetDefaultExtension(const nsAString& aExtension)
{
  if (mXULPicker) {
    return mXULPicker->SetDefaultExtension(aExtension);
  }

  mDefaultExtension = aExtension;

  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::GetDefaultExtension(nsAString& aExtension)
{
  if (mXULPicker) {
    return mXULPicker->GetDefaultExtension(aExtension);
  }

  aExtension = mDefaultExtension;

  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::GetFilterIndex(PRInt32 *aFilterIndex)
{
  if (mXULPicker) {
    return mXULPicker->GetFilterIndex(aFilterIndex);
  }

  *aFilterIndex = mSelectedType;

  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::SetFilterIndex(PRInt32 aFilterIndex)
{
  if (mXULPicker) {
    return mXULPicker->SetFilterIndex(aFilterIndex);
  }

  mSelectedType = aFilterIndex;

  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::SetDisplayDirectory(nsILocalFile *aDirectory)
{
  if (mXULPicker) {
    return mXULPicker->SetDisplayDirectory(aDirectory);
  }

  mDisplayDirectory = aDirectory;

  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::GetDisplayDirectory(nsILocalFile **aDirectory)
{
  if (mXULPicker) {
    return mXULPicker->GetDisplayDirectory(aDirectory);
  }

  NS_IF_ADDREF(*aDirectory = mDisplayDirectory);

  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::GetFile(nsILocalFile **aFile)
{
  if (mXULPicker) {
    return mXULPicker->GetFile(aFile);
  }

  NS_ENSURE_ARG_POINTER(aFile);

  *aFile = nsnull;
  if (mFile.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  file->InitWithNativePath(mFile);

  NS_ADDREF(*aFile = file);

  return NS_OK;
}

NS_IMETHODIMP
nsFilePicker::GetFileURL(nsIFileURL **aFileURL)
{
  if (mXULPicker) {
    return mXULPicker->GetFileURL(aFileURL);
  }

  nsCOMPtr<nsILocalFile> file;
  GetFile(getter_AddRefs(file));

  nsCOMPtr<nsIURI> uri;
  NS_NewFileURI(getter_AddRefs(uri), file);
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

  return CallQueryInterface(uri, aFileURL);
}

NS_IMETHODIMP
nsFilePicker::GetFiles(nsISimpleEnumerator **aFiles)
{
  if (mXULPicker) {
    return mXULPicker->GetFiles(aFiles);
  }

  NS_ENSURE_ARG_POINTER(aFiles);

  if (mMode == nsIFilePicker::modeOpenMultiple) {
    return NS_NewArrayEnumerator(aFiles, mFiles);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFilePicker::Show(PRInt16 *aReturn)
{
  if (mXULPicker) {
    return mXULPicker->Show(aReturn);
  }

  NS_ENSURE_ARG_POINTER(aReturn);

  nsXPIDLCString title;
  title.Adopt(ToNewUTF8String(mTitle));

  GtkWidget *parent = (GtkWidget*)mParentWidget->GetNativeData(NS_NATIVE_WIDGET);
  GtkFileChooserAction action = GetGtkFileChooserAction(mMode);
  const gchar *accept_button = (mMode == GTK_FILE_CHOOSER_ACTION_SAVE)
                               ? GTK_STOCK_SAVE : GTK_STOCK_OPEN;

  GtkWidget *file_chooser =
      _gtk_file_chooser_dialog_new(title, GTK_WINDOW(parent), action,
                                   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                   accept_button, GTK_RESPONSE_ACCEPT,
                                   NULL);
  if (mMode == nsIFilePicker::modeOpenMultiple) {
    _gtk_file_chooser_set_select_multiple ((GtkFileChooser*) (file_chooser), TRUE);
  }

  if (mMode == nsIFilePicker::modeSave) {
    char *default_filename = ToNewUTF8String(mDefault);
    _gtk_file_chooser_set_current_name((GtkFileChooser*) (file_chooser),
                                       NS_STATIC_CAST(const gchar*, default_filename));
    nsMemory::Free(default_filename);
  }

  PRInt32 count = mFilters.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    GtkFileFilter *filter = _gtk_file_filter_new ();

    // This is fun... the GTK file picker does not accept a list of filters
    // so we need to split out each string, and add it manually.
    nsCAutoString pattern_list;
    pattern_list.Assign(*mFilters[i]);
    nsCString::const_iterator start, end, cur;
    pattern_list.BeginReading(start);
    pattern_list.EndReading(end);

    cur = start;
    while (cur != end) {
      // Look for our delimiter of '; ' (semi-colon space)
      if (*cur == ';') {
        ++cur;
        if (*cur != ' ') {
          NS_ERROR("The supplied filter is invalid.  Go read nsIFilePicker.");
          #ifdef DEBUG
          printf("The ill eagle filter: %s\n", mFilters[i]->get());
          #endif
          break;
        }
        --cur;
        const char* pattern = ToNewCString(Substring(start, cur));
        _gtk_file_filter_add_pattern(filter, pattern);
        printf("%s\n", pattern);
        nsMemory::Free((void*)pattern);
        ++cur; ++cur;
        start = cur;
      }

      ++cur;
    }

    // Finally add the last one
    const char *pattern = ToNewCString(Substring(start, end));
    _gtk_file_filter_add_pattern(filter, pattern);
    nsMemory::Free((void*)pattern);

    if (!mFilterNames[i]->IsEmpty()) {
      // If we have a name for our filter, let's use that.
      const char *filter_name = mFilterNames[i]->get();
      _gtk_file_filter_set_name (filter, filter_name);
    } else {
      // If we don't have a name, let's just use the filter pattern.
      const char *filter_pattern = mFilters[i]->get();
      _gtk_file_filter_set_name (filter, filter_pattern);
    }

    _gtk_file_chooser_add_filter ((GtkFileChooser*) (file_chooser), filter);
  }

  gint response = gtk_dialog_run (GTK_DIALOG (file_chooser));

  switch (response) {
    case GTK_RESPONSE_ACCEPT:
    ReadValuesFromFileChooser(file_chooser);
    *aReturn = nsIFilePicker::returnOK;
    if (mMode == modeSave) {
      nsCOMPtr<nsILocalFile> file;
      GetFile(getter_AddRefs(file));
      if (file) {
        PRBool exists = PR_FALSE;
        file->Exists(&exists);
        if (exists) {
          *aReturn = nsIFilePicker::returnReplace;
        }
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
