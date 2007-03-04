pref("general.useragent.extra.minimo", "Minimo/0.015");

pref("keyword.enabled", true);
pref("keyword.URL", "http://www.google.com/xhtml?q=");

pref("browser.cache.disk.enable",           false);
pref("browser.cache.disk.capacity",         1024);
pref("browser.cache.memory.enable",         true);
pref("browser.cache.memory.capacity",       1024);

// -1 = determine dynamically, 0 = none, n = memory capacity in kilobytes
pref("browser.cache.disk_cache_ssl",        false);

// 0 = once-per-session, 1 = each-time, 2 = never, 3 = when-appropriate/automatically
pref("browser.cache.check_doc_frequency",   3);

pref("browser.sessionhistory.max_entries", 10);

// Fastback caching - if this pref is negative, then we calculate the number
// of content viewers to cache based on the amount of available memory.
pref("browser.sessionhistory.max_total_viewers", 0);

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
pref("browser.anchor_color",                "#0000EE");
pref("browser.active_color",                "#EE0000");
pref("browser.visited_color",               "#551A8B");
pref("browser.underline_anchors",           true);
pref("browser.blink_allowed",               true);
pref("browser.enable_automatic_image_resizing", false);

pref("browser.display.use_focus_colors",    false);
pref("browser.display.focus_background_color", "#117722");
pref("browser.display.focus_text_color",     "#ffffff");
pref("browser.display.focus_ring_width",     1);
pref("browser.display.focus_ring_on_anything", false);

pref("browser.triple_click_selects_paragraph", true);

pref("accessibility.browsewithcaret", false);
pref("accessibility.warn_on_browsewithcaret", true);

pref("browser.history_expire_days", 9);

// loading and rendering of framesets and iframes
pref("browser.frames.enabled", true);

// form submission
pref("browser.forms.submit.backwards_compatible", true);

// xxxbsmedberg more toolkit prefs?
// Tab browser preferences.
pref("browser.tabs.autoHide", true);
pref("browser.tabs.forceHide", false);
pref("browser.tabs.warnOnClose", true);
pref("browser.tabs.warnOnCloseOther", true);
// 0 = append, 1 = replace
pref("browser.tabs.loadGroup", 1);

// lets new tab/window load something different than first window
// -1 - use navigator startup preference
//  0 - loads blank page
//  1 - loads home page
//  2 - loads last page visited
pref("browser.tabs.loadOnNewTab", 0);
pref("browser.windows.loadOnNewWindow", 1);


pref("browser.link.open_newwindow", 3);
pref("browser.link.open_external", 3);
pref("browser.link.open_newwindow.restriction", 0);

// size of scrollbar snapping region
pref("slider.snapMultiplier", 6);

// dispatch left clicks only to content in browser (still allows clicks to chrome/xul)
pref("nglayout.events.dispatchLeftClickOnly", true);

// whether or not to use xbl form controls
pref("nglayout.debug.enable_xbl_forms", false);

// size of scrollbar snapping region
pref("slider.snapMultiplier", 6);

// option to choose plug-in finder
pref("application.use_ns_plugin_finder", false);

// Default Capability Preferences: Security-Critical! 
// Editing these may create a security risk - be sure you know what you're doing
//pref("capability.policy.default.barprop.visible.set", "UniversalBrowserWrite");

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

pref("capability.policy.default.HTMLDocument.close.get", "allAccess");
pref("capability.policy.default.HTMLDocument.open.get", "allAccess");

pref("capability.policy.default.Location.hash.set", "allAccess");
pref("capability.policy.default.Location.href.set", "allAccess");
pref("capability.policy.default.Location.reload.get", "allAccess");
pref("capability.policy.default.Location.replace.get", "allAccess");

pref("capability.policy.default.Navigator.preference", "allAccess");
pref("capability.policy.default.Navigator.preferenceinternal.get", "UniversalPreferencesRead");
pref("capability.policy.default.Navigator.preferenceinternal.set", "UniversalPreferencesWrite");

pref("capability.policy.default.Window.blur.get", "allAccess");
pref("capability.policy.default.Window.close.get", "allAccess");
pref("capability.policy.default.Window.closed.get", "allAccess");
pref("capability.policy.default.Window.Components", "allAccess");
pref("capability.policy.default.Window.document.get", "allAccess");
pref("capability.policy.default.Window.focus.get", "allAccess");
pref("capability.policy.default.Window.frames.get", "allAccess");
pref("capability.policy.default.Window.history.get", "allAccess");
pref("capability.policy.default.Window.length.get", "allAccess");
pref("capability.policy.default.Window.location", "allAccess");
pref("capability.policy.default.Window.opener.get", "allAccess");
pref("capability.policy.default.Window.parent.get", "allAccess");
pref("capability.policy.default.Window.self.get", "allAccess");
pref("capability.policy.default.Window.top.get", "allAccess");
pref("capability.policy.default.Window.window.get", "allAccess");

