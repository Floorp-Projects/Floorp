/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>
#include <stdlib.h>

#include "mozilla/ArrayUtils.h"
#include "mozilla/Services.h"

#include "MozContainer.h"
#include "nsIPrintSettings.h"
#include "nsIWidget.h"
#include "nsPrintDialogGTK.h"
#include "nsPrintSettingsGTK.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIStringBundle.h"
#include "nsIPrintSettingsService.h"
#include "nsPIDOMWindow.h"
#include "nsPrintfCString.h"
#include "nsIGIOService.h"
#include "nsServiceManagerUtils.h"
#include "WidgetUtils.h"
#include "WidgetUtilsGtk.h"
#include "nsIObserverService.h"

// for gdk_x11_window_get_xid
#include <gdk/gdk.h>
#ifdef MOZ_X11
#  include <gdk/gdkx.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gio/gunixfdlist.h>

// for dlsym
#include <dlfcn.h>
#include "MainThreadUtils.h"

using namespace mozilla;
using namespace mozilla::widget;

static const char header_footer_tags[][4] = {"", "&T", "&U", "&D", "&P", "&PT"};

#define CUSTOM_VALUE_INDEX gint(ArrayLength(header_footer_tags))

static GtkWindow* get_gtk_window_for_nsiwidget(nsIWidget* widget) {
  return GTK_WINDOW(widget->GetNativeData(NS_NATIVE_SHELLWIDGET));
}

static void ShowCustomDialog(GtkComboBox* changed_box, gpointer user_data) {
  if (gtk_combo_box_get_active(changed_box) != CUSTOM_VALUE_INDEX) {
    g_object_set_data(G_OBJECT(changed_box), "previous-active",
                      GINT_TO_POINTER(gtk_combo_box_get_active(changed_box)));
    return;
  }

  GtkWindow* printDialog = GTK_WINDOW(user_data);
  nsCOMPtr<nsIStringBundleService> bundleSvc =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);

  nsCOMPtr<nsIStringBundle> printBundle;
  bundleSvc->CreateBundle("chrome://global/locale/printdialog.properties",
                          getter_AddRefs(printBundle));
  nsAutoString intlString;

  printBundle->GetStringFromName("headerFooterCustom", intlString);
  GtkWidget* prompt_dialog = gtk_dialog_new_with_buttons(
      NS_ConvertUTF16toUTF8(intlString).get(), printDialog,
      (GtkDialogFlags)(GTK_DIALOG_MODAL), GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, nullptr);
  gtk_dialog_set_default_response(GTK_DIALOG(prompt_dialog),
                                  GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_alternative_button_order(
      GTK_DIALOG(prompt_dialog), GTK_RESPONSE_ACCEPT, GTK_RESPONSE_REJECT, -1);

  printBundle->GetStringFromName("customHeaderFooterPrompt", intlString);
  GtkWidget* custom_label =
      gtk_label_new(NS_ConvertUTF16toUTF8(intlString).get());
  GtkWidget* custom_entry = gtk_entry_new();
  GtkWidget* question_icon =
      gtk_image_new_from_stock(GTK_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);

  // To be convenient, prefill the textbox with the existing value, if any, and
  // select it all so they can easily both edit it and type in a new one.
  const char* current_text =
      (const char*)g_object_get_data(G_OBJECT(changed_box), "custom-text");
  if (current_text) {
    gtk_entry_set_text(GTK_ENTRY(custom_entry), current_text);
    gtk_editable_select_region(GTK_EDITABLE(custom_entry), 0, -1);
  }
  gtk_entry_set_activates_default(GTK_ENTRY(custom_entry), TRUE);

  GtkWidget* custom_vbox = gtk_vbox_new(TRUE, 2);
  gtk_box_pack_start(GTK_BOX(custom_vbox), custom_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(custom_vbox), custom_entry, FALSE, FALSE,
                     5);  // Make entry 5px underneath label
  GtkWidget* custom_hbox = gtk_hbox_new(FALSE, 2);
  gtk_box_pack_start(GTK_BOX(custom_hbox), question_icon, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(custom_hbox), custom_vbox, FALSE, FALSE,
                     10);  // Make question icon 10px away from content
  gtk_container_set_border_width(GTK_CONTAINER(custom_hbox), 2);
  gtk_widget_show_all(custom_hbox);

  gtk_box_pack_start(
      GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(prompt_dialog))),
      custom_hbox, FALSE, FALSE, 0);
  gint diag_response = gtk_dialog_run(GTK_DIALOG(prompt_dialog));

  if (diag_response == GTK_RESPONSE_ACCEPT) {
    const gchar* response_text = gtk_entry_get_text(GTK_ENTRY(custom_entry));
    g_object_set_data_full(G_OBJECT(changed_box), "custom-text",
                           strdup(response_text), (GDestroyNotify)free);
    g_object_set_data(G_OBJECT(changed_box), "previous-active",
                      GINT_TO_POINTER(CUSTOM_VALUE_INDEX));
  } else {
    // Go back to the previous index
    gint previous_active = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(changed_box), "previous-active"));
    gtk_combo_box_set_active(changed_box, previous_active);
  }

  gtk_widget_destroy(prompt_dialog);
}

