var gHelpURL = 'chrome://help/content/help.xul';

// these are keys for resolving the preferences dialog subframe
// in terms of the context-sensitive help that should be loaded
// from the help button. The "?mail_prefs_display" things given
// here represent (for the help window itself) the help content.
var fm = {
  "chrome://communicator/content/pref/pref-appearance.xul": "?appearance_pref_appearance", 
  "chrome://communicator/content/pref/pref-fonts.xul": "?appearance_pref_fonts", 
  "chrome://communicator/content/pref/pref-colors.xul": "?appearance_pref_colors", 
  "chrome://communicator/content/pref/pref-themes.xul": "?appearance_pref_themes", 
  "chrome://communicator/content/pref/pref-navigator.xul": "?navigator_pref_navigator",
  "chrome://communicator/content/pref/pref-history.xul": "?navigator_pref_history",
  "chrome://communicator/content/pref/pref-languages.xul": "?navigator_pref_languages",
  "chrome://communicator/content/pref/pref-applications.xul": "?navigator_pref_helper_applications",
  "chrome://communicator/content/pref/pref-smart_browsing.xul": "?navigator_pref_smart_browsing",
  "chrome://communicator/content/pref/pref-search.xul": "?navigator_pref_internet_searching",
  "chrome://messenger/content/pref-mailnews.xul": "?mail_prefs_general",
  "chrome://messenger/content/pref-viewing_messages.xul": "?mail_prefs_display",
  "chrome://messenger/content/messengercompose/pref-composing_messages.xul": "?mail_prefs_messages",
  "chrome://messenger/content/messengercompose/pref-formatting.xul": "?mail_prefs_formatting",
  "chrome://messenger/content/addressbook/pref-addressing.xul": "?nav_view",
  "chrome://editor/content/pref-composer.xul": "?composer_prefs_general",
  "chrome://editor/content/pref-editing.xul":  "?composer_prefs_newpage",
  "chrome://messenger/content/pref-mailnews.xul": "?mail_prefs_general",
  "chrome://messenger/content/pref-viewing_messages.xul": "?mail_prefs_display",
  "chrome://messenger/content/messengercompose/pref-composing_messages.xul": "?mail_prefs_messages",
  "chrome://messenger/content/messengercompose/pref-formatting.xul": "?mail_prefs_formatting",
  "chrome://messenger/content/addressbook/pref-addressing.xul": "?mail_prefs_addressing",
  "chrome://communicator/content/pref/pref-offline.xul": "?mail_prefs_offline",
  "chrome://aim/content/pref-IM_instantmessage.xul": "?im_prefs_general",
  "chrome://aim/content/pref-IM_privacy.xul": "?im_prefs_privacy",
  "chrome://aim/content/pref-IM_notification.xul": "?im_prefs_notification",
  "chrome://aim/content/pref-IM_away.xul": "?im_prefs_away",
  "chrome://aim/content/pref-IM_connection.xul": "?im_prefs_connection",
  "chrome://communicator/content/pref/pref-security.xul": "?sec_gen",
  "chrome://cookie/content/pref-cookies.xul": "?cookies_prefs",
  "chrome://cookie/content/pref-images.xul": "?images_prefs",
  "chrome://wallet/content/pref-wallet.xul": "?forms_prefs",
  "chrome://wallet/content/pref-masterpass.xul": "?passwords_master",
  "chrome://wallet/content/pref-passwords.xul": "?passwords_prefs",
  "chrome://pippki/content/pref-ssl.xul": "?ssl_prefs",
  "chrome://pippki/content/pref-certs.xul": "?certs_prefs",
  "chrome://pippki/content/pref-validation.xul": "?validation_prefs",
  "chrome://communicator/content/pref/pref-advanced.xul": "?advanced_pref_advanced",
  "chrome://communicator/content/pref/pref-cache.xul": "?advanced_pref_cache",
  "chrome://communicator/content/pref/pref-smartupdate.xul": "?advanced_pref_installation",
  "chrome://communicator/content/pref/pref-mousewheel.xul": "?advanced_pref_mouse_wheel",
  "chrome://communicator/content/pref/pref-winhooks.xul": "?advanced_pref_system"
} 

function doHelpButton() {
  subsrc = document.getElementById("panelFrame").getAttribute("src");
  if ( fm[subsrc] ) {
  	openHelp(gHelpURL + fm[subsrc]);  
  } else { 
	openHelp(gHelpURL + '?prefs'); 
  }
}