// Restrictions on the DOM for mail/news - see bugs 66938 and 84545
pref("capability.policy.mailnews.sites", "mailbox: imap: news:");

pref("capability.policy.mailnews.*.attributes.get", "noAccess");
pref("capability.policy.mailnews.*.baseURI.get", "noAccess");

// XMLExtras
pref("capability.policy.default.XMLHttpRequest.channel", "noAccess");
pref("capability.policy.default.XMLHttpRequest.getInterface", "noAccess");
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
pref("dom.disable_window_open_feature.directories", false);
pref("dom.disable_window_open_feature.personalbar", false);
pref("dom.disable_window_open_feature.menubar",     false);
pref("dom.disable_window_open_feature.scrollbars",  false);
pref("dom.disable_window_open_feature.resizable",   false);
pref("dom.disable_window_open_feature.minimizable", false);
pref("dom.disable_window_open_feature.status",      true);

pref("dom.allow_scripts_to_close_windows",          false);

pref("dom.disable_open_during_load",                false);
pref("dom.popup_maximum",                           20);
pref("dom.popup_allowed_events", "change click dblclick mouseup reset submit");
pref("dom.disable_open_click_delay", 5000);

// Disable popups from plugins by default
//   0 = openAllowed
//   1 = openControlled
//   2 = openAbused
pref("privacy.popups.disable_from_plugins", 2);

pref("dom.event.contextmenu.enabled",       true);

pref("javascript.enabled",                  true);
pref("javascript.allow.mailnews",           false);
pref("javascript.options.strict",           false);


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
pref("network.protocol-handler.external.disk", false);
pref("network.protocol-handler.external.disks", false);
pref("network.protocol-handler.external.afp", false);

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
pref("network.http.keep-alive.timeout", 600);

// limit the absolute number of http connections.
pref("network.http.max-connections", 24);

// limit the absolute number of http connections that can be established per
// host.  if a http proxy server is enabled, then the "server" is the proxy
// server.  Otherwise, "server" is the http origin server.
pref("network.http.max-connections-per-server", 8);

// if network.http.keep-alive is true, and if NOT connecting via a proxy, then
// a new connection will only be attempted if the number of active persistent
// connections to the server is less then max-persistent-connections-per-server.
pref("network.http.max-persistent-connections-per-server", 2);

// if network.http.keep-alive is true, and if connecting via a proxy, then a
// new connection will only be attempted if the number of active persistent
// connections to the proxy is less then max-persistent-connections-per-proxy.
pref("network.http.max-persistent-connections-per-proxy", 4);

// amount of time (in seconds) to suspend pending requests, before spawning a
// new connection, once the limit on the number of persistent connections per
// host has been reached.  however, a new connection will not be created if
// max-connections or max-connections-per-server has also been reached.
pref("network.http.request.max-start-delay", 10);

// Headers
pref("network.http.accept.default", "text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5");
pref("network.http.sendRefererHeader",      2); // 0=don't send any, 1=send only on clicks, 2=send on image requests as well

// Controls whether we send HTTPS referres to other HTTPS sites.
// By default this is enabled for compatibility (see bug 141641)
pref("network.http.sendSecureXSiteReferrer", true);

// Maximum number of consecutive redirects before aborting.
pref("network.http.redirection-limit", 20);

// Enable http compression: comment this out in case of problems with 1.1
// NOTE: support for "compress" has been disabled per bug 196406.
pref("network.http.accept-encoding" ,"gzip,deflate");

pref("network.http.pipelining"      , false);
pref("network.http.proxy.pipelining", false);

// Max number of requests in the pipeline
pref("network.http.pipelining.maxrequests" , 4);

// </http>

// This preference controls whether or not internationalized domain names (IDN)
// are handled.  IDN requires a nsIIDNService implementation.
pref("network.enableIDN", false);

// This preference specifies a list of domains for which DNS lookups will be
// IPv4 only. Works around broken DNS servers which can't handle IPv6 lookups
// and/or allows the user to disable IPv6 on a per-domain basis. See bug 68796.
pref("network.dns.ipv4OnlyDomains", ".doubleclick.net");