class nsPrintDialogWidgetGTK {
 public:
  nsPrintDialogWidgetGTK(nsPIDOMWindowOuter* aParent, bool aHaveSelection,
                         nsIPrintSettings* aPrintSettings);
  ~nsPrintDialogWidgetGTK() { gtk_widget_destroy(dialog); }
  NS_ConvertUTF16toUTF8 GetUTF8FromBundle(const char* aKey);
  gint Run();

  nsresult ImportSettings(nsIPrintSettings* aNSSettings);
  nsresult ExportSettings(nsIPrintSettings* aNSSettings);

 private:
  GtkWidget* dialog;
  GtkWidget* shrink_to_fit_toggle;
  GtkWidget* print_bg_colors_toggle;
  GtkWidget* print_bg_images_toggle;
  GtkWidget* selection_only_toggle;
  GtkWidget* header_dropdown[3];  // {left, center, right}
  GtkWidget* footer_dropdown[3];

  nsCOMPtr<nsIStringBundle> printBundle;

  bool useNativeSelection;

  GtkWidget* ConstructHeaderFooterDropdown(const char16_t* currentString);
  const char* OptionWidgetToString(GtkWidget* dropdown);

  /* Code to copy between GTK and NS print settings structures.
   * In the following,
   *   "Import" means to copy from NS to GTK
   *   "Export" means to copy from GTK to NS
   */
  void ExportHeaderFooter(nsIPrintSettings* aNS);
};

