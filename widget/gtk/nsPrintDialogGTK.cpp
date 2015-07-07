/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>
#include <stdlib.h>

#include "mozilla/ArrayUtils.h"

#include "mozcontainer.h"
#include "nsIPrintSettings.h"
#include "nsIWidget.h"
#include "nsPrintDialogGTK.h"
#include "nsPrintSettingsGTK.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIFile.h"
#include "nsIStringBundle.h"
#include "nsIPrintSettingsService.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIBaseWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShell.h"
#include "WidgetUtils.h"

using namespace mozilla;
using namespace mozilla::widget;

static const char header_footer_tags[][4] =  {"", "&T", "&U", "&D", "&P", "&PT"};

#define CUSTOM_VALUE_INDEX gint(ArrayLength(header_footer_tags))

static GtkWindow *
get_gtk_window_for_nsiwidget(nsIWidget *widget)
{
  return GTK_WINDOW(widget->GetNativeData(NS_NATIVE_SHELLWIDGET));
}

static void
ShowCustomDialog(GtkComboBox *changed_box, gpointer user_data)
{
  if (gtk_combo_box_get_active(changed_box) != CUSTOM_VALUE_INDEX) {
    g_object_set_data(G_OBJECT(changed_box), "previous-active", GINT_TO_POINTER(gtk_combo_box_get_active(changed_box)));
    return;
  }

  GtkWindow* printDialog = GTK_WINDOW(user_data);
  nsCOMPtr<nsIStringBundleService> bundleSvc =
       do_GetService(NS_STRINGBUNDLE_CONTRACTID);

  nsCOMPtr<nsIStringBundle> printBundle;
  bundleSvc->CreateBundle("chrome://global/locale/printdialog.properties", getter_AddRefs(printBundle));
  nsXPIDLString intlString;

  printBundle->GetStringFromName(MOZ_UTF16("headerFooterCustom"), getter_Copies(intlString));
  GtkWidget* prompt_dialog = gtk_dialog_new_with_buttons(NS_ConvertUTF16toUTF8(intlString).get(), printDialog,
#if (MOZ_WIDGET_GTK == 2)
                                                         (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
#else
                                                         (GtkDialogFlags)(GTK_DIALOG_MODAL),
#endif
                                                         GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                                         GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                                         nullptr);
  gtk_dialog_set_default_response(GTK_DIALOG(prompt_dialog), GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_alternative_button_order(GTK_DIALOG(prompt_dialog),
                                          GTK_RESPONSE_ACCEPT,
                                          GTK_RESPONSE_REJECT,
                                          -1);

  printBundle->GetStringFromName(MOZ_UTF16("customHeaderFooterPrompt"), getter_Copies(intlString));
  GtkWidget* custom_label = gtk_label_new(NS_ConvertUTF16toUTF8(intlString).get());
  GtkWidget* custom_entry = gtk_entry_new();
  GtkWidget* question_icon = gtk_image_new_from_stock(GTK_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);

  // To be convenient, prefill the textbox with the existing value, if any, and select it all so they can easily
  // both edit it and type in a new one.
  const char* current_text = (const char*) g_object_get_data(G_OBJECT(changed_box), "custom-text");
  if (current_text) {
    gtk_entry_set_text(GTK_ENTRY(custom_entry), current_text);
    gtk_editable_select_region(GTK_EDITABLE(custom_entry), 0, -1);
  }
  gtk_entry_set_activates_default(GTK_ENTRY(custom_entry), TRUE);

  GtkWidget* custom_vbox = gtk_vbox_new(TRUE, 2);
  gtk_box_pack_start(GTK_BOX(custom_vbox), custom_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(custom_vbox), custom_entry, FALSE, FALSE, 5); // Make entry 5px underneath label
  GtkWidget* custom_hbox = gtk_hbox_new(FALSE, 2);
  gtk_box_pack_start(GTK_BOX(custom_hbox), question_icon, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(custom_hbox), custom_vbox, FALSE, FALSE, 10); // Make question icon 10px away from content
  gtk_container_set_border_width(GTK_CONTAINER(custom_hbox), 2);
  gtk_widget_show_all(custom_hbox);

  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(prompt_dialog))), 
                     custom_hbox, FALSE, FALSE, 0);
  gint diag_response = gtk_dialog_run(GTK_DIALOG(prompt_dialog));

  if (diag_response == GTK_RESPONSE_ACCEPT) {
    const gchar* response_text = gtk_entry_get_text(GTK_ENTRY(custom_entry));
    g_object_set_data_full(G_OBJECT(changed_box), "custom-text", strdup(response_text), (GDestroyNotify) free);
    g_object_set_data(G_OBJECT(changed_box), "previous-active", GINT_TO_POINTER(CUSTOM_VALUE_INDEX));
  } else {
    // Go back to the previous index
    gint previous_active = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(changed_box), "previous-active"));
    gtk_combo_box_set_active(changed_box, previous_active);
  }

  gtk_widget_destroy(prompt_dialog);
}