// This preference can be used to turn off IPv6 name lookups. See bug 68796.
pref("network.dns.disableIPv6", true);

// This preference controls whether or not URLs with UTF-8 characters are
// escaped.  Set this preference to TRUE for strict RFC2396 conformance.
pref("network.standard-url.escape-utf8", true);

// This preference controls whether or not URLs are always encoded and sent as
// UTF-8.
pref("network.standard-url.encode-utf8", false);

// directory listing format
// 2: HTML
// 3: XUL directory viewer
// all other values are treated like 2
pref("network.dir.format", 2);

// enables the prefetch service (i.e., prefetching of <link rel="next"> URLs).
pref("network.prefetch-next", false);


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

// sspitzer:  change this back to "news" when we get to beta.
// for now, set this to news.mozilla.org because you can only
// post to the server specified by this pref.
pref("network.hosts.nntp_server",           "news.mozilla.org");

pref("permissions.default.image",           1); // 1-Accept, 2-Deny, 3-dontAcceptForeign
pref("network.image.warnAboutImages",       false);
pref("network.proxy.autoconfig_url",        "");
pref("network.proxy.type",                  0);
pref("network.proxy.ftp",                   "");
pref("network.proxy.ftp_port",              0);
pref("network.proxy.gopher",                "");
pref("network.proxy.gopher_port",           0);
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
pref("network.cookie.cookieBehavior",       0); // 0-Accept, 1-dontAcceptForeign, 2-dontUse, 3-p3p
pref("network.cookie.disableCookieForMailNews", true); // disable all cookies for mail
pref("network.cookie.lifetimePolicy",       0); // accept normally, 1-askBeforeAccepting, 2-acceptForSession,3-acceptForNDays
pref("network.cookie.alwaysAcceptSessionCookies", false);
pref("network.cookie.prefsMigrated",        false);
pref("network.cookie.lifetime.days",        90);

// The following default value is for p3p medium mode.
// See xpfe/components/permissions/content/cookieP3P.xul for the definitions of low/medium/hi
pref("network.cookie.p3p",                  "ffffaaaa");
pref("network.cookie.p3plevel",             1); // 0=low, 1=medium, 2=high, 3=custom

pref("network.enablePad",                   false); // Allow client to do proxy autodiscovery
pref("converter.html2txt.structs",          true); // Output structured phrases (strong, em, code, sub, sup, b, i, u)
pref("converter.html2txt.header_strategy",  1); // 0 = no indention; 1 = indention, increased with header level; 2 = numbering and slight indention

pref("ime.password.onFocus.dontCare",       false);
pref("ime.password.onBlur.dontCare",        false);

// l12n and i18n
pref("intl.accept_languages", "chrome://global/locale/intl.properties");
// collationOption is only set on linux for japanese. see bug 18338 and 62015
// we need to check if this pref is still useful.
pref("intl.collationOption",  "chrome://global-platform/locale/intl.properties");
pref("intl.charsetmenu.browser.static", "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more1",  "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more2",  "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more3",  "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more4",  "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.more5",  "chrome://global/locale/intl.properties");
pref("intl.charsetmenu.browser.unicode",  "chrome://global/locale/intl.properties");
pref("intl.charset.detector", "chrome://global/locale/intl.properties");
pref("intl.charset.default",  "chrome://global-platform/locale/intl.properties");
pref("font.language.group", "chrome://global/locale/intl.properties");
pref("intl.menuitems.alwaysappendaccesskeys","chrome://global/locale/intl.properties");
pref("intl.menuitems.insertseparatorbeforeaccesskeys","chrome://global/locale/intl.properties");

pref("images.dither", "auto");
pref("security.directory",              "");

pref("signed.applets.codebase_principal_support", false);
pref("security.checkloaduri", true);
pref("security.xpconnect.plugin.unrestricted", true);
// security-sensitive dialogs should delay button enabling. In milliseconds.
pref("security.dialog_enable_delay", 2000);

// Clipboard behavior
pref("clipboard.autocopy", false);

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
//  Controls Text Mode
// ------------------
// 1 = logicalcontrolstextmodeBidiCmd
// 2 = visualcontrolstextmodeBidi <-- NO LONGER SUPPORTED
// 3 = containercontrolstextmodeBidi *
pref("bidi.controlstextmode", 1);
// ------------------
//  Numeral Style
// ------------------
// 0 = nominalnumeralBidi *
// 1 = regularcontextnumeralBidi
// 2 = hindicontextnumeralBidi
// 3 = arabicnumeralBidi
// 4 = hindinumeralBidi
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