nsPrintDialogWidgetGTK::nsPrintDialogWidgetGTK(nsPIDOMWindowOuter* aParent,
                                               bool aHaveSelection,
                                               nsIPrintSettings* aSettings) {
  nsCOMPtr<nsIWidget> widget = WidgetUtils::DOMWindowToWidget(aParent);
  NS_ASSERTION(widget, "Need a widget for dialog to be modal.");
  GtkWindow* gtkParent = get_gtk_window_for_nsiwidget(widget);
  NS_ASSERTION(gtkParent, "Need a GTK window for dialog to be modal.");

  nsCOMPtr<nsIStringBundleService> bundleSvc =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  bundleSvc->CreateBundle("chrome://global/locale/printdialog.properties",
                          getter_AddRefs(printBundle));

  dialog = gtk_print_unix_dialog_new(GetUTF8FromBundle("printTitleGTK").get(),
                                     gtkParent);

  gtk_print_unix_dialog_set_manual_capabilities(
      GTK_PRINT_UNIX_DIALOG(dialog),
      GtkPrintCapabilities(
          GTK_PRINT_CAPABILITY_COPIES | GTK_PRINT_CAPABILITY_COLLATE |
          GTK_PRINT_CAPABILITY_REVERSE | GTK_PRINT_CAPABILITY_SCALE |
          GTK_PRINT_CAPABILITY_GENERATE_PDF));

  // The vast majority of magic numbers in this widget construction are padding.
  // e.g. for the set_border_width below, 12px matches that of just about every
  // other window.
  GtkWidget* custom_options_tab = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(custom_options_tab), 12);
  GtkWidget* tab_label =
      gtk_label_new(GetUTF8FromBundle("optionsTabLabelGTK").get());

  // Check buttons for shrink-to-fit and print selection
  GtkWidget* check_buttons_container = gtk_vbox_new(TRUE, 2);
  shrink_to_fit_toggle = gtk_check_button_new_with_mnemonic(
      GetUTF8FromBundle("shrinkToFit").get());
  gtk_box_pack_start(GTK_BOX(check_buttons_container), shrink_to_fit_toggle,
                     FALSE, FALSE, 0);

  // GTK+2.18 and above allow us to add a "Selection" option to the main
  // settings screen, rather than adding an option on a custom tab like we must
  // do on older versions.
  if (gtk_major_version > 2 ||
      (gtk_major_version == 2 && gtk_minor_version >= 18)) {
    useNativeSelection = true;
    g_object_set(dialog, "support-selection", TRUE, "has-selection",
                 aHaveSelection, "embed-page-setup", TRUE, nullptr);
  } else {
    useNativeSelection = false;
    selection_only_toggle = gtk_check_button_new_with_mnemonic(
        GetUTF8FromBundle("selectionOnly").get());
    gtk_widget_set_sensitive(selection_only_toggle, aHaveSelection);
    gtk_box_pack_start(GTK_BOX(check_buttons_container), selection_only_toggle,
                       FALSE, FALSE, 0);
  }

  // Check buttons for printing background
  GtkWidget* appearance_buttons_container = gtk_vbox_new(TRUE, 2);
  print_bg_colors_toggle = gtk_check_button_new_with_mnemonic(
      GetUTF8FromBundle("printBGColors").get());
  print_bg_images_toggle = gtk_check_button_new_with_mnemonic(
      GetUTF8FromBundle("printBGImages").get());
  gtk_box_pack_start(GTK_BOX(appearance_buttons_container),
                     print_bg_colors_toggle, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(appearance_buttons_container),
                     print_bg_images_toggle, FALSE, FALSE, 0);

  // "Appearance" options label, bold and center-aligned
  GtkWidget* appearance_label = gtk_label_new(nullptr);
  char* pangoMarkup = g_markup_printf_escaped(
      "<b>%s</b>", GetUTF8FromBundle("printBGOptions").get());
  gtk_label_set_markup(GTK_LABEL(appearance_label), pangoMarkup);
  g_free(pangoMarkup);
  gtk_misc_set_alignment(GTK_MISC(appearance_label), 0, 0);

  GtkWidget* appearance_container = gtk_alignment_new(0, 0, 0, 0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(appearance_container), 8, 0, 12, 0);
  gtk_container_add(GTK_CONTAINER(appearance_container),
                    appearance_buttons_container);

  GtkWidget* appearance_vertical_squasher = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(appearance_vertical_squasher), appearance_label,
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(appearance_vertical_squasher),
                     appearance_container, FALSE, FALSE, 0);

  // "Header & Footer" options label, bold and center-aligned
  GtkWidget* header_footer_label = gtk_label_new(nullptr);
  pangoMarkup = g_markup_printf_escaped(
      "<b>%s</b>", GetUTF8FromBundle("headerFooter").get());
  gtk_label_set_markup(GTK_LABEL(header_footer_label), pangoMarkup);
  g_free(pangoMarkup);
  gtk_misc_set_alignment(GTK_MISC(header_footer_label), 0, 0);

  GtkWidget* header_footer_container = gtk_alignment_new(0, 0, 0, 0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(header_footer_container), 8, 0, 12,
                            0);

  // --- Table for making the header and footer options ---
  GtkWidget* header_footer_table = gtk_table_new(3, 3, FALSE);  // 3x3 table
  nsString header_footer_str[3];

  aSettings->GetHeaderStrLeft(header_footer_str[0]);
  aSettings->GetHeaderStrCenter(header_footer_str[1]);
  aSettings->GetHeaderStrRight(header_footer_str[2]);

  for (unsigned int i = 0; i < ArrayLength(header_dropdown); i++) {
    header_dropdown[i] =
        ConstructHeaderFooterDropdown(header_footer_str[i].get());
    // Those 4 magic numbers in the middle provide the position in the table.
    // The last two numbers mean 2 px padding on every side.
    gtk_table_attach(GTK_TABLE(header_footer_table), header_dropdown[i], i,
                     (i + 1), 0, 1, (GtkAttachOptions)0, (GtkAttachOptions)0, 2,
                     2);
  }

  const char labelKeys[][7] = {"left", "center", "right"};
  for (unsigned int i = 0; i < ArrayLength(labelKeys); i++) {
    gtk_table_attach(GTK_TABLE(header_footer_table),
                     gtk_label_new(GetUTF8FromBundle(labelKeys[i]).get()), i,
                     (i + 1), 1, 2, (GtkAttachOptions)0, (GtkAttachOptions)0, 2,
                     2);
  }

  aSettings->GetFooterStrLeft(header_footer_str[0]);
  aSettings->GetFooterStrCenter(header_footer_str[1]);
  aSettings->GetFooterStrRight(header_footer_str[2]);

  for (unsigned int i = 0; i < ArrayLength(footer_dropdown); i++) {
    footer_dropdown[i] =
        ConstructHeaderFooterDropdown(header_footer_str[i].get());
    gtk_table_attach(GTK_TABLE(header_footer_table), footer_dropdown[i], i,
                     (i + 1), 2, 3, (GtkAttachOptions)0, (GtkAttachOptions)0, 2,
                     2);
  }
  // ---

  gtk_container_add(GTK_CONTAINER(header_footer_container),
                    header_footer_table);

  GtkWidget* header_footer_vertical_squasher = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(header_footer_vertical_squasher),
                     header_footer_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(header_footer_vertical_squasher),
                     header_footer_container, FALSE, FALSE, 0);

  // Construction of everything
  gtk_box_pack_start(GTK_BOX(custom_options_tab), check_buttons_container,
                     FALSE, FALSE, 10);  // 10px padding
  gtk_box_pack_start(GTK_BOX(custom_options_tab), appearance_vertical_squasher,
                     FALSE, FALSE, 10);
  gtk_box_pack_start(GTK_BOX(custom_options_tab),
                     header_footer_vertical_squasher, FALSE, FALSE, 0);

  gtk_print_unix_dialog_add_custom_tab(GTK_PRINT_UNIX_DIALOG(dialog),
                                       custom_options_tab, tab_label);
  gtk_widget_show_all(custom_options_tab);
}