class nsPrintDialogWidgetGTK {
  public:
    nsPrintDialogWidgetGTK(nsIDOMWindow *aParent, nsIPrintSettings *aPrintSettings);
    ~nsPrintDialogWidgetGTK() { gtk_widget_destroy(dialog); }
    NS_ConvertUTF16toUTF8 GetUTF8FromBundle(const char* aKey);
    const gint Run();

    nsresult ImportSettings(nsIPrintSettings *aNSSettings);
    nsresult ExportSettings(nsIPrintSettings *aNSSettings);

  private:
    GtkWidget* dialog;
    GtkWidget* radio_as_laid_out;
    GtkWidget* radio_selected_frame;
    GtkWidget* radio_separate_frames;
    GtkWidget* shrink_to_fit_toggle;
    GtkWidget* print_bg_colors_toggle;
    GtkWidget* print_bg_images_toggle;
    GtkWidget* selection_only_toggle;
    GtkWidget* header_dropdown[3];  // {left, center, right}
    GtkWidget* footer_dropdown[3];

    nsCOMPtr<nsIStringBundle> printBundle;

    bool useNativeSelection;

    GtkWidget* ConstructHeaderFooterDropdown(const char16_t *currentString);
    const char* OptionWidgetToString(GtkWidget *dropdown);

    /* Code to copy between GTK and NS print settings structures.
     * In the following, 
     *   "Import" means to copy from NS to GTK
     *   "Export" means to copy from GTK to NS
     */
    void ExportFramePrinting(nsIPrintSettings *aNS, GtkPrintSettings *aSettings);
    void ExportHeaderFooter(nsIPrintSettings *aNS);
};

