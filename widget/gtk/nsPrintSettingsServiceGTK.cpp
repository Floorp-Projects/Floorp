/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintSettingsServiceGTK.h"
#include "nsPrintSettingsGTK.h"

using namespace mozilla::embedding;

static void
serialize_gtk_printsettings_to_printdata(const gchar *key,
                                         const gchar *value,
                                         gpointer aData)
{
  PrintData* data = (PrintData*)aData;
  CStringKeyValue pair;
  pair.key() = key;
  pair.value() = value;
  data->GTKPrintSettings().AppendElement(pair);
}

NS_IMETHODIMP
nsPrintSettingsServiceGTK::SerializeToPrintData(nsIPrintSettings* aSettings,
                                                nsIWebBrowserPrint* aWBP,
                                                PrintData* data)
{
  nsresult rv = nsPrintSettingsService::SerializeToPrintData(aSettings, aWBP, data);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsPrintSettingsGTK> settingsGTK(do_QueryInterface(aSettings));
  NS_ENSURE_STATE(settingsGTK);

  GtkPrintSettings* gtkPrintSettings = settingsGTK->GetGtkPrintSettings();
  NS_ENSURE_STATE(gtkPrintSettings);

  gtk_print_settings_foreach(
    gtkPrintSettings,
    serialize_gtk_printsettings_to_printdata,
    data);

  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsServiceGTK::DeserializeToPrintSettings(const PrintData& data,
                                                      nsIPrintSettings* settings)
{
  nsCOMPtr<nsPrintSettingsGTK> settingsGTK(do_QueryInterface(settings));
  NS_ENSURE_STATE(settingsGTK);

  nsresult rv = nsPrintSettingsService::DeserializeToPrintSettings(data, settings);
  NS_ENSURE_SUCCESS(rv, rv);

  // Instead of re-using the GtkPrintSettings that nsIPrintSettings is
  // wrapping, we'll create a new one to deserialize to and replace it
  // within nsIPrintSettings.
  GtkPrintSettings* newGtkPrintSettings = gtk_print_settings_new();

  for (uint32_t i = 0; i < data.GTKPrintSettings().Length(); ++i) {
    CStringKeyValue pair = data.GTKPrintSettings()[i];
    gtk_print_settings_set(newGtkPrintSettings,
                           pair.key().get(),
                           pair.value().get());
  }

  settingsGTK->SetGtkPrintSettings(newGtkPrintSettings);

  // nsPrintSettingsGTK is holding a reference to newGtkPrintSettings
  g_object_unref(newGtkPrintSettings);
  newGtkPrintSettings = nullptr;
  return NS_OK;
}

nsresult nsPrintSettingsServiceGTK::_CreatePrintSettings(nsIPrintSettings** _retval)
{
  *_retval = nullptr;
  nsPrintSettingsGTK* printSettings = new nsPrintSettingsGTK(); // does not initially ref count
  NS_ENSURE_TRUE(printSettings, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*_retval = printSettings); // ref count

  return NS_OK;
}