NS_ConvertUTF16toUTF8 nsPrintDialogWidgetGTK::GetUTF8FromBundle(
    const char* aKey) {
  nsAutoString intlString;
  printBundle->GetStringFromName(aKey, intlString);
  return NS_ConvertUTF16toUTF8(
      intlString);  // Return the actual object so we don't lose reference
}

const char* nsPrintDialogWidgetGTK::OptionWidgetToString(GtkWidget* dropdown) {
  gint index = gtk_combo_box_get_active(GTK_COMBO_BOX(dropdown));

  NS_ASSERTION(index <= CUSTOM_VALUE_INDEX,
               "Index of dropdown is higher than expected!");

  if (index == CUSTOM_VALUE_INDEX)
    return (const char*)g_object_get_data(G_OBJECT(dropdown), "custom-text");
  return header_footer_tags[index];
}

gint nsPrintDialogWidgetGTK::Run() {
  const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_hide(dialog);
  return response;
}

void nsPrintDialogWidgetGTK::ExportHeaderFooter(nsIPrintSettings* aNS) {
  const char* header_footer_str;
  header_footer_str = OptionWidgetToString(header_dropdown[0]);
  aNS->SetHeaderStrLeft(NS_ConvertUTF8toUTF16(header_footer_str));

  header_footer_str = OptionWidgetToString(header_dropdown[1]);
  aNS->SetHeaderStrCenter(NS_ConvertUTF8toUTF16(header_footer_str));

  header_footer_str = OptionWidgetToString(header_dropdown[2]);
  aNS->SetHeaderStrRight(NS_ConvertUTF8toUTF16(header_footer_str));

  header_footer_str = OptionWidgetToString(footer_dropdown[0]);
  aNS->SetFooterStrLeft(NS_ConvertUTF8toUTF16(header_footer_str));

  header_footer_str = OptionWidgetToString(footer_dropdown[1]);
  aNS->SetFooterStrCenter(NS_ConvertUTF8toUTF16(header_footer_str));

  header_footer_str = OptionWidgetToString(footer_dropdown[2]);
  aNS->SetFooterStrRight(NS_ConvertUTF8toUTF16(header_footer_str));
}