nsPrintDialogWidgetGTK::nsPrintDialogWidgetGTK(nsIDOMWindow *aParent, nsIPrintSettings *aSettings)
{
  nsCOMPtr<nsIWidget> widget = WidgetUtils::DOMWindowToWidget(aParent);
  NS_ASSERTION(widget, "Need a widget for dialog to be modal.");
  GtkWindow* gtkParent = get_gtk_window_for_nsiwidget(widget);
  NS_ASSERTION(gtkParent, "Need a GTK window for dialog to be modal.");

  nsCOMPtr<nsIStringBundleService> bundleSvc = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  bundleSvc->CreateBundle("chrome://global/locale/printdialog.properties", getter_AddRefs(printBundle));

  dialog = gtk_print_unix_dialog_new(GetUTF8FromBundle("printTitleGTK").get(), gtkParent);

  gtk_print_unix_dialog_set_manual_capabilities(GTK_PRINT_UNIX_DIALOG(dialog),
                    GtkPrintCapabilities(
                        GTK_PRINT_CAPABILITY_PAGE_SET
                      | GTK_PRINT_CAPABILITY_COPIES
                      | GTK_PRINT_CAPABILITY_COLLATE
                      | GTK_PRINT_CAPABILITY_REVERSE
                      | GTK_PRINT_CAPABILITY_SCALE
                      | GTK_PRINT_CAPABILITY_GENERATE_PDF
                      | GTK_PRINT_CAPABILITY_GENERATE_PS
                    )
                  );

  // The vast majority of magic numbers in this widget construction are padding. e.g. for
  // the set_border_width below, 12px matches that of just about every other window.
  GtkWidget* custom_options_tab = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(custom_options_tab), 12);
  GtkWidget* tab_label = gtk_label_new(GetUTF8FromBundle("optionsTabLabelGTK").get());

  int16_t frameUIFlag;
  aSettings->GetHowToEnableFrameUI(&frameUIFlag);
  radio_as_laid_out = gtk_radio_button_new_with_mnemonic(nullptr, GetUTF8FromBundle("asLaidOut").get());
  if (frameUIFlag == nsIPrintSettings::kFrameEnableNone)
    gtk_widget_set_sensitive(radio_as_laid_out, FALSE);

  radio_selected_frame = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(radio_as_laid_out),
                                                                        GetUTF8FromBundle("selectedFrame").get());
  if (frameUIFlag == nsIPrintSettings::kFrameEnableNone ||
      frameUIFlag == nsIPrintSettings::kFrameEnableAsIsAndEach)
    gtk_widget_set_sensitive(radio_selected_frame, FALSE);

  radio_separate_frames = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(radio_as_laid_out),
                                                                         GetUTF8FromBundle("separateFrames").get());
  if (frameUIFlag == nsIPrintSettings::kFrameEnableNone)
    gtk_widget_set_sensitive(radio_separate_frames, FALSE);

  // "Print Frames" options label, bold and center-aligned
  GtkWidget* print_frames_label = gtk_label_new(nullptr);
  char* pangoMarkup = g_markup_printf_escaped("<b>%s</b>", GetUTF8FromBundle("printFramesTitleGTK").get());
  gtk_label_set_markup(GTK_LABEL(print_frames_label), pangoMarkup);
  g_free(pangoMarkup);
  gtk_misc_set_alignment(GTK_MISC(print_frames_label), 0, 0);

  // Align the radio buttons slightly so they appear to fall under the aforementioned label as per the GNOME HIG
  GtkWidget* frames_radio_container = gtk_alignment_new(0, 0, 0, 0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(frames_radio_container), 8, 0, 12, 0);

  // Radio buttons for the print frames options
  GtkWidget* frames_radio_list = gtk_vbox_new(TRUE, 2);
  gtk_box_pack_start(GTK_BOX(frames_radio_list), radio_as_laid_out, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(frames_radio_list), radio_selected_frame, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(frames_radio_list), radio_separate_frames, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(frames_radio_container), frames_radio_list);

  // Check buttons for shrink-to-fit and print selection
  GtkWidget* check_buttons_container = gtk_vbox_new(TRUE, 2);
  shrink_to_fit_toggle = gtk_check_button_new_with_mnemonic(GetUTF8FromBundle("shrinkToFit").get());
  gtk_box_pack_start(GTK_BOX(check_buttons_container), shrink_to_fit_toggle, FALSE, FALSE, 0);

  // GTK+2.18 and above allow us to add a "Selection" option to the main settings screen,
  // rather than adding an option on a custom tab like we must do on older versions.
  bool canSelectText;
  aSettings->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &canSelectText);
  if (gtk_major_version > 2 ||
      (gtk_major_version == 2 && gtk_minor_version >= 18)) {
    useNativeSelection = true;
    g_object_set(dialog,
                 "support-selection", TRUE,
                 "has-selection", canSelectText,
                 "embed-page-setup", TRUE,
                 nullptr);
  } else {
    useNativeSelection = false;
    selection_only_toggle = gtk_check_button_new_with_mnemonic(GetUTF8FromBundle("selectionOnly").get());
    gtk_widget_set_sensitive(selection_only_toggle, canSelectText);
    gtk_box_pack_start(GTK_BOX(check_buttons_container), selection_only_toggle, FALSE, FALSE, 0);
  }

  // Check buttons for printing background
  GtkWidget* appearance_buttons_container = gtk_vbox_new(TRUE, 2);
  print_bg_colors_toggle = gtk_check_button_new_with_mnemonic(GetUTF8FromBundle("printBGColors").get());
  print_bg_images_toggle = gtk_check_button_new_with_mnemonic(GetUTF8FromBundle("printBGImages").get());
  gtk_box_pack_start(GTK_BOX(appearance_buttons_container), print_bg_colors_toggle, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(appearance_buttons_container), print_bg_images_toggle, FALSE, FALSE, 0);

  // "Appearance" options label, bold and center-aligned
  GtkWidget* appearance_label = gtk_label_new(nullptr);
  pangoMarkup = g_markup_printf_escaped("<b>%s</b>", GetUTF8FromBundle("printBGOptions").get());
  gtk_label_set_markup(GTK_LABEL(appearance_label), pangoMarkup);
  g_free(pangoMarkup);
  gtk_misc_set_alignment(GTK_MISC(appearance_label), 0, 0);

  GtkWidget* appearance_container = gtk_alignment_new(0, 0, 0, 0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(appearance_container), 8, 0, 12, 0);
  gtk_container_add(GTK_CONTAINER(appearance_container), appearance_buttons_container);

  GtkWidget* appearance_vertical_squasher = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(appearance_vertical_squasher), appearance_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(appearance_vertical_squasher), appearance_container, FALSE, FALSE, 0);

  // "Header & Footer" options label, bold and center-aligned
  GtkWidget* header_footer_label = gtk_label_new(nullptr);
  pangoMarkup = g_markup_printf_escaped("<b>%s</b>", GetUTF8FromBundle("headerFooter").get());
  gtk_label_set_markup(GTK_LABEL(header_footer_label), pangoMarkup);
  g_free(pangoMarkup);
  gtk_misc_set_alignment(GTK_MISC(header_footer_label), 0, 0);

  GtkWidget* header_footer_container = gtk_alignment_new(0, 0, 0, 0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(header_footer_container), 8, 0, 12, 0);


  // --- Table for making the header and footer options ---
  GtkWidget* header_footer_table = gtk_table_new(3, 3, FALSE); // 3x3 table
  nsXPIDLString header_footer_str[3];

  aSettings->GetHeaderStrLeft(getter_Copies(header_footer_str[0]));
  aSettings->GetHeaderStrCenter(getter_Copies(header_footer_str[1]));
  aSettings->GetHeaderStrRight(getter_Copies(header_footer_str[2]));

  for (unsigned int i = 0; i < ArrayLength(header_dropdown); i++) {
    header_dropdown[i] = ConstructHeaderFooterDropdown(header_footer_str[i].get());
    // Those 4 magic numbers in the middle provide the position in the table.
    // The last two numbers mean 2 px padding on every side.
    gtk_table_attach(GTK_TABLE(header_footer_table), header_dropdown[i], i, (i + 1),
                     0, 1, (GtkAttachOptions) 0, (GtkAttachOptions) 0, 2, 2);
  }

  const char labelKeys[][7] = {"left", "center", "right"};
  for (unsigned int i = 0; i < ArrayLength(labelKeys); i++) {
    gtk_table_attach(GTK_TABLE(header_footer_table),
                     gtk_label_new(GetUTF8FromBundle(labelKeys[i]).get()),
                     i, (i + 1), 1, 2, (GtkAttachOptions) 0, (GtkAttachOptions) 0, 2, 2);
  }

  aSettings->GetFooterStrLeft(getter_Copies(header_footer_str[0]));
  aSettings->GetFooterStrCenter(getter_Copies(header_footer_str[1]));
  aSettings->GetFooterStrRight(getter_Copies(header_footer_str[2]));

  for (unsigned int i = 0; i < ArrayLength(footer_dropdown); i++) {
    footer_dropdown[i] = ConstructHeaderFooterDropdown(header_footer_str[i].get());
    gtk_table_attach(GTK_TABLE(header_footer_table), footer_dropdown[i], i, (i + 1),
                     2, 3, (GtkAttachOptions) 0, (GtkAttachOptions) 0, 2, 2);
  }
  // ---

  gtk_container_add(GTK_CONTAINER(header_footer_container), header_footer_table);

  GtkWidget* header_footer_vertical_squasher = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(header_footer_vertical_squasher), header_footer_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(header_footer_vertical_squasher), header_footer_container, FALSE, FALSE, 0);

  // Construction of everything
  gtk_box_pack_start(GTK_BOX(custom_options_tab), print_frames_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(custom_options_tab), frames_radio_container, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(custom_options_tab), check_buttons_container, FALSE, FALSE, 10); // 10px padding
  gtk_box_pack_start(GTK_BOX(custom_options_tab), appearance_vertical_squasher, FALSE, FALSE, 10);
  gtk_box_pack_start(GTK_BOX(custom_options_tab), header_footer_vertical_squasher, FALSE, FALSE, 0);

  gtk_print_unix_dialog_add_custom_tab(GTK_PRINT_UNIX_DIALOG(dialog), custom_options_tab, tab_label);
  gtk_widget_show_all(custom_options_tab);
}

