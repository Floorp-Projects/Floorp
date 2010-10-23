/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benjamin Smedberg <bsmedberg@covad.net>
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

/* The prefs in this file are shipped with the GRE and should apply to all
 * embedding situations. Application-specific preferences belong somewhere else,
 * for example xpfe/bootstrap/browser-prefs.js
 *
 * Platform-specific #ifdefs at the end of this file override the generic
 * entries at the top.
 */

// SYNTAX HINTS:  dashes are delimiters.  Use underscores instead.
//  The first character after a period must be alphabetic.

pref("keyword.URL", "http://www.google.com/search?ie=UTF-8&oe=utf-8&q=");
pref("keyword.enabled", false);
pref("general.useragent.locale", "chrome://global/locale/intl.properties");
pref("general.useragent.compatMode.firefox", false);

pref("general.config.obscure_value", 13); // for MCD .cfg files

pref("general.warnOnAboutConfig", true);

// maximum number of dated backups to keep at any time
pref("browser.bookmarks.max_backups",       5);

pref("browser.cache.disk.enable",           true);
// Is this the first-time smartsizing has been introduced?
pref("browser.cache.disk.smart_size.first_run", true);
// Does the user want smart-sizing?
pref("browser.cache.disk.smart_size.enabled", true);
// Size explicitly set by the user. Used when smart_size.enabled == false
#ifndef WINCE
pref("browser.cache.disk.capacity",         256000);
#else
pref("browser.cache.disk.capacity",         20000);
#endif
pref("browser.cache.memory.enable",         true);
//pref("browser.cache.memory.capacity",     -1);
// -1 = determine dynamically, 0 = none, n = memory capacity in kilobytes
pref("browser.cache.disk_cache_ssl",        true);
// 0 = once-per-session, 1 = each-time, 2 = never, 3 = when-appropriate/automatically
pref("browser.cache.check_doc_frequency",   3);

pref("browser.cache.offline.enable",           true);
#ifndef WINCE
// offline cache capacity in kilobytes
pref("browser.cache.offline.capacity",         512000);

// offline apps should be limited to this much data in global storage
// (in kilobytes)
pref("offline-apps.quota.max",        204800);

// the user should be warned if offline app disk usage exceeds this amount
// (in kilobytes)
pref("offline-apps.quota.warn",        51200);
#else
// Limited disk space on WinCE, tighten limits.
pref("browser.cache.offline.capacity", 15000);
pref("offline-apps.quota.max",          7000);
pref("offline-apps.quota.warn",         4000);
#endif

// Whether or not indexedDB is enabled.
pref("dom.indexedDB.enabled", true);
// Space to allow indexedDB databases before prompting (in MB).
pref("dom.indexedDB.warningQuota", 50);

// Fastback caching - if this pref is negative, then we calculate the number
// of content viewers to cache based on the amount of available memory.
pref("browser.sessionhistory.max_total_viewers", -1);

pref("ui.use_native_colors", true);
pref("ui.use_native_popup_windows", false);
pref("ui.click_hold_context_menus", false);
pref("browser.display.use_document_fonts",  1);  // 0 = never, 1 = quick, 2 = always
pref("browser.display.use_document_colors", true);
pref("browser.display.use_system_colors",   false);
pref("browser.display.foreground_color",    "#000000");
pref("browser.display.background_color",    "#FFFFFF");
pref("browser.display.force_inline_alttext", false); // true = force ALT text for missing images to be layed out inline
// 0 = no external leading, 
// 1 = use external leading only when font provides, 
// 2 = add extra leading both internal leading and external leading are zero
pref("browser.display.normal_lineheight_calc_control", 2);
pref("browser.display.show_image_placeholders", true); // true = show image placeholders while image is loaded and when image is broken
// min font device pixel size at which to turn on high quality
pref("browser.display.auto_quality_min_font_size", 20);
pref("browser.anchor_color",                "#0000EE");
pref("browser.active_color",                "#EE0000");
pref("browser.visited_color",               "#551A8B");
pref("browser.underline_anchors",           true);
pref("browser.blink_allowed",               true);
pref("browser.enable_automatic_image_resizing", false);
pref("browser.enable_click_image_resizing", true);

// See http://dev.w3.org/html5/spec/forms.html#attr-fe-autofocus
pref("browser.autofocus", true);

// See http://whatwg.org/specs/web-apps/current-work/#ping
pref("browser.send_pings", false);
pref("browser.send_pings.max_per_link", 1);           // limit the number of pings that are sent per link click
pref("browser.send_pings.require_same_host", false);  // only send pings to the same host if this is true

pref("browser.display.use_focus_colors",    false);
pref("browser.display.focus_background_color", "#117722");
pref("browser.display.focus_text_color",     "#ffffff");
pref("browser.display.focus_ring_width",     1);
pref("browser.display.focus_ring_on_anything", false);
// focus ring border style.
// 0 = solid border, 1 = dotted border
pref("browser.display.focus_ring_style", 1);

pref("browser.helperApps.alwaysAsk.force",  false);
pref("browser.helperApps.neverAsk.saveToDisk", "");
pref("browser.helperApps.neverAsk.openFile", "");

// xxxbsmedberg: where should prefs for the toolkit go?
pref("browser.chrome.toolbar_tips",         true);
// 0 = Pictures Only, 1 = Text Only, 2 = Pictures and Text
pref("browser.chrome.toolbar_style",        2);
// max image size for which it is placed in the tab icon for tabbrowser.
// if 0, no images are used for tab icons for image documents.
pref("browser.chrome.image_icons.max_size", 1024);

pref("browser.triple_click_selects_paragraph", true);

// When loading <video> or <audio>, check for Access-Control-Allow-Origin
// header, and disallow the connection if not present or permitted.
pref("media.enforce_same_site_origin", false);

// Media cache size in kilobytes
pref("media.cache_size", 512000);

#ifdef MOZ_RAW
pref("media.raw.enabled", true);
#endif
#ifdef MOZ_OGG
pref("media.ogg.enabled", true);
#endif
#ifdef MOZ_WAVE
pref("media.wave.enabled", true);
#endif
#ifdef MOZ_WEBM
pref("media.webm.enabled", true);
#endif

// Whether to autostart a media element with an |autoplay| attribute
pref("media.autoplay.enabled", true);

// 0 = Off, 1 = Full, 2 = Tagged Images Only. 
// See eCMSMode in gfx/thebes/gfxPlatform.h
pref("gfx.color_management.mode", 2);
pref("gfx.color_management.display_profile", "");
pref("gfx.color_management.rendering_intent", 0);

pref("gfx.3d_video.enabled", false);

pref("gfx.downloadable_fonts.enabled", true);
pref("gfx.downloadable_fonts.sanitize", true);
#ifdef XP_MACOSX
pref("gfx.downloadable_fonts.sanitize.preserve_otl_tables", false);
#else
pref("gfx.downloadable_fonts.sanitize.preserve_otl_tables", true);
#endif

pref("gfx.font_rendering.harfbuzz.level", 1);

#ifdef XP_WIN
#ifndef WINCE
pref("gfx.font_rendering.directwrite.enabled", false);
#endif
#endif

pref("accessibility.browsewithcaret", false);
pref("accessibility.warn_on_browsewithcaret", true);

pref("accessibility.browsewithcaret_shortcut.enabled", true);

#ifndef XP_MACOSX
// Tab focus model bit field:
// 1 focuses text controls, 2 focuses other form elements, 4 adds links.
// Most users will want 1, 3, or 7.
// On OS X, we use Full Keyboard Access system preference,
// unless accessibility.tabfocus is set by the user.
pref("accessibility.tabfocus", 7);
pref("accessibility.tabfocus_applies_to_xul", false);

// Forcibly disable a11y on win32, even if something attempts
// to enable it.
pref("accessibility.win32.force_disabled", false);

// On OS X, we follow the "Click in the scrollbar to:" system preference
// unless this preference was set manually
pref("ui.scrollToClick", 0);

#else
// Only on mac tabfocus is expected to handle UI widgets as well as web content
pref("accessibility.tabfocus_applies_to_xul", true);
#endif

pref("accessibility.usetexttospeech", "");
pref("accessibility.usebrailledisplay", "");
pref("accessibility.accesskeycausesactivation", true);
pref("accessibility.mouse_focuses_formcontrol", false);

// Type Ahead Find
pref("accessibility.typeaheadfind", true);
pref("accessibility.typeaheadfind.autostart", true);
// casesensitive: controls the find bar's case-sensitivity
//     0 - "never"  (case-insensitive)
//     1 - "always" (case-sensitive)
// other - "auto"   (case-sensitive for mixed-case input, insensitive otherwise)
pref("accessibility.typeaheadfind.casesensitive", 0);
pref("accessibility.typeaheadfind.linksonly", true);
pref("accessibility.typeaheadfind.startlinksonly", false);
pref("accessibility.typeaheadfind.timeout", 4000);
pref("accessibility.typeaheadfind.enabletimeout", true);
pref("accessibility.typeaheadfind.soundURL", "beep");
pref("accessibility.typeaheadfind.enablesound", true);
pref("accessibility.typeaheadfind.prefillwithselection", true);

// use Mac OS X Appearance panel text smoothing setting when rendering text, disabled by default
pref("gfx.use_text_smoothing_setting", false);

// loading and rendering of framesets and iframes
pref("browser.frames.enabled", true);

// Number of characters to consider emphasizing for rich autocomplete results
pref("toolkit.autocomplete.richBoundaryCutoff", 200);

pref("toolkit.scrollbox.smoothScroll", true);
pref("toolkit.scrollbox.scrollIncrement", 20);
pref("toolkit.scrollbox.clickToScroll.scrollDelay", 150);

// view source
pref("view_source.syntax_highlight", true);
pref("view_source.wrap_long_lines", false);
pref("view_source.editor.external", false);
pref("view_source.editor.path", "");
// allows to add further arguments to the editor; use the %LINE% placeholder
// for jumping to a specific line (e.g. "/line:%LINE%" or "--goto %LINE%")
pref("view_source.editor.args", "");

// dispatch left clicks only to content in browser (still allows clicks to chrome/xul)
pref("nglayout.events.dispatchLeftClickOnly", true);

// whether or not to draw images while dragging
pref("nglayout.enable_drag_images", true);

// scrollbar snapping region
// 0 - off
// 1 and higher - slider thickness multiple
pref("slider.snapMultiplier", 0);

// option to choose plug-in finder
pref("application.use_ns_plugin_finder", false);

// URI fixup prefs
pref("browser.fixup.alternate.enabled", true);
pref("browser.fixup.alternate.prefix", "www.");
pref("browser.fixup.alternate.suffix", ".com");
pref("browser.fixup.hide_user_pass", true);

// Location Bar AutoComplete
pref("browser.urlbar.autocomplete.enabled", true);

// Print header customization
// Use the following codes:
// &T - Title
// &U - Document URL
// &D - Date/Time
// &P - Page Number
// &PT - Page Number "of" Page total
// Set each header to a string containing zero or one of these codes
// and the code will be replaced in that string by the corresponding data
pref("print.print_headerleft", "&T");
pref("print.print_headercenter", "");
pref("print.print_headerright", "&U");
pref("print.print_footerleft", "&PT");
pref("print.print_footercenter", "");
pref("print.print_footerright", "&D");
pref("print.show_print_progress", true);

// xxxbsmedberg: more toolkit prefs

// When this is set to false each window has its own PrintSettings
// and a change in one window does not affect the others
pref("print.use_global_printsettings", true);

// Save the Printings after each print job
pref("print.save_print_settings", true);

pref("print.whileInPrintPreview", true);

// Cache old Presentation when going into Print Preview
pref("print.always_cache_old_pres", false);

// Enables you to specify the amount of the paper that is to be treated
// as unwriteable.  The print_edge_XXX and print_margin_XXX preferences
// are treated as offsets that are added to this pref.
// Default is "-1", which means "use the system default".  (If there is
// no system default, then the -1 is treated as if it were 0.)
// This is used by both Printing and Print Preview.
// Units are in 1/100ths of an inch.
pref("print.print_unwriteable_margin_top",    -1);
pref("print.print_unwriteable_margin_left",   -1);
pref("print.print_unwriteable_margin_right",  -1);
pref("print.print_unwriteable_margin_bottom", -1);

// Enables you to specify the gap from the edge of the paper's 
// unwriteable area to the margin.
// This is used by both Printing and Print Preview
// Units are in 1/100ths of an inch.
pref("print.print_edge_top", 0);
pref("print.print_edge_left", 0);
pref("print.print_edge_right", 0);
pref("print.print_edge_bottom", 0);

// Pref used by the spellchecker extension to control the 
// maximum number of misspelled words that will be underlined
// in a document.
pref("extensions.spellcheck.inline.max-misspellings", 500);

// Prefs used by libeditor. Prefs specific to seamonkey composer
// belong in comm-central/editor/ui/composer.js

pref("editor.use_custom_colors", false);
pref("editor.singleLine.pasteNewlines",      2);
pref("editor.quotesPreformatted",            false);
pref("editor.use_css",                       true);
pref("editor.css.default_length_unit",       "px");
pref("editor.resizing.preserve_ratio",       true);
pref("editor.positioning.offset",            0);


// Default Capability Preferences: Security-Critical! 
// Editing these may create a security risk - be sure you know what you're doing
//pref("capability.policy.default.barprop.visible.set", "UniversalBrowserWrite");

pref("capability.policy.default_policynames", "mailnews");

pref("capability.policy.default.DOMException.code", "allAccess");
pref("capability.policy.default.DOMException.message", "allAccess");
pref("capability.policy.default.DOMException.name", "allAccess");
pref("capability.policy.default.DOMException.result", "allAccess");
pref("capability.policy.default.DOMException.toString.get", "allAccess");

pref("capability.policy.default.History.back.get", "allAccess");
pref("capability.policy.default.History.current", "UniversalBrowserRead");
pref("capability.policy.default.History.forward.get", "allAccess");
pref("capability.policy.default.History.go.get", "allAccess");
pref("capability.policy.default.History.item", "UniversalBrowserRead");
pref("capability.policy.default.History.next", "UniversalBrowserRead");
pref("capability.policy.default.History.previous", "UniversalBrowserRead");
pref("capability.policy.default.History.toString", "UniversalBrowserRead");

pref("capability.policy.default.Location.hash.set", "allAccess");
pref("capability.policy.default.Location.href.set", "allAccess");
pref("capability.policy.default.Location.replace.get", "allAccess");

pref("capability.policy.default.Navigator.preference", "allAccess");
pref("capability.policy.default.Navigator.preferenceinternal.get", "UniversalPreferencesRead");
pref("capability.policy.default.Navigator.preferenceinternal.set", "UniversalPreferencesWrite");

pref("capability.policy.default.Window.blur.get", "allAccess");
pref("capability.policy.default.Window.close.get", "allAccess");
pref("capability.policy.default.Window.closed.get", "allAccess");
pref("capability.policy.default.Window.focus.get", "allAccess");
pref("capability.policy.default.Window.frames.get", "allAccess");
pref("capability.policy.default.Window.history.get", "allAccess");
pref("capability.policy.default.Window.length.get", "allAccess");
pref("capability.policy.default.Window.location", "allAccess");
pref("capability.policy.default.Window.opener.get", "allAccess");
pref("capability.policy.default.Window.parent.get", "allAccess");
pref("capability.policy.default.Window.postMessage.get", "allAccess");
pref("capability.policy.default.Window.self.get", "allAccess");
pref("capability.policy.default.Window.top.get", "allAccess");
pref("capability.policy.default.Window.window.get", "allAccess");

pref("capability.policy.default.Selection.addSelectionListener", "UniversalXPConnect");
pref("capability.policy.default.Selection.removeSelectionListener", "UniversalXPConnect");

// Restrictions on the DOM for mail/news - see bugs 66938 and 84545
pref("capability.policy.mailnews.sites", "mailbox: imap: news:");

