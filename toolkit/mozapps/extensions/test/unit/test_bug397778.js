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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *      Dave Townsend <dtownsend@oxymoronical.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * ***** END LICENSE BLOCK *****
 */

const PREF_MATCH_OS_LOCALE = "intl.locale.matchOS";
const PREF_SELECTED_LOCALE = "general.useragent.locale";

const ADDON = "test_bug397778";
const ID = "bug397778@tests.mozilla.org";

function run_test()
{
  // Setup for test
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1");
  gPrefs.setBoolPref(PREF_MATCH_OS_LOCALE, false);
  gPrefs.setCharPref(PREF_SELECTED_LOCALE, "fr-FR");

  // Install test add-on
  startupEM();
  gEM.installItemFromFile(do_get_addon(ADDON), NS_INSTALL_LOCATION_APPPROFILE);
  var addon = gEM.getItemForID(ID);
  do_check_neq(addon, null);
  do_check_eq(addon.name, "fr Name");
  restartEM();

  addon = gEM.getItemForID(ID);
  do_check_neq(addon, null);
  do_check_eq(addon.name, "fr Name");
  do_check_eq(getManifestProperty(ID, "description"), "fr Description");
  
  // Disable item
  gEM.disableItem(ID);
  restartEM();
  addon = gEM.getItemForID(ID);
  do_check_neq(addon, null);
  do_check_eq(addon.name, "fr Name");
  
  // Change locale. The more specific de-DE is the best match
  gPrefs.setCharPref(PREF_SELECTED_LOCALE, "de");
  addon = gEM.getItemForID(ID);
  do_check_neq(addon, null);
  do_check_eq(addon.name, "de-DE Name");
  do_check_eq(getManifestProperty(ID, "description"), "");
  
  // Change locale. Locale case should have no effect
  gPrefs.setCharPref(PREF_SELECTED_LOCALE, "DE-de");
  addon = gEM.getItemForID(ID);
  do_check_neq(addon, null);
  do_check_eq(addon.name, "de-DE Name");
  do_check_eq(getManifestProperty(ID, "description"), "");
  
  // Change locale. es-ES should closely match
  gPrefs.setCharPref(PREF_SELECTED_LOCALE, "es-AR");
  addon = gEM.getItemForID(ID);
  do_check_neq(addon, null);
  do_check_eq(addon.name, "es-ES Name");
  do_check_eq(getManifestProperty(ID, "description"), "es-ES Description");
  
  // Change locale. Either zh-CN or zh-TW could match
  gPrefs.setCharPref(PREF_SELECTED_LOCALE, "zh");
  addon = gEM.getItemForID(ID);
  do_check_neq(addon, null);
  if (addon.name != "zh-TW Name" && addon.name != "zh-CN Name")
    do_throw("zh matched to " + addon.name);
  
  // Unknown locale should try to match against en-US as well. Of en,en-GB
  // en should match as being less specific
  gPrefs.setCharPref(PREF_SELECTED_LOCALE, "nl-NL");
  addon = gEM.getItemForID(ID);
  do_check_neq(addon, null);
  do_check_eq(addon.name, "en Name");
  do_check_eq(getManifestProperty(ID, "description"), "en Description");
  
  shutdownEM();
}