NS_ConvertUTF16toUTF8
nsPrintDialogWidgetGTK::GetUTF8FromBundle(const char *aKey)
{
  nsXPIDLString intlString;
  printBundle->GetStringFromName(NS_ConvertUTF8toUTF16(aKey).get(), getter_Copies(intlString));
  return NS_ConvertUTF16toUTF8(intlString);  // Return the actual object so we don't lose reference
}

const char*
nsPrintDialogWidgetGTK::OptionWidgetToString(GtkWidget *dropdown)
{
  gint index = gtk_combo_box_get_active(GTK_COMBO_BOX(dropdown));

  NS_ASSERTION(index <= CUSTOM_VALUE_INDEX, "Index of dropdown is higher than expected!");

  if (index == CUSTOM_VALUE_INDEX)
    return (const char*) g_object_get_data(G_OBJECT(dropdown), "custom-text");
  else
    return header_footer_tags[index];
}

const gint
nsPrintDialogWidgetGTK::Run()
{
  const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_hide(dialog);
  return response;
}

void
nsPrintDialogWidgetGTK::ExportFramePrinting(nsIPrintSettings *aNS, GtkPrintSettings *aSettings)
{
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_as_laid_out)))
    aNS->SetPrintFrameType(nsIPrintSettings::kFramesAsIs);
  else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_selected_frame)))
    aNS->SetPrintFrameType(nsIPrintSettings::kSelectedFrame);
  else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_separate_frames)))
    aNS->SetPrintFrameType(nsIPrintSettings::kEachFrameSep);
  else
    aNS->SetPrintFrameType(nsIPrintSettings::kNoFrames);
}