nsresult nsPrintDialogWidgetGTK::ImportSettings(nsIPrintSettings* aNSSettings) {
  MOZ_ASSERT(aNSSettings, "aSettings must not be null");
  NS_ENSURE_TRUE(aNSSettings, NS_ERROR_FAILURE);

  nsCOMPtr<nsPrintSettingsGTK> aNSSettingsGTK(do_QueryInterface(aNSSettings));
  if (!aNSSettingsGTK) return NS_ERROR_FAILURE;

  GtkPrintSettings* settings = aNSSettingsGTK->GetGtkPrintSettings();
  GtkPageSetup* setup = aNSSettingsGTK->GetGtkPageSetup();

  // Set our custom fields:

  bool geckoBool;
  aNSSettings->GetShrinkToFit(&geckoBool);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(shrink_to_fit_toggle),
                               geckoBool);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(print_bg_colors_toggle),
                               aNSSettings->GetPrintBGColors());

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(print_bg_images_toggle),
                               aNSSettings->GetPrintBGImages());

  // Temporarily set the pages-per-sheet on the GtkPrintSettings:
  int32_t pagesPerSide;
  aNSSettings->GetNumPagesPerSheet(&pagesPerSide);
  gtk_print_settings_set_number_up(settings, pagesPerSide);

  gtk_print_unix_dialog_set_settings(GTK_PRINT_UNIX_DIALOG(dialog), settings);
  gtk_print_unix_dialog_set_page_setup(GTK_PRINT_UNIX_DIALOG(dialog), setup);

  return NS_OK;
}

nsresult nsPrintDialogWidgetGTK::ExportSettings(nsIPrintSettings* aNSSettings) {
  MOZ_ASSERT(aNSSettings, "aSettings must not be null");
  NS_ENSURE_TRUE(aNSSettings, NS_ERROR_FAILURE);

  GtkPrintSettings* settings =
      gtk_print_unix_dialog_get_settings(GTK_PRINT_UNIX_DIALOG(dialog));
  GtkPageSetup* setup =
      gtk_print_unix_dialog_get_page_setup(GTK_PRINT_UNIX_DIALOG(dialog));
  GtkPrinter* printer =
      gtk_print_unix_dialog_get_selected_printer(GTK_PRINT_UNIX_DIALOG(dialog));
  if (settings && setup && printer) {
    ExportHeaderFooter(aNSSettings);

    aNSSettings->SetOutputFormat(nsIPrintSettings::kOutputFormatNative);

    // Print-to-file is true by default. This must be turned off or else
    // printing won't occur! (We manually copy the spool file when this flag is
    // set, because we love our embedders) Even if it is print-to-file in GTK's
    // case, GTK does The Right Thing when we send the job.
    aNSSettings->SetOutputDestination(
        nsIPrintSettings::kOutputDestinationPrinter);

    aNSSettings->SetShrinkToFit(
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shrink_to_fit_toggle)));

    aNSSettings->SetPrintBGColors(gtk_toggle_button_get_active(
        GTK_TOGGLE_BUTTON(print_bg_colors_toggle)));
    aNSSettings->SetPrintBGImages(gtk_toggle_button_get_active(
        GTK_TOGGLE_BUTTON(print_bg_images_toggle)));

    // Move any pages-per-sheet value from the GtkPrintSettings to our
    // nsIPrintSettings. (We handle pages-per-sheet internally and don't want
    // the native Linux printing code to doubly apply that value!)
    int32_t pagesPerSide = gtk_print_settings_get_number_up(settings);
    gtk_print_settings_set_number_up(settings, 1);
    aNSSettings->SetNumPagesPerSheet(pagesPerSide);

    // Try to save native settings in the session object
    nsCOMPtr<nsPrintSettingsGTK> aNSSettingsGTK(do_QueryInterface(aNSSettings));
    if (aNSSettingsGTK) {
      aNSSettingsGTK->SetGtkPrintSettings(settings);
      aNSSettingsGTK->SetGtkPageSetup(setup);
      aNSSettingsGTK->SetGtkPrinter(printer);
      bool printSelectionOnly;
      if (useNativeSelection) {
        _GtkPrintPages pageSetting =
            (_GtkPrintPages)gtk_print_settings_get_print_pages(settings);
        printSelectionOnly = (pageSetting == _GTK_PRINT_PAGES_SELECTION);
      } else {
        printSelectionOnly = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(selection_only_toggle));
      }
      aNSSettingsGTK->SetPrintSelectionOnly(printSelectionOnly);
    }
  }

  if (settings) g_object_unref(settings);
  return NS_OK;
}