// used for double-click word selection behavior. Win will override.
pref("layout.word_select.eat_space_to_next_word", true);
pref("layout.word_select.stop_at_punctuation", true);

// pref to control whether or not to replace backslashes with Yen signs
// in documents encoded in one of Japanese legacy encodings (EUC-JP, 
// Shift_JIS, ISO-2022-JP)
pref("layout.enable_japanese_specific_transform", false);

// pref to force frames to be resizable
pref("layout.frames.force_resizability", true);

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

pref("svg.enabled", false);

// Help Windows NT, 2000, and XP dialup a RAS connection
// when a network address is unreachable.
pref("network.autodial-helper.enabled", true);

pref("font.name-list.cursive.he", "Tahoma");
pref("font.name-list.cursive.ko", "Tahoma"); 
pref("font.name-list.monospace.he", "Tahoma");
pref("font.name-list.monospace.ja", "Tahoma");
pref("font.name-list.monospace.ko", "Tahoma"); 
pref("font.name-list.sans-serif.ja", "Tahoma");
pref("font.name-list.sans-serif.ko", "Tahoma"); 
pref("font.name-list.serif.he", "Tahoma");
pref("font.name-list.serif.ja", "Tahoma");
pref("font.name-list.serif.ko", "Tahoma"); 
pref("font.name.cursive.ar", "Tahoma");
pref("font.name.cursive.el", "Tahoma");
pref("font.name.cursive.he", "Tahoma");
pref("font.name.cursive.ko", "Tahoma"); // "Gungseo"
pref("font.name.cursive.th", "Tahoma");
pref("font.name.cursive.tr", "Tahoma");
pref("font.name.cursive.x-baltic", "Tahoma");
pref("font.name.cursive.x-central-euro", "Tahoma");
pref("font.name.cursive.x-cyrillic", "Tahoma");
pref("font.name.cursive.x-unicode", "Tahoma");
pref("font.name.cursive.x-western", "Tahoma");
pref("font.name.monospace.ar", "Tahoma");
pref("font.name.monospace.el", "Tahoma");
pref("font.name.monospace.he", "Tahoma");
pref("font.name.monospace.ja", "Tahoma"); // "MS Gothic"
pref("font.name.monospace.ko", "Tahoma"); // "GulimChe" 
pref("font.name.monospace.th", "Tahoma");
pref("font.name.monospace.tr", "Tahoma");
pref("font.name.monospace.x-baltic", "Tahoma");
pref("font.name.monospace.x-central-euro", "Tahoma");
pref("font.name.monospace.x-cyrillic", "Tahoma");
pref("font.name.monospace.x-unicode", "Tahoma");
pref("font.name.monospace.x-western", "Tahoma");
pref("font.name.sans-serif.ar", "Tahoma");
pref("font.name.sans-serif.el", "Tahoma");
pref("font.name.sans-serif.he", "Tahoma");
pref("font.name.sans-serif.ja", "Tahoma"); // "MS PGothic"
pref("font.name.sans-serif.ko", "Tahoma"); // "Gulim" 
pref("font.name.sans-serif.th", "Tahoma");
pref("font.name.sans-serif.tr", "Tahoma");
pref("font.name.sans-serif.x-baltic", "Tahoma");
pref("font.name.sans-serif.x-central-euro", "Tahoma");
pref("font.name.sans-serif.x-cyrillic", "Tahoma");
pref("font.name.sans-serif.x-unicode", "Tahoma");
pref("font.name.sans-serif.x-western", "Tahoma");
pref("font.name.serif.ar", "Tahoma");
pref("font.name.serif.el", "Tahoma");
pref("font.name.serif.he", "Tahoma");
pref("font.name.serif.ja", "Tahoma"); // "MS PMincho"
pref("font.name.serif.ko", "Tahoma"); // "Batang" 
pref("font.name.serif.th", "Tahoma");
pref("font.name.serif.tr", "Tahoma");
pref("font.name.serif.x-baltic", "Tahoma");
pref("font.name.serif.x-central-euro", "Tahoma");
pref("font.name.serif.x-cyrillic", "Tahoma");
pref("font.name.serif.x-unicode", "Tahoma");
pref("font.name.serif.x-western", "Tahoma");