void
nsPrintDialogWidgetGTK::ExportHeaderFooter(nsIPrintSettings *aNS)
{
  const char* header_footer_str;
  header_footer_str = OptionWidgetToString(header_dropdown[0]);
  aNS->SetHeaderStrLeft(NS_ConvertUTF8toUTF16(header_footer_str).get());

  header_footer_str = OptionWidgetToString(header_dropdown[1]);
  aNS->SetHeaderStrCenter(NS_ConvertUTF8toUTF16(header_footer_str).get());

  header_footer_str = OptionWidgetToString(header_dropdown[2]);
  aNS->SetHeaderStrRight(NS_ConvertUTF8toUTF16(header_footer_str).get());

  header_footer_str = OptionWidgetToString(footer_dropdown[0]);
  aNS->SetFooterStrLeft(NS_ConvertUTF8toUTF16(header_footer_str).get());

  header_footer_str = OptionWidgetToString(footer_dropdown[1]);
  aNS->SetFooterStrCenter(NS_ConvertUTF8toUTF16(header_footer_str).get());

  header_footer_str = OptionWidgetToString(footer_dropdown[2]);
  aNS->SetFooterStrRight(NS_ConvertUTF8toUTF16(header_footer_str).get());
}

nsresult
nsPrintDialogWidgetGTK::ImportSettings(nsIPrintSettings *aNSSettings)
{
  NS_PRECONDITION(aNSSettings, "aSettings must not be null");
  NS_ENSURE_TRUE(aNSSettings, NS_ERROR_FAILURE);

  nsCOMPtr<nsPrintSettingsGTK> aNSSettingsGTK(do_QueryInterface(aNSSettings));
  if (!aNSSettingsGTK)
    return NS_ERROR_FAILURE;

  GtkPrintSettings* settings = aNSSettingsGTK->GetGtkPrintSettings();
  GtkPageSetup* setup = aNSSettingsGTK->GetGtkPageSetup();

  bool geckoBool;
  aNSSettings->GetShrinkToFit(&geckoBool);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(shrink_to_fit_toggle), geckoBool);

  aNSSettings->GetPrintBGColors(&geckoBool);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(print_bg_colors_toggle), geckoBool);

  aNSSettings->GetPrintBGImages(&geckoBool);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(print_bg_images_toggle), geckoBool);

  gtk_print_unix_dialog_set_settings(GTK_PRINT_UNIX_DIALOG(dialog), settings);
  gtk_print_unix_dialog_set_page_setup(GTK_PRINT_UNIX_DIALOG(dialog), setup);
  
  return NS_OK;
}