GtkWidget* nsPrintDialogWidgetGTK::ConstructHeaderFooterDropdown(
    const char16_t* currentString) {
  GtkWidget* dropdown = gtk_combo_box_text_new();
  const char hf_options[][22] = {"headerFooterBlank", "headerFooterTitle",
                                 "headerFooterURL",   "headerFooterDate",
                                 "headerFooterPage",  "headerFooterPageTotal",
                                 "headerFooterCustom"};

  for (unsigned int i = 0; i < ArrayLength(hf_options); i++) {
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(dropdown), nullptr,
                              GetUTF8FromBundle(hf_options[i]).get());
  }

  bool shouldBeCustom = true;
  NS_ConvertUTF16toUTF8 currentStringUTF8(currentString);

  for (unsigned int i = 0; i < ArrayLength(header_footer_tags); i++) {
    if (!strcmp(currentStringUTF8.get(), header_footer_tags[i])) {
      gtk_combo_box_set_active(GTK_COMBO_BOX(dropdown), i);
      g_object_set_data(G_OBJECT(dropdown), "previous-active",
                        GINT_TO_POINTER(i));
      shouldBeCustom = false;
      break;
    }
  }

  if (shouldBeCustom) {
    gtk_combo_box_set_active(GTK_COMBO_BOX(dropdown), CUSTOM_VALUE_INDEX);
    g_object_set_data(G_OBJECT(dropdown), "previous-active",
                      GINT_TO_POINTER(CUSTOM_VALUE_INDEX));
    char* custom_string = strdup(currentStringUTF8.get());
    g_object_set_data_full(G_OBJECT(dropdown), "custom-text", custom_string,
                           (GDestroyNotify)free);
  }

  g_signal_connect(dropdown, "changed", (GCallback)ShowCustomDialog, dialog);
  return dropdown;
}

NS_IMPL_ISUPPORTS(nsPrintDialogServiceGTK, nsIPrintDialogService)

nsPrintDialogServiceGTK::nsPrintDialogServiceGTK() = default;

nsPrintDialogServiceGTK::~nsPrintDialogServiceGTK() = default;

NS_IMETHODIMP
nsPrintDialogServiceGTK::Init() { return NS_OK; }

NS_IMETHODIMP
nsPrintDialogServiceGTK::ShowPrintDialog(mozIDOMWindowProxy* aParent,
                                         bool aHaveSelection,
                                         nsIPrintSettings* aSettings) {
  MOZ_ASSERT(aParent, "aParent must not be null");
  MOZ_ASSERT(aSettings, "aSettings must not be null");

  nsPrintDialogWidgetGTK printDialog(nsPIDOMWindowOuter::From(aParent),
                                     aHaveSelection, aSettings);
  nsresult rv = printDialog.ImportSettings(aSettings);

  NS_ENSURE_SUCCESS(rv, rv);

  const gint response = printDialog.Run();

  // Handle the result
  switch (response) {
    case GTK_RESPONSE_OK:  // Proceed
      rv = printDialog.ExportSettings(aSettings);
      break;

    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_CLOSE:
    case GTK_RESPONSE_DELETE_EVENT:
    case GTK_RESPONSE_NONE:
      rv = NS_ERROR_ABORT;
      break;

    case GTK_RESPONSE_APPLY:  // Print preview
    default:
      NS_WARNING("Unexpected response");
      rv = NS_ERROR_ABORT;
  }
  return rv;
}