pref("capability.policy.mailnews.*.attributes.get", "noAccess");
pref("capability.policy.mailnews.*.baseURI.get", "noAccess");
pref("capability.policy.mailnews.*.data.get", "noAccess");
pref("capability.policy.mailnews.*.getAttribute", "noAccess");
pref("capability.policy.mailnews.HTMLDivElement.getAttribute", "sameOrigin");
pref("capability.policy.mailnews.*.getAttributeNS", "noAccess");
pref("capability.policy.mailnews.*.getAttributeNode", "noAccess");
pref("capability.policy.mailnews.*.getAttributeNodeNS", "noAccess");
pref("capability.policy.mailnews.*.getNamedItem", "noAccess");
pref("capability.policy.mailnews.*.getNamedItemNS", "noAccess");
pref("capability.policy.mailnews.*.host.get", "noAccess");
pref("capability.policy.mailnews.*.hostname.get", "noAccess");
pref("capability.policy.mailnews.*.href.get", "noAccess");
pref("capability.policy.mailnews.*.innerHTML.get", "noAccess");
pref("capability.policy.mailnews.*.lowSrc.get", "noAccess");
pref("capability.policy.mailnews.*.nodeValue.get", "noAccess");
pref("capability.policy.mailnews.*.pathname.get", "noAccess");
pref("capability.policy.mailnews.*.protocol.get", "noAccess");
pref("capability.policy.mailnews.*.src.get", "noAccess");
pref("capability.policy.mailnews.*.substringData.get", "noAccess");
pref("capability.policy.mailnews.*.text.get", "noAccess");
pref("capability.policy.mailnews.*.textContent", "noAccess");
pref("capability.policy.mailnews.*.title.get", "noAccess");
pref("capability.policy.mailnews.*.wholeText", "noAccess");
pref("capability.policy.mailnews.DOMException.toString", "noAccess");
pref("capability.policy.mailnews.HTMLAnchorElement.toString", "noAccess");
pref("capability.policy.mailnews.HTMLDocument.domain", "noAccess");
pref("capability.policy.mailnews.HTMLDocument.URL", "noAccess");
pref("capability.policy.mailnews.*.documentURI", "noAccess");
pref("capability.policy.mailnews.Location.toString", "noAccess");
pref("capability.policy.mailnews.Range.toString", "noAccess");
pref("capability.policy.mailnews.Window.blur", "noAccess");
pref("capability.policy.mailnews.Window.focus", "noAccess");
pref("capability.policy.mailnews.Window.innerWidth.set", "noAccess");
pref("capability.policy.mailnews.Window.innerHeight.set", "noAccess");
pref("capability.policy.mailnews.Window.moveBy", "noAccess");
pref("capability.policy.mailnews.Window.moveTo", "noAccess");
pref("capability.policy.mailnews.Window.name.set", "noAccess");
pref("capability.policy.mailnews.Window.outerHeight.set", "noAccess");
pref("capability.policy.mailnews.Window.outerWidth.set", "noAccess");
pref("capability.policy.mailnews.Window.resizeBy", "noAccess");
pref("capability.policy.mailnews.Window.resizeTo", "noAccess");
pref("capability.policy.mailnews.Window.screenX.set", "noAccess");
pref("capability.policy.mailnews.Window.screenY.set", "noAccess");
pref("capability.policy.mailnews.Window.sizeToContent", "noAccess");
pref("capability.policy.mailnews.document.load", "noAccess");
pref("capability.policy.mailnews.XMLHttpRequest.channel", "noAccess");
pref("capability.policy.mailnews.XMLHttpRequest.getInterface", "noAccess");
pref("capability.policy.mailnews.XMLHttpRequest.responseXML", "noAccess");
pref("capability.policy.mailnews.XMLHttpRequest.responseText", "noAccess");
pref("capability.policy.mailnews.XMLHttpRequest.status", "noAccess");
pref("capability.policy.mailnews.XMLHttpRequest.statusText", "noAccess");
pref("capability.policy.mailnews.XMLHttpRequest.abort", "noAccess");
pref("capability.policy.mailnews.XMLHttpRequest.getAllResponseHeaders", "noAccess");
pref("capability.policy.mailnews.XMLHttpRequest.getResponseHeader", "noAccess");
pref("capability.policy.mailnews.XMLHttpRequest.open", "noAccess");
pref("capability.policy.mailnews.XMLHttpRequest.send", "noAccess");
pref("capability.policy.mailnews.XMLHttpRequest.setRequestHeader", "noAccess");
pref("capability.policy.mailnews.XMLHttpRequest.readyState", "noAccess");
pref("capability.policy.mailnews.XMLHttpRequest.overrideMimeType", "noAccess");
pref("capability.policy.mailnews.XMLHttpRequest.onload", "noAccess");
pref("capability.policy.mailnews.XMLHttpRequest.onerror", "noAccess");
pref("capability.policy.mailnews.XMLHttpRequest.onreadystatechange", "noAccess");
pref("capability.policy.mailnews.XMLSerializer.serializeToString", "noAccess");
pref("capability.policy.mailnews.XMLSerializer.serializeToStream", "noAccess");
pref("capability.policy.mailnews.DOMParser.parseFromString", "noAccess");
pref("capability.policy.mailnews.DOMParser.parseFromStream", "noAccess");
pref("capability.policy.mailnews.SOAPCall.transportURI", "noAccess");
pref("capability.policy.mailnews.SOAPCall.verifySourceHeader", "noAccess");
pref("capability.policy.mailnews.SOAPCall.invoke", "noAccess");
pref("capability.policy.mailnews.SOAPCall.asyncInvoke", "noAccess");
pref("capability.policy.mailnews.SOAPResponse.fault", "noAccess");
pref("capability.policy.mailnews.SOAPEncoding.styleURI", "noAccess");
pref("capability.policy.mailnews.SOAPEncoding.getAssociatedEncoding", "noAccess");
pref("capability.policy.mailnews.SOAPEncoding.setEncoder", "noAccess");
pref("capability.policy.mailnews.SOAPEncoding.getEncoder", "noAccess");
pref("capability.policy.mailnews.SOAPEncoding.setDecoder", "noAccess");
pref("capability.policy.mailnews.SOAPEncoding.setDecoder", "noAccess");
pref("capability.policy.mailnews.SOAPEncoding.getDecoder", "noAccess");
pref("capability.policy.mailnews.SOAPEncoding.defaultEncoder", "noAccess");
pref("capability.policy.mailnews.SOAPEncoding.defaultDecoder", "noAccess");
pref("capability.policy.mailnews.SOAPEncoding.schemaCollection", "noAccess");
pref("capability.policy.mailnews.SOAPEncoding.encode", "noAccess");
pref("capability.policy.mailnews.SOAPEncoding.decode", "noAccess");
pref("capability.policy.mailnews.SOAPEncoding.mapSchemaURI", "noAccess");
pref("capability.policy.mailnews.SOAPEncoding.unmapSchemaURI", "noAccess");
pref("capability.policy.mailnews.SOAPEncoding.getInternalSchemaURI", "noAccess");
pref("capability.policy.mailnews.SOAPEncoding.getExternalSchemaURI", "noAccess");
pref("capability.policy.mailnews.SOAPFault.element", "noAccess");
pref("capability.policy.mailnews.SOAPFault.faultNamespaceURI", "noAccess");
pref("capability.policy.mailnews.SOAPFault.faultCode", "noAccess");
pref("capability.policy.mailnews.SOAPFault.faultString", "noAccess");
pref("capability.policy.mailnews.SOAPFault.faultActor", "noAccess");
pref("capability.policy.mailnews.SOAPFault.detail", "noAccess");
pref("capability.policy.mailnews.SOAPHeaderBlock.actorURI", "noAccess");
pref("capability.policy.mailnews.SOAPHeaderBlock.mustUnderstand", "noAccess");
pref("capability.policy.mailnews.SOAPParameter", "noAccess");
pref("capability.policy.mailnews.SOAPPropertyBagMutator.propertyBag", "noAccess");
pref("capability.policy.mailnews.SOAPPropertyBagMutator.addProperty", "noAccess");
pref("capability.policy.mailnews.SchemaLoader.load", "noAccess");
pref("capability.policy.mailnews.SchemaLoader.loadAsync", "noAccess");
pref("capability.policy.mailnews.SchemaLoader.processSchemaElement", "noAccess");
pref("capability.policy.mailnews.SchemaLoader.onLoad", "noAccess");
pref("capability.policy.mailnews.SchemaLoader.onError", "noAccess");
pref("capability.policy.mailnews.WSDLLoader.load", "noAccess");
pref("capability.policy.mailnews.WSDLLoader.loadAsync", "noAccess");
pref("capability.policy.mailnews.WSDLLoader.onLoad", "noAccess");
pref("capability.policy.mailnews.WSDLLoader.onError", "noAccess");
pref("capability.policy.mailnews.WebServiceProxyFactory.createProxy", "noAccess");
pref("capability.policy.mailnews.WebServiceProxyFactory.createProxyAsync", "noAccess");
pref("capability.policy.mailnews.WebServiceProxyFactory.onLoad", "noAccess");
pref("capability.policy.mailnews.WebServiceProxyFactory.onError", "noAccess");

// XMLExtras
pref("capability.policy.default.XMLHttpRequest.channel", "noAccess");
pref("capability.policy.default.XMLHttpRequest.getInterface", "noAccess");
pref("capability.policy.default.XMLHttpRequest.open-uri", "allAccess");
pref("capability.policy.default.DOMParser.parseFromStream", "noAccess");

// Clipboard
pref("capability.policy.default.Clipboard.cutcopy", "noAccess");
pref("capability.policy.default.Clipboard.paste", "noAccess");

// Scripts & Windows prefs
pref("dom.disable_image_src_set",           false);
pref("dom.disable_window_flip",             false);
pref("dom.disable_window_move_resize",      false);
pref("dom.disable_window_status_change",    false);

pref("dom.disable_window_open_feature.titlebar",    false);
pref("dom.disable_window_open_feature.close",       false);
pref("dom.disable_window_open_feature.toolbar",     false);
pref("dom.disable_window_open_feature.location",    false);
pref("dom.disable_window_open_feature.personalbar", false);
pref("dom.disable_window_open_feature.menubar",     false);
pref("dom.disable_window_open_feature.scrollbars",  false);
pref("dom.disable_window_open_feature.resizable",   true);
pref("dom.disable_window_open_feature.minimizable", false);
pref("dom.disable_window_open_feature.status",      true);

pref("dom.allow_scripts_to_close_windows",          false);

pref("dom.disable_open_during_load",                false);
pref("dom.popup_maximum",                           20);
pref("dom.popup_allowed_events", "change click dblclick mouseup reset submit");
pref("dom.disable_open_click_delay", 1000);

pref("dom.storage.enabled", true);
pref("dom.storage.default_quota",      5120);

// Parsing perf prefs. For now just mimic what the old code did.
#ifndef XP_WIN
pref("content.sink.pending_event_mode", 0);
#endif

// Disable popups from plugins by default
//   0 = openAllowed
//   1 = openControlled
//   2 = openAbused
pref("privacy.popups.disable_from_plugins", 2);

pref("dom.event.contextmenu.enabled",       true);

pref("javascript.enabled",                  true);
pref("javascript.options.strict",           false);
#ifdef DEBUG
pref("javascript.options.strict.debug",     true);
#endif
pref("javascript.options.relimit",          true);
pref("javascript.options.tracejit.content",  true);
pref("javascript.options.tracejit.chrome",   true);
pref("javascript.options.methodjit.content", true);
pref("javascript.options.methodjit.chrome",  false);
pref("javascript.options.jitprofiling.content", true);
pref("javascript.options.jitprofiling.chrome",  false);
// This preference limits the memory usage of javascript.
// If you want to change these values for your device,
// please find Bug 417052 comment 17 and Bug 456721
// Comment 32.
pref("javascript.options.mem.high_water_mark", 32);
pref("javascript.options.mem.gc_frequency",   300);

// advanced prefs
pref("advanced.mailftp",                    false);
pref("image.animation_mode",                "normal");

// Same-origin policy for file URIs, "false" is traditional
pref("security.fileuri.strict_origin_policy", true);

// If there is ever a security firedrill that requires
// us to block certian ports global, this is the pref 
// to use.  Is is a comma delimited list of port numbers
// for example:
//   pref("network.security.ports.banned", "1,2,3,4,5");
// prevents necko connecting to ports 1-5 unless the protocol
// overrides.

// Default action for unlisted external protocol handlers
pref("network.protocol-handler.external-default", true);      // OK to load
pref("network.protocol-handler.warn-external-default", true); // warn before load

// Prevent using external protocol handlers for these schemes
pref("network.protocol-handler.external.hcp", false);
pref("network.protocol-handler.external.vbscript", false);
pref("network.protocol-handler.external.javascript", false);
pref("network.protocol-handler.external.data", false);
pref("network.protocol-handler.external.ms-help", false);
pref("network.protocol-handler.external.shell", false);
pref("network.protocol-handler.external.vnd.ms.radio", false);
#ifdef XP_MACOSX
pref("network.protocol-handler.external.help", false);
#endif
pref("network.protocol-handler.external.disk", false);
pref("network.protocol-handler.external.disks", false);
pref("network.protocol-handler.external.afp", false);
pref("network.protocol-handler.external.moz-icon", false);

// An exposed protocol handler is one that can be used in all contexts.  A
// non-exposed protocol handler is one that can only be used internally by the
// application.  For example, a non-exposed protocol would not be loaded by the
// application in response to a link click or a X-remote openURL command.
// Instead, it would be deferred to the system's external protocol handler.
// Only internal/built-in protocol handlers can be marked as exposed.

// This pref controls the default settings.  Per protocol settings can be used
// to override this value.
pref("network.protocol-handler.expose-all", true);

// Example: make IMAP an exposed protocol
// pref("network.protocol-handler.expose.imap", true);

pref("network.hosts.smtp_server",           "mail");
pref("network.hosts.pop_server",            "mail");

// <http>
pref("network.http.version", "1.1");	  // default
// pref("network.http.version", "1.0");   // uncomment this out in case of problems
// pref("network.http.version", "0.9");   // it'll work too if you're crazy
// keep-alive option is effectively obsolete. Nevertheless it'll work with
// some older 1.0 servers:

pref("network.http.proxy.version", "1.1");    // default
// pref("network.http.proxy.version", "1.0"); // uncomment this out in case of problems
                                              // (required if using junkbuster proxy)

// enable caching of http documents
pref("network.http.use-cache", true);

// this preference can be set to override the socket type used for normal
// HTTP traffic.  an empty value indicates the normal TCP/IP socket type.
pref("network.http.default-socket-type", "");

pref("network.http.keep-alive", true); // set it to false in case of problems
pref("network.http.proxy.keep-alive", true);
// There is a problem with some IIS7 servers that don't close the connection
// properly after it times out (bug #491541). Default timeout on IIS7 is
// 120 seconds. We need to reuse or drop the connection within this time.
// We set the timeout a little shorter to keep a reserve for cases when
// the packet is lost or delayed on the route.
pref("network.http.keep-alive.timeout", 115);

// limit the absolute number of http connections.
pref("network.http.max-connections", 30);

// limit the absolute number of http connections that can be established per
// host.  if a http proxy server is enabled, then the "server" is the proxy
// server.  Otherwise, "server" is the http origin server.
pref("network.http.max-connections-per-server", 15);

// if network.http.keep-alive is true, and if NOT connecting via a proxy, then
// a new connection will only be attempted if the number of active persistent
// connections to the server is less then max-persistent-connections-per-server.
pref("network.http.max-persistent-connections-per-server", 6);

// if network.http.keep-alive is true, and if connecting via a proxy, then a
// new connection will only be attempted if the number of active persistent
// connections to the proxy is less then max-persistent-connections-per-proxy.
pref("network.http.max-persistent-connections-per-proxy", 8);

// amount of time (in seconds) to suspend pending requests, before spawning a
// new connection, once the limit on the number of persistent connections per
// host has been reached.  however, a new connection will not be created if
// max-connections or max-connections-per-server has also been reached.
pref("network.http.request.max-start-delay", 10);