pref("font.default.ar", "Tahoma");
pref("font.default.el", "Tahoma");
pref("font.default.he", "Tahoma");
pref("font.default.ja", "Tahoma");
pref("font.default.ko", "Tahoma");
pref("font.default.th", "Tahoma");
pref("font.default.tr", "Tahoma");
pref("font.default.x-armn", "Tahoma");
pref("font.default.x-baltic", "Tahoma");
pref("font.default.x-beng", "Tahoma");
pref("font.default.x-cans", "Tahoma");
pref("font.default.x-central-euro", "Tahoma");
pref("font.default.x-cyrillic", "Tahoma");
pref("font.default.x-devanagari", "Tahoma");
pref("font.default.x-ethi", "Tahoma");
pref("font.default.x-geor", "Tahoma");
pref("font.default.x-gujr", "Tahoma");
pref("font.default.x-guru", "Tahoma");
pref("font.default.x-khmr", "Tahoma");
pref("font.default.x-mlym", "Tahoma");
pref("font.default.x-tamil", "Tahoma");
pref("font.default.x-unicode", "Tahoma");
pref("font.default.x-western", "Tahoma");
pref("font.default.zh-CN", "Tahoma");
pref("font.default.zh-HK", "Tahoma");
pref("font.default.zh-TW", "Tahoma");

pref("security.warn_entering_secure", false);
pref("security.warn_entering_weak", false);
pref("security.warn_leaving_secure", false);
pref("security.warn_submit_insecure", false);

pref("network.autodial-helper.enabled", true);

pref("config.wince.overrideHWKeys", false);

pref("ssr.enabled", true);

pref("skey.enabled", true);

pref("browser.startup.homepage", "http://www.meer.net/~dougt/minimo_ce/start.html");

pref("browser.display.zoomui",10);
pref("browser.display.zoomcontent",10);

pref("snav.enabled", true);
pref("accessibility.tabfocus", 7);
pref("accessibility.tabfocus_applies_to_xul", false);

pref("browser.formfill.enable", true);


/* These are opts. for slower devices */


/* slow computer, slow connection */
pref("content.max.tokenizing.time", 2250000);
pref("content.notify.interval", 750000);
pref("content.notify.ontimer", true);
pref("content.switch.threshold", 750000);
pref("nglayout.initialpaint.delay", 750);
pref("network.http.max-connections", 32);
pref("network.http.max-connections-per-server", 8);
pref("network.http.max-persistent-connections-per-proxy", 8);
pref("network.http.max-persistent-connections-per-server", 4);
pref("dom.disable_window_status_change", true);

pref("browser.chrome.favicons",true);
pref("browser.chrome.site_icons",true);

pref("dom.max_script_run_time", 60);

/* Bookmark store as a pref for now, homebar section as well */

pref("browser.bookmark.store","<bm></bm>");
pref("browser.bookmark.homebar","<bm><li iconsrc='chrome://minimo/skin/extensions/icon-star.png'>chrome://minimo/content/rssview/rssload.xhtml?url=http://del.icio.us/rss/tag/mobile-sites</li><li iconsrc='chrome://minimo/skin/extensions/icon-google.png'>http://www.google.com/ig</li><li iconsrc='chrome://minimo/skin/extensions/icon-cash.png'>chrome://minimo/content/rssview/rssload.xhtml?url=http://www.xanadb.com/ticker/goog+yhoo+msft+nok+fte.pa</li><li iconsrc='http://www.yokel.com/favicon.ico'>http://www.yokel.com</li><li iconsrc='http://www.inner-browsing.com/shop/icon-shopping.png'>http://www.inner-browsing.com/shop/newshop.xhtml</li><li iconsrc='http://m.technorati.com/favicon.ico'>http://m.technorati.com/</li><li iconsrc='http://mobile.wunderground.com/favicon.ico'>http://mobile.wunderground.com/</li><li iconsrc='http://www.yelp.com/favicon.ico'>http://www.yelp.com</li><li iconsrc='http://www.meer.net/dougt/minimo_ce/start/update.cgi'>http://www.mozilla.org/projects/minimo/update/latest-update.xhtml</li></bm>");

pref("browser.download.dir", "\\Storage Card");
pref("browser.download.progressDnlgDialog.dontAskForLaunch", false);
pref("browser.cache.disk.parent_directory","\\Storage Card");
pref("browser.cache.disk.enable",false);

pref("signon.rememberSignons", true);
pref("signon.SignonFileName",  "signons.txt");
pref("signon.SignonFileName2", "signons2.txt");


