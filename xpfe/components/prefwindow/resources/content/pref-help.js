// these are keys for resolving the preferences dialog subframe
// in terms of the context-sensitive help that should be loaded
// from the help button. The "mail_prefs_display" things given
// here represent (for the help window itself) the help content.
var fm = {
  "chrome://communicator/content/pref/pref-appearance.xul": "appearance_pref", 
  "chrome://communicator/content/pref/pref-fonts.xul": "appearance_pref_fonts", 
  "chrome://communicator/content/pref/pref-colors.xul": "appearance_pref_colors", 
  "chrome://communicator/content/pref/pref-themes.xul": "appearance_pref_themes", 
  "chrome://content-packs/content/pref-contentpacks.xul": "appearance_pref_content_packs",
  "chrome://communicator/content/pref/pref-navigator.xul": "navigator_pref_navigator",
  "chrome://communicator/content/pref/pref-history.xul": "navigator_pref_history",
  "chrome://communicator/content/pref/pref-languages.xul": "navigator_pref_languages",
  "chrome://communicator/content/pref/pref-applications.xul": "navigator_pref_helper_applications",
  "chrome://communicator/content/pref/pref-smart_browsing.xul": "navigator_pref_smart_browsing",
  "chrome://communicator/content/pref/pref-search.xul": "navigator_pref_internet_searching",
  "chrome://communicator/content/pref/pref-scripts.xul": "advanced_pref_scripts",
  "chrome://messenger/content/pref-mailnews.xul": "mail_prefs_general",
  "chrome://messenger/content/pref-viewing_messages.xul": "mail_prefs_display",
  "chrome://messenger/content/messengercompose/pref-composing_messages.xul": "mail_prefs_messages",
  "chrome://messenger/content/messengercompose/pref-formatting.xul": "mail_prefs_formatting",
  "chrome://messenger/content/addressbook/pref-addressing.xul": "nav_view",
  "chrome://messenger/content/pref-labels.xul": "mail-prefs-labels",
  "chrome://messenger/content/pref-receipts.xul": "mail-prefs-receipts",
  "chrome://editor/content/pref-composer.xul": "composer_prefs_general",
  "chrome://editor/content/pref-editing.xul":  "composer_prefs_newpage",
  "chrome://editor/content/pref-toolbars.xul":  "composer_prefs_toolbars",
  "chrome://messenger/content/pref-mailnews.xul": "mail_prefs_general",
  "chrome://messenger/content/pref-viewing_messages.xul": "mail_prefs_display",
  "chrome://messenger/content/messengercompose/pref-composing_messages.xul": "mail_prefs_messages",
  "chrome://messenger/content/messengercompose/pref-formatting.xul": "mail_prefs_formatting",
  "chrome://messenger/content/addressbook/pref-addressing.xul": "mail_prefs_addressing",
  "chrome://communicator/content/pref/pref-offline.xul": "mail_prefs_offline",
  "chrome://aim/content/pref-IM_instantmessage.xul": "im_prefs_general",
  "chrome://aim/content/pref-IM_privacy.xul": "im_prefs_privacy",
  "chrome://aim/content/pref-IM_notification.xul": "im_prefs_notification",
  "chrome://aim/content/pref-IM_away.xul": "im_prefs_away",
  "chrome://aim/content/pref-IM_connection.xul": "im_prefs_connection",
  "chrome://aim/content/pref-IM_buddyicon.xul": "im_prefs_buddy_icons",
  "chrome://aim/content/pref-IM_style.xul": "im_prefs_styles",
  "chrome://aim/content/pref-Icq_icq.xul": "icq-prefs-gen",
  "chrome://aim/content/pref-Icq_privacy.xul": "icq-prefs-privacy",
  "chrome://aim/content/pref-Icq_notification.xul": "icq-prefs-notifs",
  "chrome://aim/content/pref-Icq_away.xul": "icq_prefs_away",
  "chrome://aim/content/pref-Icq_connection.xul": "icq_prefs_connect",
  "chrome://communicator/content/pref/pref-security.xul": "sec_gen",
  "chrome://cookie/content/pref-cookies.xul": "cookies_prefs",
  "chrome://cookie/content/pref-images.xul": "images_prefs",
  "chrome://cookie/content/pref-popups.xul": "pop_up_blocking",
  "chrome://wallet/content/pref-wallet.xul": "forms_prefs",
  "chrome://pippki/content/pref-masterpass.xul": "passwords_master",
  "chrome://wallet/content/pref-passwords.xul": "passwords_prefs",
  "chrome://pippki/content/pref-ssl.xul": "ssl_prefs",
  "chrome://pippki/content/pref-certs.xul": "certs_prefs",
  "chrome://pippki/content/pref-validation.xul": "validation_prefs",
  "chrome://communicator/content/pref/pref-advanced.xul": "advanced_pref_advanced",
  "chrome://communicator/content/pref/pref-cache.xul": "advanced_pref_cache",
  "chrome://communicator/content/pref/pref-debug.xul": "debug",
  "chrome://communicator/content/pref/pref-debug1.xul": "debug_event",
  "chrome://communicator/content/pref/pref-debug2.xul": "debug_network",
  "chrome://communicator/content/pref/pref-http.xul": "advanced_http_networking",
  "chrome://communicator/content/pref/pref-inspector.xul": "inspector",
  "chrome://communicator/content/pref/pref-download.xul": "navigator_pref_downloads",
  "chrome://communicator/content/pref/pref-mousewheel.xul": "advanced_pref_mouse_wheel",
  "chrome://communicator/content/pref/pref-smartupdate.xul": "advanced_pref_installation",
  "chrome://communicator/content/pref/pref-tabs.xul": "navigator_pref_tabbed_browsing",
  "chrome://communicator/content/pref/pref-winhooks.xul": "advanced_pref_system",
  "chrome://communicator/content/pref/pref-proxies.xul": "advanced_pref_proxies",
  "chrome://communicator/content/pref/pref-keynav.xul":  "advanced_pref_keyboard_nav"
} 

function doHelpButton() {
  var subsrc = document.getElementById("panelFrame").getAttribute("src");
  if ( fm[subsrc] ) {
  	openHelp(fm[subsrc]);  
  } else { 
	openHelp('prefs'); 
  }
}