nsresult
nsPrintDialogWidgetGTK::ExportSettings(nsIPrintSettings *aNSSettings)
{
  NS_PRECONDITION(aNSSettings, "aSettings must not be null");
  NS_ENSURE_TRUE(aNSSettings, NS_ERROR_FAILURE);

  GtkPrintSettings* settings = gtk_print_unix_dialog_get_settings(GTK_PRINT_UNIX_DIALOG(dialog));
  GtkPageSetup* setup = gtk_print_unix_dialog_get_page_setup(GTK_PRINT_UNIX_DIALOG(dialog));
  GtkPrinter* printer = gtk_print_unix_dialog_get_selected_printer(GTK_PRINT_UNIX_DIALOG(dialog));
  if (settings && setup && printer) {
    ExportFramePrinting(aNSSettings, settings);
    ExportHeaderFooter(aNSSettings);

    aNSSettings->SetOutputFormat(nsIPrintSettings::kOutputFormatNative);

    // Print-to-file is true by default. This must be turned off or else printing won't occur!
    // (We manually copy the spool file when this flag is set, because we love our embedders)
    // Even if it is print-to-file in GTK's case, GTK does The Right Thing when we send the job.
    aNSSettings->SetPrintToFile(false);

    aNSSettings->SetShrinkToFit(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(shrink_to_fit_toggle)));

    aNSSettings->SetPrintBGColors(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(print_bg_colors_toggle)));
    aNSSettings->SetPrintBGImages(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(print_bg_images_toggle)));

    // Try to save native settings in the session object
    nsCOMPtr<nsPrintSettingsGTK> aNSSettingsGTK(do_QueryInterface(aNSSettings));
    if (aNSSettingsGTK) {
      aNSSettingsGTK->SetGtkPrintSettings(settings);
      aNSSettingsGTK->SetGtkPageSetup(setup);
      aNSSettingsGTK->SetGtkPrinter(printer);
      bool printSelectionOnly;
      if (useNativeSelection) {
        _GtkPrintPages pageSetting = (_GtkPrintPages)gtk_print_settings_get_print_pages(settings);
        printSelectionOnly = (pageSetting == _GTK_PRINT_PAGES_SELECTION);
      } else {
        printSelectionOnly = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(selection_only_toggle));
      }
      aNSSettingsGTK->SetForcePrintSelectionOnly(printSelectionOnly);
    }
  }

  if (settings)
    g_object_unref(settings);
  return NS_OK;
}

GtkWidget*
nsPrintDialogWidgetGTK::ConstructHeaderFooterDropdown(const char16_t *currentString)
{
#if (MOZ_WIDGET_GTK == 2)
  GtkWidget* dropdown = gtk_combo_box_new_text();
#else
  GtkWidget* dropdown = gtk_combo_box_text_new();
#endif
  const char hf_options[][22] = {"headerFooterBlank", "headerFooterTitle",
                                 "headerFooterURL", "headerFooterDate",
                                 "headerFooterPage", "headerFooterPageTotal",
                                 "headerFooterCustom"};

  for (unsigned int i = 0; i < ArrayLength(hf_options); i++) {
#if (MOZ_WIDGET_GTK == 2)
    gtk_combo_box_append_text(GTK_COMBO_BOX(dropdown), GetUTF8FromBundle(hf_options[i]).get());
#else
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(dropdown), nullptr, 
                              GetUTF8FromBundle(hf_options[i]).get());
#endif
  }

  bool shouldBeCustom = true;
  NS_ConvertUTF16toUTF8 currentStringUTF8(currentString);

  for (unsigned int i = 0; i < ArrayLength(header_footer_tags); i++) {
    if (!strcmp(currentStringUTF8.get(), header_footer_tags[i])) {
      gtk_combo_box_set_active(GTK_COMBO_BOX(dropdown), i);
      g_object_set_data(G_OBJECT(dropdown), "previous-active", GINT_TO_POINTER(i));
      shouldBeCustom = false;
      break;
    }
  }

  if (shouldBeCustom) {
    gtk_combo_box_set_active(GTK_COMBO_BOX(dropdown), CUSTOM_VALUE_INDEX);
    g_object_set_data(G_OBJECT(dropdown), "previous-active", GINT_TO_POINTER(CUSTOM_VALUE_INDEX));
    char* custom_string = strdup(currentStringUTF8.get());
    g_object_set_data_full(G_OBJECT(dropdown), "custom-text", custom_string, (GDestroyNotify) free);
  }

  g_signal_connect(dropdown, "changed", (GCallback) ShowCustomDialog, dialog);
  return dropdown;
}