// Headers
pref("network.http.accept.default", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
pref("network.http.sendRefererHeader",      2); // 0=don't send any, 1=send only on clicks, 2=send on image requests as well

// Controls whether we send HTTPS referres to other HTTPS sites.
// By default this is enabled for compatibility (see bug 141641)
pref("network.http.sendSecureXSiteReferrer", true);

// Maximum number of consecutive redirects before aborting.
pref("network.http.redirection-limit", 20);

// Enable http compression: comment this out in case of problems with 1.1
// NOTE: support for "compress" has been disabled per bug 196406.
// NOTE: separate values with comma+space (", "): see bug 576033
pref("network.http.accept-encoding", "gzip, deflate");

pref("network.http.pipelining"      , false);
pref("network.http.pipelining.ssl"  , false); // disable pipelining over SSL
pref("network.http.proxy.pipelining", false);

// Max number of requests in the pipeline
pref("network.http.pipelining.maxrequests" , 4);

// Prompt for 307 redirects
pref("network.http.prompt-temp-redirect", true);

// On networks deploying QoS, it is recommended that these be lockpref()'d,
// since inappropriate marking can easily overwhelm bandwidth reservations
// for certain services (i.e. EF for VoIP, AF4x for interactive video,
// AF3x for broadcast/streaming video, etc)

// default value for HTTP
// in a DSCP environment this should be 40 (0x28, or AF11), per RFC-4594,
// Section 4.8 "High-Throughput Data Service Class"
pref("network.http.qos", 0);

// default values for FTP
// in a DSCP environment this should be 40 (0x28, or AF11), per RFC-4594,
// Section 4.8 "High-Throughput Data Service Class", and 80 (0x50, or AF22)
// per Section 4.7 "Low-Latency Data Service Class".
pref("network.ftp.data.qos", 0);
pref("network.ftp.control.qos", 0);

// </http>

// <ws>: WebSocket
pref("network.websocket.enabled", true);
// </ws>

// If false, remote JAR files that are served with a content type other than
// application/java-archive or application/x-jar will not be opened
// by the jar channel.
pref("network.jar.open-unsafe-types", false);

// This preference controls whether or not internationalized domain names (IDN)
// are handled.  IDN requires a nsIIDNService implementation.
pref("network.enableIDN", true);

// This preference, if true, causes all UTF-8 domain names to be normalized to
// punycode.  The intention is to allow UTF-8 domain names as input, but never
// generate them from punycode.
pref("network.IDN_show_punycode", false);

// TLDs with "network.IDN.whitelist.tld" explicitly set to true are treated as 
// IDN-safe. Otherwise, they're treated as unsafe and punycode will be used
// for displaying them in the UI (e.g. URL bar). Note that these preferences
// are referred to ONLY when "network.IDN_show_punycode" is false. In other
// words, all IDNs will be shown in punycode if "network.IDN_show_punycode"
// is true.

// ccTLDs
pref("network.IDN.whitelist.ac", true);
pref("network.IDN.whitelist.ar", true);
pref("network.IDN.whitelist.at", true);
pref("network.IDN.whitelist.br", true);
pref("network.IDN.whitelist.ch", true);
pref("network.IDN.whitelist.cl", true);
pref("network.IDN.whitelist.cn", true);
pref("network.IDN.whitelist.de", true);
pref("network.IDN.whitelist.dk", true);
pref("network.IDN.whitelist.es", true);
pref("network.IDN.whitelist.fi", true);
pref("network.IDN.whitelist.gr", true);
pref("network.IDN.whitelist.hu", true);
pref("network.IDN.whitelist.io", true);
pref("network.IDN.whitelist.ir", true);
pref("network.IDN.whitelist.is", true);
pref("network.IDN.whitelist.jp", true);
pref("network.IDN.whitelist.kr", true);
pref("network.IDN.whitelist.li", true);
pref("network.IDN.whitelist.lt", true);
pref("network.IDN.whitelist.lu", true);
pref("network.IDN.whitelist.no", true);
pref("network.IDN.whitelist.nu", true);
pref("network.IDN.whitelist.nz", true);
pref("network.IDN.whitelist.pl", true);
pref("network.IDN.whitelist.pr", true);
pref("network.IDN.whitelist.se", true);
pref("network.IDN.whitelist.sh", true);
pref("network.IDN.whitelist.th", true);
pref("network.IDN.whitelist.tm", true);
pref("network.IDN.whitelist.tw", true);
pref("network.IDN.whitelist.vn", true);

// IDN ccTLDs
// ae, UAE, .<Emarat>
pref("network.IDN.whitelist.xn--mgbaam7a8h", true); 
// cn, China, .<China> with variants
pref("network.IDN.whitelist.xn--fiqz9s", true); // Traditional
pref("network.IDN.whitelist.xn--fiqs8s", true); // Simplified
// hk, Hong Kong, .<Hong Kong>
pref("network.IDN.whitelist.xn--j6w193g", true);
// ir, Iran, <.Iran> with variants
pref("network.IDN.whitelist.xn--mgba3a4f16a", true);
pref("network.IDN.whitelist.xn--mgba3a4fra", true);
// jo, Jordan, .<Al-Ordon>
pref("network.IDN.whitelist.xn--mgbayh7gpa", true);
// ru, Russian Federation, .<RF>
pref("network.IDN.whitelist.xn--p1ai", true);
// sa, Saudi Arabia, .<al-Saudiah> with variants
pref("network.IDN.whitelist.xn--mgberp4a5d4ar", true); 
pref("network.IDN.whitelist.xn--mgberp4a5d4a87g", true);
pref("network.IDN.whitelist.xn--mgbqly7c0a67fbc", true);
pref("network.IDN.whitelist.xn--mgbqly7cvafr", true);
// tw, Taiwan, <.Taiwan> with variants
pref("network.IDN.whitelist.xn--kpry57d", true);  // Traditional
pref("network.IDN.whitelist.xn--kprw13d", true);  // Simplified

// gTLDs
pref("network.IDN.whitelist.biz", true);
pref("network.IDN.whitelist.cat", true);
pref("network.IDN.whitelist.info", true);
pref("network.IDN.whitelist.museum", true);
pref("network.IDN.whitelist.org", true);
pref("network.IDN.whitelist.tel", true);

// NOTE: Before these can be removed, one of bug 414812's tests must be updated
//       or it will likely fail!  Please CC jwalden+bmo on the bug associated
//       with removing these so he can provide a patch to make the necessary
//       changes to avoid bustage.
// ".test" localised TLDs for ICANN's top-level IDN trial
pref("network.IDN.whitelist.xn--0zwm56d", true);
pref("network.IDN.whitelist.xn--11b5bs3a9aj6g", true);
pref("network.IDN.whitelist.xn--80akhbyknj4f", true);
pref("network.IDN.whitelist.xn--9t4b11yi5a", true);
pref("network.IDN.whitelist.xn--deba0ad", true);
pref("network.IDN.whitelist.xn--g6w251d", true);
pref("network.IDN.whitelist.xn--hgbk6aj7f53bba", true);
pref("network.IDN.whitelist.xn--hlcj6aya9esc7a", true);
pref("network.IDN.whitelist.xn--jxalpdlp", true);
pref("network.IDN.whitelist.xn--kgbechtv", true);
pref("network.IDN.whitelist.xn--zckzah", true);

// If a domain includes any of the following characters, it may be a spoof 
// attempt and so we always display the domain name as punycode. This would 
// override the settings "network.IDN_show_punycode" and 
// "network.IDN.whitelist.*".
pref("network.IDN.blacklist_chars", "\u0020\u00A0\u00BC\u00BD\u00BE\u01C3\u02D0\u0337\u0338\u0589\u05C3\u05F4\u0609\u060A\u066A\u06D4\u0701\u0702\u0703\u0704\u115F\u1160\u1735\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A\u200B\u2024\u2027\u2028\u2029\u202F\u2039\u203A\u2041\u2044\u2052\u205F\u2153\u2154\u2155\u2156\u2157\u2158\u2159\u215A\u215B\u215C\u215D\u215E\u215F\u2215\u2236\u23AE\u2571\u29F6\u29F8\u2AFB\u2AFD\u2FF0\u2FF1\u2FF2\u2FF3\u2FF4\u2FF5\u2FF6\u2FF7\u2FF8\u2FF9\u2FFA\u2FFB\u3000\u3002\u3014\u3015\u3033\u3164\u321D\u321E\u33AE\u33AF\u33C6\u33DF\uA789\uFE14\uFE15\uFE3F\uFE5D\uFE5E\uFEFF\uFF0E\uFF0F\uFF61\uFFA0\uFFF9\uFFFA\uFFFB\uFFFC\uFFFD");

// This preference specifies a list of domains for which DNS lookups will be
// IPv4 only. Works around broken DNS servers which can't handle IPv6 lookups
// and/or allows the user to disable IPv6 on a per-domain basis. See bug 68796.
pref("network.dns.ipv4OnlyDomains", "");

// This preference can be used to turn off IPv6 name lookups. See bug 68796.
pref("network.dns.disableIPv6", false);

// This preference controls whether or not URLs with UTF-8 characters are
// escaped.  Set this preference to TRUE for strict RFC2396 conformance.
pref("network.standard-url.escape-utf8", true);

// This preference controls whether or not URLs are always encoded and sent as
// UTF-8.
pref("network.standard-url.encode-utf8", true);

// This preference controls whether or not queries are encoded and sent as
// UTF-8.
pref("network.standard-url.encode-query-utf8", false);

// Idle timeout for ftp control connections - 5 minute default
pref("network.ftp.idleConnectionTimeout", 300);

// directory listing format
// 2: HTML
// 3: XUL directory viewer
// all other values are treated like 2
pref("network.dir.format", 2);

// enables the prefetch service (i.e., prefetching of <link rel="next"> URLs).
pref("network.prefetch-next", true);


// The following prefs pertain to the negotiate-auth extension (see bug 17578),
// which provides transparent Kerberos or NTLM authentication using the SPNEGO
// protocol.  Each pref is a comma-separated list of keys, where each key has
// the format:
//   [scheme "://"] [host [":" port]]
// For example, "foo.com" would match "http://www.foo.com/bar", etc.

// This list controls which URIs can use the negotiate-auth protocol.  This
// list should be limited to the servers you know you'll need to login to.
pref("network.negotiate-auth.trusted-uris", "");
// This list controls which URIs can support delegation.
pref("network.negotiate-auth.delegation-uris", "");

// Allow SPNEGO by default when challenged by a proxy server.
pref("network.negotiate-auth.allow-proxies", true);

// Path to a specific gssapi library
pref("network.negotiate-auth.gsslib", "");

// Specify if the gss lib comes standard with the OS
pref("network.negotiate-auth.using-native-gsslib", true);

#ifdef XP_WIN

// Default to using the SSPI intead of GSSAPI on windows 
pref("network.auth.use-sspi", true);

#endif

// Controls which NTLM authentication implementation we default to. True forces
// the use of our generic (internal) NTLM authentication implementation vs. any
// native implementation provided by the os. This pref is for diagnosing issues
// with native NTLM. (See bug 520607 for details.) Using generic NTLM authentication
// can expose the user to reflection attack vulnerabilities. Do not change this
// unless you know what you're doing!
// This pref should be removed 6 months after the release of firefox 3.6. 
pref("network.auth.force-generic-ntlm", false);

// The following prefs are used to enable automatic use of the operating
// system's NTLM implementation to silently authenticate the user with their
// Window's domain logon.  The trusted-uris pref follows the format of the
// trusted-uris pref for negotiate authentication.
pref("network.automatic-ntlm-auth.allow-proxies", true);
pref("network.automatic-ntlm-auth.trusted-uris", "");

// This preference controls whether or not the LM hash will be included in
// response to a NTLM challenge.  By default, this is disabled since servers
// should almost never need the LM hash, and the LM hash is what makes NTLM
// authentication less secure.  See bug 250691 for further details.
// NOTE: automatic-ntlm-auth which leverages the OS-provided NTLM
//       implementation will not be affected by this preference.
pref("network.ntlm.send-lm-response", false);

pref("permissions.default.image",           1); // 1-Accept, 2-Deny, 3-dontAcceptForeign

pref("network.proxy.type",                  5);
pref("network.proxy.ftp",                   "");
pref("network.proxy.ftp_port",              0);
pref("network.proxy.http",                  "");
pref("network.proxy.http_port",             0);
pref("network.proxy.ssl",                   "");
pref("network.proxy.ssl_port",              0);
pref("network.proxy.socks",                 "");
pref("network.proxy.socks_port",            0);
pref("network.proxy.socks_version",         5);
pref("network.proxy.socks_remote_dns",      false);
pref("network.proxy.no_proxies_on",         "localhost, 127.0.0.1");
pref("network.proxy.failover_timeout",      1800); // 30 minutes
pref("network.online",                      true); //online/offline
pref("network.cookie.cookieBehavior",       0); // 0-Accept, 1-dontAcceptForeign, 2-dontUse
pref("network.cookie.thirdparty.sessionOnly", false);
pref("network.cookie.lifetimePolicy",       0); // accept normally, 1-askBeforeAccepting, 2-acceptForSession,3-acceptForNDays
pref("network.cookie.alwaysAcceptSessionCookies", false);
pref("network.cookie.prefsMigrated",        false);
pref("network.cookie.lifetime.days",        90);

// The PAC file to load.  Ignored unless network.proxy.type is 2.
pref("network.proxy.autoconfig_url", "");

// If we cannot load the PAC file, then try again (doubling from interval_min
// until we reach interval_max or the PAC file is successfully loaded).
pref("network.proxy.autoconfig_retry_interval_min", 5);    // 5 seconds
pref("network.proxy.autoconfig_retry_interval_max", 300);  // 5 minutes

pref("converter.html2txt.structs",          true); // Output structured phrases (strong, em, code, sub, sup, b, i, u)
pref("converter.html2txt.header_strategy",  1); // 0 = no indention; 1 = indention, increased with header level; 2 = numbering and slight indention

pref("intl.accept_languages",               "chrome://global/locale/intl.properties");
pref("intl.accept_charsets",                "iso-8859-1,*,utf-8");
pref("intl.menuitems.alwaysappendaccesskeys","chrome://global/locale/intl.properties");
pref("intl.menuitems.insertseparatorbeforeaccesskeys","chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.static",     "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more1",      "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more2",      "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more3",      "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more4",      "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more5",      "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.unicode",    "UTF-8, UTF-16LE, UTF-16BE, UTF-32, UTF-32LE, UTF-32BE");
pref("intl.charsetmenu.mailedit",           "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.cache",      "");
pref("intl.charsetmenu.mailview.cache",     "");
pref("intl.charsetmenu.composer.cache",     "");
pref("intl.charsetmenu.browser.cache.size", 5);
pref("intl.charset.detector",               "chrome://global/locale/intl.properties");
pref("intl.charset.default",                "chrome://global-platform/locale/intl.properties");
pref("intl.ellipsis",                       "chrome://global-platform/locale/intl.properties");
pref("intl.locale.matchOS",                 false);
// fallback charset list for Unicode conversion (converting from Unicode)
// currently used for mail send only to handle symbol characters (e.g Euro, trademark, smartquotes)
// for ISO-8859-1
pref("intl.fallbackCharsetList.ISO-8859-1", "windows-1252");
pref("font.language.group",                 "chrome://global/locale/intl.properties");

// these locales have right-to-left UI
pref("intl.uidirection.ar", "rtl");
pref("intl.uidirection.he", "rtl");
pref("intl.uidirection.fa", "rtl");

pref("font.mathfont-family", "STIXNonUnicode, STIXSizeOneSym, STIXSize1, STIXGeneral, Standard Symbols L, DejaVu Sans, Cambria Math");

// Some CJK fonts have bad underline offset, their CJK character glyphs are overlapped (or adjoined)  to its underline.
// These fonts are ignored the underline offset, instead of it, the underline is lowered to bottom of its em descent.
pref("font.blacklist.underline_offset", "FangSong,Gulim,GulimChe,MingLiU,MingLiU-ExtB,MingLiU_HKSCS,MingLiU-HKSCS-ExtB,MS Gothic,MS Mincho,MS PGothic,MS PMincho,MS UI Gothic,PMingLiU,PMingLiU-ExtB,SimHei,SimSun,SimSun-ExtB,Hei,Kai,Apple LiGothic,Apple LiSung,Osaka");

pref("images.dither", "auto");
pref("security.directory",              "");

pref("signed.applets.codebase_principal_support", false);
pref("security.checkloaduri", true);
pref("security.xpconnect.plugin.unrestricted", true);
// security-sensitive dialogs should delay button enabling. In milliseconds.
pref("security.dialog_enable_delay", 2000);

pref("security.csp.enable", true);
pref("security.csp.debug", false);

// Modifier key prefs: default to Windows settings,
// menu access key = alt, accelerator key = control.
// Use 17 for Ctrl, 18 for Alt, 224 for Meta, 0 for none. Mac settings in macprefs.js
pref("ui.key.accelKey", 17);
pref("ui.key.menuAccessKey", 18);
pref("ui.key.generalAccessKey", -1);

// If generalAccessKey is -1, use the following two prefs instead.
// Use 0 for disabled, 1 for Shift, 2 for Ctrl, 4 for Alt, 8 for Meta
// (values can be combined, e.g. 5 for Alt+Shift)
pref("ui.key.chromeAccess", 4);
pref("ui.key.contentAccess", 5);

pref("ui.key.menuAccessKeyFocuses", false); // overridden below
pref("ui.key.saveLink.shift", true); // true = shift, false = meta

// Disable page loading activity cursor by default.
pref("ui.use_activity_cursor", false);

// Middle-mouse handling
pref("middlemouse.paste", false);
pref("middlemouse.openNewWindow", true);
pref("middlemouse.contentLoadURL", false);
pref("middlemouse.scrollbarPosition", false);

// Clipboard behavior
pref("clipboard.autocopy", false);

// mouse wheel scroll transaction period of time (in milliseconds)
pref("mousewheel.transaction.timeout", 1500);
// mouse wheel scroll transaction is held even if the mouse cursor is moved.
pref("mousewheel.transaction.ignoremovedelay", 100);

// Macbook touchpad two finger pixel scrolling
pref("mousewheel.enable_pixel_scrolling", true);

// prefs for app level mouse wheel scrolling acceleration.
// number of mousewheel clicks when acceleration starts
// acceleration can be turned off if pref is set to -1
pref("mousewheel.acceleration.start", -1);
// factor to be multiplied for constant acceleration
pref("mousewheel.acceleration.factor", 10);

// Prefs for override the system mouse wheel scrolling speed on the root
// content of the web pages.  When
// "mousewheel.system_scroll_override_on_root_content.enabled" is true and the system
// scrolling speed isn't customized by the user, the root content scrolling
// speed is multiplied by the following factors.  The value will be used as
// 1/100.  E.g., 200 means 2.00.
// NOTE: Even if "mousewheel.system_scroll_override_on_root_content.enabled" is
// true, when Gecko detects the user customized the system scrolling speed
// settings, the override isn't executed.
pref("mousewheel.system_scroll_override_on_root_content.vertical.factor", 200);
pref("mousewheel.system_scroll_override_on_root_content.horizontal.factor", 200);

// 0=lines, 1=pages, 2=history , 3=text size
pref("mousewheel.withnokey.action",0);
pref("mousewheel.withnokey.numlines",6);
pref("mousewheel.withnokey.sysnumlines",true);
pref("mousewheel.withcontrolkey.action",0);
pref("mousewheel.withcontrolkey.numlines",1);
pref("mousewheel.withcontrolkey.sysnumlines",true);
// mousewheel.withshiftkey, see the Mac note below.
pref("mousewheel.withshiftkey.action",0);
pref("mousewheel.withshiftkey.numlines",1);
pref("mousewheel.withshiftkey.sysnumlines",true);
pref("mousewheel.withaltkey.action",2);
pref("mousewheel.withaltkey.numlines",1);
pref("mousewheel.withaltkey.sysnumlines",false);
pref("mousewheel.withmetakey.action",0);
pref("mousewheel.withmetakey.numlines",1);
pref("mousewheel.withmetakey.sysnumlines",true);

// activate horizontal scrolling by default
pref("mousewheel.horizscroll.withnokey.action",0);
pref("mousewheel.horizscroll.withnokey.numlines",1);
pref("mousewheel.horizscroll.withnokey.sysnumlines",true);
pref("mousewheel.horizscroll.withcontrolkey.action",0);
pref("mousewheel.horizscroll.withcontrolkey.numlines",1);
pref("mousewheel.horizscroll.withcontrolkey.sysnumlines",true);
pref("mousewheel.horizscroll.withshiftkey.action",0);
pref("mousewheel.horizscroll.withshiftkey.numlines",1);
pref("mousewheel.horizscroll.withshiftkey.sysnumlines",true);
pref("mousewheel.horizscroll.withaltkey.action",2);
pref("mousewheel.horizscroll.withaltkey.numlines",-1);
pref("mousewheel.horizscroll.withaltkey.sysnumlines",false);
pref("mousewheel.horizscroll.withmetakey.action",0);
pref("mousewheel.horizscroll.withmetakey.numlines",1);
pref("mousewheel.horizscroll.withmetakey.sysnumlines",true);

pref("profile.confirm_automigration",true);
// profile.migration_behavior determines how the profiles root is set
// 0 - use NS_APP_USER_PROFILES_ROOT_DIR
// 1 - create one based on the NS4.x profile root
// 2 - use, if not empty, profile.migration_directory otherwise same as 0
pref("profile.migration_behavior",0);
pref("profile.migration_directory", "");

// the amount of time (in seconds) that must elapse
// before we think your mozilla profile is defunct
// and you'd benefit from re-migrating from 4.x
// see bug #137886 for more details
//
// if -1, we never think your profile is defunct
// and users will never see the remigrate UI.
pref("profile.seconds_until_defunct", -1);
// We can show it anytime from menus
pref("profile.manage_only_at_launch", false);

pref("prefs.converted-to-utf8",false);

// --------------------------------------------------
// IBMBIDI 
// --------------------------------------------------
//
// ------------------
//  Text Direction
// ------------------
// 1 = directionLTRBidi *
// 2 = directionRTLBidi
pref("bidi.direction", 1);
// ------------------
//  Text Type
// ------------------
// 1 = charsettexttypeBidi *
// 2 = logicaltexttypeBidi
// 3 = visualtexttypeBidi
pref("bidi.texttype", 1);
// ------------------
//  Numeral Style
// ------------------
// 0 = nominalnumeralBidi *
// 1 = regularcontextnumeralBidi
// 2 = hindicontextnumeralBidi
// 3 = arabicnumeralBidi
// 4 = hindinumeralBidi
// 5 = persiancontextnumeralBidi
// 6 = persiannumeralBidi
pref("bidi.numeral", 0);
// ------------------
//  Support Mode
// ------------------
// 1 = mozillaBidisupport *
// 2 = OsBidisupport
// 3 = disableBidisupport
pref("bidi.support", 1);
// ------------------
//  Charset Mode
// ------------------
// 1 = doccharactersetBidi *
// 2 = defaultcharactersetBidi
pref("bidi.characterset", 1);
// Whether delete and backspace should immediately delete characters not
// visually adjacent to the caret, or adjust the visual position of the caret
// on the first keypress and delete the character on a second keypress
pref("bidi.edit.delete_immediately", false);

// Bidi caret movement style:
// 0 = logical
// 1 = visual
// 2 = visual, but logical during selection
pref("bidi.edit.caret_movement_style", 2);

// Setting this pref to |true| forces Bidi UI menu items and keyboard shortcuts
// to be exposed, and enables the directional caret hook. By default, only
// expose it for bidi-associated system locales.
pref("bidi.browser.ui", false);

// used for double-click word selection behavior. Win will override.
pref("layout.word_select.eat_space_to_next_word", false);
pref("layout.word_select.stop_at_punctuation", true);

// controls caret style and word-delete during text selection
// 0 = use platform default
// 1 = caret moves and blinks as when there is no selection; word
//     delete deselects the selection and then deletes word (Windows default)
// 2 = caret moves to selection edge and is not visible during selection; 
//     word delete deletes the selection (Mac default)
// 3 = caret moves and blinks as when there is no selection; word delete
//     deletes the selection (Unix default)
pref("layout.selection.caret_style", 0);

// pref to control whether or not to replace backslashes with Yen signs
// in documents encoded in one of Japanese legacy encodings (EUC-JP, 
// Shift_JIS, ISO-2022-JP)
pref("layout.enable_japanese_specific_transform", false);

// pref to force frames to be resizable
pref("layout.frames.force_resizability", false);

// pref to report CSS errors to the error console
pref("layout.css.report_errors", true);

// Should the :visited selector ever match (otherwise :link matches instead)?
pref("layout.css.visited_links_enabled", true);

// Override DPI. A value of -1 means use the maximum of 96 and the system DPI.
// A value of 0 means use the system DPI. A positive value is used as the DPI.
// This sets the physical size of a device pixel and thus controls the
// interpretation of physical units such as "pt".
pref("layout.css.dpi", -1);

// Set the number of device pixels per CSS pixel. A value <= 0 means choose
// automatically based on user settings for the platform (e.g., "UI scale factor"
// on Mac). A positive value is used as-is. This effectively controls the size
// of a CSS "px". This is only used for windows on the screen, not for printing.
// XXX the default value here should be 0, but before we can set it to 0,
// we have to get this feature working on all platforms.
pref("layout.css.devPixelsPerPx", "1.0");

// pref for which side vertical scrollbars should be on
// 0 = end-side in UI direction
// 1 = end-side in document/content direction
// 2 = right
// 3 = left
pref("layout.scrollbar.side", 0);

// pref to control browser frame rate, in Hz. A value <= 0 means choose
// automatically based on knowledge of the platform (or 60Hz if no platform-
// specific information is available).
pref("layout.frame_rate", -1);

// pref to control precision of the frame rate timer. When true,
// we use a "precise" timer, which means each notification fires
// Nms after the start of the last notification. That means if the
// processing of the notification is slow, the timer can fire immediately
// after we've just finished processing the last notification, which might
// lead to starvation problems.
// When false, we use a "slack" timer which fires Nms after the *end*
// of the last notification. This can give less tight frame rates
// but provides more time for other operations when the browser is
// heavily loaded.
pref("layout.frame_rate.precise", false);

// pref to permit users to make verified SOAP calls by default
pref("capability.policy.default.SOAPCall.invokeVerifySourceHeader", "allAccess");

// if true, allow plug-ins to override internal imglib decoder mime types in full-page mode
pref("plugin.override_internal_types", false);
pref("plugin.expose_full_path", false); // if true navigator.plugins reveals full path

// See bug 136985.  Gives embedders a pref to hook into to show
// a popup blocker if they choose.
pref("browser.popups.showPopupBlocker", true);

// Pref to control whether the viewmanager code does double-buffering or not
// See http://bugzilla.mozilla.org/show_bug.cgi?id=169483 for further details...
pref("viewmanager.do_doublebuffering", true);

// whether use prefs from system
pref("config.use_system_prefs", false);

// if the system has enabled accessibility
pref("config.use_system_prefs.accessibility", false);

// enable single finger gesture input (win7+ tablets)
pref("gestures.enable_single_finger_input", true);

/*
 * What are the entities that you want Mozilla to save using mnemonic
 * names rather than numeric codes? E.g. If set, we'll output &nbsp;
 * otherwise, we may output 0xa0 depending on the charset.
 *
 * "none"   : don't use any entity names; only use numeric codes.
 * "basic"  : use entity names just for &nbsp; &amp; &lt; &gt; &quot; for 
 *            interoperability/exchange with products that don't support more
 *            than that.
 * "latin1" : use entity names for 8bit accented letters and other special
 *            symbols between 128 and 255.
 * "html"   : use entity names for 8bit accented letters, greek letters, and
 *            other special markup symbols as defined in HTML4.
 */
//pref("editor.encode_entity",                 "html");

pref("editor.resizing.preserve_ratio",       true);
pref("editor.positioning.offset",            0);

pref("dom.max_chrome_script_run_time", 20);
pref("dom.max_script_run_time", 10);

#ifndef DEBUG
// How long a plugin is allowed to process a synchronous IPC message
// before we consider it "hung".
pref("dom.ipc.plugins.timeoutSecs", 45);
// How long a plugin launch is allowed to take before
// we consider it failed.
pref("dom.ipc.plugins.processLaunchTimeoutSecs", 45);
#else
// No timeout in DEBUG builds
pref("dom.ipc.plugins.timeoutSecs", 0);
pref("dom.ipc.plugins.processLaunchTimeoutSecs", 0);
#endif

#ifdef XP_WIN
// Disable oopp for java on windows. They run their own
// process isolation which conflicts with our implementation.
pref("dom.ipc.plugins.java.enabled", false);
#endif

#ifndef ANDROID
#ifndef XP_MACOSX
#ifdef XP_UNIX
// Linux plugins using Xt instead of Xembed don't work out-of-process yet.
pref("dom.ipc.plugins.enabled.libvlcplugin.so", false);
pref("dom.ipc.plugins.enabled.nppdf.so", false);
#endif
#endif
#endif

pref("svg.enabled", true);
pref("svg.smil.enabled", true);

pref("font.minimum-size.ar", 0);
pref("font.minimum-size.x-armn", 0);
pref("font.minimum-size.x-beng", 0);
pref("font.minimum-size.x-baltic", 0);
pref("font.minimum-size.x-central-euro", 0);
pref("font.minimum-size.zh-CN", 0);
pref("font.minimum-size.zh-HK", 0);
pref("font.minimum-size.zh-TW", 0);
pref("font.minimum-size.x-cyrillic", 0);
pref("font.minimum-size.x-devanagari", 0);
pref("font.minimum-size.x-ethi", 0);
pref("font.minimum-size.x-geor", 0);
pref("font.minimum-size.el", 0);
pref("font.minimum-size.x-gujr", 0);
pref("font.minimum-size.x-guru", 0);
pref("font.minimum-size.he", 0);
pref("font.minimum-size.ja", 0);
pref("font.minimum-size.x-knda", 0);
pref("font.minimum-size.x-khmr", 0);
pref("font.minimum-size.ko", 0);
pref("font.minimum-size.x-mlym", 0);
pref("font.minimum-size.x-orya", 0);
pref("font.minimum-size.x-sinh", 0);
pref("font.minimum-size.x-tamil", 0);
pref("font.minimum-size.x-telu", 0);
pref("font.minimum-size.x-tibt", 0);
pref("font.minimum-size.th", 0);
pref("font.minimum-size.tr", 0);
pref("font.minimum-size.x-cans", 0);
pref("font.minimum-size.x-western", 0);
pref("font.minimum-size.x-unicode", 0);
pref("font.minimum-size.x-user-def", 0);

#ifdef XP_WIN

pref("font.name.serif.ar", "Times New Roman");
pref("font.name.sans-serif.ar", "Arial");
pref("font.name.monospace.ar", "Courier New");
pref("font.name.cursive.ar", "Comic Sans MS");

pref("font.name.serif.el", "Times New Roman");
pref("font.name.sans-serif.el", "Arial");
pref("font.name.monospace.el", "Courier New");
pref("font.name.cursive.el", "Comic Sans MS");

pref("font.name.serif.he", "Narkisim");
pref("font.name.sans-serif.he", "Arial");
pref("font.name.monospace.he", "Fixed Miriam Transparent");
pref("font.name.cursive.he", "Guttman Yad");
pref("font.name-list.serif.he", "Narkisim, David");
pref("font.name-list.monospace.he", "Fixed Miriam Transparent, Miriam Fixed, Rod, Courier New");
pref("font.name-list.cursive.he", "Guttman Yad, Ktav, Arial");

pref("font.name.serif.ja", "ＭＳ Ｐ明朝"); // "MS PMincho"
pref("font.name.sans-serif.ja", "ＭＳ Ｐゴシック"); // "MS PGothic"
pref("font.name.monospace.ja", "ＭＳ ゴシック"); // "MS Gothic"
pref("font.name-list.serif.ja", "MS PMincho, MS Mincho, MS PGothic, MS Gothic");
pref("font.name-list.sans-serif.ja", "MS PGothic, MS Gothic, MS PMincho, MS Mincho");
pref("font.name-list.monospace.ja", "MS Gothic, MS Mincho, MS PGothic, MS PMincho");

pref("font.name.serif.ko", "바탕"); // "Batang" 
pref("font.name.sans-serif.ko", "굴림"); // "Gulim" 
pref("font.name.monospace.ko", "굴림체"); // "GulimChe" 
pref("font.name.cursive.ko", "궁서"); // "Gungseo"

pref("font.name-list.serif.ko", "Batang, Gulim"); 
pref("font.name-list.sans-serif.ko", "Gulim"); 
pref("font.name-list.monospace.ko", "GulimChe"); 
pref("font.name-list.cursive.ko", "Gungseo"); 

pref("font.name.serif.th", "Tahoma");
pref("font.name.sans-serif.th", "Tahoma");
pref("font.name.monospace.th", "Tahoma");
pref("font.name.cursive.th", "Tahoma");

pref("font.name.serif.tr", "Times New Roman");
pref("font.name.sans-serif.tr", "Arial");
pref("font.name.monospace.tr", "Courier New");
pref("font.name.cursive.tr", "Comic Sans MS");

pref("font.name.serif.x-baltic", "Times New Roman");
pref("font.name.sans-serif.x-baltic", "Arial");
pref("font.name.monospace.x-baltic", "Courier New");
pref("font.name.cursive.x-baltic", "Comic Sans MS");

pref("font.name.serif.x-central-euro", "Times New Roman");
pref("font.name.sans-serif.x-central-euro", "Arial");
pref("font.name.monospace.x-central-euro", "Courier New");
pref("font.name.cursive.x-central-euro", "Comic Sans MS");

pref("font.name.serif.x-cyrillic", "Times New Roman");
pref("font.name.sans-serif.x-cyrillic", "Arial");
pref("font.name.monospace.x-cyrillic", "Courier New");
pref("font.name.cursive.x-cyrillic", "Comic Sans MS");

pref("font.name.serif.x-unicode", "Times New Roman");
pref("font.name.sans-serif.x-unicode", "Arial");
pref("font.name.monospace.x-unicode", "Courier New");
pref("font.name.cursive.x-unicode", "Comic Sans MS");

pref("font.name.serif.x-western", "Times New Roman");
pref("font.name.sans-serif.x-western", "Arial");
pref("font.name.monospace.x-western", "Courier New");
pref("font.name.cursive.x-western", "Comic Sans MS");

pref("font.name.serif.zh-CN", "宋体"); //MS Song
pref("font.name.sans-serif.zh-CN", "宋体"); //MS Song
pref("font.name.monospace.zh-CN", "宋体"); //MS Song
pref("font.name-list.serif.zh-CN", "MS Song, SimSun");
pref("font.name-list.sans-serif.zh-CN", "MS Song, SimSun");
pref("font.name-list.monospace.zh-CN", "MS Song, SimSun");

// Per Taiwanese users' demand. They don't want to use TC fonts for
// rendering Latin letters. (bug 88579)
pref("font.name.serif.zh-TW", "Times New Roman"); 
pref("font.name.sans-serif.zh-TW", "Arial");
pref("font.name.monospace.zh-TW", "細明體");  // MingLiU
pref("font.name-list.serif.zh-TW", "PMingLiu, MingLiU"); 
pref("font.name-list.sans-serif.zh-TW", "PMingLiU, MingLiU");
pref("font.name-list.monospace.zh-TW", "MingLiU");

// hkscsm3u.ttf (HKSCS-2001) :  http://www.microsoft.com/hk/hkscs 
// Hong Kong users have the same demand about glyphs for Latin letters (bug 88579) 
pref("font.name.serif.zh-HK", "Times New Roman"); 
pref("font.name.sans-serif.zh-HK", "Arial");
pref("font.name.monospace.zh-HK", "細明體_HKSCS"); 
pref("font.name-list.serif.zh-HK", "MingLiu_HKSCS, Ming(for ISO10646), MingLiU"); 
pref("font.name-list.sans-serif.zh-HK", "MingLiU_HKSCS, Ming(for ISO10646), MingLiU");  
pref("font.name-list.monospace.zh-HK", "MingLiU_HKSCS, Ming(for ISO10646), MingLiU");

pref("font.name.serif.x-devanagari", "Mangal");
pref("font.name.sans-serif.x-devanagari", "Raghindi");
pref("font.name.monospace.x-devanagari", "Mangal");
pref("font.name-list.serif.x-devanagari", "Mangal, Raghindi");
pref("font.name-list.monospace.x-devanagari", "Mangal, Raghindi");

pref("font.name.serif.x-tamil", "Latha");
pref("font.name.sans-serif.x-tamil", "Code2000");
pref("font.name.monospace.x-tamil", "Latha");
pref("font.name-list.serif.x-tamil", "Latha, Code2000");
pref("font.name-list.monospace.x-tamil", "Latha, Code2000");

# http://www.alanwood.net/unicode/fonts.html

pref("font.name.serif.x-armn", "Sylfaen");
pref("font.name.sans-serif.x-armn", "Arial AMU");
pref("font.name.monospace.x-armn", "Arial AMU");
pref("font.name-list.serif.x-armn", "Sylfaen,Arial Unicode MS, Code2000");
pref("font.name-list.monospace.x-armn", "Arial AMU, Arial Unicode MS, Code2000");

pref("font.name.serif.x-beng", "Vrinda");
pref("font.name.sans-serif.x-beng", "Vrinda");
pref("font.name.monospace.x-beng", "Mitra Mono");
pref("font.name-list.serif.x-beng", "Vrinda, Akaash, Likhan, Ekushey Punarbhaba, Code2000, Arial Unicode MS"); 
pref("font.name-list.sans-serif.x-beng", "Vrinda, Akaash, Likhan, Ekushey Punarbhaba, Code2000, Arial Unicode MS"); 
pref("font.name-list.monospace.x-beng", "Likhan, Mukti Narrow, Code2000, Arial Unicode MS");

pref("font.name.serif.x-cans", "Aboriginal Serif");
pref("font.name.sans-serif.x-cans", "Aboriginal Sans");
pref("font.name.monospace.x-cans", "Aboriginal Sans");
pref("font.name-list.serif.x-cans", "Aboriginal Serif, BJCree Uni");
pref("font.name-list.monospace.x-cans", "Aboriginal Sans, OskiDakelh, Pigiarniq, Uqammaq");

pref("font.name.serif.x-ethi", "Visual Geez Unicode");
pref("font.name.sans-serif.x-ethi", "GF Zemen Unicode");
pref("font.name.cursive.x-ethi", "Visual Geez Unicode Title");
pref("font.name.monospace.x-ethi", "Ethiopia Jiret");
pref("font.name-list.serif.x-ethi", "Visual Geez Unicode, Visual Geez Unicode Agazian, Code2000");
pref("font.name-list.monospace.x-ethi", "Ethiopia Jiret, Code2000");

pref("font.name.serif.x-geor", "Sylfaen");
pref("font.name.sans-serif.x-geor", "BPG Classic 99U");
pref("font.name.monospace.x-geor", "Code2000");
pref("font.name-list.serif.x-geor", "Sylfaen, BPG Paata Khutsuri U, TITUS Cyberbit Basic"); 
pref("font.name-list.monospace.x-geor", "BPG Classic 99U, Code2000, Arial Unicode MS");

pref("font.name.serif.x-gujr", "Shruti");
pref("font.name.sans-serif.x-gujr", "Shruti");
pref("font.name.monospace.x-gujr", "Code2000");
pref("font.name-list.serif.x-gujr", "Shruti, Code2000, Arial Unicode MS"); 
pref("font.name-list.monospace.x-gujr", "Code2000, Shruti, Arial Unicode MS");

pref("font.name.serif.x-guru", "Raavi");
pref("font.name.sans-serif.x-guru", "Code2000");
pref("font.name.monospace.x-guru", "Code2000");
pref("font.name-list.serif.x-guru", "Raavi, Saab, Code2000, Arial Unicode MS"); 
pref("font.name-list.monospace.x-guru", "Code2000, Raavi, Saab, Arial Unicode MS");

pref("font.name.serif.x-khmr", "PhnomPenh OT");
pref("font.name.sans-serif.x-khmr", "Khmer OS");
pref("font.name.monospace.x-khmr", "Code2000");
pref("font.name-list.serif.x-khmr", "PhnomPenh OT,.Mondulkiri U GR 1.5, Khmer OS");
pref("font.name-list.monospace.x-khmr", "Code2000, Khmer OS, Khmer OS System");

pref("font.name.serif.x-mlym", "Rachana_w01");
pref("font.name.sans-serif.x-mlym", "Rachana_w01");
pref("font.name.monospace.x-mlym", "Rachana_w01");
pref("font.name-list.serif.x-mlym", "AnjaliOldLipi, Kartika, ThoolikaUnicode, Code2000, Arial Unicode MS");
pref("font.name-list.sans-serif.x-mlym", "AnjaliOldLipi, Kartika, ThoolikaUnicode, Code2000, Arial Unicode MS");
pref("font.name-list.monospace.x-mlym", "AnjaliOldLipi, Kartika, ThoolikaUnicode, Code2000, Arial Unicode MS");

pref("font.name.serif.x-orya", "ori1Uni");
pref("font.name.sans-serif.x-orya", "ori1Uni");
pref("font.name.monospace.x-orya", "ori1Uni");
pref("font.name-list.serif.x-orya", "Kalinga, ori1Uni, Code2000, Arial Unicode MS");
pref("font.name-list.sans-serif.x-orya", "Kalinga, ori1Uni, Code2000, Arial Unicode MS");
pref("font.name-list.monospace.x-orya", "Kalinga, ori1Uni, Code2000, Arial Unicode MS");

pref("font.name.serif.x-telu", "Gautami");
pref("font.name.sans-serif.x-telu", "Gautami");
pref("font.name.monospace.x-telu", "Gautami");
pref("font.name-list.serif.x-telu", "Gautami, Akshar Unicode, Code2000, Arial Unicode MS");
pref("font.name-list.sans-serif.x-telu", "Gautami, Akshar Unicode, Code2000, Arial Unicode MS");
pref("font.name-list.monospace.x-telu", "Gautami, Akshar Unicode, Code2000, Arial Unicode MS");

pref("font.name.serif.x-knda", "Tunga");
pref("font.name.sans-serif.x-knda", "Tunga");
pref("font.name.monospace.x-knda", "Tunga");
pref("font.name-list.serif.x-knda", "Tunga, AksharUnicode, Code2000, Arial Unicode MS");
pref("font.name-list.sans-serif.x-knda", "Tunga, AksharUnicode, Code2000, Arial Unicode MS");
pref("font.name-list.monospace.x-knda", "Tunga, AksharUnicode, Code2000, Arial Unicode MS");

pref("font.name.serif.x-sinh", "Iskoola Pota");
pref("font.name.sans-serif.x-sinh", "Iskoola Pota");
pref("font.name.monospace.x-sinh", "Iskoola Pota");
pref("font.name-list.serif.x-sinh", "Iskoola Pota, AksharUnicode");
pref("font.name-list.sans-serif.x-sinh", "Iskoola Pota, AksharUnicode");
pref("font.name-list.monospace.x-sinh", "Iskoola Pota, AksharUnicode");

pref("font.name.serif.x-tibt", "Tibetan Machine Uni");
pref("font.name.sans-serif.x-tibt", "Tibetan Machine Uni");
pref("font.name.monospace.x-tibt", "Tibetan Machine Uni");
pref("font.name-list.serif.x-tibt", "Tibetan Machine Uni, Jomolhari, Microsoft Himalaya");
pref("font.name-list.sans-serif.x-tibt", "Tibetan Machine Uni, Jomolhari, Microsoft Himalaya");
pref("font.name-list.monospace.x-tibt", "Tibetan Machine Uni, Jomolhari, Microsoft Himalaya");

pref("font.default.ar", "sans-serif");
pref("font.size.variable.ar", 16);
pref("font.size.fixed.ar", 13);

pref("font.default.el", "serif");
pref("font.size.variable.el", 16);
pref("font.size.fixed.el", 13);

pref("font.default.he", "sans-serif");
pref("font.size.variable.he", 16);
pref("font.size.fixed.he", 13);

pref("font.default.ja", "sans-serif");
pref("font.size.variable.ja", 16);
pref("font.size.fixed.ja", 16);

pref("font.default.ko", "sans-serif");
pref("font.size.variable.ko", 16);
pref("font.size.fixed.ko", 16);

pref("font.default.th", "serif");
pref("font.size.variable.th", 16);
pref("font.size.fixed.th", 13);
pref("font.minimum-size.th", 10);

pref("font.default.tr", "serif");
pref("font.size.variable.tr", 16);
pref("font.size.fixed.tr", 13);

pref("font.default.x-baltic", "serif");
pref("font.size.variable.x-baltic", 16);
pref("font.size.fixed.x-baltic", 13);

pref("font.default.x-central-euro", "serif");
pref("font.size.variable.x-central-euro", 16);
pref("font.size.fixed.x-central-euro", 13);

pref("font.default.x-cyrillic", "serif");
pref("font.size.variable.x-cyrillic", 16);
pref("font.size.fixed.x-cyrillic", 13);

pref("font.default.x-devanagari", "serif");
pref("font.size.variable.x-devanagari", 16);
pref("font.size.fixed.x-devanagari", 13);

pref("font.default.x-tamil", "serif");
pref("font.size.variable.x-tamil", 16);
pref("font.size.fixed.x-tamil", 13);

pref("font.default.x-armn", "serif");
pref("font.size.variable.x-armn", 16);
pref("font.size.fixed.x-armn", 13);

pref("font.default.x-beng", "serif");
pref("font.size.variable.x-beng", 16);
pref("font.size.fixed.x-beng", 13);

pref("font.default.x-cans", "serif");
pref("font.size.variable.x-cans", 16);
pref("font.size.fixed.x-cans", 13);

pref("font.default.x-ethi", "serif");
pref("font.size.variable.x-ethi", 16);
pref("font.size.fixed.x-ethi", 13);

pref("font.default.x-geor", "serif");
pref("font.size.variable.x-geor", 16);
pref("font.size.fixed.x-geor", 13);

pref("font.default.x-gujr", "serif");
pref("font.size.variable.x-gujr", 16);
pref("font.size.fixed.x-gujr", 13);

pref("font.default.x-guru", "serif");
pref("font.size.variable.x-guru", 16);
pref("font.size.fixed.x-guru", 13);

pref("font.default.x-khmr", "serif");
pref("font.size.variable.x-khmr", 16);
pref("font.size.fixed.x-khmr", 13);

pref("font.default.x-mlym", "serif");
pref("font.size.variable.x-mlym", 16);
pref("font.size.fixed.x-mlym", 13);

pref("font.default.x-orya", "serif");
pref("font.size.variable.x-orya", 16);
pref("font.size.fixed.x-orya", 13);

pref("font.default.x-telu", "serif");
pref("font.size.variable.x-telu", 16);
pref("font.size.fixed.x-telu", 13);

pref("font.default.x-knda", "serif");
pref("font.size.variable.x-knda", 16);
pref("font.size.fixed.x-knda", 13);

pref("font.default.x-sinh", "serif");
pref("font.size.variable.x-sinh", 16);
pref("font.size.fixed.x-sinh", 13);

pref("font.default.x-tibt", "serif");
pref("font.size.variable.x-tibt", 16);
pref("font.size.fixed.x-tibt", 13);

pref("font.default.x-unicode", "serif");
pref("font.size.variable.x-unicode", 16);
pref("font.size.fixed.x-unicode", 13);

pref("font.default.x-western", "serif");
pref("font.size.variable.x-western", 16);
pref("font.size.fixed.x-western", 13);

pref("font.default.zh-CN", "sans-serif");
pref("font.size.variable.zh-CN", 16);
pref("font.size.fixed.zh-CN", 16);

pref("font.default.zh-TW", "sans-serif");
pref("font.size.variable.zh-TW", 16);
pref("font.size.fixed.zh-TW", 16);

pref("font.default.zh-HK", "sans-serif");
pref("font.size.variable.zh-HK", 16);
pref("font.size.fixed.zh-HK", 16);

// We have special support for Monotype Symbol on Windows.
pref("font.mathfont-family", "STIXNonUnicode, STIXSizeOneSym, STIXSize1, STIXGeneral, Symbol, DejaVu Sans, Cambria Math");

// cleartype settings - false implies default system settings 

// use cleartype rendering for downloadable fonts (win xp only)
pref("gfx.font_rendering.cleartype.use_for_downloadable_fonts", true);

// use cleartype rendering for all fonts always (win xp only)
pref("gfx.font_rendering.cleartype.always_use_for_content", false);

pref("ui.key.menuAccessKeyFocuses", true);

// override double-click word selection behavior.
pref("layout.word_select.eat_space_to_next_word", true);

// scrollbar snapping region
pref("slider.snapMultiplier", 6);

// print_extra_margin enables platforms to specify an extra gap or margin
// around the content of the page for Print Preview only
pref("print.print_extra_margin", 90); // twips (90 twips is an eigth of an inch)

// Whether to extend the native dialog with information on printing frames.
pref("print.extend_native_print_dialog", true);

// Locate Java by scanning the Sun JRE installation directory with a minimum version
// Note: Does not scan if security.enable_java is not true
pref("plugin.scan.SunJRE", "1.3");

// Locate plugins by scanning the Adobe Acrobat installation directory with a minimum version
pref("plugin.scan.Acrobat", "5.0");

// Locate plugins by scanning the Quicktime installation directory with a minimum version
pref("plugin.scan.Quicktime", "5.0");

// Locate and scan the Window Media Player installation directory for plugins with a minimum version
pref("plugin.scan.WindowsMediaPlayer", "7.0");

// Locate plugins by the directories specified in the Windows registry for PLIDs
// Which is currently HKLM\Software\MozillaPlugins\xxxPLIDxxx\Path
pref("plugin.scan.plid.all", true);

// Controls the scanning of the Navigator 4.x directory for plugins
// When pref is missing, the default is to pickup popular plugins such as
// Flash, Shockwave, Acrobat, and Quicktime. If set to true, ALL plugins
// will be picked up and if set to false the scan will not happen at all
//pref("plugin.scan.4xPluginFolder", false);

// Help Windows NT, 2000, and XP dialup a RAS connection
// when a network address is unreachable.
pref("network.autodial-helper.enabled", true);

// Pref to control whether we set ddeexec subkeys for the http
// Internet shortcut protocol if we are handling it.  These
// subkeys will be set only while we are running (to avoid the
// problem of Windows showing an alert when it tries to use DDE
// and we're not already running).
pref("advanced.system.supportDDEExec", true);

// Switch the keyboard layout per window
pref("intl.keyboard.per_window_layout", false);

#ifdef NS_ENABLE_TSF
// Enable/Disable TSF support
pref("intl.enable_tsf_support", false);

// We need to notify the layout change to TSF, but we cannot check the actual
// change now, therefore, we always notify it by this fequency.
pref("intl.tsf.on_layout_change_interval", 100);
#endif

#ifdef WINCE
// bug 506798 - can't type in bookmarks panel on WinCE
pref("ui.panel.default_level_parent", true);
#else
// See bug 448927, on topmost panel, some IMEs are not usable on Windows.
pref("ui.panel.default_level_parent", false);
#endif

pref("mousewheel.system_scroll_override_on_root_content.enabled", true);

// If your mouse drive sends WM_*SCROLL messages when you turn your mouse wheel,
// set this to true.  Then, gecko processes them as mouse wheel messages.
pref("mousewheel.emulate_at_wm_scroll", false);

// Bug 514927
// Enables or disabled the TrackPoint hack, -1 is autodetect, 0 is off,
// and 1 is on.  Set this to 1 if TrackPoint scrolling is not working.
pref("ui.trackpoint_hack.enabled", -1);
# WINNT
#endif

#ifdef XP_MACOSX
// Mac specific preference defaults
pref("browser.drag_out_of_frame_style", 1);
pref("ui.key.saveLink.shift", false); // true = shift, false = meta

// default fonts (in UTF8 and using canonical names)
// to determine canonical font names, use a debug build and 
// enable NSPR logging for module fontInfoLog:5
// canonical names immediately follow '(fontinit) family:' in the log

pref("font.name.serif.ar", "Al Bayan");
pref("font.name.sans-serif.ar", "Geeza Pro");
pref("font.name.monospace.ar", "Geeza Pro");
pref("font.name.cursive.ar", "DecoType Naskh");
pref("font.name.fantasy.ar", "KufiStandardGK");
pref("font.name-list.serif.ar", "Al Bayan");
pref("font.name-list.sans-serif.ar", "Geeza Pro");
pref("font.name-list.monospace.ar", "Geeza Pro");
pref("font.name-list.cursive.ar", "DecoType Naskh");
pref("font.name-list.fantasy.ar", "KufiStandardGK");

pref("font.name.serif.el", "Lucida Grande");
pref("font.name.sans-serif.el", "Lucida Grande");
pref("font.name.monospace.el", "Lucida Grande");
pref("font.name.cursive.el", "Lucida Grande");
pref("font.name.fantasy.el", "Lucida Grande");
pref("font.name-list.serif.el", "Lucida Grande");
pref("font.name-list.sans-serif.el", "Lucida Grande");
pref("font.name-list.monospace.el", "Lucida Grande");
pref("font.name-list.cursive.el", "Lucida Grande");
pref("font.name-list.fantasy.el", "Lucida Grande");

pref("font.name.serif.he", "Raanana");
pref("font.name.sans-serif.he", "Arial Hebrew");
pref("font.name.monospace.he", "Arial Hebrew");
pref("font.name.cursive.he", "Corsiva Hebrew");
pref("font.name.fantasy.he", "Corsiva Hebrew");
pref("font.name-list.serif.he", "Raanana");
pref("font.name-list.sans-serif.he", "Arial Hebrew");
pref("font.name-list.monospace.he", "Arial Hebrew");
pref("font.name-list.cursive.he", "Corsiva Hebrew");
pref("font.name-list.fantasy.he", "Corsiva Hebrew");

pref("font.name.serif.ja", "Hiragino Mincho Pro"); 
pref("font.name.sans-serif.ja", "Hiragino Kaku Gothic Pro"); 
pref("font.name.monospace.ja", "Osaka-Mono"); 
pref("font.name-list.serif.ja", "Hiragino Mincho Pro"); 
pref("font.name-list.sans-serif.ja", "Hiragino Kaku Gothic Pro"); 
pref("font.name-list.monospace.ja", "Osaka-Mono"); 

pref("font.name.serif.ko", "AppleMyungjo"); 
pref("font.name.sans-serif.ko", "AppleGothic"); 
pref("font.name.monospace.ko", "AppleGothic"); 
pref("font.name-list.serif.ko", "AppleMyungjo"); 
pref("font.name-list.sans-serif.ko", "AppleGothic"); 
pref("font.name-list.monospace.ko", "AppleGothic"); 

pref("font.name.serif.th", "Thonburi");
pref("font.name.sans-serif.th", "Thonburi");
pref("font.name.monospace.th", "Ayuthaya");
pref("font.name-list.serif.th", "Thonburi");
pref("font.name-list.sans-serif.th", "Thonburi");
pref("font.name-list.monospace.th", "Ayuthaya");

pref("font.name.serif.tr", "Times");
pref("font.name.sans-serif.tr", "Helvetica");
pref("font.name.monospace.tr", "Courier");
pref("font.name.cursive.tr", "Apple Chancery");
pref("font.name.fantasy.tr", "Papyrus");
pref("font.name-list.serif.tr", "Times");
pref("font.name-list.sans-serif.tr", "Helvetica");
pref("font.name-list.monospace.tr", "Courier");
pref("font.name-list.cursive.tr", "Apple Chancery");
pref("font.name-list.fantasy.tr", "Papyrus");

pref("font.name.serif.x-armn", "Mshtakan");
pref("font.name.sans-serif.x-armn", "Mshtakan");
pref("font.name.monospace.x-armn", "Mshtakan");
pref("font.name-list.serif.x-armn", "Mshtakan");
pref("font.name-list.sans-serif.x-armn", "Mshtakan");
pref("font.name-list.monospace.x-armn", "Mshtakan");
 
pref("font.name.serif.x-baltic", "Times");
pref("font.name.sans-serif.x-baltic", "Helvetica");
pref("font.name.monospace.x-baltic", "Courier");
pref("font.name.cursive.x-baltic", "Apple Chancery");
pref("font.name.fantasy.x-baltic", "Papyrus");
pref("font.name-list.serif.x-baltic", "Times");
pref("font.name-list.sans-serif.x-baltic", "Helvetica");
pref("font.name-list.monospace.x-baltic", "Courier");
pref("font.name-list.cursive.x-baltic", "Apple Chancery");
pref("font.name-list.fantasy.x-baltic", "Papyrus");

// no suitable fonts for bengali ship with mac os x
// however two can be freely downloaded
// SolaimanLipi, Rupali http://ekushey.org/?page/mac_download
pref("font.name.serif.x-beng", "সোলাইমান লিপি");
pref("font.name.sans-serif.x-beng", "রূপালী");
pref("font.name.monospace.x-beng", "রূপালী");
pref("font.name-list.serif.x-beng", "সোলাইমান লিপি");
pref("font.name-list.sans-serif.x-beng", "রূপালী");
pref("font.name-list.monospace.x-beng", "রূপালী");

pref("font.name.serif.x-cans", "Euphemia UCAS");
pref("font.name.sans-serif.x-cans", "Euphemia UCAS");
pref("font.name.monospace.x-cans", "Euphemia UCAS");
pref("font.name-list.serif.x-cans", "Euphemia UCAS");
pref("font.name-list.sans-serif.x-cans", "Euphemia UCAS");
pref("font.name-list.monospace.x-cans", "Euphemia UCAS");

pref("font.name.serif.x-central-euro", "Times");
pref("font.name.sans-serif.x-central-euro", "Helvetica");
pref("font.name.monospace.x-central-euro", "Courier");
pref("font.name.cursive.x-central-euro", "Apple Chancery");
pref("font.name.fantasy.x-central-euro", "Papyrus");
pref("font.name-list.serif.x-central-euro", "Times");
pref("font.name-list.sans-serif.x-central-euro", "Helvetica");
pref("font.name-list.monospace.x-central-euro", "Courier");
pref("font.name-list.cursive.x-central-euro", "Apple Chancery");
pref("font.name-list.fantasy.x-central-euro", "Papyrus");

pref("font.name.serif.x-cyrillic", "Times CY");
pref("font.name.sans-serif.x-cyrillic", "Helvetica CY");
pref("font.name.monospace.x-cyrillic", "Monaco CY");
pref("font.name.cursive.x-cyrillic", "Geneva CY");
pref("font.name.fantasy.x-cyrillic", "Charcoal CY");
pref("font.name-list.serif.x-cyrillic", "Times CY");
pref("font.name-list.sans-serif.x-cyrillic", "Helvetica CY");
pref("font.name-list.monospace.x-cyrillic", "Monaco CY");
pref("font.name-list.cursive.x-cyrillic", "Geneva CY");
pref("font.name-list.fantasy.x-cyrillic", "Charcoal CY");

pref("font.name.serif.x-devanagari", "Devanagari MT");
pref("font.name.sans-serif.x-devanagari", "Devanagari MT");
pref("font.name.monospace.x-devanagari", "Devanagari MT");
pref("font.name-list.serif.x-devanagari", "Devanagari MT");
pref("font.name-list.sans-serif.x-devanagari", "Devanagari MT");
pref("font.name-list.monospace.x-devanagari", "Devanagari MT");

// no suitable fonts for ethiopic ship with mac os x
// however one can be freely downloaded
// Abyssinica SIL http://scripts.sil.org/AbyssinicaSIL_Download
pref("font.name.serif.x-ethi", "Abyssinica SIL");
pref("font.name.sans-serif.x-ethi", "Abyssinica SIL");
pref("font.name.monospace.x-ethi", "Abyssinica SIL");
pref("font.name-list.serif.x-ethi", "Abyssinica SIL");
pref("font.name-list.sans-serif.x-ethi", "Abyssinica SIL");
pref("font.name-list.monospace.x-ethi", "Abyssinica SIL");

// no suitable fonts for georgian ship with mac os x
// however some can be freely downloaded
// TITUS Cyberbit Basic http://titus.fkidg1.uni-frankfurt.de/unicode/tituut.asp
// Zuzumbo http://homepage.mac.com/rsiradze/FileSharing91.html
pref("font.name.serif.x-geor", "TITUS Cyberbit Basic");
pref("font.name.sans-serif.x-geor", "Zuzumbo");
pref("font.name.monospace.x-geor", "Zuzumbo");
pref("font.name-list.serif.x-geor", "TITUS Cyberbit Basic"); 
pref("font.name-list.sans-serif.x-geor", "Zuzumbo");
pref("font.name-list.monospace.x-geor", "Zuzumbo");

pref("font.name.serif.x-gujr", "Gujarati MT");
pref("font.name.sans-serif.x-gujr", "Gujarati MT");
pref("font.name.monospace.x-gujr", "Gujarati MT");
pref("font.name-list.serif.x-gujr", "Gujarati MT"); 
pref("font.name-list.sans-serif.x-gujr", "Gujarati MT");
pref("font.name-list.monospace.x-gujr", "Gujarati MT");

pref("font.name.serif.x-guru", "Gurmukhi MT");
pref("font.name.sans-serif.x-guru", "Gurmukhi MT");
pref("font.name.monospace.x-guru", "Gurmukhi MT");
pref("font.name-list.serif.x-guru", "Gurmukhi MT"); 
pref("font.name-list.sans-serif.x-guru", "Gurmukhi MT");
pref("font.name-list.monospace.x-guru", "Gurmukhi MT");

// no suitable fonts for khmer ship with mac os x
// add this section when fonts exist

// no suitable fonts for malayalam ship with mac os x
// add this section when fonts exist

// no suitable fonts for oriya ship with mac os x
// add this section when fonts exist

// no suitable fonts for telugu ship with mac os x
// however one can be freely downloaded
// Pothana http://web.nickshanks.com/typography/telugu/
pref("font.name.serif.x-telu", "Pothana");
pref("font.name.sans-serif.x-telu", "Pothana");
pref("font.name.monospace.x-telu", "Pothana");
pref("font.name-list.serif.x-telu", "Pothana");
pref("font.name-list.sans-serif.x-telu", "Pothana");
pref("font.name-list.monospace.x-telu", "Pothana");

// no suitable fonts for kannada ship with mac os x
// however one can be freely downloaded
// Kedage http://web.nickshanks.com/typography/kannada/
pref("font.name.serif.x-knda", "Kedage");
pref("font.name.sans-serif.x-knda", "Kedage");
pref("font.name.monospace.x-knda", "Kedage");
pref("font.name-list.serif.x-knda", "Kedage");
pref("font.name-list.sans-serif.x-knda", "Kedage");
pref("font.name-list.monospace.x-knda", "Kedage");

// no suitable fonts for sinhala ship with mac os x
// add this section when fonts exist

pref("font.name.serif.x-tamil", "InaiMathi");
pref("font.name.sans-serif.x-tamil", "InaiMathi");
pref("font.name.monospace.x-tamil", "InaiMathi");
pref("font.name-list.serif.x-tamil", "InaiMathi");
pref("font.name-list.sans-serif.x-tamil", "InaiMathi");
pref("font.name-list.monospace.x-tamil", "InaiMathi");

// Kailasa ships with mac os x >= 10.5
pref("font.name.serif.x-tibt", "Kailasa");
pref("font.name.sans-serif.x-tibt", "Kailasa");
pref("font.name.monospace.x-tibt", "Kailasa");
pref("font.name-list.serif.x-tibt", "Kailasa");
pref("font.name-list.sans-serif.x-tibt", "Kailasa");
pref("font.name-list.monospace.x-tibt", "Kailasa");

pref("font.name.serif.x-unicode", "Times");
pref("font.name.sans-serif.x-unicode", "Helvetica");
pref("font.name.monospace.x-unicode", "Courier");
pref("font.name.cursive.x-unicode", "Apple Chancery");
pref("font.name.fantasy.x-unicode", "Papyrus");
pref("font.name-list.serif.x-unicode", "Times");
pref("font.name-list.sans-serif.x-unicode", "Helvetica");
pref("font.name-list.monospace.x-unicode", "Courier");
pref("font.name-list.cursive.x-unicode", "Apple Chancery");
pref("font.name-list.fantasy.x-unicode", "Papyrus");

pref("font.name.serif.x-western", "Times");
pref("font.name.sans-serif.x-western", "Helvetica");
pref("font.name.monospace.x-western", "Courier");
pref("font.name.cursive.x-western", "Apple Chancery");
pref("font.name.fantasy.x-western", "Papyrus");
pref("font.name-list.serif.x-western", "Times");
pref("font.name-list.sans-serif.x-western", "Helvetica");
pref("font.name-list.monospace.x-western", "Courier");
pref("font.name-list.cursive.x-western", "Apple Chancery");
pref("font.name-list.fantasy.x-western", "Papyrus");

pref("font.name.serif.zh-CN", "STSong");
pref("font.name.sans-serif.zh-CN", "STHeiti");
pref("font.name.monospace.zh-CN", "STHeiti");
pref("font.name-list.serif.zh-CN", "STSong");
pref("font.name-list.sans-serif.zh-CN", "STHeiti");
pref("font.name-list.monospace.zh-CN", "STHeiti");

pref("font.name.serif.zh-TW", "Apple LiSung"); 
pref("font.name.sans-serif.zh-TW", "Apple LiGothic");  
pref("font.name.monospace.zh-TW", "Apple LiGothic");  
pref("font.name-list.serif.zh-TW", "Apple LiSung"); 
pref("font.name-list.sans-serif.zh-TW", "Apple LiGothic");  
pref("font.name-list.monospace.zh-TW", "Apple LiGothic");  

pref("font.name.serif.zh-HK", "LiSong Pro");
pref("font.name.sans-serif.zh-HK", "LiHei Pro");
pref("font.name.monospace.zh-HK", "LiHei Pro");
pref("font.name-list.serif.zh-HK", "LiSong Pro");
pref("font.name-list.sans-serif.zh-HK", "LiHei Pro");
pref("font.name-list.monospace.zh-HK", "LiHei Pro");

pref("font.default.ar", "sans-serif");
pref("font.size.variable.ar", 16);
pref("font.size.fixed.ar", 13);

pref("font.default.el", "serif");
pref("font.size.variable.el", 16);
pref("font.size.fixed.el", 13);

pref("font.default.he", "sans-serif");
pref("font.size.variable.he", 16);
pref("font.size.fixed.he", 13);

pref("font.default.ja", "sans-serif");
pref("font.size.variable.ja", 16);
pref("font.size.fixed.ja", 16);

pref("font.default.ko", "sans-serif");
pref("font.size.variable.ko", 16);
pref("font.size.fixed.ko", 16);

pref("font.default.th", "serif");
pref("font.size.variable.th", 16);
pref("font.size.fixed.th", 13);
pref("font.minimum-size.th", 10);

pref("font.default.tr", "serif");
pref("font.size.variable.tr", 16);
pref("font.size.fixed.tr", 13);

pref("font.default.x-armn", "serif");
pref("font.size.variable.x-armn", 16);
pref("font.size.fixed.x-armn", 13);

pref("font.default.x-baltic", "serif");
pref("font.size.variable.x-baltic", 16);
pref("font.size.fixed.x-baltic", 13);

pref("font.default.x-beng", "serif");
pref("font.size.variable.x-beng", 16);
pref("font.size.fixed.x-beng", 13);

pref("font.default.x-cans", "serif");
pref("font.size.variable.x-cans", 16);
pref("font.size.fixed.x-cans", 13);

pref("font.default.x-central-euro", "serif");
pref("font.size.variable.x-central-euro", 16);
pref("font.size.fixed.x-central-euro", 13);

pref("font.default.x-cyrillic", "serif");
pref("font.size.variable.x-cyrillic", 16);
pref("font.size.fixed.x-cyrillic", 13);

pref("font.default.x-devanagari", "serif");
pref("font.size.variable.x-devanagari", 16);
pref("font.size.fixed.x-devanagari", 13);

pref("font.default.x-ethi", "serif");
pref("font.size.variable.x-ethi", 16);
pref("font.size.fixed.x-ethi", 13);

pref("font.default.x-geor", "serif");
pref("font.size.variable.x-geor", 16);
pref("font.size.fixed.x-geor", 13);

pref("font.default.x-gujr", "serif");
pref("font.size.variable.x-gujr", 16);
pref("font.size.fixed.x-gujr", 13);

pref("font.default.x-guru", "serif");
pref("font.size.variable.x-guru", 16);
pref("font.size.fixed.x-guru", 13);

pref("font.default.x-khmr", "serif");
pref("font.size.variable.x-khmr", 16);
pref("font.size.fixed.x-khmr", 13);

pref("font.default.x-mlym", "serif");
pref("font.size.variable.x-mlym", 16);
pref("font.size.fixed.x-mlym", 13);

pref("font.default.x-tamil", "serif");
pref("font.size.variable.x-tamil", 16);
pref("font.size.fixed.x-tamil", 13);

pref("font.default.x-orya", "serif");
pref("font.size.variable.x-orya", 16);
pref("font.size.fixed.x-orya", 13);

pref("font.default.x-telu", "serif");
pref("font.size.variable.x-telu", 16);
pref("font.size.fixed.x-telu", 13);

pref("font.default.x-knda", "serif");
pref("font.size.variable.x-knda", 16);
pref("font.size.fixed.x-knda", 13);

pref("font.default.x-sinh", "serif");
pref("font.size.variable.x-sinh", 16);
pref("font.size.fixed.x-sinh", 13);

pref("font.default.x-tibt", "serif");
pref("font.size.variable.x-tibt", 16);
pref("font.size.fixed.x-tibt", 13);

pref("font.default.x-unicode", "serif");
pref("font.size.variable.x-unicode", 16);
pref("font.size.fixed.x-unicode", 13);

pref("font.default.x-western", "serif");
pref("font.size.variable.x-western", 16);
pref("font.size.fixed.x-western", 13);

pref("font.default.zh-CN", "sans-serif");
pref("font.size.variable.zh-CN", 15);
pref("font.size.fixed.zh-CN", 16);

pref("font.default.zh-TW", "sans-serif");
pref("font.size.variable.zh-TW", 15);
pref("font.size.fixed.zh-TW", 16);

pref("font.default.zh-HK", "sans-serif");
pref("font.size.variable.zh-HK", 15);
pref("font.size.fixed.zh-HK", 16);

// Apple's Symbol is Unicode so use it
pref("font.mathfont-family", "STIXNonUnicode, STIXSizeOneSym, STIXSize1, STIXGeneral, Symbol, DejaVu Sans, Cambria Math");

// individual font faces to be treated as independent families
// names are Postscript names of each face
pref("font.single-face-list", "Osaka-Mono");

// optimization hint for fonts with localized names to be read in at startup, otherwise read in at lookup miss
// names are canonical family names (typically English names)
pref("font.preload-names-list", "Hiragino Kaku Gothic Pro,Hiragino Mincho Pro,STSong");

// Override the Windows settings: no menu key, meta accelerator key. ctrl for general access key in HTML/XUL
// Use 17 for Ctrl, 18 for Option, 224 for Cmd, 0 for none
pref("ui.key.menuAccessKey", 0);
pref("ui.key.accelKey", 224);
// (pinkerton, joki, saari) IE5 for mac uses Control for access keys. The HTML4 spec
// suggests to use command on mac, but this really sucks (imagine someone having a "q"
// as an access key and not letting you quit the app!). As a result, we've made a 
// command decision 1 day before tree lockdown to change it to the control key.
pref("ui.key.generalAccessKey", -1);

// If generalAccessKey is -1, use the following two prefs instead.
// Use 0 for disabled, 1 for Shift, 2 for Ctrl, 4 for Alt, 8 for Meta (Cmd)
// (values can be combined, e.g. 3 for Ctrl+Shift)
pref("ui.key.chromeAccess", 2);
pref("ui.key.contentAccess", 2);

// print_extra_margin enables platforms to specify an extra gap or margin
// around the content of the page for Print Preview only
pref("print.print_extra_margin", 90); // twips (90 twips is an eigth of an inch)

// See bug 404131, topmost <panel> element wins to Dashboard on MacOSX.
pref("ui.panel.default_level_parent", false);

pref("mousewheel.system_scroll_override_on_root_content.enabled", false);

# XP_MACOSX
#endif

#ifdef XP_OS2

pref("ui.key.menuAccessKeyFocuses", true);

pref("font.alias-list", "sans,sans-serif,serif,monospace,Tms Rmn,Helv,Courier,Times New Roman");

pref("font.mathfont-family", "STIXNonUnicode, STIXSizeOneSym, STIXSize1, STIXGeneral, DejaVu Sans");

// Languages only need lists if we have a default that might not be available.
// Tms Rmn and Helv cannot be used by Thebes but the OS/2 version of FontConfig
// maps them to Times New Roman and Helvetica, respectively. Those fonts and
// Courier are available on OS/2 by default.

pref("font.name.serif.ar", "Tms Rmn");
pref("font.name.sans-serif.ar", "Helv");
pref("font.name.monospace.ar", "Courier");

pref("font.name.serif.el", "Tms Rmn");
pref("font.name.sans-serif.el", "Helv");
pref("font.name.monospace.el", "Courier");

pref("font.name.serif.he", "Tms Rmn");
pref("font.name.sans-serif.he", "Helv");
pref("font.name.monospace.he", "Courier");

pref("font.name.serif.ja", "Times New Roman WT J");
pref("font.name-list.serif.ja", "Times New Roman WT J, Times New Roman WT, Times New Roman MT 30, Tms Rmn");
pref("font.name.sans-serif.ja", "Helv");
pref("font.name.monospace.ja", "Kochi Gothic");
pref("font.name-list.monospace.ja", "Kochi Gothic, Kochi Mincho, Courier New, Courier");

pref("font.name.serif.ko", "Times New Roman WT K");
pref("font.name-list.serif.ko", "Times New Roman WT K, Times New Roman WT, Times New Roman MT 30, Tms Rmn");
pref("font.name.sans-serif.ko", "Helv");
pref("font.name.monospace.ko", "Courier");

pref("font.name.serif.th", "Tms Rmn");
pref("font.name.sans-serif.th", "Helv");
pref("font.name.monospace.th", "Courier");

pref("font.name.serif.tr", "Tms Rmn");
pref("font.name.sans-serif.tr", "Helv");
pref("font.name.monospace.tr", "Courier");

pref("font.name.serif.x-baltic", "Tms Rmn");
pref("font.name.sans-serif.x-baltic", "Helv");
pref("font.name.monospace.x-baltic", "Courier");

pref("font.name.serif.x-central-euro", "Tms Rmn");
pref("font.name.sans-serif.x-central-euro", "Helv");
pref("font.name.monospace.x-central-euro", "Courier");

pref("font.name.serif.x-cyrillic", "Tms Rmn");
pref("font.name.sans-serif.x-cyrillic", "Helv");
pref("font.name.monospace.x-cyrillic", "Courier");

// Unicode fonts
// Fontconfig will match substrings, so that we only need to list e.g.
// Times New Roman WT and it will search for the J, SC, TC, K variants.
// The DejaVu fonts are shipped with eCS, so list them first but include other
// fonts that OS/2 users are likely to have.
pref("font.name.serif.x-unicode", "Times New Roman MT 30");
pref("font.name-list.serif.x-unicode", "DejaVu Serif, FreeSerif, Times New Roman WT, Times New Roman MT 30, Tms Rmn");
pref("font.name.sans-serif.x-unicode", "Lucida Sans Unicode");
pref("font.name-list.sans-serif.x-unicode", "DejaVu Sans, FreeSans, Arial Unicode, Lucida Sans Unicode, Helv");
pref("font.name.monospace.x-unicode", "DejaVu Sans Mono");
pref("font.name-list.monospace.x-unicode", "DejaVu Sans Mono, FreeMono, Andale Mono, Courier New, Courier");
pref("font.name.fantasy.x-unicode", "Times New Roman MT 30");
pref("font.name-list.fantasy.x-unicode", "DejaVu Serif, FreeSerif, Times New Roman WT, Times New Roman MT 30");
pref("font.name.cursive.x-unicode", "Times New Roman MT 30");
pref("font.name-list.cursive.x-unicode", "DejaVu Serif, FreeSerif, Times New Roman WT, Times New Roman MT 30");

pref("font.name.serif.x-western", "Tms Rmn");
pref("font.name.sans-serif.x-western", "Helv");
pref("font.name.monospace.x-western", "Courier");

pref("font.name.serif.zh-CN", "Times New Roman WT SC");
pref("font.name-list.serif.zh_CN", "Times New Roman WT SC, Times New Roman MT 30, Times New Roman WT, Tms Rmn");
pref("font.name.sans-serif.zh-CN", "Helv");
pref("font.name.monospace.zh-CN", "Courier");

pref("font.name.serif.zh-TW", "Times New Roman WT TC");
pref("font.name-list.serif.zh-TW", "Times New Roman WT TC, Times New Roman MT 30, Times New Roman WT, Tms Rmn");
pref("font.name.sans-serif.zh-TW", "Helv");
pref("font.name.monospace.zh-TW", "Courier");

// just copied values from zh-TW
pref("font.name.serif.zh-HK", "Times New Roman WT TC");
pref("font.name-list.serif.zh-HK", "Times New Roman WT TC, Times New Roman MT 30, Times New Roman WT, Tms Rmn");
pref("font.name.sans-serif.zh-HK", "Helv");
pref("font.name.monospace.zh-HK", "Courier");

pref("font.default", "serif");

pref("font.default.ar", "serif");
pref("font.size.variable.ar", 16);
pref("font.size.fixed.ar", 13);

pref("font.default.el", "serif");
pref("font.size.variable.el", 16);
pref("font.size.fixed.el", 13);

pref("font.default.he", "serif");
pref("font.size.variable.he", 16);
pref("font.size.fixed.he", 13);

pref("font.default.ja", "serif");
pref("font.size.variable.ja", 16);
pref("font.size.fixed.ja", 16);

pref("font.default.ko", "serif");
pref("font.size.variable.ko", 16);
pref("font.size.fixed.ko", 16);

pref("font.default.th", "serif");
pref("font.size.variable.th", 16);
pref("font.size.fixed.th", 13);

pref("font.default.tr", "serif");
pref("font.size.variable.tr", 16);
pref("font.size.fixed.tr", 13);

pref("font.default.x-baltic", "serif");
pref("font.size.variable.x-baltic", 16);
pref("font.size.fixed.x-baltic", 13);

pref("font.default.x-central-euro", "serif");
pref("font.size.variable.x-central-euro", 16);
pref("font.size.fixed.x-central-euro", 13);

pref("font.default.x-cyrillic", "serif");
pref("font.size.variable.x-cyrillic", 16);
pref("font.size.fixed.x-cyrillic", 13);

pref("font.default.x-devanagari", "serif");
pref("font.size.variable.x-devanagari", 16);
pref("font.size.fixed.x-devanagari", 13);

pref("font.default.x-tamil", "serif");
pref("font.size.variable.x-tamil", 16);
pref("font.size.fixed.x-tamil", 13);

pref("font.default.x-unicode", "serif");
pref("font.size.variable.x-unicode", 16);
pref("font.size.fixed.x-unicode", 13);

pref("font.default.x-western", "serif");
pref("font.size.variable.x-western", 16);
pref("font.size.fixed.x-western", 13);

pref("font.default.zh-CN", "serif");
pref("font.size.variable.zh-CN", 16);
pref("font.size.fixed.zh-CN", 16);

pref("font.default.zh-TW", "serif");
pref("font.size.variable.zh-TW", 16);
pref("font.size.fixed.zh-TW", 16);

pref("font.default.zh-HK", "serif");
pref("font.size.variable.zh-HK", 16);
pref("font.size.fixed.zh-HK", 16);

pref("netinst.profile.show_profile_wizard", true); 

pref("middlemouse.paste", true);

// override double-click word selection behavior.
pref("layout.word_select.eat_space_to_next_word", true);
pref("layout.word_select.stop_at_punctuation", false);

// If false, will always use closest matching size for a given
// image font.  If true, will substitute a vector font for a given
// image font if the given size is smaller/larger than can be handled
// by the image font.
pref("browser.display.substitute_vector_fonts", true);

// print_extra_margin enables platforms to specify an extra gap or margin
// around the content of the page for Print Preview only
pref("print.print_extra_margin", 90); // twips (90 twips is an eigth of an inch)

pref("applications.telnet", "telnetpm.exe");
pref("applications.telnet.host", "%host%");
pref("applications.telnet.port", "-p %port%");

pref("mail.compose.max_recycled_windows", 0);

// Disable IPv6 name lookups by default.
// This is because OS/2 doesn't support IPv6
pref("network.dns.disableIPv6", true);

// IMEs of OS/2 might use non-topmost windows for topmost <panel> element,
// see bug 451015. If there are other problems by this value, we may need to
// change this value.
pref("ui.panel.default_level_parent", false);

pref("mousewheel.system_scroll_override_on_root_content.enabled", false);

# OS2
#endif

#ifdef XP_BEOS

pref("intl.font_charset", "");
pref("intl.font_spec_list", "");
pref("mail.signature_date", 0);

pref("font.alias-list", "sans,sans-serif,serif,monospace");

pref("font.default.ar", "sans-serif");
pref("font.size.variable.ar", 16);
pref("font.size.fixed.ar", 13);

pref("font.default.el", "serif");
pref("font.size.variable.el", 16);
pref("font.size.fixed.el", 13);

pref("font.default.he", "sans-serif");
pref("font.size.variable.he", 16);
pref("font.size.fixed.he", 13);

pref("font.default.ja", "sans-serif");
pref("font.size.variable.ja", 16);
pref("font.size.fixed.ja", 16);

pref("font.default.ko", "sans-serif");
pref("font.size.variable.ko", 16);
pref("font.size.fixed.ko", 16);

pref("font.default.th", "serif");
pref("font.size.variable.th", 16);
pref("font.size.fixed.th", 13);

pref("font.default.tr", "serif");
pref("font.size.variable.tr", 16);
pref("font.size.fixed.tr", 13);

pref("font.default.x-baltic", "serif");
pref("font.size.variable.x-baltic", 16);
pref("font.size.fixed.x-baltic", 13);

pref("font.default.x-central-euro", "serif");
pref("font.size.variable.x-central-euro", 16);
pref("font.size.fixed.x-central-euro", 13);

pref("font.default.x-cyrillic", "serif");
pref("font.size.variable.x-cyrillic", 16);
pref("font.size.fixed.x-cyrillic", 13);

pref("font.default.x-unicode", "serif");
pref("font.size.variable.x-unicode", 16);
pref("font.size.fixed.x-unicode", 13);

pref("font.default.x-western", "serif");
pref("font.size.variable.x-western", 16);
pref("font.size.fixed.x-western", 13);

pref("font.default.zh-CN", "sans-serif");
pref("font.size.variable.zh-CN", 16);
pref("font.size.fixed.zh-CN", 16);

pref("font.default.zh-TW", "sans-serif");
pref("font.size.variable.zh-TW", 16);
pref("font.size.fixed.zh-TW", 16);

pref("font.default.zh-HK", "sans-serif");
pref("font.size.variable.zh-HK", 16);
pref("font.size.fixed.zh-HK", 16);

/**
 * Set default accelKey to "Alt", which is the default under BeOS.
 * The generalAccessKey is used for shortcuts on web pages, set to
 * Ctrl+Shift. The menuAccessKey is now the "windows" key.
 */
pref("ui.key.accelKey", 18);
pref("ui.key.menuAccessKey", 17);
pref("ui.key.generalAccessKey", -1);

// If generalAccessKey is -1, use the following two prefs instead.
// Use 0 for disabled, 1 for Shift, 2 for Ctrl, 4 for Alt, 8 for Meta
// (values can be combined, e.g. 3 for Ctrl+Shift)
pref("ui.key.chromeAccess", 2);
pref("ui.key.contentAccess", 3);

// xxx toolkit?
pref("browser.download.dir", "/boot/home/Downloads");

// IMEs of BeOS might use non-topmost windows for topmost <panel> element,
// see bug 451015. If there are other problems by this value, we may need to
// change this value.
pref("ui.panel.default_level_parent", false);

pref("mousewheel.system_scroll_override_on_root_content.enabled", false);

# BeOS
#endif

#ifdef ANDROID
// Handled differently under Mac/Windows
pref("network.hosts.smtp_server", "localhost");
pref("network.hosts.pop_server", "pop");
pref("network.protocol-handler.warn-external.file", false);
pref("browser.drag_out_of_frame_style", 1);

// Middle-mouse handling
pref("middlemouse.paste", true);
pref("middlemouse.contentLoadURL", true);
pref("middlemouse.openNewWindow", true);
pref("middlemouse.scrollbarPosition", true);

// Clipboard behavior
pref("clipboard.autocopy", true);

pref("browser.urlbar.clickSelectsAll", false);

// Tab focus model bit field:
// 1 focuses text controls, 2 focuses other form elements, 4 adds links.
// Leave this at the default, 7, to match mozilla1.0-era user expectations.
// pref("accessibility.tabfocus", 1);

// autocomplete keyboard grab workaround
pref("autocomplete.grab_during_popup", true);
pref("autocomplete.ungrab_during_mode_switch", true);

// Default to using the system filepicker if possible, but allow
// toggling to use the XUL filepicker
pref("ui.allow_platform_file_picker", true);

// should NetworkManager be authoritative for online/offline status?
pref("toolkit.networkmanager.disable", true);

pref("helpers.global_mime_types_file", "/etc/mime.types");
pref("helpers.global_mailcap_file", "/etc/mailcap");
pref("helpers.private_mime_types_file", "~/.mime.types");
pref("helpers.private_mailcap_file", "~/.mailcap");
pref("java.global_java_version_file", "/etc/.java/versions");
pref("java.private_java_version_file", "~/.java/versions");
pref("java.default_java_location_solaris", "/usr/j2se");
pref("java.default_java_location_others", "/usr/java");
pref("java.java_plugin_library_name", "javaplugin_oji");
pref("applications.telnet", "xterm -e telnet %h %p");
pref("applications.tn3270", "xterm -e tn3270 %h");
pref("applications.rlogin", "xterm -e rlogin %h");
pref("applications.rlogin_with_user", "xterm -e rlogin %h -l %u");
pref("print.print_command", "lpr ${MOZ_PRINTER_NAME:+-P\"$MOZ_PRINTER_NAME\"}");
pref("print.printer_list", ""); // list of printers, separated by spaces
pref("print.print_reversed", false);
pref("print.print_color", true);
pref("print.print_landscape", false);
pref("print.print_paper_size", 0);

// print_extra_margin enables platforms to specify an extra gap or margin
// around the content of the page for Print Preview only
pref("print.print_extra_margin", 0); // twips

pref("font.allow_double_byte_special_chars", true);
// font names

pref("font.alias-list", "sans,sans-serif,serif,monospace");

// ar

pref("font.name.serif.el", "Droid Serif");
pref("font.name.sans-serif.el", "Droid Sans");
pref("font.name.monospace.el", "Droid Sans Mono");

pref("font.name.serif.he", "Droid Serif");
pref("font.name.sans-serif.he", "Droid Sans");
pref("font.name.monospace.he", "Droid Sans Mono");

pref("font.name.serif.ja", "Droid Serif");
pref("font.name.sans-serif.ja", "Droid Sans");
pref("font.name.monospace.ja", "Droid Sans Mono");

pref("font.name.serif.ko", "Droid Serif");
pref("font.name.sans-serif.ko", "Droid Sans");
pref("font.name.monospace.ko", "Droid Sans Mono");

pref("font.name.serif.th", "Droid Serif");
pref("font.name.sans-serif.th", "Droid Sans");
pref("font.name.monospace.th", "Droid Sans Mono");

pref("font.name.serif.tr", "Droid Serif");
pref("font.name.sans-serif.tr", "Droid Sans");
pref("font.name.monospace.tr", "Droid Sans Mono");

pref("font.name.serif.x-baltic", "Droid Serif");
pref("font.name.sans-serif.x-baltic", "Droid Sans");
pref("font.name.monospace.x-baltic", "Droid Sans Mono");

pref("font.name.serif.x-central-euro", "Droid Serif");
pref("font.name.sans-serif.x-central-euro", "Droid Sans");
pref("font.name.monospace.x-central-euro", "Droid Sans Mono");

pref("font.name.serif.x-cyrillic", "Droid Serif");
pref("font.name.sans-serif.x-cyrillic", "Droid Sans");
pref("font.name.monospace.x-cyrillic", "Droid Sans Mono");

pref("font.name.serif.x-unicode", "Droid Serif");
pref("font.name.sans-serif.x-unicode", "Droid Sans");
pref("font.name.monospace.x-unicode", "Droid Sans Mono");

pref("font.name.serif.x-user-def", "Droid Serif");
pref("font.name.sans-serif.x-user-def", "Droid Sans");
pref("font.name.monospace.x-user-def", "Droid Sans Mono");

pref("font.name.serif.x-western", "Droid Serif");
pref("font.name.sans-serif.x-western", "Droid Sans");
pref("font.name.monospace.x-western", "Droid Sans Mono");

pref("font.name.serif.zh-CN", "Droid Serif");
pref("font.name.sans-serif.zh-CN", "Droid Sans");
pref("font.name.monospace.zh-CN", "Droid Sans Mono");

// ming_uni.ttf (HKSCS-2001) 
// http://www.info.gov.hk/digital21/eng/hkscs/download/uime.exe
pref("font.name.serif.zh-HK", "Droid Serif");
pref("font.name.sans-serif.zh-HK", "Droid Sans");
pref("font.name.monospace.zh-HK", "Droid Sans Mono");

// zh-TW

pref("font.default.ar", "sans-serif");
pref("font.size.variable.ar", 16);
pref("font.size.fixed.ar", 12);

pref("font.default.el", "sans-serif");
pref("font.size.variable.el", 16);
pref("font.size.fixed.el", 12);

pref("font.default.he", "sans-serif");
pref("font.size.variable.he", 16);
pref("font.size.fixed.he", 12);

pref("font.default.ja", "sans-serif");
pref("font.size.variable.ja", 16);
pref("font.size.fixed.ja", 16);

pref("font.default.ko", "sans-serif");
pref("font.size.variable.ko", 16);
pref("font.size.fixed.ko", 16);

pref("font.default.th", "serif");
pref("font.size.variable.th", 16);
pref("font.size.fixed.th", 13);
pref("font.minimum-size.th", 13);

pref("font.default.tr", "sans-serif");
pref("font.size.variable.tr", 16);
pref("font.size.fixed.tr", 12);

pref("font.default.x-baltic", "sans-serif");
pref("font.size.variable.x-baltic", 16);
pref("font.size.fixed.x-baltic", 12);

pref("font.default.x-central-euro", "sans-serif");
pref("font.size.variable.x-central-euro", 16);
pref("font.size.fixed.x-central-euro", 12);

pref("font.default.x-cyrillic", "sans-serif");
pref("font.size.variable.x-cyrillic", 16);
pref("font.size.fixed.x-cyrillic", 12);

pref("font.default.x-unicode", "sans-serif");
pref("font.size.variable.x-unicode", 16);
pref("font.size.fixed.x-unicode", 12);

pref("font.default.x-user-def", "sans-serif");
pref("font.size.variable.x-user-def", 16);
pref("font.size.fixed.x-user-def", 12);

pref("font.default.x-western", "sans-serif");
pref("font.size.variable.x-western", 16);
pref("font.size.fixed.x-western", 12);

pref("font.default.zh-CN", "sans-serif");
pref("font.size.variable.zh-CN", 16);
pref("font.size.fixed.zh-CN", 16);

pref("font.default.zh-TW", "sans-serif");
pref("font.size.variable.zh-TW", 16);
pref("font.size.fixed.zh-TW", 16);

pref("font.default.zh-HK", "sans-serif");
pref("font.size.variable.zh-HK", 16);
pref("font.size.fixed.zh-HK", 16);

pref("font.default.x-devanagari", "serif");
pref("font.size.variable.x-devanagari", 16);
pref("font.size.fixed.x-devanagari", 13);

pref("font.default.x-tamil", "serif");
pref("font.size.variable.x-tamil", 16);
pref("font.size.fixed.x-tamil", 13);

pref("font.default.x-armn", "serif");
pref("font.size.variable.x-armn", 16);
pref("font.size.fixed.x-armn", 13);

pref("font.default.x-beng", "serif");
pref("font.size.variable.x-beng", 16);
pref("font.size.fixed.x-beng", 13);

pref("font.default.x-cans", "serif");
pref("font.size.variable.x-cans", 16);
pref("font.size.fixed.x-cans", 13);

pref("font.default.x-ethi", "serif");
pref("font.size.variable.x-ethi", 16);
pref("font.size.fixed.x-ethi", 13);

pref("font.default.x-geor", "serif");
pref("font.size.variable.x-geor", 16);
pref("font.size.fixed.x-geor", 13);

pref("font.default.x-gujr", "serif");
pref("font.size.variable.x-gujr", 16);
pref("font.size.fixed.x-gujr", 13);

pref("font.default.x-guru", "serif");
pref("font.size.variable.x-guru", 16);
pref("font.size.fixed.x-guru", 13);

pref("font.default.x-khmr", "serif");
pref("font.size.variable.x-khmr", 16);
pref("font.size.fixed.x-khmr", 13);

pref("font.default.x-mlym", "serif");
pref("font.size.variable.x-mlym", 16);
pref("font.size.fixed.x-mlym", 13);

pref("font.default.x-orya", "serif");
pref("font.size.variable.x-orya", 16);
pref("font.size.fixed.x-orya", 13);

pref("font.default.x-telu", "serif");
pref("font.size.variable.x-telu", 16);
pref("font.size.fixed.x-telu", 13);

pref("font.default.x-knda", "serif");
pref("font.size.variable.x-knda", 16);
pref("font.size.fixed.x-knda", 13);

pref("font.default.x-sinh", "serif");
pref("font.size.variable.x-sinh", 16);
pref("font.size.fixed.x-sinh", 13);

pref("font.default.x-tibt", "serif");
pref("font.size.variable.x-tibt", 16);
pref("font.size.fixed.x-tibt", 13);

/* PostScript print module prefs */
// pref("print.postscript.enabled",      true);
pref("print.postscript.paper_size",    "letter");
pref("print.postscript.orientation",   "portrait");
pref("print.postscript.print_command", "lpr ${MOZ_PRINTER_NAME:+-P\"$MOZ_PRINTER_NAME\"}");

// Setting default_level_parent to true makes the default level for popup
// windows "top" instead of "parent".  On GTK2 platform, this is implemented
// with override-redirect windows which is the normal way to implement
// temporary popup windows.  Setting this to false would make the default
// level "parent" which is implemented with managed windows.
// A problem with using managed windows is that metacity sometimes deactivates
// the parent window when the managed popup is shown.
pref("ui.panel.default_level_parent", true);

pref("mousewheel.system_scroll_override_on_root_content.enabled", false);

# ANDROID
#endif

#ifndef ANDROID
#ifndef XP_MACOSX
#ifdef XP_UNIX
// Handled differently under Mac/Windows
pref("network.hosts.smtp_server", "localhost");
pref("network.hosts.pop_server", "pop");
pref("network.protocol-handler.warn-external.file", false);
pref("browser.drag_out_of_frame_style", 1);

// Middle-mouse handling
pref("middlemouse.paste", true);
pref("middlemouse.contentLoadURL", true);
pref("middlemouse.openNewWindow", true);
pref("middlemouse.scrollbarPosition", true);

// Clipboard behavior
pref("clipboard.autocopy", true);

pref("browser.urlbar.clickSelectsAll", false);

// Tab focus model bit field:
// 1 focuses text controls, 2 focuses other form elements, 4 adds links.
// Leave this at the default, 7, to match mozilla1.0-era user expectations.
// pref("accessibility.tabfocus", 1);

// autocomplete keyboard grab workaround
pref("autocomplete.grab_during_popup", true);
pref("autocomplete.ungrab_during_mode_switch", true);

// Default to using the system filepicker if possible, but allow
// toggling to use the XUL filepicker
pref("ui.allow_platform_file_picker", true);

// should NetworkManager be authoritative for online/offline status?
pref("toolkit.networkmanager.disable", true);

pref("helpers.global_mime_types_file", "/etc/mime.types");
pref("helpers.global_mailcap_file", "/etc/mailcap");
pref("helpers.private_mime_types_file", "~/.mime.types");
pref("helpers.private_mailcap_file", "~/.mailcap");
pref("java.global_java_version_file", "/etc/.java/versions");
pref("java.private_java_version_file", "~/.java/versions");
pref("java.default_java_location_solaris", "/usr/j2se");
pref("java.default_java_location_others", "/usr/java");
pref("java.java_plugin_library_name", "javaplugin_oji");
pref("applications.telnet", "xterm -e telnet %h %p");
pref("applications.tn3270", "xterm -e tn3270 %h");
pref("applications.rlogin", "xterm -e rlogin %h");
pref("applications.rlogin_with_user", "xterm -e rlogin %h -l %u");
pref("print.print_command", "lpr ${MOZ_PRINTER_NAME:+-P\"$MOZ_PRINTER_NAME\"}");
pref("print.printer_list", ""); // list of printers, separated by spaces
pref("print.print_reversed", false);
pref("print.print_color", true);
pref("print.print_landscape", false);
pref("print.print_paper_size", 0);

// print_extra_margin enables platforms to specify an extra gap or margin
// around the content of the page for Print Preview only
pref("print.print_extra_margin", 0); // twips

pref("font.allow_double_byte_special_chars", true);
// font names

pref("font.alias-list", "sans,sans-serif,serif,monospace");

// ar

pref("font.name.serif.el", "serif");
pref("font.name.sans-serif.el", "sans-serif");
pref("font.name.monospace.el", "monospace");

pref("font.name.serif.he", "serif");
pref("font.name.sans-serif.he", "sans-serif");
pref("font.name.monospace.he", "monospace");

pref("font.name.serif.ja", "serif");
pref("font.name.sans-serif.ja", "sans-serif");
pref("font.name.monospace.ja", "monospace");

pref("font.name.serif.ko", "serif");
pref("font.name.sans-serif.ko", "sans-serif");
pref("font.name.monospace.ko", "monospace");

pref("font.name.serif.th", "serif");
pref("font.name.sans-serif.th", "sans-serif");
pref("font.name.monospace.th", "monospace");

pref("font.name.serif.tr", "serif");
pref("font.name.sans-serif.tr", "sans-serif");
pref("font.name.monospace.tr", "monospace");

pref("font.name.serif.x-baltic", "serif");
pref("font.name.sans-serif.x-baltic", "sans-serif");
pref("font.name.monospace.x-baltic", "monospace");

pref("font.name.serif.x-central-euro", "serif");
pref("font.name.sans-serif.x-central-euro", "sans-serif");
pref("font.name.monospace.x-central-euro", "monospace");

pref("font.name.serif.x-cyrillic", "serif");
pref("font.name.sans-serif.x-cyrillic", "sans-serif");
pref("font.name.monospace.x-cyrillic", "monospace");

pref("font.name.serif.x-unicode", "serif");
pref("font.name.sans-serif.x-unicode", "sans-serif");
pref("font.name.monospace.x-unicode", "monospace");

pref("font.name.serif.x-user-def", "serif");
pref("font.name.sans-serif.x-user-def", "sans-serif");
pref("font.name.monospace.x-user-def", "monospace");

pref("font.name.serif.x-western", "serif");
pref("font.name.sans-serif.x-western", "sans-serif");
pref("font.name.monospace.x-western", "monospace");

pref("font.name.serif.zh-CN", "serif");
pref("font.name.sans-serif.zh-CN", "sans-serif");
pref("font.name.monospace.zh-CN", "monospace");

// ming_uni.ttf (HKSCS-2001) 
// http://www.info.gov.hk/digital21/eng/hkscs/download/uime.exe
pref("font.name.serif.zh-HK", "serif");
pref("font.name.sans-serif.zh-HK", "sans-serif");
pref("font.name.monospace.zh-HK", "monospace");

// zh-TW

pref("font.default.ar", "sans-serif");
pref("font.size.variable.ar", 16);
pref("font.size.fixed.ar", 12);

pref("font.default.el", "serif");
pref("font.size.variable.el", 16);
pref("font.size.fixed.el", 12);

pref("font.default.he", "sans-serif");
pref("font.size.variable.he", 16);
pref("font.size.fixed.he", 12);

pref("font.default.ja", "sans-serif");
pref("font.size.variable.ja", 16);
pref("font.size.fixed.ja", 16);

pref("font.default.ko", "sans-serif");
pref("font.size.variable.ko", 16);
pref("font.size.fixed.ko", 16);

pref("font.default.th", "serif");
pref("font.size.variable.th", 16);
pref("font.size.fixed.th", 13);
pref("font.minimum-size.th", 13);

pref("font.default.tr", "serif");
pref("font.size.variable.tr", 16);
pref("font.size.fixed.tr", 12);

pref("font.default.x-baltic", "serif");
pref("font.size.variable.x-baltic", 16);
pref("font.size.fixed.x-baltic", 12);

pref("font.default.x-central-euro", "serif");
pref("font.size.variable.x-central-euro", 16);
pref("font.size.fixed.x-central-euro", 12);

pref("font.default.x-cyrillic", "serif");
pref("font.size.variable.x-cyrillic", 16);
pref("font.size.fixed.x-cyrillic", 12);

pref("font.default.x-unicode", "serif");
pref("font.size.variable.x-unicode", 16);
pref("font.size.fixed.x-unicode", 12);

pref("font.default.x-user-def", "serif");
pref("font.size.variable.x-user-def", 16);
pref("font.size.fixed.x-user-def", 12);

pref("font.default.x-western", "serif");
pref("font.size.variable.x-western", 16);
pref("font.size.fixed.x-western", 12);

pref("font.default.zh-CN", "sans-serif");
pref("font.size.variable.zh-CN", 16);
pref("font.size.fixed.zh-CN", 16);

pref("font.default.zh-TW", "sans-serif");
pref("font.size.variable.zh-TW", 16);
pref("font.size.fixed.zh-TW", 16);

pref("font.default.zh-HK", "sans-serif");
pref("font.size.variable.zh-HK", 16);
pref("font.size.fixed.zh-HK", 16);

pref("font.default.x-devanagari", "serif");
pref("font.size.variable.x-devanagari", 16);
pref("font.size.fixed.x-devanagari", 13);

pref("font.default.x-tamil", "serif");
pref("font.size.variable.x-tamil", 16);
pref("font.size.fixed.x-tamil", 13);

pref("font.default.x-armn", "serif");
pref("font.size.variable.x-armn", 16);
pref("font.size.fixed.x-armn", 13);

pref("font.default.x-beng", "serif");
pref("font.size.variable.x-beng", 16);
pref("font.size.fixed.x-beng", 13);

pref("font.default.x-cans", "serif");
pref("font.size.variable.x-cans", 16);
pref("font.size.fixed.x-cans", 13);

pref("font.default.x-ethi", "serif");
pref("font.size.variable.x-ethi", 16);
pref("font.size.fixed.x-ethi", 13);

pref("font.default.x-geor", "serif");
pref("font.size.variable.x-geor", 16);
pref("font.size.fixed.x-geor", 13);

pref("font.default.x-gujr", "serif");
pref("font.size.variable.x-gujr", 16);
pref("font.size.fixed.x-gujr", 13);

pref("font.default.x-guru", "serif");
pref("font.size.variable.x-guru", 16);
pref("font.size.fixed.x-guru", 13);

pref("font.default.x-khmr", "serif");
pref("font.size.variable.x-khmr", 16);
pref("font.size.fixed.x-khmr", 13);

pref("font.default.x-mlym", "serif");
pref("font.size.variable.x-mlym", 16);
pref("font.size.fixed.x-mlym", 13);

pref("font.default.x-orya", "serif");
pref("font.size.variable.x-orya", 16);
pref("font.size.fixed.x-orya", 13);

pref("font.default.x-telu", "serif");
pref("font.size.variable.x-telu", 16);
pref("font.size.fixed.x-telu", 13);

pref("font.default.x-knda", "serif");
pref("font.size.variable.x-knda", 16);
pref("font.size.fixed.x-knda", 13);

pref("font.default.x-sinh", "serif");
pref("font.size.variable.x-sinh", 16);
pref("font.size.fixed.x-sinh", 13);

pref("font.default.x-tibt", "serif");
pref("font.size.variable.x-tibt", 16);
pref("font.size.fixed.x-tibt", 13);

/* PostScript print module prefs */
// pref("print.postscript.enabled",      true);
pref("print.postscript.paper_size",    "letter");
pref("print.postscript.orientation",   "portrait");
pref("print.postscript.print_command", "lpr ${MOZ_PRINTER_NAME:+-P\"$MOZ_PRINTER_NAME\"}");

// On GTK2 platform, we should use topmost window level for the default window
// level of <panel> element of XUL. GTK2 has only two window types. One is
// normal top level window, other is popup window. The popup window is always
// topmost window level, therefore, we are using normal top level window for
// non-topmost panel, but it is pretty hacky. On some Window Managers, we have
// 2 problems:
// 1. The non-topmost panel steals focus from its parent window at showing.
// 2. The parent of non-topmost panel is not activated when the panel is hidden.
// So, we have no reasons we should use non-toplevel window for popup.
pref("ui.panel.default_level_parent", true);

pref("mousewheel.system_scroll_override_on_root_content.enabled", false);

# XP_UNIX
#endif
#endif
#endif

#if OS_ARCH==OpenVMS

pref("mail.use_builtin_movemail", false);

pref("helpers.global_mime_types_file", "/sys$manager/netscape/mime.types");
pref("helpers.global_mailcap_file", "/sys$manager/netscape/mailcap");
pref("helpers.private_mime_types_file", "/sys$login/.mime.types");
pref("helpers.private_mailcaptypes_file", "/sys$login/.mailcap");

pref("applications.telnet", "create /term /detach \"telnet %h %p\"");
pref("applications.tn3270", "create /term /detach \"telnet /term=IBM-3278-5 %h %p\"");
pref("applications.rlogin", "create /term /detach \"rlogin %h\"");
pref("applications.rlogin_with_user", "create /term /detach \"rlogin %h -l %u\"");

/* PostScript module specific (see unix.js for additional configuration details) */
pref("print.postscript.print_command", "print /delete");
/* Print module independent */
pref("print.print_command", "print /delete");
pref("print.print_color", false);

pref("browser.cache.disk.capacity", 4096);
pref("plugin.soname.list", "");

# OpenVMS
#endif

#if OS_ARCH==AIX

// Override default Japanese fonts
pref("font.name.serif.ja", "dt-interface system-jisx0208.1983-0");
pref("font.name.sans-serif.ja", "dt-interface system-jisx0208.1983-0");
pref("font.name.monospace.ja", "dt-interface user-jisx0208.1983-0");

// Override default Cyrillic fonts
pref("font.name.serif.x-cyrillic", "dt-interface system-iso8859-5");
pref("font.name.sans-serif.x-cyrillic", "dt-interface system-iso8859-5");
pref("font.name.monospace.x-cyrillic", "dt-interface user-iso8859-5");

// Override default Unicode fonts
pref("font.name.serif.x-unicode", "dt-interface system-ucs2.cjk_japan-0");
pref("font.name.sans-serif.x-unicode", "dt-interface system-ucs2.cjk_japan-0");
pref("font.name.monospace.x-unicode", "dt-interface user-ucs2.cjk_japan-0");

# AIX
#endif

#ifdef SOLARIS

pref("print.postscript.print_command", "lp -c -s ${MOZ_PRINTER_NAME:+-d\"$MOZ_PRINTER_NAME\"}");
pref("print.print_command", "lp -c -s ${MOZ_PRINTER_NAME:+-d\"$MOZ_PRINTER_NAME\"}");

# Solaris
#endif

// Login Manager prefs
pref("signon.rememberSignons",              true);
pref("signon.SignonFileName",               "signons.txt"); // obsolete 
pref("signon.SignonFileName2",              "signons2.txt"); // obsolete
pref("signon.SignonFileName3",              "signons3.txt"); // obsolete
pref("signon.autofillForms",                true);
pref("signon.autologin.proxy",              false);
pref("signon.debug",                        false);

// Satchel (Form Manager) prefs
pref("browser.formfill.debug",            false);
pref("browser.formfill.enable",           true);
pref("browser.formfill.expire_days",      180);
pref("browser.formfill.saveHttpsForms",   true);
pref("browser.formfill.agedWeight",       2);
pref("browser.formfill.bucketSize",       1);
pref("browser.formfill.maxTimeGroupings", 25);
pref("browser.formfill.timeGroupingSize", 604800);
pref("browser.formfill.boundaryWeight",   25);
pref("browser.formfill.prefixWeight",     5);

// Zoom prefs
pref("browser.zoom.full", false);
pref("zoom.minPercent", 30);
pref("zoom.maxPercent", 300);
pref("toolkit.zoomManager.zoomValues", ".3,.5,.67,.8,.9,1,1.1,1.2,1.33,1.5,1.7,2,2.4,3");

// Image cache prefs
// The maximum size, in bytes, of the decoded images we cache
pref("image.cache.size", 5242880);
// A weight, from 0-1000, to place on time when comparing to size.
// Size is given a weight of 1000 - timeweight.
pref("image.cache.timeweight", 500);

// The default Accept header sent for images loaded over HTTP(S)
pref("image.http.accept", "image/png,image/*;q=0.8,*/*;q=0.5");

//
// Image memory management prefs
//

// Discards inactive image frames and re-decodes them on demand from
// compressed data.
pref("image.mem.discardable", true);

// Prevents images from automatically being decoded on load, instead allowing
// them to be decoded on demand when they are drawn.
pref("image.mem.decodeondraw", false);

// Minimum timeout for image discarding (in milliseconds). The actual time in
// which an image must inactive for it to be discarded will vary between this
// value and twice this value.
pref("image.mem.min_discard_timeout_ms", 10000);

// Chunk size for calls to the image decoders
pref("image.mem.decode_bytes_at_a_time", 200000);

// The longest time we can spend in an iteration of an async decode
pref("image.mem.max_ms_before_yield", 400);

// The maximum source data size for which we auto sync decode
pref("image.mem.max_bytes_for_sync_decode", 150000);

// WebGL prefs
// keep disabled on linux-x64 until bug 578877 is fixed
#ifdef _AMD64_
#ifdef MOZ_X11
// MOZ_X11 && AMD64
pref("webgl.enabled_for_all_sites", false);
#else
pref("webgl.enabled_for_all_sites", true);
#endif
#else
pref("webgl.enabled_for_all_sites", true);
#endif
pref("webgl.shader_validator", true);
pref("webgl.force_osmesa", false);
pref("webgl.mochitest_native_gl", false);
pref("webgl.osmesalib", "");
pref("webgl.verbose", false);

#ifdef XP_WIN
#ifndef WINCE
// The default TCP send window on Windows is too small, and autotuning only occurs on receive
pref("network.tcp.sendbuffer", 131072);
#endif
#endif

#ifdef WINCE
pref("mozilla.widget.disable-native-theme", true);
pref("gfx.color_management.mode", 0);
#endif

// Default value of acceleration for all widgets.
#ifdef XP_WIN
pref("layers.accelerate-all", true);
#else
#ifdef XP_MACOSX
pref("layers.accelerate-all", true);
#else
pref("layers.accelerate-all", false);
#endif
#endif

// Whether to allow acceleration on layers at all.
pref("layers.accelerate-none", false);

#ifdef XP_WIN
#ifndef WINCE
// Whether to disable the automatic detection and use of direct2d.
pref("gfx.direct2d.disabled", false);
// Whether to attempt to enable Direct2D regardless of automatic detection or
// blacklisting
pref("gfx.direct2d.force-enabled", false);

pref("layers.prefer-opengl", false);
pref("layers.prefer-d3d9", false);
#endif
#endif

// Enable/Disable the geolocation API for content
pref("geo.enabled", true);

// Enable/Disable the orientation API for content
pref("accelerometer.enabled", true);

// Enable/Disable HTML5 parser
pref("html5.enable", true);
// Toggle which thread the HTML5 parser uses for stream parsing
pref("html5.offmainthread", true);
// Time in milliseconds between the time a network buffer is seen and the 
// timer firing when the timer hasn't fired previously in this parse in the 
// off-the-main-thread HTML5 parser.
pref("html5.flushtimer.initialdelay", 120);
// Time in milliseconds between the time a network buffer is seen and the 
// timer firing when the timer has already fired previously in this parse.
pref("html5.flushtimer.subsequentdelay", 120);

// Push/Pop/Replace State prefs
pref("browser.history.allowPushState", true);
pref("browser.history.allowReplaceState", true);
pref("browser.history.allowPopState", true);
pref("browser.history.maxStateObjectSize", 655360);

// XPInstall prefs
pref("xpinstall.whitelist.required", true);
pref("extensions.alwaysUnpack", false);

pref("network.buffer.cache.count", 24);
pref("network.buffer.cache.size",  32768);
