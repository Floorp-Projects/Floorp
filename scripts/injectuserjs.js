const userjs = `
// ** Theme Default Options ****************************************************
// userchrome.css usercontent.css activate
pref("toolkit.legacyUserProfileCustomizations.stylesheets", true);

// Fill SVG Color
pref("svg.context-properties.content.enabled", true);

// Restore Compact Mode - 89 Above
pref("browser.compactmode.show", true);

// about:home Search Bar - 89 Above
pref("browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar", false);

// CSS's\`:has()\` selector #457 - 103 Above
pref("layout.css.has-selector.enabled", true);

// Browser Theme Based Scheme - Will be activate 95 Above
// pref("layout.css.prefers-color-scheme.content-override", 3);

// ** Theme Related Options ****************************************************
// == Theme Distribution Settings ==============================================
// The rows that are located continuously must be changed \`true\`/\`false\` explicitly because there is a collision.
// https://github.com/black7375/Firefox-UI-Fix/wiki/Options#important
pref("userChrome.tab.connect_to_window",         false); // Original, Photon
pref("userChrome.tab.color_like_toolbar",        false); // Original, Photon

pref("userChrome.tab.lepton_like_padding",       false); // Original
pref("userChrome.tab.photon_like_padding",       false); // Photon

pref("userChrome.tab.dynamic_separator",          true); // Original, Proton
pref("userChrome.tab.static_separator",          false); // Photon
pref("userChrome.tab.static_separator.selected_accent", false); // Just option
pref("userChrome.tab.bar_separator",             false); // Just option

pref("userChrome.tab.newtab_button_like_tab",    false); // Original
pref("userChrome.tab.newtab_button_smaller",     false); // Photon
pref("userChrome.tab.newtab_button_proton",       true); // Proton

pref("userChrome.icon.panel_full",                true); // Original, Proton
pref("userChrome.icon.panel_photon",             false); // Photon

// Original Only
pref("userChrome.tab.box_shadow",                false);
pref("userChrome.tab.bottom_rounded_corner",     false);

// Photon Only
pref("userChrome.tab.photon_like_contextline",   false);
pref("userChrome.rounding.square_tab",           false);

// == Theme Default Settings ===================================================
// -- User Chrome --------------------------------------------------------------
pref("userChrome.compatibility.theme",       true);
pref("userChrome.compatibility.os",          true);

pref("userChrome.theme.built_in_contrast",   true);
pref("userChrome.theme.system_default",      true);
pref("userChrome.theme.proton_color",        true);
pref("userChrome.theme.proton_chrome",       true); // Need proton_color
pref("userChrome.theme.fully_color",         true); // Need proton_color
pref("userChrome.theme.fully_dark",          true); // Need proton_color

pref("userChrome.decoration.cursor",         true);
pref("userChrome.decoration.field_border",   true);
pref("userChrome.decoration.download_panel", true);
pref("userChrome.decoration.animate",        true);

pref("userChrome.padding.tabbar_width",      true);
pref("userChrome.padding.tabbar_height",     true);
pref("userChrome.padding.toolbar_button",    true);
pref("userChrome.padding.navbar_width",      true);
pref("userChrome.padding.urlbar",            true);
pref("userChrome.padding.bookmarkbar",       true);
pref("userChrome.padding.infobar",           true);
pref("userChrome.padding.menu",              true);
pref("userChrome.padding.bookmark_menu",     true);
pref("userChrome.padding.global_menubar",    true);
pref("userChrome.padding.panel",             true);
pref("userChrome.padding.popup_panel",       true);

pref("userChrome.tab.multi_selected",        true);
pref("userChrome.tab.unloaded",              true);
pref("userChrome.tab.letters_cleary",        true);
pref("userChrome.tab.close_button_at_hover", true);
pref("userChrome.tab.sound_hide_label",      true);
pref("userChrome.tab.sound_with_favicons",   true);
pref("userChrome.tab.pip",                   true);
pref("userChrome.tab.container",             true);
pref("userChrome.tab.crashed",               true);

pref("userChrome.fullscreen.overlap",        true);
pref("userChrome.fullscreen.show_bookmarkbar", true);

pref("userChrome.icon.library",              true);
pref("userChrome.icon.panel",                true);
pref("userChrome.icon.menu",                 true);
pref("userChrome.icon.context_menu",         true);
pref("userChrome.icon.global_menu",          true);
pref("userChrome.icon.global_menubar",       true);
pref("userChrome.icon.1-25px_stroke",        true);

// -- User Content -------------------------------------------------------------
pref("userContent.player.ui",             true);
pref("userContent.player.icon",           true);
pref("userContent.player.noaudio",        true);
pref("userContent.player.size",           true);
pref("userContent.player.click_to_play",  true);
pref("userContent.player.animate",        true);

pref("userContent.newTab.full_icon",      true);
pref("userContent.newTab.animate",        true);
pref("userContent.newTab.pocket_to_last", true);
pref("userContent.newTab.searchbar",      true);

pref("userContent.page.field_border",     true);
pref("userContent.page.illustration",     true);
pref("userContent.page.proton_color",     true);
pref("userContent.page.dark_mode",        true); // Need proton_color
pref("userContent.page.proton",           true); // Need proton_color

// ** Useful Options ***********************************************************
// Tab preview
// https://blog.nightly.mozilla.org/2024/02/06/a-preview-of-tab-previews-these-weeks-in-firefox-issue-153/
pref("browser.tabs.cardPreview.enabled",   true);

// Paste suggestion at urlbar
// https://blog.nightly.mozilla.org/2023/12/04/url-gonna-want-to-check-this-out-these-weeks-in-firefox-issue-150/
pref("browser.urlbar.clipboard.featureGate", true);

// Integrated calculator at urlbar
pref("browser.urlbar.suggest.calculator", true);`;

// do use `pref` instead of`user_pref`
// idk why but .. yes...

import * as fs from "node:fs/promises";

export async function injectUserJS(version) {
  const grepref = (await fs.readFile("dist/bin/greprefs.js")).toString();
  if (!grepref.includes(version)) {
    await fs.writeFile("dist/bin/greprefs.js", grepref + `\n//${version}\n${userjs}`);
  }
}