NS_IMETHODIMP
nsPrintDialogServiceGTK::ShowPageSetupDialog(mozIDOMWindowProxy* aParent,
                                             nsIPrintSettings* aNSSettings) {
  MOZ_ASSERT(aParent, "aParent must not be null");
  MOZ_ASSERT(aNSSettings, "aSettings must not be null");
  NS_ENSURE_TRUE(aNSSettings, NS_ERROR_FAILURE);

  nsCOMPtr<nsIWidget> widget =
      WidgetUtils::DOMWindowToWidget(nsPIDOMWindowOuter::From(aParent));
  NS_ASSERTION(widget, "Need a widget for dialog to be modal.");
  GtkWindow* gtkParent = get_gtk_window_for_nsiwidget(widget);
  NS_ASSERTION(gtkParent, "Need a GTK window for dialog to be modal.");

  nsCOMPtr<nsPrintSettingsGTK> aNSSettingsGTK(do_QueryInterface(aNSSettings));
  if (!aNSSettingsGTK) return NS_ERROR_FAILURE;

  // We need to init the prefs here because aNSSettings in its current form is a
  // dummy in both uses of the word
  nsCOMPtr<nsIPrintSettingsService> psService =
      do_GetService("@mozilla.org/gfx/printsettings-service;1");
  if (psService) {
    nsString printName;
    aNSSettings->GetPrinterName(printName);
    if (printName.IsVoid()) {
      psService->GetLastUsedPrinterName(printName);
      aNSSettings->SetPrinterName(printName);
    }
    psService->InitPrintSettingsFromPrefs(aNSSettings, true,
                                          nsIPrintSettings::kInitSaveAll);
  }

  // Frustratingly, gtk_print_run_page_setup_dialog doesn't tell us whether
  // the user cancelled or confirmed the dialog! So to avoid needlessly
  // refreshing the preview when Page Setup was cancelled, we compare the
  // serializations of old and new settings; if they're the same, bail out.
  GtkPrintSettings* gtkSettings = aNSSettingsGTK->GetGtkPrintSettings();
  GtkPageSetup* oldPageSetup = aNSSettingsGTK->GetGtkPageSetup();
  GKeyFile* oldKeyFile = g_key_file_new();
  gtk_page_setup_to_key_file(oldPageSetup, oldKeyFile, nullptr);
  gsize oldLength;
  gchar* oldData = g_key_file_to_data(oldKeyFile, &oldLength, nullptr);
  g_key_file_free(oldKeyFile);

  GtkPageSetup* newPageSetup =
      gtk_print_run_page_setup_dialog(gtkParent, oldPageSetup, gtkSettings);

  GKeyFile* newKeyFile = g_key_file_new();
  gtk_page_setup_to_key_file(newPageSetup, newKeyFile, nullptr);
  gsize newLength;
  gchar* newData = g_key_file_to_data(newKeyFile, &newLength, nullptr);
  g_key_file_free(newKeyFile);

  bool unchanged =
      (oldLength == newLength && !memcmp(oldData, newData, oldLength));
  g_free(oldData);
  g_free(newData);
  if (unchanged) {
    g_object_unref(newPageSetup);
    return NS_ERROR_ABORT;
  }

  aNSSettingsGTK->SetGtkPageSetup(newPageSetup);

  // Now newPageSetup has a refcount of 2 (SetGtkPageSetup will addref), put it
  // to 1 so if this gets replaced we don't leak.
  g_object_unref(newPageSetup);

  if (psService)
    psService->MaybeSavePrintSettingsToPrefs(
        aNSSettings, nsIPrintSettings::kInitSaveOrientation |
                         nsIPrintSettings::kInitSavePaperSize |
                         nsIPrintSettings::kInitSaveUnwriteableMargins);

  return NS_OK;
}