NS_IMPL_ISUPPORTS(nsPrintDialogServiceGTK, nsIPrintDialogService)

nsPrintDialogServiceGTK::nsPrintDialogServiceGTK()
{
}

nsPrintDialogServiceGTK::~nsPrintDialogServiceGTK()
{
}

NS_IMETHODIMP
nsPrintDialogServiceGTK::Init()
{
  return NS_OK;
}

NS_IMETHODIMP
nsPrintDialogServiceGTK::Show(nsIDOMWindow *aParent, nsIPrintSettings *aSettings,
                              nsIWebBrowserPrint *aWebBrowserPrint)
{
  NS_PRECONDITION(aParent, "aParent must not be null");
  NS_PRECONDITION(aSettings, "aSettings must not be null");

  nsPrintDialogWidgetGTK printDialog(aParent, aSettings);
  nsresult rv = printDialog.ImportSettings(aSettings);

  NS_ENSURE_SUCCESS(rv, rv);

  const gint response = printDialog.Run();

  // Handle the result
  switch (response) {
    case GTK_RESPONSE_OK:                // Proceed
      rv = printDialog.ExportSettings(aSettings);
      break;

    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_CLOSE:
    case GTK_RESPONSE_DELETE_EVENT:
    case GTK_RESPONSE_NONE:
      rv = NS_ERROR_ABORT;
      break;

    case GTK_RESPONSE_APPLY:                // Print preview
    default:
      NS_WARNING("Unexpected response");
      rv = NS_ERROR_ABORT;
  }
  return rv;
}

NS_IMETHODIMP
nsPrintDialogServiceGTK::ShowPageSetup(nsIDOMWindow *aParent,
                                       nsIPrintSettings *aNSSettings)
{
  NS_PRECONDITION(aParent, "aParent must not be null");
  NS_PRECONDITION(aNSSettings, "aSettings must not be null");
  NS_ENSURE_TRUE(aNSSettings, NS_ERROR_FAILURE);

  nsCOMPtr<nsIWidget> widget = WidgetUtils::DOMWindowToWidget(aParent);
  NS_ASSERTION(widget, "Need a widget for dialog to be modal.");
  GtkWindow* gtkParent = get_gtk_window_for_nsiwidget(widget);
  NS_ASSERTION(gtkParent, "Need a GTK window for dialog to be modal.");

  nsCOMPtr<nsPrintSettingsGTK> aNSSettingsGTK(do_QueryInterface(aNSSettings));
  if (!aNSSettingsGTK)
    return NS_ERROR_FAILURE;

  // We need to init the prefs here because aNSSettings in its current form is a dummy in both uses of the word
  nsCOMPtr<nsIPrintSettingsService> psService = do_GetService("@mozilla.org/gfx/printsettings-service;1");
  if (psService) {
    nsXPIDLString printName;
    aNSSettings->GetPrinterName(getter_Copies(printName));
    if (!printName) {
      psService->GetDefaultPrinterName(getter_Copies(printName));
      aNSSettings->SetPrinterName(printName.get());
    }
    psService->InitPrintSettingsFromPrefs(aNSSettings, true, nsIPrintSettings::kInitSaveAll);
  }

  GtkPrintSettings* gtkSettings = aNSSettingsGTK->GetGtkPrintSettings();
  GtkPageSetup* oldPageSetup = aNSSettingsGTK->GetGtkPageSetup();

  GtkPageSetup* newPageSetup = gtk_print_run_page_setup_dialog(gtkParent, oldPageSetup, gtkSettings);

  aNSSettingsGTK->SetGtkPageSetup(newPageSetup);

  // Now newPageSetup has a refcount of 2 (SetGtkPageSetup will addref), put it to 1 so if
  // this gets replaced we don't leak.
  g_object_unref(newPageSetup);

  if (psService)
    psService->SavePrintSettingsToPrefs(aNSSettings, true, nsIPrintSettings::kInitSaveAll);

  return NS_OK;
}
