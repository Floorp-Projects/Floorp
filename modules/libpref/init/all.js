/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* The prefs in this file are shipped with the GRE and should apply to all
 * embedding situations. Application-specific preferences belong somewhere else,
 * such as browser/app/profile/firefox.js or mobile/android/app/mobile.js.
 *
 * For the syntax used by this file, consult the comments at the top of
 * modules/libpref/parser/src/lib.rs.
 *
 * Some prefs, especially VarCache prefs, are defined in StaticPrefList.h
 * rather than this (or any other) data file.
 */

pref("keyword.enabled", false);
pref("general.useragent.compatMode.firefox", false);

// This pref exists only for testing purposes. In order to disable all
// overrides by default, don't initialize UserAgentOverrides.jsm.
pref("general.useragent.site_specific_overrides", true);

pref("general.config.obscure_value", 13); // for MCD .cfg files

pref("general.warnOnAboutConfig", true);

// maximum number of dated backups to keep at any time
pref("browser.bookmarks.max_backups",       5);

pref("browser.cache.disk.enable",           true);
// Is this the first-time smartsizing has been introduced?
pref("browser.cache.disk.smart_size.first_run", true);
// Does the user want smart-sizing?
pref("browser.cache.disk.smart_size.enabled", true);
// Which max value should we use for smart-sizing?
pref("browser.cache.disk.smart_size.use_old_max", true);
// Size (in KB) explicitly set by the user. Used when smart_size.enabled == false
pref("browser.cache.disk.capacity",         256000);
// When smartsizing is disabled we could potentially fill all disk space by
// cache data when the disk capacity is not set correctly. To avoid that we
// check the free space every time we write some data to the cache. The free
// space is checked against two limits. Once the soft limit is reached we start
// evicting the least useful entries, when we reach the hard limit writing to
// the entry fails.
pref("browser.cache.disk.free_space_soft_limit", 5120); // 5MB
pref("browser.cache.disk.free_space_hard_limit", 1024); // 1MB
// Max-size (in KB) for entries in disk cache. Set to -1 for no limit.
// (Note: entries bigger than 1/8 of disk-cache are never cached)
pref("browser.cache.disk.max_entry_size",    51200);  // 50 MB
pref("browser.cache.memory.enable",         true);
// -1 = determine dynamically, 0 = none, n = memory capacity in kilobytes
//pref("browser.cache.memory.capacity",     -1);
// Max-size (in KB) for entries in memory cache. Set to -1 for no limit.
// (Note: entries bigger than than 90% of the mem-cache are never cached)
pref("browser.cache.memory.max_entry_size",  5120);
// Memory limit (in kB) for new cache data not yet written to disk. Writes to
// the cache are buffered and written to disk on background with low priority.
// With a slow persistent storage these buffers may grow when data is coming
// fast from the network. When the amount of unwritten data is exceeded, new
// writes will simply fail. We have two buckets, one for important data
// (priority) like html, css, fonts and js, and one for other data like images,
// video, etc.
// Note: 0 means no limit.
pref("browser.cache.disk.max_chunks_memory_usage", 40960);
pref("browser.cache.disk.max_priority_chunks_memory_usage", 40960);

pref("browser.cache.disk_cache_ssl",        true);
// 0 = once-per-session, 1 = each-time, 2 = never, 3 = when-appropriate/automatically
pref("browser.cache.check_doc_frequency",   3);
// Limit of recent metadata we keep in memory for faster access, in Kb
pref("browser.cache.disk.metadata_memory_limit", 250); // 0.25 MB
// The number of chunks we preload ahead of read.  One chunk has currently 256kB.
pref("browser.cache.disk.preload_chunk_count", 4); // 1 MB of read ahead
// The half life used to re-compute cache entries frecency in hours.
pref("browser.cache.frecency_half_life_hours", 6);

// Number of seconds the cache spends writting pending data and closing files
// after the shutdown has been signalled.  Past that time data are never written
// and files are left open given up to the OS to do the cleanup.
pref("browser.cache.max_shutdown_io_lag", 2);

pref("browser.cache.offline.enable",           true);

// AppCache over insecure connection is disabled by default
pref("browser.cache.offline.insecure.enable",  false);

// enable offline apps by default, disable prompt
pref("offline-apps.allow_by_default",          true);

// offline cache capacity in kilobytes
pref("browser.cache.offline.capacity",         512000);

// the user should be warned if offline app disk usage exceeds this amount
// (in kilobytes)
pref("offline-apps.quota.warn",        51200);

// zlib compression level used for cache compression:
// 0 => disable compression
// 1 => best speed
// 9 => best compression
// cache compression turned off for now - see bug #715198
pref("browser.cache.compression_level", 0);

// Don't show "Open with" option on download dialog if true.
pref("browser.download.forbid_open_with", false);

// Remove navigator.registerContentHandler
pref("dom.registerContentHandler.enabled", false);

// Insecure registerProtocolHandler is disabled by default
pref("dom.registerProtocolHandler.insecure.enabled", false);

// Whether or not testing features are enabled.
pref("dom.quotaManager.testing", false);

// Whether or not indexedDB is enabled.
pref("dom.indexedDB.enabled", true);
// Whether or not indexedDB experimental features are enabled.
pref("dom.indexedDB.experimental", false);
// Enable indexedDB logging.
pref("dom.indexedDB.logging.enabled", true);
// Detailed output in log messages.
pref("dom.indexedDB.logging.details", true);
// Enable profiler marks for indexedDB events.
pref("dom.indexedDB.logging.profiler-marks", false);
// Enable passing the "storage" option to indexedDB.open.
pref("dom.indexedDB.storageOption.enabled", false);

// Whether or not File Handle is enabled.
pref("dom.fileHandle.enabled", true);

// Whether window.onappinstalled from "W3C Web Manifest" is enabled
pref("dom.manifest.onappinstalled", false);

// Whether or not selection events are enabled
pref("dom.select_events.enabled", true);

// Whether or not selection events on text controls are enabled
#ifdef NIGHTLY_BUILD
pref("dom.select_events.textcontrols.enabled", true);
#else
pref("dom.select_events.textcontrols.enabled", false);
#endif

// The number of workers per domain allowed to run concurrently.
// We're going for effectively infinite, while preventing abuse.
pref("dom.workers.maxPerDomain", 512);

pref("dom.serviceWorkers.enabled", false);

// The amount of time (milliseconds) service workers keep running after each event.
pref("dom.serviceWorkers.idle_timeout", 30000);

// The amount of time (milliseconds) service workers can be kept running using waitUntil promises.
pref("dom.serviceWorkers.idle_extended_timeout", 300000);

// The amount of time (milliseconds) an update request is delayed when triggered
// by a service worker that doesn't control any clients.
pref("dom.serviceWorkers.update_delay", 1000);

// Enable test for 24 hours update, service workers will always treat last update check time is over 24 hours
pref("dom.serviceWorkers.testUpdateOverOneDay", false);

// Whether nonzero values can be returned from performance.timing.*
pref("dom.enable_performance", true);

// Whether resource timing will be gathered and returned by performance.GetEntries*
pref("dom.enable_resource_timing", true);

// Whether performance.GetEntries* will contain an entry for the active document
pref("dom.enable_performance_navigation_timing", true);

// Enable printing performance marks/measures to log
pref("dom.performance.enable_user_timing_logging", false);

// Enable notification of performance timing
pref("dom.performance.enable_notify_performance_timing", false);

// Enable collecting of docgroup activity in the scheduler
pref("dom.performance.enable_scheduler_timing", false);

// Enable Permission API's .revoke() method
pref("dom.permissions.revoke.enable", false);

// Enable exposing timeToNonBlankPaint
pref("dom.performance.time_to_non_blank_paint.enabled", false);

// Enable exposing timeToDOMContentFlushed
pref("dom.performance.time_to_dom_content_flushed.enabled", false);

// Enable Performance Observer API
pref("dom.enable_performance_observer", true);

// Enable requestIdleCallback API
pref("dom.requestIdleCallback.enabled", true);

// Whether the Gamepad API is enabled
pref("dom.gamepad.enabled", true);
pref("dom.gamepad.test.enabled", false);
#ifdef RELEASE_OR_BETA
pref("dom.gamepad.non_standard_events.enabled", false);
#else
pref("dom.gamepad.non_standard_events.enabled", true);
#endif
pref("dom.gamepad.extensions.enabled", true);
pref("dom.gamepad.haptic_feedback.enabled", true);

// If this is true, TextEventDispatcher dispatches keydown and keyup events
// even during composition (keypress events are never fired during composition
// even if this is true).
#ifdef EARLY_BETA_OR_EARLIER
pref("dom.keyboardevent.dispatch_during_composition", true);
#else
pref("dom.keyboardevent.dispatch_during_composition", false);
#endif

// If this is true, TextEventDispatcher dispatches keypress event with setting
// WidgetEvent::mFlags::mOnlySystemGroupDispatchInContent to true if it won't
// cause inputting printable character.
#ifdef NIGHTLY_BUILD
pref("dom.keyboardevent.keypress.dispatch_non_printable_keys_only_system_group_in_content", true);
// Blacklist of domains of web apps which are not aware of strict keypress
// dispatching behavior.  This is comma separated list.  If you need to match
// all sub-domains, you can specify it as "*.example.com".  Additionally, you
// can limit the path.  E.g., "example.com/foo" means "example.com/foo*".  So,
// if you need to limit under a directory, the path should end with "/" like
// "example.com/foo/".  Note that this cannot limit port number for now.
pref("dom.keyboardevent.keypress.hack.dispatch_non_printable_keys",
     "docs.google.com,mail.google.com,hangouts.google.com,keep.google.com,inbox.google.com,*.etherpad.org/p/,etherpad.wikimedia.org/p/,board.net/p/,pad.riseup.net/p/,*.sandstorm.io,factor.cc/pad/,*.etherpad.fr/p/,piratenpad.de/p/,notes.typo3.org/p/,etherpad.net/p/,*.framapad.org/p/,pad.ouvaton.coop/,pad.systemli.org/p/,pad.lqdn.fr/p/,public.etherpad-mozilla.org/p/,*.cloudron.me/p/,pad.aquilenet.fr/p/,free.primarypad.com/p/,pad.ondesk.work/p/,demo.maadix.org/etherpad/pads/,paper.dropbox.com/doc");
#else
pref("dom.keyboardevent.keypress.dispatch_non_printable_keys_only_system_group_in_content", false);
#endif

// Whether the WebMIDI API is enabled
pref("dom.webmidi.enabled", false);

// Whether to enable the JavaScript start-up cache. This causes one of the first
// execution to record the bytecode of the JavaScript function used, and save it
// in the existing cache entry. On the following loads of the same script, the
// bytecode would be loaded from the cache instead of being generated once more.
pref("dom.script_loader.bytecode_cache.enabled", true);

// Ignore the heuristics of the bytecode cache, and always record on the first
// visit. (used for testing purposes).

// Choose one strategy to use to decide when the bytecode should be encoded and
// saved. The following strategies are available right now:
//   * -2 : (reader mode) The bytecode cache would be read, but it would never
//          be saved.
//   * -1 : (eager mode) The bytecode would be saved as soon as the script is
//          seen for the first time, independently of the size or last access
//          time.
//   *  0 : (default) The bytecode would be saved in order to minimize the
//          page-load time.
//
// Other values might lead to experimental strategies. For more details, have a
// look at: ScriptLoader::ShouldCacheBytecode function.
pref("dom.script_loader.bytecode_cache.strategy", 0);

#ifdef JS_BUILD_BINAST
pref("dom.script_loader.binast_encoding.enabled", false);
#endif

// Fastback caching - if this pref is negative, then we calculate the number
// of content viewers to cache based on the amount of available memory.
pref("browser.sessionhistory.max_total_viewers", -1);

pref("ui.use_native_colors", true);
pref("ui.click_hold_context_menus", false);

// Pop up context menu on mouseup instead of mousedown, if that's the OS default.
// Note: ignored on Windows (context menus always use mouseup)
pref("ui.context_menus.after_mouseup", false);
// Duration of timeout of incremental search in menus (ms).  0 means infinite.
pref("ui.menu.incremental_search.timeout", 1000);
// If true, all popups won't hide automatically on blur
pref("ui.popup.disable_autohide", false);

pref("browser.display.use_document_fonts",  1);  // 0 = never, 1 = quick, 2 = always
// 0 = default: always, except in high contrast mode
// 1 = always
// 2 = never
pref("browser.display.document_color_use", 0);
pref("browser.display.use_system_colors",   false);
pref("browser.display.foreground_color",    "#000000");
pref("browser.display.background_color",    "#FFFFFF");
pref("browser.display.force_inline_alttext", false); // true = force ALT text for missing images to be layed out inline
// 0 = no external leading,
// 1 = use external leading only when font provides,
// 2 = add extra leading both internal leading and external leading are zero
pref("browser.display.normal_lineheight_calc_control", 2);
// enable showing image placeholders while image is loading or when image is broken
pref("browser.display.show_image_placeholders", true);
// if browser.display.show_image_placeholders is true then this controls whether the loading image placeholder and border is shown or not
pref("browser.display.show_loading_image_placeholder", false);
// min font device pixel size at which to turn on high quality
pref("browser.display.auto_quality_min_font_size", 20);
pref("browser.anchor_color",                "#0000EE");
pref("browser.active_color",                "#EE0000");
pref("browser.visited_color",               "#551A8B");
pref("browser.underline_anchors",           true);
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

pref("browser.helperApps.neverAsk.saveToDisk", "");
pref("browser.helperApps.neverAsk.openFile", "");
pref("browser.helperApps.deleteTempFileOnExit", false);

// xxxbsmedberg: where should prefs for the toolkit go?
pref("browser.chrome.toolbar_tips",         true);
// max image size for which it is placed in the tab icon for tabbrowser.
// if 0, no images are used for tab icons for image documents.
pref("browser.chrome.image_icons.max_size", 1024);

pref("browser.triple_click_selects_paragraph", true);

// Print/Preview Shrink-To-Fit won't shrink below 20% for text-ish documents.
pref("print.shrink-to-fit.scale-limit-percent", 20);

// Whether we should display simplify page checkbox on print preview UI
pref("print.use_simplify_page", false);

// Disable support for MathML
pref("mathml.disabled",    false);

// Enable scale transform for stretchy MathML operators. See bug 414277.
pref("mathml.scale_stretchy_operators.enabled", true);

// Used by ChannelMediaResource to run data callbacks from HTTP channel
// off the main thread.
pref("media.omt_data_delivery.enabled", true);

// We'll throttle the download if the download rate is throttle-factor times
// the estimated playback rate, AND we satisfy the cache readahead_limit
// above. The estimated playback rate is time_duration/length_in_bytes.
// This means we'll only throttle the download if there's no concern that
// throttling would cause us to stop and buffer.
pref("media.throttle-factor", 2);
// By default, we'll throttle media download once we've reached the the
// readahead_limit if the download is fast. This pref toggles the "and the
// download is fast" check off, so that we can always throttle the download
// once the readaheadd limit is reached even on a slow network.
pref("media.throttle-regardless-of-download-rate", false);

// Master HTML5 media volume scale.
pref("media.volume_scale", "1.0");

// Whether we should play videos opened in a "video document", i.e. videos
// opened as top-level documents, as opposed to inside a media element.
pref("media.play-stand-alone", true);

pref("media.hardware-video-decoding.enabled", true);
pref("media.hardware-video-decoding.force-enabled", false);

#ifdef MOZ_WMF
pref("media.wmf.dxva.enabled", true);
pref("media.wmf.dxva.d3d11.enabled", true);
pref("media.wmf.dxva.max-videos", 8);
pref("media.wmf.low-latency.enabled", false);
pref("media.wmf.amd.vp9.enabled", false);
pref("media.wmf.amd.highres.enabled", true);
pref("media.wmf.allow-unsupported-resolutions", false);
pref("media.wmf.use-nv12-format", true);
pref("media.wmf.disable-d3d11-for-dlls", "igd11dxva64.dll: 20.19.15.4463, 20.19.15.4454, 20.19.15.4444, 20.19.15.4416, 20.19.15.4404, 20.19.15.4390, 20.19.15.4380, 20.19.15.4377, 20.19.15.4364, 20.19.15.4360, 20.19.15.4352, 20.19.15.4331, 20.19.15.4326, 20.19.15.4300; igd10iumd32.dll: 20.19.15.4444, 20.19.15.4424, 20.19.15.4409, 20.19.15.4390, 20.19.15.4380, 20.19.15.4360, 10.18.10.4358, 20.19.15.4331, 20.19.15.4312, 20.19.15.4300, 10.18.15.4281, 10.18.15.4279, 10.18.10.4276, 10.18.15.4268, 10.18.15.4256, 10.18.10.4252, 10.18.15.4248, 10.18.14.4112, 10.18.10.3958, 10.18.10.3496, 10.18.10.3431, 10.18.10.3412, 10.18.10.3355, 9.18.10.3234, 9.18.10.3071, 9.18.10.3055, 9.18.10.3006; igd10umd32.dll: 9.17.10.4229, 9.17.10.3040, 9.17.10.2884, 9.17.10.2857, 8.15.10.2274, 8.15.10.2272, 8.15.10.2246, 8.15.10.1840, 8.15.10.1808; igd10umd64.dll: 9.17.10.4229, 9.17.10.2884, 9.17.10.2857, 10.18.10.3496; isonyvideoprocessor.dll: 4.1.2247.8090, 4.1.2153.6200; tosqep.dll: 1.2.15.526, 1.1.12.201, 1.0.11.318, 1.0.11.215, 1.0.10.1224; tosqep64.dll: 1.1.12.201, 1.0.11.215; nvwgf2um.dll: 22.21.13.8253, 22.21.13.8233, 22.21.13.8205, 22.21.13.8189, 22.21.13.8178, 22.21.13.8165, 21.21.13.7892, 21.21.13.7878, 21.21.13.7866, 21.21.13.7849, 21.21.13.7654, 21.21.13.7653, 21.21.13.7633, 21.21.13.7619, 21.21.13.7563, 21.21.13.7306, 21.21.13.7290, 21.21.13.7270, 21.21.13.7254, 21.21.13.6939, 21.21.13.6926, 21.21.13.6909, 21.21.13.4201, 21.21.13.4200, 10.18.13.6881, 10.18.13.6839, 10.18.13.6510, 10.18.13.6472, 10.18.13.6143, 10.18.13.5946, 10.18.13.5923, 10.18.13.5921, 10.18.13.5891, 10.18.13.5887, 10.18.13.5582, 10.18.13.5445, 10.18.13.5382, 10.18.13.5362, 9.18.13.4788, 9.18.13.4752, 9.18.13.4725, 9.18.13.4709, 9.18.13.4195, 9.18.13.4192, 9.18.13.4144, 9.18.13.4052, 9.18.13.3788, 9.18.13.3523, 9.18.13.3235, 9.18.13.3165, 9.18.13.2723, 9.18.13.2702, 9.18.13.1422, 9.18.13.1407, 9.18.13.1106, 9.18.13.546; atidxx32.dll: 21.19.151.3, 21.19.142.257, 21.19.137.514, 21.19.137.1, 21.19.134.1, 21.19.128.7, 21.19.128.4, 20.19.0.32837, 20.19.0.32832, 8.17.10.682, 8.17.10.671, 8.17.10.661, 8.17.10.648, 8.17.10.644, 8.17.10.625, 8.17.10.605, 8.17.10.581, 8.17.10.569, 8.17.10.560, 8.17.10.545, 8.17.10.539, 8.17.10.531, 8.17.10.525, 8.17.10.520, 8.17.10.519, 8.17.10.514, 8.17.10.511, 8.17.10.494, 8.17.10.489, 8.17.10.483, 8.17.10.453, 8.17.10.451, 8.17.10.441, 8.17.10.436, 8.17.10.432, 8.17.10.425, 8.17.10.418, 8.17.10.414, 8.17.10.401, 8.17.10.395, 8.17.10.385, 8.17.10.378, 8.17.10.362, 8.17.10.355, 8.17.10.342, 8.17.10.331, 8.17.10.318, 8.17.10.310, 8.17.10.286, 8.17.10.269, 8.17.10.261, 8.17.10.247, 8.17.10.240, 8.15.10.212; atidxx64.dll: 21.19.151.3, 21.19.142.257, 21.19.137.514, 21.19.137.1, 21.19.134.1, 21.19.128.7, 21.19.128.4, 20.19.0.32832, 8.17.10.682, 8.17.10.661, 8.17.10.644, 8.17.10.625; nvumdshim.dll: 10.18.13.6822");
pref("media.wmf.disable-d3d9-for-dlls", "igdumd64.dll: 8.15.10.2189, 8.15.10.2119, 8.15.10.2104, 8.15.10.2102, 8.771.1.0; atiumd64.dll: 7.14.10.833, 7.14.10.867, 7.14.10.885, 7.14.10.903, 7.14.10.911, 8.14.10.768, 9.14.10.1001, 9.14.10.1017, 9.14.10.1080, 9.14.10.1128, 9.14.10.1162, 9.14.10.1171, 9.14.10.1183, 9.14.10.1197, 9.14.10.945, 9.14.10.972, 9.14.10.984, 9.14.10.996");
pref("media.wmf.deblacklisting-for-telemetry-in-gpu-process", true);
pref("media.wmf.play-stand-alone", true);
pref("media.wmf.use-sync-texture", true);
#endif
pref("media.gmp.decoder.aac", 0);
pref("media.gmp.decoder.h264", 0);
pref("media.opus.enabled", true);
pref("media.wave.enabled", true);
pref("media.webm.enabled", true);

// GMP storage version number. At startup we check the version against
// media.gmp.storage.version.observed, and if the versions don't match,
// we clear storage and set media.gmp.storage.version.observed=expected.
// This provides a mechanism to clear GMP storage when non-compatible
// changes are made.
pref("media.gmp.storage.version.expected", 1);

// Filter what triggers user notifications.
// See DecoderDoctorDocumentWatcher::ReportAnalysis for details.
#ifdef NIGHTLY_BUILD
pref("media.decoder-doctor.notifications-allowed", "MediaWMFNeeded,MediaWidevineNoWMF,MediaCannotInitializePulseAudio,MediaCannotPlayNoDecoders,MediaUnsupportedLibavcodec,MediaDecodeError");
#else
pref("media.decoder-doctor.notifications-allowed", "MediaWMFNeeded,MediaWidevineNoWMF,MediaCannotInitializePulseAudio,MediaCannotPlayNoDecoders,MediaUnsupportedLibavcodec");
#endif
pref("media.decoder-doctor.decode-errors-allowed", "");
pref("media.decoder-doctor.decode-warnings-allowed", "");
// Whether we report partial failures.
pref("media.decoder-doctor.verbose", false);
// URL to report decode issues
pref("media.decoder-doctor.new-issue-endpoint", "https://webcompat.com/issues/new");

#ifdef MOZ_WEBRTC
pref("media.navigator.enabled", true);
pref("media.navigator.video.enabled", true);
pref("media.navigator.video.default_fps",30);
pref("media.navigator.video.use_remb", true);
pref("media.navigator.video.use_tmmbr", false);
pref("media.navigator.audio.use_fec", true);
pref("media.navigator.video.red_ulpfec_enabled", false);

pref("media.peerconnection.dtmf.enabled", true);

pref("media.webrtc.debug.trace_mask", 0);
pref("media.webrtc.debug.multi_log", false);
pref("media.webrtc.debug.log_file", "");
pref("media.webrtc.debug.aec_dump_max_size", 4194304); // 4MB

pref("media.navigator.video.default_width",0);  // adaptive default
pref("media.navigator.video.default_height",0); // adaptive default
pref("media.peerconnection.enabled", true);
pref("media.peerconnection.video.enabled", true);
pref("media.navigator.video.max_fs", 12288); // Enough for 2048x1536
pref("media.navigator.video.max_fr", 60);
pref("media.navigator.video.h264.level", 31); // 0x42E01f - level 3.1
pref("media.navigator.video.h264.max_br", 0);
pref("media.navigator.video.h264.max_mbps", 0);
pref("media.peerconnection.video.vp9_enabled", true);
pref("media.peerconnection.video.vp9_preferred", false);
pref("media.getusermedia.aec", 1);
pref("media.getusermedia.browser.enabled", false);
pref("media.getusermedia.channels", 0);
#if defined(ANDROID)
pref("media.getusermedia.camera.off_while_disabled.enabled", false);
pref("media.getusermedia.microphone.off_while_disabled.enabled", false);
#else
pref("media.getusermedia.camera.off_while_disabled.enabled", true);
pref("media.getusermedia.microphone.off_while_disabled.enabled", true);
#endif
pref("media.getusermedia.camera.off_while_disabled.delay_ms", 3000);
pref("media.getusermedia.microphone.off_while_disabled.delay_ms", 3000);
// Desktop is typically VGA capture or more; and qm_select will not drop resolution
// below 1/2 in each dimension (or so), so QVGA (320x200) is the lowest here usually.
pref("media.peerconnection.video.min_bitrate", 0);
pref("media.peerconnection.video.start_bitrate", 0);
pref("media.peerconnection.video.max_bitrate", 0);
pref("media.peerconnection.video.min_bitrate_estimate", 0);
pref("media.peerconnection.video.denoising", false);
pref("media.navigator.audio.fake_frequency", 1000);
pref("media.navigator.permission.disabled", false);
pref("media.navigator.streams.fake", false);
pref("media.peerconnection.simulcast", true);
pref("media.peerconnection.default_iceservers", "[]");
pref("media.peerconnection.ice.loopback", false); // Set only for testing in offline environments.
pref("media.peerconnection.ice.tcp", true);
pref("media.peerconnection.ice.tcp_so_sock_count", 0); // Disable SO gathering
pref("media.peerconnection.ice.link_local", false); // Set only for testing IPV6 in networks that don't assign IPV6 addresses
pref("media.peerconnection.ice.force_interface", ""); // Limit to only a single interface
pref("media.peerconnection.ice.relay_only", false); // Limit candidates to TURN
pref("media.peerconnection.use_document_iceservers", true);
pref("media.peerconnection.identity.enabled", true);
pref("media.peerconnection.identity.timeout", 10000);
pref("media.peerconnection.ice.stun_client_maximum_transmits", 7);
pref("media.peerconnection.ice.trickle_grace_period", 5000);
pref("media.peerconnection.ice.no_host", false);
pref("media.peerconnection.ice.default_address_only", false);
pref("media.peerconnection.ice.proxy_only", false);
pref("media.peerconnection.rtpsourcesapi.enabled", true);

// These values (aec, agc, and noise) are from media/webrtc/trunk/webrtc/common_types.h
// kXxxUnchanged = 0, kXxxDefault = 1, and higher values are specific to each
// setting (for Xxx = Ec, Agc, or Ns).  Defaults are all set to kXxxDefault here.
pref("media.peerconnection.turn.disable", false);
#if defined(MOZ_WEBRTC_HARDWARE_AEC_NS)
pref("media.getusermedia.aec_enabled", false);
pref("media.getusermedia.noise_enabled", false);
#else
pref("media.getusermedia.aec_enabled", true);
pref("media.getusermedia.noise_enabled", true);
#endif
pref("media.getusermedia.aec_extended_filter", true);
pref("media.getusermedia.noise", 1);
pref("media.getusermedia.agc_enabled", false);
pref("media.getusermedia.agc", 3); // kAgcAdaptiveDigital
// capture_delay: Adjustments for OS-specific input delay (lower bound)
// playout_delay: Adjustments for OS-specific AudioStream+cubeb+output delay (lower bound)
// full_duplex: enable cubeb full-duplex capture/playback
pref("media.navigator.audio.full_duplex", true);
#if defined(XP_MACOSX)
pref("media.peerconnection.capture_delay", 50);
#elif defined(XP_WIN)
pref("media.peerconnection.capture_delay", 50);
#elif defined(ANDROID)
pref("media.peerconnection.capture_delay", 100);
#else
pref("media.peerconnection.capture_delay", 70);
#endif
#endif

pref("dom.webaudio.enabled", true);

// Exposes the navigator.webdriver attribute.
pref("dom.webdriver.enabled", true);

#if !defined(ANDROID)
pref("media.getusermedia.screensharing.enabled", true);
#endif

pref("media.getusermedia.audiocapture.enabled", false);

// TextTrack WebVTT Region extension support.
pref("media.webvtt.regions.enabled", true);

// WebVTT pseudo element and class support.
pref("media.webvtt.pseudo.enabled", true);

// AudioTrack and VideoTrack support
pref("media.track.enabled", false);

// Whether to enable MediaSource support.
pref("media.mediasource.enabled", true);

pref("media.mediasource.mp4.enabled", true);

#if defined(XP_WIN) || defined(XP_MACOSX) || defined(MOZ_WIDGET_ANDROID)
pref("media.mediasource.webm.enabled", false);
#else
pref("media.mediasource.webm.enabled", true);
#endif
pref("media.mediasource.webm.audio.enabled", true);

// Whether to enable MediaSource v2 support.
pref("media.mediasource.experimental.enabled", false);

pref("media.benchmark.vp9.threshold", 150);
pref("media.benchmark.frames", 300);
pref("media.benchmark.timeout", 1000);

#ifdef MOZ_WEBSPEECH
pref("media.webspeech.synth.enabled", false);
#endif
#ifdef MOZ_WEBM_ENCODER
pref("media.encoder.webm.enabled", true);
#endif

// Whether to allow recording of AudioNodes with MediaRecorder
pref("media.recorder.audio_node.enabled", false);

// Whether MediaRecorder's video encoder should allow dropping frames in order
// to keep up under load. Useful for tests but beware of memory consumption!
pref("media.recorder.video.frame_drops", true);

// Whether to autostart a media element with an |autoplay| attribute
pref("media.autoplay.enabled", true);

// If "media.autoplay.enabled" is false, and this pref is true, then audible media
// would only be allowed to autoplay after website has been activated by specific
// user gestures, but the non-audible media won't be restricted.
#ifdef NIGHTLY_BUILD
pref("media.autoplay.enabled.user-gestures-needed", false);
#endif

// The default number of decoded video frames that are enqueued in
// MediaDecoderReader's mVideoQueue.
pref("media.video-queue.default-size", 10);

// The maximum number of queued frames to send to the compositor.
// By default, send all of them.
pref("media.video-queue.send-to-compositor-size", 9999);

// Whether to disable the video stats to prevent fingerprinting
pref("media.video_stats.enabled", true);

// Log level for cubeb, the audio input/output system. Valid values are
// "verbose", "normal" and "" (log disabled).
pref("media.cubeb.logging_level", "");

// Cubeb sandbox (remoting) control
#ifdef XP_LINUX
pref("media.cubeb.sandbox", true);
pref("media.audioipc.pool_size", 2);
// 64 * 4 kB stack per pool thread.
pref("media.audioipc.stack_size", 262144);
#else
pref("media.cubeb.sandbox", false);
#endif

pref("media.webaudio.audiocontextoptions-samplerate.enabled", true);

// Weather we allow AMD switchable graphics
pref("layers.amd-switchable-gfx.enabled", true);

// Whether to use async panning and zooming
pref("layers.async-pan-zoom.enabled", true);

// Whether to enable event region building during painting
pref("layout.event-regions.enabled", false);

// Whether to enable arbitrary layer geometry for OpenGL compositor
pref("layers.geometry.opengl.enabled", true);

// Whether to enable arbitrary layer geometry for Basic compositor
pref("layers.geometry.basic.enabled", true);

// Whether to enable arbitrary layer geometry for DirectX compositor
pref("layers.geometry.d3d11.enabled", true);

// APZ preferences. For documentation/details on what these prefs do, check
// gfx/layers/apz/src/AsyncPanZoomController.cpp.
pref("apz.allow_checkerboarding", true);
pref("apz.allow_immediate_handoff", true);
pref("apz.allow_zooming", false);
pref("apz.android.chrome_fling_physics.enabled", true);
pref("apz.android.chrome_fling_physics.friction", "0.015");
pref("apz.android.chrome_fling_physics.inflexion", "0.35");
pref("apz.android.chrome_fling_physics.stop_threshold", "0.1");
pref("apz.autoscroll.enabled", true);

// Whether to lock touch scrolling to one axis at a time
// 0 = FREE (No locking at all)
// 1 = STANDARD (Once locked, remain locked until scrolling ends)
// 2 = STICKY (Allow lock to be broken, with hysteresis)
pref("apz.axis_lock.mode", 0);
pref("apz.axis_lock.lock_angle", "0.5235987");        // PI / 6 (30 degrees)
pref("apz.axis_lock.breakout_threshold", "0.03125");  // 1/32 inches
pref("apz.axis_lock.breakout_angle", "0.3926991");    // PI / 8 (22.5 degrees)
pref("apz.axis_lock.direct_pan_angle", "1.047197");   // PI / 3 (60 degrees)
pref("apz.content_response_timeout", 400);
pref("apz.drag.enabled", true);
pref("apz.drag.initial.enabled", true);
pref("apz.drag.touch.enabled", true);
pref("apz.danger_zone_x", 50);
pref("apz.danger_zone_y", 100);
pref("apz.disable_for_scroll_linked_effects", false);
pref("apz.displayport_expiry_ms", 15000);
pref("apz.enlarge_displayport_when_clipped", false);
pref("apz.fling_accel_base_mult", "1.0");
pref("apz.fling_accel_interval_ms", 500);
pref("apz.fling_accel_min_velocity", "1.5");
pref("apz.fling_accel_supplemental_mult", "1.0");
pref("apz.fling_curve_function_x1", "0.0");
pref("apz.fling_curve_function_y1", "0.0");
pref("apz.fling_curve_function_x2", "1.0");
pref("apz.fling_curve_function_y2", "1.0");
pref("apz.fling_curve_threshold_inches_per_ms", "-1.0");
pref("apz.fling_friction", "0.002");
pref("apz.fling_min_velocity_threshold", "0.5");
pref("apz.fling_stop_on_tap_threshold", "0.05");
pref("apz.fling_stopped_threshold", "0.01");
pref("apz.frame_delay.enabled", true);
#if !defined(MOZ_WIDGET_ANDROID)
pref("apz.keyboard.enabled", true);
pref("apz.keyboard.passive-listeners", true);
#else
pref("apz.keyboard.enabled", false);
pref("apz.keyboard.passive-listeners", false);
#endif
pref("apz.max_tap_time", 300);
pref("apz.max_velocity_inches_per_ms", "-1.0");
pref("apz.max_velocity_queue_size", 5);
pref("apz.min_skate_speed", "1.0");
pref("apz.minimap.enabled", false);
pref("apz.one_touch_pinch.enabled", true);
pref("apz.overscroll.enabled", false);
pref("apz.overscroll.min_pan_distance_ratio", "1.0");
pref("apz.overscroll.spring_friction", "0.015");
pref("apz.overscroll.spring_stiffness", "0.0018");
pref("apz.overscroll.stop_distance_threshold", "5.0");
pref("apz.overscroll.stop_velocity_threshold", "0.01");
pref("apz.overscroll.stretch_factor", "0.35");
pref("apz.paint_skipping.enabled", true);
// Fetch displayport updates early from the message queue
pref("apz.peek_messages.enabled", true);
pref("apz.pinch_lock.mode", 1);
pref("apz.pinch_lock.scoll_lock_threshold", "0.03125");  // 1/32 inches
pref("apz.pinch_lock.span_breakout_threshold", "0.03125");  // 1/32 inches
pref("apz.pinch_lock.span_lock_threshold", "0.03125");  // 1/32 inches
pref("apz.popups.enabled", false);

// Whether to print the APZC tree for debugging
pref("apz.printtree", false);

#ifdef NIGHTLY_BUILD
pref("apz.record_checkerboarding", true);
#else
pref("apz.record_checkerboarding", false);
#endif
pref("apz.second_tap_tolerance", "0.5");
pref("apz.test.logging_enabled", false);
pref("apz.touch_start_tolerance", "0.1");
pref("apz.touch_move_tolerance", "0.1");
pref("apz.velocity_bias", "0.0");
pref("apz.velocity_relevance_time_ms", 150);
pref("apz.x_skate_highmem_adjust", "0.0");
pref("apz.y_skate_highmem_adjust", "0.0");
pref("apz.x_skate_size_multiplier", "1.25");
pref("apz.y_skate_size_multiplier", "3.5");
pref("apz.x_stationary_size_multiplier", "1.5");
pref("apz.y_stationary_size_multiplier", "3.5");
pref("apz.zoom_animation_duration_ms", 250);
pref("apz.scale_repaint_delay_ms", 500);

#if defined(MOZ_WIDGET_ANDROID)
// Mobile prefs
pref("apz.allow_zooming", true);
pref("apz.enlarge_displayport_when_clipped", true);
pref("apz.y_skate_size_multiplier", "1.5");
pref("apz.y_stationary_size_multiplier", "1.5");
#endif

#ifdef XP_MACOSX
// Whether to run in native HiDPI mode on machines with "Retina"/HiDPI display;
//   <= 0 : hidpi mode disabled, display will just use pixel-based upscaling
//   == 1 : hidpi supported if all screens share the same backingScaleFactor
//   >= 2 : hidpi supported even with mixed backingScaleFactors (somewhat broken)
pref("gfx.hidpi.enabled", 2);
#endif

#if !defined(MOZ_WIDGET_ANDROID)
// Use containerless scrolling for now on desktop.
pref("layout.scroll.root-frame-containers", false);
#endif

pref("layout.scrollbars.always-layerize-track", false);

// Whether to enable LayerScope tool and default listening port
pref("gfx.layerscope.enabled", false);
pref("gfx.layerscope.port", 23456);

// Log severe performance warnings to the error console and profiles.
// This should be use to quickly find which slow paths are used by test cases.
pref("gfx.perf-warnings.enabled", false);

// 0 = Off, 1 = Full, 2 = Tagged Images Only.
// See eCMSMode in gfx/thebes/gfxPlatform.h
pref("gfx.color_management.mode", 2);
pref("gfx.color_management.display_profile", "");
pref("gfx.color_management.rendering_intent", 0);
pref("gfx.color_management.enablev4", false);

pref("gfx.downloadable_fonts.enabled", true);
pref("gfx.downloadable_fonts.fallback_delay", 3000);
pref("gfx.downloadable_fonts.fallback_delay_short", 100);

// disable downloadable font cache so that behavior is consistently
// the uncached load behavior across pages (useful for testing reflow problems)
pref("gfx.downloadable_fonts.disable_cache", false);

pref("gfx.downloadable_fonts.woff2.enabled", true);

// Whether OTS validation should be applied to OpenType Layout (OTL) tables
#ifdef RELEASE_OR_BETA
pref("gfx.downloadable_fonts.otl_validation", false);
#else
pref("gfx.downloadable_fonts.otl_validation", true);
#endif

// Whether to preserve color bitmap tables in fonts (bypassing OTS).
// Currently these are supported only on platforms where we use Freetype
// to render fonts (Linux/Gtk and Android).
pref("gfx.downloadable_fonts.keep_color_bitmaps", false);

// Whether to validate OpenType variation tables in fonts
pref("gfx.downloadable_fonts.validate_variation_tables", true);

#ifdef ANDROID
pref("gfx.bundled_fonts.enabled", true);
pref("gfx.bundled_fonts.force-enabled", false);
#endif

// Do we fire a notification about missing fonts, so the front-end can decide
// whether to try and do something about it (e.g. download additional fonts)?
pref("gfx.missing_fonts.notify", false);

// prefs controlling the font (name/cmap) loader that runs shortly after startup
#ifdef XP_WIN
pref("gfx.font_loader.delay", 120000);         // 2 minutes after startup
pref("gfx.font_loader.interval", 1000);        // every 1 second until complete
#else
pref("gfx.font_loader.delay", 8000);           // 8 secs after startup
pref("gfx.font_loader.interval", 50);          // run every 50 ms
#endif

// whether to always search all font cmaps during system font fallback
pref("gfx.font_rendering.fallback.always_use_cmaps", false);

// cache shaped word results
pref("gfx.font_rendering.wordcache.charlimit", 32);

// cache shaped word results
pref("gfx.font_rendering.wordcache.maxentries", 10000);

pref("gfx.font_rendering.graphite.enabled", true);

#ifdef XP_WIN
pref("gfx.font_rendering.directwrite.use_gdi_table_loading", true);
#endif

#ifdef XP_WIN
// comma separated list of backends to use in order of preference
// e.g., pref("gfx.canvas.azure.backends", "direct2d,skia,cairo");
pref("gfx.canvas.azure.backends", "direct2d1.1,skia,cairo");
pref("gfx.content.azure.backends", "direct2d1.1,skia,cairo");
#else
#ifdef XP_MACOSX
pref("gfx.content.azure.backends", "skia");
pref("gfx.canvas.azure.backends", "skia");
// Accelerated cg canvas where available (10.7+)
pref("gfx.canvas.azure.accelerated", true);
#else
pref("gfx.canvas.azure.backends", "skia");
pref("gfx.content.azure.backends", "skia");
#endif
#endif

pref("gfx.canvas.skiagl.dynamic-cache", true);

pref("gfx.text.disable-aa", false);

pref("gfx.work-around-driver-bugs", true);

pref("gfx.draw-color-bars", false);

pref("gfx.logging.painted-pixel-count.enabled", false);
pref("gfx.logging.texture-usage.enabled", false);
pref("gfx.logging.peak-texture-usage.enabled", false);

pref("gfx.ycbcr.accurate-conversion", false);

// On Nightly, we expose two prefs: gfx.webrender.all and gfx.webrender.enabled.
// The first enables WR+additional features, and the second just enables WR.
// For developer convenience, building with --enable-webrender=true or just
// --enable-webrender will set gfx.webrender.enabled to true by default.
// On non-Nightly, we ignore these prefs.
//
// On both Nightly and non-Nightly, we have a pref gfx.webrender.all.qualified
// which is not exposed via about:config. That pref enables WR but only on
// qualified hardware. This is the pref we'll eventually flip to deploy WebRender
// to the target population.
#ifdef NIGHTLY_BUILD
pref("gfx.webrender.all", false);
#ifdef MOZ_ENABLE_WEBRENDER
pref("gfx.webrender.enabled", true);
#else
pref("gfx.webrender.enabled", false);
#endif
#endif

#ifdef XP_WIN
pref("gfx.webrender.force-angle", true);
pref("gfx.webrender.dcomp-win.enabled", true);
pref("gfx.webrender.program-binary", true);
pref("gfx.webrender.program-binary-disk", true);
#endif

#ifdef XP_MACOSX
pref("gfx.compositor.glcontext.opaque", false);
#endif

pref("gfx.webrender.highlight-painted-layers", false);
pref("gfx.webrender.async-scene-build", true);
pref("gfx.webrender.blob-images", true);
pref("gfx.webrender.blob.invalidation", true);
pref("gfx.webrender.hit-test", true);

// WebRender debugging utilities.
pref("gfx.webrender.debug.texture-cache", false);
pref("gfx.webrender.debug.render-targets", false);
pref("gfx.webrender.debug.alpha-primitives", false);
pref("gfx.webrender.debug.profiler", false);
pref("gfx.webrender.debug.gpu-time-queries", false);
pref("gfx.webrender.debug.gpu-sample-queries", false);
pref("gfx.webrender.debug.disable-batching", false);
pref("gfx.webrender.debug.epochs", false);
pref("gfx.webrender.debug.compact-profiler", false);

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
#else
// Only on mac tabfocus is expected to handle UI widgets as well as web content
pref("accessibility.tabfocus_applies_to_xul", true);
#endif

// We follow the "Click in the scrollbar to:" system preference on OS X and
// "gtk-primary-button-warps-slider" property with GTK (since 2.24 / 3.6),
// unless this preference is explicitly set.
#if !defined(XP_MACOSX) && !defined(MOZ_WIDGET_GTK)
pref("ui.scrollToClick", 0);
#endif

// provide ability to turn on support for canvas focus rings
pref("canvas.focusring.enabled", true);
pref("canvas.customfocusring.enabled", false);
pref("canvas.hitregions.enabled", false);
pref("canvas.filters.enabled", true);
// Add support for canvas path objects
pref("canvas.path.enabled", true);
pref("canvas.capturestream.enabled", true);

// Disable the ImageBitmap-extensions for now.
pref("canvas.imagebitmap_extensions.enabled", false);

// We want the ability to forcibly disable platform a11y, because
// some non-a11y-related components attempt to bring it up.  See bug
// 538530 for details about Windows; we have a pref here that allows it
// to be disabled for performance and testing resons.
// See bug 761589 for the crossplatform aspect.
//
// This pref is checked only once, and the browser needs a restart to
// pick up any changes.
//
// Values are -1 always on. 1 always off, 0 is auto as some platform perform
// further checks.
pref("accessibility.force_disabled", 0);

pref("accessibility.AOM.enabled", false);

#ifdef XP_WIN
// Some accessibility tools poke at windows in the plugin process during setup
// which can cause hangs.  To hack around this set accessibility.delay_plugins
// to true, you can also try increasing accessibility.delay_plugin_time if your
// machine is slow and you still experience hangs.
// See bug 781791.
pref("accessibility.delay_plugins", false);
pref("accessibility.delay_plugin_time", 10000);

// The COM handler used for Windows e10s performance and live regions
pref("accessibility.handler.enabled", true);
#endif

pref("focusmanager.testmode", false);

pref("accessibility.usetexttospeech", "");
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
#ifdef XP_MACOSX
pref("accessibility.typeaheadfind.prefillwithselection", false);
#else
pref("accessibility.typeaheadfind.prefillwithselection", true);
#endif
pref("accessibility.typeaheadfind.matchesCountLimit", 1000);
pref("findbar.highlightAll", false);
pref("findbar.modalHighlight", false);
pref("findbar.entireword", false);
pref("findbar.iteratorTimeout", 100);

// use Mac OS X Appearance panel text smoothing setting when rendering text, disabled by default
pref("gfx.use_text_smoothing_setting", false);

// Number of characters to consider emphasizing for rich autocomplete results
pref("toolkit.autocomplete.richBoundaryCutoff", 200);

// Variable controlling logging for osfile.
pref("toolkit.osfile.log", false);

pref("toolkit.cosmeticAnimations.enabled", true);

pref("toolkit.scrollbox.smoothScroll", true);
pref("toolkit.scrollbox.scrollIncrement", 20);
pref("toolkit.scrollbox.verticalScrollDistance", 3);
pref("toolkit.scrollbox.horizontalScrollDistance", 5);
pref("toolkit.scrollbox.clickToScroll.scrollDelay", 150);

// Telemetry settings.
// Server to submit telemetry pings to.
pref("toolkit.telemetry.server", "https://incoming.telemetry.mozilla.org");
// Telemetry server owner. Please change if you set toolkit.telemetry.server to a different server
pref("toolkit.telemetry.server_owner", "Mozilla");
// Determines whether full SQL strings are returned when they might contain sensitive info
// i.e. dynamically constructed SQL strings or SQL executed by addons against addon DBs
pref("toolkit.telemetry.debugSlowSql", false);
// Whether to use the unified telemetry behavior, requires a restart.
pref("toolkit.telemetry.unified", true);
// AsyncShutdown delay before crashing in case of shutdown freeze
#ifndef MOZ_ASAN
pref("toolkit.asyncshutdown.crash_timeout", 60000); // 1 minute
#else
// MOZ_ASAN builds can be considerably slower. Extending the grace period
// of both asyncshutdown and the terminator.
pref("toolkit.asyncshutdown.crash_timeout", 180000); // 3 minutes
#endif // MOZ_ASAN
// Extra logging for AsyncShutdown barriers and phases
pref("toolkit.asyncshutdown.log", false);

#ifdef MOZILLA_OFFICIAL
// enable JS dump() function.
pref("browser.dom.window.dump.enabled", false, sticky);
#else
pref("browser.dom.window.dump.enabled", true, sticky);
#endif

// Controls whether EventEmitter module throws dump message on each emit
pref("toolkit.dump.emit", false);

// view source
pref("view_source.syntax_highlight", true);
pref("view_source.wrap_long_lines", false);
pref("view_source.editor.path", "");
// allows to add further arguments to the editor; use the %LINE% placeholder
// for jumping to a specific line (e.g. "/line:%LINE%" or "--goto %LINE%")
pref("view_source.editor.args", "");

// When true this will word-wrap plain text documents.
pref("plain_text.wrap_long_lines", false);

// whether or not to draw images while dragging
pref("nglayout.enable_drag_images", true);

// enable/disable paint flashing --- useful for debugging
// the first one applies to everything, the second one only to chrome
pref("nglayout.debug.paint_flashing", false);
pref("nglayout.debug.paint_flashing_chrome", false);

// enable/disable widget update area flashing --- only supported with
// BasicLayers (other layer managers always update the entire widget area)
pref("nglayout.debug.widget_update_flashing", false);

// Enable/disable display list invalidation logging --- useful for debugging.
pref("nglayout.debug.invalidation", false);

// Whether frame visibility tracking is enabled globally.
pref("layout.framevisibility.enabled", true);

pref("layout.framevisibility.numscrollportwidths", 0);
pref("layout.framevisibility.numscrollportheights", 1);

// scrollbar snapping region
// 0 - off
// 1 and higher - slider thickness multiple
pref("slider.snapMultiplier", 0);

// URI fixup prefs
pref("browser.fixup.alternate.enabled", true);
pref("browser.fixup.alternate.prefix", "www.");
pref("browser.fixup.alternate.suffix", ".com");
pref("browser.fixup.dns_first_for_single_words", false);
pref("browser.fixup.hide_user_pass", true);

// Location Bar AutoComplete
pref("browser.urlbar.autocomplete.enabled", true);
// This is disabled until Bug 1340663 figures out the remaining requirements.
pref("browser.urlbar.usepreloadedtopurls.enabled", false);
pref("browser.urlbar.usepreloadedtopurls.expire_days", 14);

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

// Print via the parent process. This is only used when e10s is enabled.
#if !defined(MOZ_WIDGET_ANDROID)
pref("print.print_via_parent", true);
#else
pref("print.print_via_parent", false);
#endif

// Variation fonts can't always be embedded in certain output formats
// such as PDF. To work around this, draw the variation fonts using
// paths instead of using font embedding.
pref("print.font-variations-as-paths", true);

// Pref used by the spellchecker extension to control the
// maximum number of misspelled words that will be underlined
// in a document.
pref("extensions.spellcheck.inline.max-misspellings", 500);

// Prefs used by libeditor. Prefs specific to seamonkey composer
// belong in comm-central/editor/ui/composer.js

pref("editor.use_custom_colors", false);
pref("editor.singleLine.pasteNewlines",      2);
pref("editor.use_css",                       false);
pref("editor.css.default_length_unit",       "px");
pref("editor.resizing.preserve_ratio",       true);
pref("editor.positioning.offset",            0);
pref("editor.use_div_for_default_newlines",  true);

// Scripts & Windows prefs
pref("dom.disable_beforeunload",            false);
pref("dom.disable_window_flip",             false);
pref("dom.disable_window_move_resize",      false);

pref("dom.disable_window_open_feature.titlebar",    false);
pref("dom.disable_window_open_feature.close",       false);
pref("dom.disable_window_open_feature.toolbar",     false);
pref("dom.disable_window_open_feature.location",    false);
pref("dom.disable_window_open_feature.personalbar", false);
pref("dom.disable_window_open_feature.menubar",     false);
pref("dom.disable_window_open_feature.resizable",   true);
pref("dom.disable_window_open_feature.minimizable", false);
pref("dom.disable_window_open_feature.status",      true);
pref("dom.disable_window_showModalDialog",          true);

pref("dom.allow_scripts_to_close_windows",          false);

pref("dom.require_user_interaction_for_beforeunload", true);

pref("dom.disable_open_during_load",                false);
pref("dom.popup_maximum",                           20);
pref("dom.popup_allowed_events", "change click dblclick mouseup pointerup notificationclick reset submit touchend");

pref("dom.disable_open_click_delay", 1000);
pref("dom.serviceWorkers.disable_open_click_delay", 1000);

pref("dom.storage.enabled", true);
pref("dom.storage.default_quota",      5120);
pref("dom.storage.testing", false);

pref("dom.send_after_paint_to_content", false);

// Timeout clamp in ms for timeouts we clamp
pref("dom.min_timeout_value", 4);
// And for background windows
pref("dom.min_background_timeout_value", 1000);
// Timeout clamp in ms for tracking timeouts we clamp
// Note that this requires the privacy.trackingprotection.annotate_channels pref to be on in order to have any effect.
pref("dom.min_tracking_timeout_value", 4);
// And for background windows
// Note that this requires the privacy.trackingprotection.annotate_channels pref to be on in order to have any effect.
pref("dom.min_tracking_background_timeout_value", 4);
// Delay in ms from document load until we start throttling background timeouts.
pref("dom.timeout.throttling_delay", 30000);

// Time (in ms) that it takes to regenerate 1ms.
pref("dom.timeout.background_budget_regeneration_rate", 100);
// Maximum value (in ms) for the background budget. Only valid for
// values greater than 0.
pref("dom.timeout.background_throttling_max_budget", 50);
// Time (in ms) that it takes to regenerate 1ms.
pref("dom.timeout.foreground_budget_regeneration_rate", 1);
// Maximum value (in ms) for the background budget. Only valid for
// values greater than 0.
pref("dom.timeout.foreground_throttling_max_budget", -1);
// The maximum amount a timeout can be delayed by budget throttling
pref("dom.timeout.budget_throttling_max_delay", 15000);
// Turn on budget throttling by default
pref("dom.timeout.enable_budget_timer_throttling", true);

// Don't use new input types
pref("dom.experimental_forms", false);

// Enable <input type=color> by default. It will be turned off for remaining
// platforms which don't have a color picker implemented yet.
pref("dom.forms.color", true);

// Support for input type=date and type=time.
pref("dom.forms.datetime", true);

// Support for input type=month, type=week and type=datetime-local. By default,
// disabled.
pref("dom.forms.datetime.others", false);

// Enable time picker UI. By default, disabled.
pref("dom.forms.datetime.timepicker", false);

// Support @autocomplete values for form autofill feature.
pref("dom.forms.autocomplete.formautofill", false);

// Enable search in <select> dropdowns (more than 40 options)
pref("dom.forms.selectSearch", false);
// Allow for webpages to provide custom styling for <select>
// popups. Disabled on GTK due to bug 1338283.
#ifdef MOZ_WIDGET_GTK
pref("dom.forms.select.customstyling", false);
#else
pref("dom.forms.select.customstyling", true);
#endif
pref("dom.select_popup_in_parent.enabled", false);
// Bug 1421229 - content-select
pref("dom.select_popup_in_content.enabled", false);

// Enable Directory API. By default, disabled.
pref("dom.input.dirpicker", false);

// Enable not moving the cursor to end when a text input or textarea has .value
// set to the value it already has.  By default, enabled.
pref("dom.input.skip_cursor_move_for_same_value_set", true);

pref("dom.cycle_collector.incremental", true);

// Whether to shim a Components object on untrusted windows.
#ifdef NIGHTLY_BUILD
pref("dom.use_components_shim", false);
#else // NIGHTLY_BUILD
pref("dom.use_components_shim", true);
#endif // NIGHTLY_BUILD

// Parsing perf prefs. For now just mimic what the old code did.
#ifndef XP_WIN
pref("content.sink.pending_event_mode", 0);
#endif

// Disable popups from plugins by default
//   0 = openAllowed
//   1 = openControlled
//   2 = openBlocked
//   3 = openAbused
pref("privacy.popups.disable_from_plugins", 3);

// send "do not track" HTTP header, disabled by default
pref("privacy.donottrackheader.enabled",    false);
// If true, close button will be shown on permission prompts
// and for all PopupNotifications, the secondary action of
// the popup will be called when the popup is dismissed.
pref("privacy.permissionPrompts.showCloseButton", false);
// Enforce tracking protection in all modes
pref("privacy.trackingprotection.enabled",  false);
// Enforce tracking protection in Private Browsing mode
pref("privacy.trackingprotection.pbmode.enabled",  true);
// Annotate channels based on the tracking protection list in all modes
pref("privacy.trackingprotection.annotate_channels",  true);
// First Party Isolation (double keying), disabled by default
pref("privacy.firstparty.isolate",                        false);
// If false, two windows in the same domain with different first party domains
// (top level URLs) can access resources through window.opener.
// This pref is effective only when "privacy.firstparty.isolate" is true.
pref("privacy.firstparty.isolate.restrict_opener_access", true);
// Anti-fingerprinting, disabled by default
pref("privacy.resistFingerprinting", false);
// We automatically decline canvas permission requests if they are not initiated
// from user input. Just in case that breaks something, we allow the user to revert
// this behaior with this obscure pref. We do not intend to support this long term.
// If you do set it, to work around some broken website, please file a bug with
// information so we can understand why it is needed.
pref("privacy.resistFingerprinting.autoDeclineNoUserInputCanvasPrompts", true);
// A subset of Resist Fingerprinting protections focused specifically on timers for testing
// This affects the Animation API, the performance APIs, Date.getTime, Event.timestamp,
//   File.lastModified, audioContext.currentTime, canvas.captureStream.currentTime
pref("privacy.reduceTimerPrecision", true);
// Dynamically tune the resolution of the timer reduction for both of the two above prefs
pref("privacy.resistFingerprinting.reduceTimerPrecision.microseconds", 1000);
// Enable jittering the clock one precision value forward
pref("privacy.resistFingerprinting.reduceTimerPrecision.jitter", true);
// Lower the priority of network loads for resources on the tracking protection list.
// Note that this requires the privacy.trackingprotection.annotate_channels pref to be on in order to have any effect.
#ifdef NIGHTLY_BUILD
pref("privacy.trackingprotection.lower_network_priority", true);
#else
pref("privacy.trackingprotection.lower_network_priority", false);
#endif

pref("dom.event.contextmenu.enabled",       true);
pref("dom.event.clipboardevents.enabled",   true);
pref("dom.event.highrestimestamp.enabled",  true);
pref("dom.event.coalesce_mouse_move",       true);

#ifdef NIGHTLY_BUILD
pref("dom.webcomponents.shadowdom.enabled", true);
pref("dom.webcomponents.customelements.enabled", true);
#else
pref("dom.webcomponents.shadowdom.enabled", false);
pref("dom.webcomponents.customelements.enabled", false);
#endif

pref("javascript.enabled",                  true);
// Enable Array.prototype.values
pref("javascript.options.array_prototype_values", true);
pref("javascript.options.strict",           false);
#ifdef DEBUG
pref("javascript.options.strict.debug",     false);
#endif
pref("javascript.options.baselinejit",      true);
pref("javascript.options.ion",              true);
pref("javascript.options.asmjs",            true);
pref("javascript.options.wasm",             true);
pref("javascript.options.wasm_ionjit",      true);
pref("javascript.options.wasm_baselinejit", true);
#ifdef ENABLE_WASM_GC
pref("javascript.options.wasm_gc",          false);
#endif
pref("javascript.options.native_regexp",    true);
pref("javascript.options.parallel_parsing", true);
#if !defined(RELEASE_OR_BETA) && !defined(ANDROID) && !defined(XP_IOS)
pref("javascript.options.asyncstack",       true);
#else
pref("javascript.options.asyncstack",       false);
#endif
pref("javascript.options.throw_on_asmjs_validation_failure", false);
pref("javascript.options.ion.offthread_compilation", true);
#ifdef DEBUG
pref("javascript.options.jit.full_debug_checks", false);
#endif
// This preference instructs the JS engine to discard the
// source of any privileged JS after compilation. This saves
// memory, but makes things like Function.prototype.toSource()
// fail.
pref("javascript.options.discardSystemSource", false);

// Many of the the following preferences tune the SpiderMonkey GC, if you
// change the defaults here please also consider changing them in
// js/src/jsgc.cpp.  They're documented in js/src/jsapi.h.

// JSGC_MAX_MALLOC_BYTES
// How much malloc memory can be allocated before triggering a GC, in MB.
pref("javascript.options.mem.high_water_mark", 128);

// JSGC_MAX_BYTES
// SpiderMonkey defaults to 2^32-1 bytes, but this is measured in MB so that
// cannot be represented directly in order to show it in about:config.
pref("javascript.options.mem.max", -1);

// JSGC_MAX_NURSERY_BYTES
#if defined(ANDROID) || defined(XP_IOS) || defined(MOZ_B2G)
pref("javascript.options.mem.nursery.max_kb", 4096);
#else
pref("javascript.options.mem.nursery.max_kb", 16384);
#endif

// JSGC_MODE
pref("javascript.options.mem.gc_per_zone", true);
pref("javascript.options.mem.gc_incremental", true);

// JSGC_SLICE_TIME_BUDGET
// Override the shell's default of unlimited slice time.
pref("javascript.options.mem.gc_incremental_slice_ms", 5);

// JSGC_COMPACTING_ENABLED
pref("javascript.options.mem.gc_compacting", true);

// JSGC_HIGH_FREQUENCY_TIME_LIMIT
pref("javascript.options.mem.gc_high_frequency_time_limit_ms", 1000);

// JSGC_HIGH_FREQUENCY_LOW_LIMIT
pref("javascript.options.mem.gc_high_frequency_low_limit_mb", 100);

// JSGC_HIGH_FREQUENCY_HIGH_LIMIT
pref("javascript.options.mem.gc_high_frequency_high_limit_mb", 500);

// JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX
pref("javascript.options.mem.gc_high_frequency_heap_growth_max", 300);

// JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN
pref("javascript.options.mem.gc_high_frequency_heap_growth_min", 150);

// JSGC_LOW_FREQUENCY_HEAP_GROWTH
pref("javascript.options.mem.gc_low_frequency_heap_growth", 150);

// JSGC_DYNAMIC_HEAP_GROWTH
// Override SpiderMonkey default (false).
pref("javascript.options.mem.gc_dynamic_heap_growth", true);

// JSGC_DYNAMIC_MARK_SLICE
// Override SpiderMonkey default (false).
pref("javascript.options.mem.gc_dynamic_mark_slice", true);

// JSGC_ALLOCATION_THRESHOLD
pref("javascript.options.mem.gc_allocation_threshold_mb", 30);

// JSGC_ALLOCATION_THRESHOLD_FACTOR
pref("javascript.options.mem.gc_allocation_threshold_factor", 90);

// JSGC_ALLOCATION_THRESHOLD_FACTOR_AVOID_INTERRUPT
pref("javascript.options.mem.gc_allocation_threshold_factor_avoid_interrupt", 90);

// JSGC_MIN_EMPTY_CHUNK_COUNT
pref("javascript.options.mem.gc_min_empty_chunk_count", 1);

// JSGC_MAX_EMPTY_CHUNK_COUNT
pref("javascript.options.mem.gc_max_empty_chunk_count", 30);

pref("javascript.options.showInConsole", false);

pref("javascript.options.shared_memory", false);

pref("javascript.options.throw_on_debuggee_would_run", false);
pref("javascript.options.dump_stack_on_debuggee_would_run", false);

// Spectre security vulnerability mitigations.
pref("javascript.options.spectre.index_masking", true);
pref("javascript.options.spectre.object_mitigations.barriers", true);
pref("javascript.options.spectre.object_mitigations.misc", true);
pref("javascript.options.spectre.string_mitigations", true);
pref("javascript.options.spectre.value_masking", true);
pref("javascript.options.spectre.jit_to_C++_calls", true);

// Streams API
pref("javascript.options.streams", false);

// advanced prefs
pref("advanced.mailftp",                    false);
pref("image.animation_mode",                "normal");

// Same-origin policy for file URIs, "false" is traditional
pref("security.fileuri.strict_origin_policy", true);

// If this pref is true, prefs in the logging.config branch will be cleared on
// startup. This is done so that setting a log-file and log-modules at runtime
// doesn't persist across restarts leading to huge logfile and low disk space.
pref("logging.config.clear_on_startup", true);

// If there is ever a security firedrill that requires
// us to block certian ports global, this is the pref
// to use.  Is is a comma delimited list of port numbers
// for example:
//   pref("network.security.ports.banned", "1,2,3,4,5");
// prevents necko connecting to ports 1-5 unless the protocol
// overrides.

// Allow necko to do A/B testing. Will generally only happen if
// telemetry is also enabled as otherwise there is no way to report
// the results
pref("network.allow-experiments", true);

// Allow the network changed event to get sent when a network topology or
// setup change is noticed while running.
pref("network.notify.changed", true);

// Allow network detection of IPv6 related changes (bug 1245059)
#if defined(XP_WIN)
pref("network.notify.IPv6", false);
#else
pref("network.notify.IPv6", true);
#endif

// Transmit UDP busy-work to the LAN when anticipating low latency
// network reads and on wifi to mitigate 802.11 Power Save Polling delays
pref("network.tickle-wifi.enabled", false);
pref("network.tickle-wifi.duration", 400);
pref("network.tickle-wifi.delay", 16);

// Turn off interprocess security checks. Needed to run xpcshell tests.
pref("network.disable.ipc.security", false);

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

// Don't allow  external protocol handlers for common typos
pref("network.protocol-handler.external.ttp", false);  // http
pref("network.protocol-handler.external.htp", false);  // http
pref("network.protocol-handler.external.ttps", false); // https
pref("network.protocol-handler.external.tps", false);  // https
pref("network.protocol-handler.external.ps", false);   // https
pref("network.protocol-handler.external.htps", false); // https
pref("network.protocol-handler.external.ile", false);  // file
pref("network.protocol-handler.external.le", false);   // file

// An exposed protocol handler is one that can be used in all contexts.  A
// non-exposed protocol handler is one that can only be used internally by the
// application.  For example, a non-exposed protocol would not be loaded by the
// application in response to a link click or a X-remote openURL command.
// Instead, it would be deferred to the system's external protocol handler.
// Only internal/built-in protocol handlers can be marked as exposed.

// This pref controls the default settings.  Per protocol settings can be used
// to override this value.
pref("network.protocol-handler.expose-all", true);

// Warning for about:networking page
pref("network.warnOnAboutNetworking", true);

// Example: make IMAP an exposed protocol
// pref("network.protocol-handler.expose.imap", true);

// Whether IOService.connectivity and NS_IsOffline depends on connectivity status
pref("network.manage-offline-status", true);
// If set to true, IOService.offline depends on IOService.connectivity
pref("network.offline-mirrors-connectivity", false);

// <http>
pref("network.http.version", "1.1");      // default
// pref("network.http.version", "1.0");   // uncomment this out in case of problems
// pref("network.http.version", "0.9");   // it'll work too if you're crazy
// keep-alive option is effectively obsolete. Nevertheless it'll work with
// some older 1.0 servers:

pref("network.http.proxy.version", "1.1");    // default
// pref("network.http.proxy.version", "1.0"); // uncomment this out in case of problems
                                              // (required if using junkbuster proxy)

// this preference can be set to override the socket type used for normal
// HTTP traffic.  an empty value indicates the normal TCP/IP socket type.
pref("network.http.default-socket-type", "");

// There is a problem with some IIS7 servers that don't close the connection
// properly after it times out (bug #491541). Default timeout on IIS7 is
// 120 seconds. We need to reuse or drop the connection within this time.
// We set the timeout a little shorter to keep a reserve for cases when
// the packet is lost or delayed on the route.
pref("network.http.keep-alive.timeout", 115);

// Timeout connections if an initial response is not received after 5 mins.
pref("network.http.response.timeout", 300);

// Limit the absolute number of http connections.
// Note: the socket transport service will clamp the number below this if the OS
// cannot allocate that many FDs
#ifdef ANDROID
pref("network.http.max-connections", 256);
#else
pref("network.http.max-connections", 900);
#endif

// If NOT connecting via a proxy, then
// a new connection will only be attempted if the number of active persistent
// connections to the server is less then max-persistent-connections-per-server.
pref("network.http.max-persistent-connections-per-server", 6);

// Number of connections that we can open beyond the standard parallelism limit defined
// by max-persistent-connections-per-server/-proxy to handle urgent-start marked requests
pref("network.http.max-urgent-start-excessive-connections-per-host", 3);

// If connecting via a proxy, then a
// new connection will only be attempted if the number of active persistent
// connections to the proxy is less then max-persistent-connections-per-proxy.
pref("network.http.max-persistent-connections-per-proxy", 32);

// amount of time (in seconds) to suspend pending requests, before spawning a
// new connection, once the limit on the number of persistent connections per
// host has been reached.  however, a new connection will not be created if
// max-connections or max-connections-per-server has also been reached.
pref("network.http.request.max-start-delay", 10);

// If a connection is reset, we will retry it max-attempts times.
pref("network.http.request.max-attempts", 10);

// Headers
pref("network.http.accept.default", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");

// Prefs allowing granular control of referers
// 0=don't send any, 1=send only on clicks, 2=send on image requests as well
pref("network.http.sendRefererHeader",      2);
// Set the default Referrer Policy; to be used unless overriden by the site
// 0=no-referrer, 1=same-origin, 2=strict-origin-when-cross-origin,
// 3=no-referrer-when-downgrade
pref("network.http.referer.defaultPolicy", 3);
// Set the Private Browsing Default Referrer Policy;
// to be used unless overriden by the site;
// values are identical to defaultPolicy above
pref("network.http.referer.defaultPolicy.pbmode", 2);
// false=real referer, true=spoof referer (use target URI as referer)
pref("network.http.referer.spoofSource", false);
// false=allow onion referer, true=hide onion referer (use empty referer)
pref("network.http.referer.hideOnionSource", false);
// 0=full URI, 1=scheme+host+port+path, 2=scheme+host+port
pref("network.http.referer.trimmingPolicy", 0);
// 0=full URI, 1=scheme+host+port+path, 2=scheme+host+port
pref("network.http.referer.XOriginTrimmingPolicy", 0);
// 0=always send, 1=send iff base domains match, 2=send iff hosts match
pref("network.http.referer.XOriginPolicy", 0);

// Include an origin header on non-GET and non-HEAD requests regardless of CORS
// 0=never send, 1=send when same-origin only, 2=always send
pref("network.http.sendOriginHeader", 0);

// Maximum number of consecutive redirects before aborting.
pref("network.http.redirection-limit", 20);

// Enable http compression: comment this out in case of problems with 1.1
// NOTE: support for "compress" has been disabled per bug 196406.
// NOTE: separate values with comma+space (", "): see bug 576033
pref("network.http.accept-encoding", "gzip, deflate");
pref("network.http.accept-encoding.secure", "gzip, deflate, br");

// Prompt for redirects resulting in unsafe HTTP requests
pref("network.http.prompt-temp-redirect", false);

// If true generate CORRUPTED_CONTENT errors for entities that
// contain an invalid Assoc-Req response header
pref("network.http.assoc-req.enforce", false);

// On networks deploying QoS, it is recommended that these be lockpref()'d,
// since inappropriate marking can easily overwhelm bandwidth reservations
// for certain services (i.e. EF for VoIP, AF4x for interactive video,
// AF3x for broadcast/streaming video, etc)

// default value for HTTP
// in a DSCP environment this should be 40 (0x28, or AF11), per RFC-4594,
// Section 4.8 "High-Throughput Data Service Class"
pref("network.http.qos", 0);

// The number of milliseconds after sending a SYN for an HTTP connection,
// to wait before trying a different connection. 0 means do not use a second
// connection.
pref("network.http.connection-retry-timeout", 250);

// The number of seconds after sending initial SYN for an HTTP connection
// to give up if the OS does not give up first
pref("network.http.connection-timeout", 90);

// Close a connection if tls handshake does not finish in given number of
// seconds.
pref("network.http.tls-handshake-timeout", 30);

// The number of seconds after which we time out a connection of a retry (fallback)
// socket when a certain IP family is already preferred.  This shorter connection
// timeout allows us to find out more quickly that e.g. an IPv6 host is no longer
// available and let us try an IPv4 address, if provided for the host name.
// Set to '0' to use the default connection timeout.
pref("network.http.fallback-connection-timeout", 5);

// The number of seconds to allow active connections to prove that they have
// traffic before considered stalled, after a network change has been detected
// and signalled.
pref("network.http.network-changed.timeout", 5);

// The maximum number of current global half open sockets allowable
// when starting a new speculative connection.
pref("network.http.speculative-parallel-limit", 6);

// Whether or not to block requests for non head js/css items (e.g. media)
// while those elements load.
pref("network.http.rendering-critical-requests-prioritization", true);

// Disable IPv6 for backup connections to workaround problems about broken
// IPv6 connectivity.
pref("network.http.fast-fallback-to-IPv4", true);

// Try and use SPDY when using SSL
pref("network.http.spdy.enabled", true);
pref("network.http.spdy.enabled.http2", true);
pref("network.http.spdy.enabled.deps", true);
pref("network.http.spdy.enforce-tls-profile", true);
pref("network.http.spdy.chunk-size", 16000);
pref("network.http.spdy.timeout", 170);
pref("network.http.spdy.coalesce-hostnames", true);
pref("network.http.spdy.persistent-settings", false);
pref("network.http.spdy.ping-threshold", 58);
pref("network.http.spdy.ping-timeout", 8);
pref("network.http.spdy.send-buffer-size", 131072);
pref("network.http.spdy.allow-push", true);
pref("network.http.spdy.push-allowance", 131072);   // 128KB
pref("network.http.spdy.pull-allowance", 12582912); // 12MB
pref("network.http.spdy.default-concurrent", 100);
pref("network.http.spdy.default-hpack-buffer", 65536); // 64k

// alt-svc allows separation of transport routing from
// the origin host without using a proxy.
pref("network.http.altsvc.enabled", true);
pref("network.http.altsvc.oe", true);

// Turn on 0RTT data for TLS 1.3
pref("security.tls.enable_0rtt_data", true);

// the origin extension impacts h2 coalescing
pref("network.http.originextension", true);

pref("network.http.diagnostics", false);

pref("network.http.pacing.requests.enabled", true);
pref("network.http.pacing.requests.min-parallelism", 6);
pref("network.http.pacing.requests.hz", 80);
pref("network.http.pacing.requests.burst", 10);

// TCP Keepalive config for HTTP connections.
pref("network.http.tcp_keepalive.short_lived_connections", true);
// Max time from initial request during which conns are considered short-lived.
pref("network.http.tcp_keepalive.short_lived_time", 60);
// Idle time of TCP connection until first keepalive probe sent.
pref("network.http.tcp_keepalive.short_lived_idle_time", 10);

pref("network.http.tcp_keepalive.long_lived_connections", true);
pref("network.http.tcp_keepalive.long_lived_idle_time", 600);

pref("network.http.enforce-framing.http1", false); // should be named "strict"
pref("network.http.enforce-framing.soft", true);

// Max size, in bytes, for received HTTP response header.
pref("network.http.max_response_header_size", 393216);

// If we should attempt to race the cache and network
pref("network.http.rcwn.enabled", true);
pref("network.http.rcwn.cache_queue_normal_threshold", 8);
pref("network.http.rcwn.cache_queue_priority_threshold", 2);
// We might attempt to race the cache with the network only if a resource
// is smaller than this size.
pref("network.http.rcwn.small_resource_size_kb", 256);

pref("network.http.rcwn.min_wait_before_racing_ms", 0);
pref("network.http.rcwn.max_wait_before_racing_ms", 500);

// The ratio of the transaction count for the focused window and the count of
// all available active connections.
pref("network.http.focused_window_transaction_ratio", "0.9");

// Whether or not we give more priority to active tab.
// Note that this requires restart for changes to take effect.
pref("network.http.active_tab_priority", true);
// </http>

// default values for FTP
// in a DSCP environment this should be 40 (0x28, or AF11), per RFC-4594,
// Section 4.8 "High-Throughput Data Service Class", and 80 (0x50, or AF22)
// per Section 4.7 "Low-Latency Data Service Class".
pref("network.ftp.data.qos", 0);
pref("network.ftp.control.qos", 0);
pref("network.ftp.enabled", true);

// The max time to spend on xpcom events between two polls in ms.
pref("network.sts.max_time_for_events_between_two_polls", 100);

// The number of seconds we don't let poll() handing indefinitely after network
// link change has been detected so we can detect breakout of the pollable event.
// Expected in seconds, 0 to disable.
pref("network.sts.poll_busy_wait_period", 50);

// The number of seconds we cap poll() timeout to during the network link change
// detection period.
// Expected in seconds, 0 to disable.
pref("network.sts.poll_busy_wait_period_timeout", 7);

// During shutdown we limit PR_Close calls. If time exceeds this pref (in ms)
// let sockets just leak.
pref("network.sts.max_time_for_pr_close_during_shutdown", 5000);

// When the polling socket pair we use to wake poll() up on demand doesn't
// get signalled (is not readable) within this timeout, we try to repair it.
// This timeout can be disabled by setting this pref to 0.
// The value is expected in seconds.
pref("network.sts.pollable_event_timeout", 6);

// 2147483647 == PR_INT32_MAX == ~2 GB
pref("network.websocket.max-message-size", 2147483647);

// Should we automatically follow http 3xx redirects during handshake
pref("network.websocket.auto-follow-http-redirects", false);

// the number of seconds to wait for websocket connection to be opened
pref("network.websocket.timeout.open", 20);

// the number of seconds to wait for a clean close after sending the client
// close message
pref("network.websocket.timeout.close", 20);

// the number of seconds of idle read activity to sustain before sending a
// ping probe. 0 to disable.
pref("network.websocket.timeout.ping.request", 0);

// the deadline, expressed in seconds, for some read activity to occur after
// generating a ping. If no activity happens then an error and unclean close
// event is sent to the javascript websockets application
pref("network.websocket.timeout.ping.response", 10);

// Defines whether or not to try to negotiate the permessage compression
// extension with the websocket server.
pref("network.websocket.extensions.permessage-deflate", true);

// the maximum number of concurrent websocket sessions. By specification there
// is never more than one handshake oustanding to an individual host at
// one time.
pref("network.websocket.max-connections", 200);

// by default scripts loaded from a https:// origin can only open secure
// (i.e. wss://) websockets.
pref("network.websocket.allowInsecureFromHTTPS", false);

// by default we delay websocket reconnects to same host/port if previous
// connection failed, per RFC 6455 section 7.2.3
pref("network.websocket.delay-failed-reconnects", true);

// </ws>

// Server-Sent Events
// Equal to the DEFAULT_RECONNECTION_TIME_VALUE value in nsEventSource.cpp
pref("dom.server-events.default-reconnection-time", 5000); // in milliseconds

// This preference, if true, causes all UTF-8 domain names to be normalized to
// punycode.  The intention is to allow UTF-8 domain names as input, but never
// generate them from punycode.
pref("network.IDN_show_punycode", false);

// If "network.IDN.use_whitelist" is set to true, TLDs with
// "network.IDN.whitelist.tld" explicitly set to true are treated as
// IDN-safe. Otherwise, they're treated as unsafe and punycode will be used
// for displaying them in the UI (e.g. URL bar), unless they conform to one of
// the profiles specified in
// https://www.unicode.org/reports/tr39/#Restriction_Level_Detection
// If "network.IDN.restriction_profile" is "high", the Highly Restrictive
// profile is used.
// If "network.IDN.restriction_profile" is "moderate", the Moderately
// Restrictive profile is used.
// In all other cases, the ASCII-Only profile is used.
// Note that these preferences are referred to ONLY when
// "network.IDN_show_punycode" is false. In other words, all IDNs will be shown
// in punycode if "network.IDN_show_punycode" is true.
pref("network.IDN.restriction_profile", "high");
pref("network.IDN.use_whitelist", false);

// ccTLDs
pref("network.IDN.whitelist.ac", true);
pref("network.IDN.whitelist.ar", true);
pref("network.IDN.whitelist.at", true);
pref("network.IDN.whitelist.br", true);
pref("network.IDN.whitelist.ca", true);
pref("network.IDN.whitelist.ch", true);
pref("network.IDN.whitelist.cl", true);
pref("network.IDN.whitelist.cn", true);
pref("network.IDN.whitelist.de", true);
pref("network.IDN.whitelist.dk", true);
pref("network.IDN.whitelist.ee", true);
pref("network.IDN.whitelist.es", true);
pref("network.IDN.whitelist.fi", true);
pref("network.IDN.whitelist.fr", true);
pref("network.IDN.whitelist.gr", true);
pref("network.IDN.whitelist.gt", true);
pref("network.IDN.whitelist.hu", true);
pref("network.IDN.whitelist.il", true);
pref("network.IDN.whitelist.io", true);
pref("network.IDN.whitelist.ir", true);
pref("network.IDN.whitelist.is", true);
pref("network.IDN.whitelist.jp", true);
pref("network.IDN.whitelist.kr", true);
pref("network.IDN.whitelist.li", true);
pref("network.IDN.whitelist.lt", true);
pref("network.IDN.whitelist.lu", true);
pref("network.IDN.whitelist.lv", true);
pref("network.IDN.whitelist.no", true);
pref("network.IDN.whitelist.nu", true);
pref("network.IDN.whitelist.nz", true);
pref("network.IDN.whitelist.pl", true);
pref("network.IDN.whitelist.pm", true);
pref("network.IDN.whitelist.pr", true);
pref("network.IDN.whitelist.re", true);
pref("network.IDN.whitelist.se", true);
pref("network.IDN.whitelist.sh", true);
pref("network.IDN.whitelist.si", true);
pref("network.IDN.whitelist.tf", true);
pref("network.IDN.whitelist.th", true);
pref("network.IDN.whitelist.tm", true);
pref("network.IDN.whitelist.tw", true);
pref("network.IDN.whitelist.ua", true);
pref("network.IDN.whitelist.vn", true);
pref("network.IDN.whitelist.wf", true);
pref("network.IDN.whitelist.yt", true);

// IDN ccTLDs
// ae, UAE, .<Emarat>
pref("network.IDN.whitelist.xn--mgbaam7a8h", true);
// cn, China, .<China> with variants
pref("network.IDN.whitelist.xn--fiqz9s", true); // Traditional
pref("network.IDN.whitelist.xn--fiqs8s", true); // Simplified
// eg, Egypt, .<Masr>
pref("network.IDN.whitelist.xn--wgbh1c", true);
// hk, Hong Kong, .<Hong Kong>
pref("network.IDN.whitelist.xn--j6w193g", true);
// ir, Iran, <.Iran> with variants
pref("network.IDN.whitelist.xn--mgba3a4f16a", true);
pref("network.IDN.whitelist.xn--mgba3a4fra", true);
// jo, Jordan, .<Al-Ordon>
pref("network.IDN.whitelist.xn--mgbayh7gpa", true);
// lk, Sri Lanka, .<Lanka> and .<Ilangai>
pref("network.IDN.whitelist.xn--fzc2c9e2c", true);
pref("network.IDN.whitelist.xn--xkc2al3hye2a", true);
// qa, Qatar, .<Qatar>
pref("network.IDN.whitelist.xn--wgbl6a", true);
// rs, Serbia, .<Srb>
pref("network.IDN.whitelist.xn--90a3ac", true);
// ru, Russian Federation, .<RF>
pref("network.IDN.whitelist.xn--p1ai", true);
// sa, Saudi Arabia, .<al-Saudiah> with variants
pref("network.IDN.whitelist.xn--mgberp4a5d4ar", true);
pref("network.IDN.whitelist.xn--mgberp4a5d4a87g", true);
pref("network.IDN.whitelist.xn--mgbqly7c0a67fbc", true);
pref("network.IDN.whitelist.xn--mgbqly7cvafr", true);
// sy, Syria, .<Souria>
pref("network.IDN.whitelist.xn--ogbpf8fl", true);
// th, Thailand, .<Thai>
pref("network.IDN.whitelist.xn--o3cw4h", true);
// tw, Taiwan, <.Taiwan> with variants
pref("network.IDN.whitelist.xn--kpry57d", true);  // Traditional
pref("network.IDN.whitelist.xn--kprw13d", true);  // Simplified

// gTLDs
pref("network.IDN.whitelist.asia", true);
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
// "network.IDN.whitelist.*".  (please keep this value in sync with the
// built-in fallback in intl/uconv/nsTextToSubURI.cpp)
pref("network.IDN.blacklist_chars", "\u0020\u00A0\u00BC\u00BD\u00BE\u01C3\u02D0\u0337\u0338\u0589\u058A\u05C3\u05F4\u0609\u060A\u066A\u06D4\u0701\u0702\u0703\u0704\u115F\u1160\u1735\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A\u200B\u200E\u200F\u2010\u2019\u2024\u2027\u2028\u2029\u202A\u202B\u202C\u202D\u202E\u202F\u2039\u203A\u2041\u2044\u2052\u205F\u2153\u2154\u2155\u2156\u2157\u2158\u2159\u215A\u215B\u215C\u215D\u215E\u215F\u2215\u2236\u23AE\u2571\u29F6\u29F8\u2AFB\u2AFD\u2FF0\u2FF1\u2FF2\u2FF3\u2FF4\u2FF5\u2FF6\u2FF7\u2FF8\u2FF9\u2FFA\u2FFB\u3000\u3002\u3014\u3015\u3033\u30A0\u3164\u321D\u321E\u33AE\u33AF\u33C6\u33DF\uA789\uFE14\uFE15\uFE3F\uFE5D\uFE5E\uFEFF\uFF0E\uFF0F\uFF61\uFFA0\uFFF9\uFFFA\uFFFB\uFFFC\uFFFD");

// This preference specifies a list of domains for which DNS lookups will be
// IPv4 only. Works around broken DNS servers which can't handle IPv6 lookups
// and/or allows the user to disable IPv6 on a per-domain basis. See bug 68796.
pref("network.dns.ipv4OnlyDomains", "");

// This preference can be used to turn off IPv6 name lookups. See bug 68796.
pref("network.dns.disableIPv6", false);

// This is the number of dns cache entries allowed
pref("network.dnsCacheEntries", 400);

// In the absence of OS TTLs, the DNS cache TTL value
pref("network.dnsCacheExpiration", 60);

// Get TTL; not supported on all platforms; nop on the unsupported ones.
pref("network.dns.get-ttl", true);

// For testing purposes! Makes the native resolver resolve IPv4 "localhost"
// instead of the actual given name.
pref("network.dns.native-is-localhost", false);

// The grace period allows the DNS cache to use expired entries, while kicking off
// a revalidation in the background.
pref("network.dnsCacheExpirationGracePeriod", 60);

// This preference can be used to turn off DNS prefetch.
pref("network.dns.disablePrefetch", false);

// This preference controls whether .onion hostnames are
// rejected before being given to DNS. RFC 7686
pref("network.dns.blockDotOnion", true);

// These domains are treated as localhost equivalent
pref("network.dns.localDomains", "");

// When non empty all non-localhost DNS queries (including IP addresses)
// resolve to this value. The value can be a name or an IP address.
// domains mapped to localhost with localDomains stay localhost.
pref("network.dns.forceResolve", "");

// Contols whether or not "localhost" should resolve when offline
pref("network.dns.offline-localhost", true);

// The maximum allowed length for a URL - 1MB default
pref("network.standard-url.max-length", 1048576);

// Whether nsIURI.host/.hostname/.spec should return a punycode string
// If set to false we will revert to previous behaviour and return a unicode string.
pref("network.standard-url.punycode-host", true);

// Idle timeout for ftp control connections - 5 minute default
pref("network.ftp.idleConnectionTimeout", 300);

// enables the prefetch service (i.e., prefetching of <link rel="next"> and
// <link rel="prefetch"> URLs).
pref("network.prefetch-next", true);
// enables the preloading (i.e., preloading of <link rel="preload"> URLs).
pref("network.preload", false);

// The following prefs pertain to the negotiate-auth extension (see bug 17578),
// which provides transparent Kerberos or NTLM authentication using the SPNEGO
// protocol.  Each pref is a comma-separated list of keys, where each key has
// the format:
//   [scheme "://"] [host [":" port]]
// For example, "foo.com" would match "http://www.foo.com/bar", etc.

// Force less-secure NTLMv1 when needed (NTLMv2 is the default).
pref("network.auth.force-generic-ntlm-v1", false);

// This list controls which URIs can use the negotiate-auth protocol.  This
// list should be limited to the servers you know you'll need to login to.
pref("network.negotiate-auth.trusted-uris", "");
// This list controls which URIs can support delegation.
pref("network.negotiate-auth.delegation-uris", "");

// Do not allow SPNEGO by default when challenged by a local server.
pref("network.negotiate-auth.allow-non-fqdn", false);

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
pref("network.automatic-ntlm-auth.allow-non-fqdn", false);
pref("network.automatic-ntlm-auth.trusted-uris", "");

// The string to return to the server as the 'workstation' that the
// user is using.  Bug 1046421 notes that the previous default, of the
// system hostname, could be used for user fingerprinting.
//
// However, in some network environments where allowedWorkstations is in use
// to provide a level of host-based access control, it must be set to a string
// that is listed in allowedWorkstations for the user's account in their
// AD Domain.
pref("network.generic-ntlm-auth.workstation", "WORKSTATION");

// This preference controls whether to allow sending default credentials (SSO) to
// NTLM/Negotiate servers allowed in the "trusted uri" list when navigating them
// in a Private Browsing window.
// If set to false, Private Browsing windows will not use default credentials and ask
// for credentials from the user explicitly.
// If set to true, and a server URL conforms other conditions for sending default
// credentials, those will be sent automatically in Private Browsing windows.
//
// This preference has no effect when the browser is set to "Never Remember History",
// in that case default credentials will always be used.
pref("network.auth.private-browsing-sso", false);

// Control how throttling of http responses works - number of ms that each
// suspend and resume period lasts (prefs named appropriately)
pref("network.http.throttle.enable", true);
pref("network.http.throttle.version", 2);

// V1 prefs
pref("network.http.throttle.suspend-for", 900);
pref("network.http.throttle.resume-for", 100);

// V2 prefs
pref("network.http.throttle.read-limit-bytes", 8000);
pref("network.http.throttle.read-interval-ms", 500);

// Common prefs
// Delay we resume throttled background responses after the last unthrottled
// response has finished.  Prevents resuming too soon during an active page load
// at which sub-resource reqeusts quickly come and go.
pref("network.http.throttle.hold-time-ms", 800);
// After the last transaction activation or last data chunk response we only
// throttle for this period of time.  This prevents comet and unresponsive
// http requests to engage long-standing throttling.
pref("network.http.throttle.max-time-ms", 500);

// Give higher priority to requests resulting from a user interaction event
// like click-to-play, image fancy-box zoom, navigation.
pref("network.http.on_click_priority", true);

// Some requests during a page load are marked as "tail", mainly trackers, but not only.
// This pref controls whether such requests are put to the tail, behind other requests
// emerging during page loading process.
pref("network.http.tailing.enabled", true);
// When the page load has not yet reached DOMContentLoaded point, tail requestes are delayed
// by (non-tailed requests count + 1) * delay-quantum milliseconds.
pref("network.http.tailing.delay-quantum", 600);
// The same as above, but applied after the document load reached DOMContentLoaded event.
pref("network.http.tailing.delay-quantum-after-domcontentloaded", 100);
// Upper limit for the calculated delay, prevents long standing and comet-like requests
// tail forever.  This is in milliseconds as well.
pref("network.http.tailing.delay-max", 6000);
// Total limit we delay tailed requests since a page load beginning.
pref("network.http.tailing.total-max", 45000);

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
pref("network.proxy.proxy_over_tls",        true);
pref("network.proxy.no_proxies_on",         "localhost, 127.0.0.1");
pref("network.proxy.failover_timeout",      1800); // 30 minutes
pref("network.online",                      true); //online/offline
pref("network.cookie.cookieBehavior",       0); // 0-Accept, 1-dontAcceptForeign, 2-dontAcceptAny, 3-limitForeign
#ifdef ANDROID
pref("network.cookie.cookieBehavior",       0); // Keep the old default of accepting all cookies
#endif
pref("network.cookie.thirdparty.sessionOnly", false);
pref("network.cookie.thirdparty.nonsecureSessionOnly", false);
pref("network.cookie.leave-secure-alone",   true);
pref("network.cookie.same-site.enabled",    true); // Honor the SameSite cookie attribute
pref("network.cookie.ipc.sync",             false);
pref("network.cookie.lifetimePolicy",       0); // 0-accept, 1-dontUse 2-acceptForSession, 3-acceptForNDays
pref("network.cookie.prefsMigrated",        false);
pref("network.cookie.lifetime.days",        90); // Ignored unless network.cookie.lifetimePolicy is 3.
// The interval in seconds to move the cookies in the child process.
// Set to 0 to disable moving the cookies.
pref("network.cookie.move.interval_sec",    10);

pref("network.cookie.maxNumber", 3000);
pref("network.cookie.maxPerHost", 180);

// The PAC file to load.  Ignored unless network.proxy.type is 2.
pref("network.proxy.autoconfig_url", "");
// Strip off paths when sending URLs to PAC scripts
pref("network.proxy.autoconfig_url.include_path", false);

// If we cannot load the PAC file, then try again (doubling from interval_min
// until we reach interval_max or the PAC file is successfully loaded).
pref("network.proxy.autoconfig_retry_interval_min", 5);    // 5 seconds
pref("network.proxy.autoconfig_retry_interval_max", 300);  // 5 minutes

// Use the HSTS preload list by default
pref("network.stricttransportsecurity.preloadlist", true);

// Use JS mDNS as a fallback
pref("network.mdns.use_js_fallback", false);

pref("converter.html2txt.structs",          true); // Output structured phrases (strong, em, code, sub, sup, b, i, u)
pref("converter.html2txt.header_strategy",  1); // 0 = no indention; 1 = indention, increased with header level; 2 = numbering and slight indention
// Whether we include ruby annotation in the text despite whether it
// is requested. This was true because we didn't explicitly strip out
// annotations. Set false by default to provide a better behavior, but
// we want to be able to pref-off it if user doesn't like it.
pref("converter.html2txt.always_include_ruby", false);

pref("intl.accept_languages",               "chrome://global/locale/intl.properties");
pref("intl.menuitems.alwaysappendaccesskeys","chrome://global/locale/intl.properties");
pref("intl.menuitems.insertseparatorbeforeaccesskeys","chrome://global/locale/intl.properties");
pref("intl.charset.detector",               "chrome://global/locale/intl.properties");
pref("intl.charset.fallback.override",      "");
pref("intl.charset.fallback.tld",           true);
pref("intl.charset.fallback.utf8_for_file", false);
pref("intl.ellipsis",                       "chrome://global-platform/locale/intl.properties");
// this pref allows user to request that all internationalization formatters
// like date/time formatting, unit formatting, calendars etc. should use
// OS locale set instead of the app locale set.
pref("intl.regional_prefs.use_os_locales",  false);
// fallback charset list for Unicode conversion (converting from Unicode)
// currently used for mail send only to handle symbol characters (e.g Euro, trademark, smartquotes)
// for ISO-8859-1
pref("intl.fallbackCharsetList.ISO-8859-1", "windows-1252");
pref("font.language.group",                 "chrome://global/locale/intl.properties");

// Android-specific pref to control if keydown and keyup events are fired even
// in during composition.  Note that those prefs are ignored if
// "dom.keyboardevent.dispatch_during_composition" is false.
#ifdef MOZ_WIDGET_ANDROID
// If true, dispatch the keydown and keyup events on any web apps even during
// composition.
#ifdef EARLY_BETA_OR_EARLIER
pref("intl.ime.hack.on_any_apps.fire_key_events_for_composition", true);
#else // #ifdef EARLY_BETA_OR_EARLIER
pref("intl.ime.hack.on_any_apps.fire_key_events_for_composition", false);
#endif // #ifdef EARLY_BETA_OR_EARLIER #else
// If true and the above pref is false, dispatch the keydown and keyup events
// only on IME-unaware web apps.  So, this supports web apps which listen to
// only keydown or keyup event to get a change to do something at every text
// input.
pref("intl.ime.hack.on_ime_unaware_apps.fire_key_events_for_composition", true);
#else // #ifdef MOZ_WIDGET_ANDROID
pref("intl.ime.hack.on_any_apps.fire_key_events_for_composition", false);
pref("intl.ime.hack.on_ime_unaware_apps.fire_key_events_for_composition", false);
#endif // #ifdef MOZ_WIDGET_ANDROID #else

// If you use legacy Chinese IME which puts an ideographic space to composition
// string as placeholder, this pref might be useful.  If this is true and when
// web contents forcibly commits composition (e.g., moving focus), the
// ideographic space will be ignored (i.e., commits with empty string).
pref("intl.ime.remove_placeholder_character_at_commit", false);

pref("intl.uidirection", -1); // -1 to set from locale; 0 for LTR; 1 for RTL

// use en-US hyphenation by default for content tagged with plain lang="en"
pref("intl.hyphenation-alias.en", "en-us");
// and for other subtags of en-*, if no specific patterns are available
pref("intl.hyphenation-alias.en-*", "en-us");

pref("intl.hyphenation-alias.af-*", "af");
pref("intl.hyphenation-alias.bg-*", "bg");
pref("intl.hyphenation-alias.ca-*", "ca");
pref("intl.hyphenation-alias.cy-*", "cy");
pref("intl.hyphenation-alias.da-*", "da");
pref("intl.hyphenation-alias.eo-*", "eo");
pref("intl.hyphenation-alias.es-*", "es");
pref("intl.hyphenation-alias.et-*", "et");
pref("intl.hyphenation-alias.fi-*", "fi");
pref("intl.hyphenation-alias.fr-*", "fr");
pref("intl.hyphenation-alias.gl-*", "gl");
pref("intl.hyphenation-alias.hr-*", "hr");
pref("intl.hyphenation-alias.hsb-*", "hsb");
pref("intl.hyphenation-alias.hu-*", "hu");
pref("intl.hyphenation-alias.ia-*", "ia");
pref("intl.hyphenation-alias.is-*", "is");
pref("intl.hyphenation-alias.it-*", "it");
pref("intl.hyphenation-alias.kmr-*", "kmr");
pref("intl.hyphenation-alias.la-*", "la");
pref("intl.hyphenation-alias.lt-*", "lt");
pref("intl.hyphenation-alias.mn-*", "mn");
pref("intl.hyphenation-alias.nl-*", "nl");
pref("intl.hyphenation-alias.pl-*", "pl");
pref("intl.hyphenation-alias.pt-*", "pt");
pref("intl.hyphenation-alias.ru-*", "ru");
pref("intl.hyphenation-alias.sl-*", "sl");
pref("intl.hyphenation-alias.sv-*", "sv");
pref("intl.hyphenation-alias.tr-*", "tr");
pref("intl.hyphenation-alias.uk-*", "uk");

// use reformed (1996) German patterns by default unless specifically tagged as de-1901
// (these prefs may soon be obsoleted by better BCP47-based tag matching, but for now...)
pref("intl.hyphenation-alias.de", "de-1996");
pref("intl.hyphenation-alias.de-*", "de-1996");
pref("intl.hyphenation-alias.de-AT-1901", "de-1901");
pref("intl.hyphenation-alias.de-DE-1901", "de-1901");
pref("intl.hyphenation-alias.de-CH-*", "de-CH");

// patterns from TeX are tagged as "sh" (Serbo-Croatian) macrolanguage;
// alias "sr" (Serbian) and "bs" (Bosnian) to those patterns
// (Croatian has its own separate patterns).
pref("intl.hyphenation-alias.sr", "sh");
pref("intl.hyphenation-alias.bs", "sh");
pref("intl.hyphenation-alias.sh-*", "sh");
pref("intl.hyphenation-alias.sr-*", "sh");
pref("intl.hyphenation-alias.bs-*", "sh");

// Norwegian has two forms, Bokmål and Nynorsk, with "no" as a macrolanguage encompassing both.
// For "no", we'll alias to "nb" (Bokmål) as that is the more widely used written form.
pref("intl.hyphenation-alias.no", "nb");
pref("intl.hyphenation-alias.no-*", "nb");
pref("intl.hyphenation-alias.nb-*", "nb");
pref("intl.hyphenation-alias.nn-*", "nn");

// All prefs of default font should be "auto".
pref("font.name.serif.ar", "");
pref("font.name.sans-serif.ar", "");
pref("font.name.monospace.ar", "");
pref("font.name.cursive.ar", "");

pref("font.name.serif.el", "");
pref("font.name.sans-serif.el", "");
pref("font.name.monospace.el", "");
pref("font.name.cursive.el", "");

pref("font.name.serif.he", "");
pref("font.name.sans-serif.he", "");
pref("font.name.monospace.he", "");
pref("font.name.cursive.he", "");

pref("font.name.serif.ja", "");
pref("font.name.sans-serif.ja", "");
pref("font.name.monospace.ja", "");
pref("font.name.cursive.ja", "");

pref("font.name.serif.ko", "");
pref("font.name.sans-serif.ko", "");
pref("font.name.monospace.ko", "");
pref("font.name.cursive.ko", "");

pref("font.name.serif.th", "");
pref("font.name.sans-serif.th", "");
pref("font.name.monospace.th", "");
pref("font.name.cursive.th", "");

pref("font.name.serif.x-cyrillic", "");
pref("font.name.sans-serif.x-cyrillic", "");
pref("font.name.monospace.x-cyrillic", "");
pref("font.name.cursive.x-cyrillic", "");

pref("font.name.serif.x-unicode", "");
pref("font.name.sans-serif.x-unicode", "");
pref("font.name.monospace.x-unicode", "");
pref("font.name.cursive.x-unicode", "");

pref("font.name.serif.x-western", "");
pref("font.name.sans-serif.x-western", "");
pref("font.name.monospace.x-western", "");
pref("font.name.cursive.x-western", "");

pref("font.name.serif.zh-CN", "");
pref("font.name.sans-serif.zh-CN", "");
pref("font.name.monospace.zh-CN", "");
pref("font.name.cursive.zh-CN", "");

pref("font.name.serif.zh-TW", "");
pref("font.name.sans-serif.zh-TW", "");
pref("font.name.monospace.zh-TW", "");
pref("font.name.cursive.zh-TW", "");

pref("font.name.serif.zh-HK", "");
pref("font.name.sans-serif.zh-HK", "");
pref("font.name.monospace.zh-HK", "");
pref("font.name.cursive.zh-HK", "");

pref("font.name.serif.x-devanagari", "");
pref("font.name.sans-serif.x-devanagari", "");
pref("font.name.monospace.x-devanagari", "");
pref("font.name.cursive.x-devanagari", "");

pref("font.name.serif.x-tamil", "");
pref("font.name.sans-serif.x-tamil", "");
pref("font.name.monospace.x-tamil", "");
pref("font.name.cursive.x-tamil", "");

pref("font.name.serif.x-armn", "");
pref("font.name.sans-serif.x-armn", "");
pref("font.name.monospace.x-armn", "");
pref("font.name.cursive.x-armn", "");

pref("font.name.serif.x-beng", "");
pref("font.name.sans-serif.x-beng", "");
pref("font.name.monospace.x-beng", "");
pref("font.name.cursive.x-beng", "");

pref("font.name.serif.x-cans", "");
pref("font.name.sans-serif.x-cans", "");
pref("font.name.monospace.x-cans", "");
pref("font.name.cursive.x-cans", "");

pref("font.name.serif.x-ethi", "");
pref("font.name.sans-serif.x-ethi", "");
pref("font.name.monospace.x-ethi", "");
pref("font.name.cursive.x-ethi", "");

pref("font.name.serif.x-geor", "");
pref("font.name.sans-serif.x-geor", "");
pref("font.name.monospace.x-geor", "");
pref("font.name.cursive.x-geor", "");

pref("font.name.serif.x-gujr", "");
pref("font.name.sans-serif.x-gujr", "");
pref("font.name.monospace.x-gujr", "");
pref("font.name.cursive.x-gujr", "");

pref("font.name.serif.x-guru", "");
pref("font.name.sans-serif.x-guru", "");
pref("font.name.monospace.x-guru", "");
pref("font.name.cursive.x-guru", "");

pref("font.name.serif.x-khmr", "");
pref("font.name.sans-serif.x-khmr", "");
pref("font.name.monospace.x-khmr", "");
pref("font.name.cursive.x-khmr", "");

pref("font.name.serif.x-mlym", "");
pref("font.name.sans-serif.x-mlym", "");
pref("font.name.monospace.x-mlym", "");
pref("font.name.cursive.x-mlym", "");

pref("font.name.serif.x-orya", "");
pref("font.name.sans-serif.x-orya", "");
pref("font.name.monospace.x-orya", "");
pref("font.name.cursive.x-orya", "");

pref("font.name.serif.x-telu", "");
pref("font.name.sans-serif.x-telu", "");
pref("font.name.monospace.x-telu", "");
pref("font.name.cursive.x-telu", "");

pref("font.name.serif.x-knda", "");
pref("font.name.sans-serif.x-knda", "");
pref("font.name.monospace.x-knda", "");
pref("font.name.cursive.x-knda", "");

pref("font.name.serif.x-sinh", "");
pref("font.name.sans-serif.x-sinh", "");
pref("font.name.monospace.x-sinh", "");
pref("font.name.cursive.x-sinh", "");

pref("font.name.serif.x-tibt", "");
pref("font.name.sans-serif.x-tibt", "");
pref("font.name.monospace.x-tibt", "");
pref("font.name.cursive.x-tibt", "");

pref("font.name.serif.x-math", "");
pref("font.name.sans-serif.x-math", "");
pref("font.name.monospace.x-math", "");
pref("font.name.cursive.x-math", "");

pref("font.name-list.serif.x-math", "Latin Modern Math, STIX Two Math, XITS Math, Cambria Math, Libertinus Math, DejaVu Math TeX Gyre, TeX Gyre Bonum Math, TeX Gyre Pagella Math, TeX Gyre Schola, TeX Gyre Termes Math, STIX Math, Asana Math, STIXGeneral, DejaVu Serif, DejaVu Sans, serif");
pref("font.name-list.sans-serif.x-math", "sans-serif");
pref("font.name-list.monospace.x-math", "monospace");

// Some CJK fonts have bad underline offset, their CJK character glyphs are overlapped (or adjoined)  to its underline.
// These fonts are ignored the underline offset, instead of it, the underline is lowered to bottom of its em descent.
pref("font.blacklist.underline_offset", "FangSong,Gulim,GulimChe,MingLiU,MingLiU-ExtB,MingLiU_HKSCS,MingLiU-HKSCS-ExtB,MS Gothic,MS Mincho,MS PGothic,MS PMincho,MS UI Gothic,PMingLiU,PMingLiU-ExtB,SimHei,SimSun,SimSun-ExtB,Hei,Kai,Apple LiGothic,Apple LiSung,Osaka");

pref("security.directory",              "");

// security-sensitive dialogs should delay button enabling. In milliseconds.
pref("security.dialog_enable_delay", 1000);
pref("security.notification_enable_delay", 500);

pref("security.csp.enable", true);
pref("security.csp.experimentalEnabled", false);
pref("security.csp.enableStrictDynamic", true);

#if defined(DEBUG) && !defined(ANDROID)
// about:welcome has been added until Bug 1448359 is fixed at which time home, newtab, and welcome will all be removed.
pref("csp.content_privileged_about_uris_without_csp", "blank,blocked,home,newtab,printpreview,srcdoc,welcome");
#endif

#ifdef NIGHTLY_BUILD
pref("security.csp.enable_violation_events", true);
#else
pref("security.csp.enable_violation_events", false);
#endif

// Default Content Security Policy to apply to signed contents.
pref("security.signed_content.CSP.default", "script-src 'self'; style-src 'self'");

// Mixed content blocking
pref("security.mixed_content.block_active_content", false);
pref("security.mixed_content.block_display_content", false);

// Upgrade mixed display content before it's blocked
pref("security.mixed_content.upgrade_display_content", false);

// Block sub requests that happen within an object
pref("security.mixed_content.block_object_subrequest", false);

// Sub-resource integrity
pref("security.sri.enable", true);

// Block scripts with wrong MIME type such as image/ or video/.
pref("security.block_script_with_wrong_mime", true);

// OCSP must-staple
pref("security.ssl.enable_ocsp_must_staple", true);

// Insecure Form Field Warning
pref("security.insecure_field_warning.contextual.enabled", false);
pref("security.insecure_field_warning.ignore_local_ip_address", true);

// Disable pinning checks by default.
pref("security.cert_pinning.enforcement_level", 0);
// Do not process hpkp headers rooted by not built in roots by default.
// This is to prevent accidental pinning from MITM devices and is used
// for tests.
pref("security.cert_pinning.process_headers_from_non_builtin_roots", false);

// If set to true, allow view-source URIs to be opened from URIs that share
// their protocol with the inner URI of the view-source URI
pref("security.view-source.reachable-from-inner-protocol", false);

// Services security settings
pref("services.settings.server", "https://firefox.settings.services.mozilla.com/v1");
pref("services.settings.changes.path", "/buckets/monitor/collections/changes/records");
pref("services.settings.default_bucket", "main");
pref("services.settings.default_signer", "remote-settings.content-signature.mozilla.org");

// Blocklist preferences
pref("extensions.blocklist.enabled", true);
// OneCRL freshness checking depends on this value, so if you change it,
// please also update security.onecrl.maximum_staleness_in_seconds.
pref("extensions.blocklist.interval", 86400);
// Required blocklist freshness for OneCRL OCSP bypass
// (default is 1.25x extensions.blocklist.interval, or 30 hours)
pref("security.onecrl.maximum_staleness_in_seconds", 108000);
pref("extensions.blocklist.url", "https://blocklists.settings.services.mozilla.com/v1/blocklist/3/%APP_ID%/%APP_VERSION%/%PRODUCT%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/%PING_COUNT%/%TOTAL_PING_COUNT%/%DAYS_SINCE_LAST_PING%/");
pref("extensions.blocklist.detailsURL", "https://blocked.cdn.mozilla.net/");
pref("extensions.blocklist.itemURL", "https://blocked.cdn.mozilla.net/%blockID%.html");
// Controls what level the blocklist switches from warning about items to forcibly
// blocking them.
pref("extensions.blocklist.level", 2);
// Blocklist via settings server (Kinto)
pref("services.blocklist.bucket", "blocklists");
pref("services.blocklist.onecrl.collection", "certificates");
pref("services.blocklist.onecrl.checked", 0);
pref("services.blocklist.onecrl.signer", "onecrl.content-signature.mozilla.org");
pref("services.blocklist.addons.collection", "addons");
pref("services.blocklist.addons.checked", 0);
pref("services.blocklist.addons.signer", "remote-settings.content-signature.mozilla.org");
pref("services.blocklist.plugins.collection", "plugins");
pref("services.blocklist.plugins.checked", 0);
pref("services.blocklist.plugins.signer", "remote-settings.content-signature.mozilla.org");
pref("services.blocklist.pinning.enabled", true);
pref("services.blocklist.pinning.bucket", "pinning");
pref("services.blocklist.pinning.collection", "pins");
pref("services.blocklist.pinning.checked", 0);
pref("services.blocklist.pinning.signer", "pinning-preload.content-signature.mozilla.org");
pref("services.blocklist.gfx.collection", "gfx");
pref("services.blocklist.gfx.checked", 0);
pref("services.blocklist.gfx.signer", "remote-settings.content-signature.mozilla.org");
// Enable blocklists via the services settings mechanism
pref("services.blocklist.update_enabled", true);

// Modifier key prefs: default to Windows settings,
// menu access key = alt, accelerator key = control.
// Use 17 for Ctrl, 18 for Alt, 224 for Meta, 91 for Win, 0 for none. Mac settings in macprefs.js
pref("ui.key.accelKey", 17);
pref("ui.key.menuAccessKey", 18);
pref("ui.key.generalAccessKey", -1);

// If generalAccessKey is -1, use the following two prefs instead.
// Use 0 for disabled, 1 for Shift, 2 for Ctrl, 4 for Alt, 8 for Meta, 16 for Win
// (values can be combined, e.g. 5 for Alt+Shift)
pref("ui.key.chromeAccess", 4);
pref("ui.key.contentAccess", 5);

pref("ui.key.menuAccessKeyFocuses", false); // overridden below

// Disable page loading activity cursor by default.
pref("ui.use_activity_cursor", false);

// Middle-mouse handling
pref("middlemouse.paste", false);
pref("middlemouse.contentLoadURL", false);
pref("middlemouse.scrollbarPosition", false);

// Clipboard behavior
pref("clipboard.autocopy", false);

// Clipboard only supports text/plain
pref("clipboard.plainTextOnly", false);

#ifdef XP_WIN
// Setting false you can disable 4th button and/or 5th button of your mouse.
// 4th button is typically mapped to "Back" and 5th button is typically mapped
// to "Forward" button.
pref("mousebutton.4th.enabled", true);
pref("mousebutton.5th.enabled", true);
#endif

// mouse wheel scroll transaction period of time (in milliseconds)
pref("mousewheel.transaction.timeout", 1500);
// mouse wheel scroll transaction is held even if the mouse cursor is moved.
pref("mousewheel.transaction.ignoremovedelay", 100);

// prefs for app level mouse wheel scrolling acceleration.
// number of mousewheel clicks when acceleration starts
// acceleration can be turned off if pref is set to -1
pref("mousewheel.acceleration.start", -1);
// factor to be multiplied for constant acceleration
pref("mousewheel.acceleration.factor", 10);

// Prefs for override the system mouse wheel scrolling speed on
// content of the web pages.  When
// "mousewheel.system_scroll_override_on_root_content.enabled" is true and the system
// scrolling speed isn't customized by the user, the content scrolling
// speed is multiplied by the following factors.  The value will be used as
// 1/100.  E.g., 200 means 2.00.
// NOTE: Even if "mousewheel.system_scroll_override_on_root_content.enabled" is
// true, when Gecko detects the user customized the system scrolling speed
// settings, the override isn't executed.
pref("mousewheel.system_scroll_override_on_root_content.vertical.factor", 200);
pref("mousewheel.system_scroll_override_on_root_content.horizontal.factor", 200);

// mousewheel.*.action can specify the action when you use mosue wheel.
// When no modifier keys are pressed or two or more modifires are pressed,
// .default is used.
// 0: Nothing happens
// 1: Scrolling contents
// 2: Go back or go forward, in your history
// 3: Zoom in or out (reflowing zoom).
// 4: Treat vertical wheel as horizontal scroll
//      This treats vertical wheel operation (i.e., deltaY) as horizontal
//      scroll.  deltaX and deltaZ are always ignored.  So, only
//      "delta_multiplier_y" pref affects the scroll speed.
// 5: Zoom in or out (pinch zoom).
pref("mousewheel.default.action", 1);
pref("mousewheel.with_alt.action", 2);
pref("mousewheel.with_control.action", 3);
pref("mousewheel.with_meta.action", 1);  // command key on Mac
pref("mousewheel.with_shift.action", 4);
pref("mousewheel.with_win.action", 1);

// mousewheel.*.action.override_x will override the action
// when the mouse wheel is rotated along the x direction.
// -1: Don't override the action.
// 0 to 3: Override the action with the specified value.
// Note that 4 isn't available because it doesn't make sense to apply the
// default action only for y direction to this pref.
pref("mousewheel.default.action.override_x", -1);
pref("mousewheel.with_alt.action.override_x", -1);
pref("mousewheel.with_control.action.override_x", -1);
pref("mousewheel.with_meta.action.override_x", -1);  // command key on Mac
pref("mousewheel.with_shift.action.override_x", -1);
pref("mousewheel.with_win.action.override_x", -1);

// mousewheel.*.delta_multiplier_* can specify the value muliplied by the delta
// value.  The values will be used after divided by 100.  I.e., 100 means 1.0,
// -100 means -1.0.  If the values were negative, the direction would be
// reverted.  The absolue value must be 100 or larger.
pref("mousewheel.default.delta_multiplier_x", 100);
pref("mousewheel.default.delta_multiplier_y", 100);
pref("mousewheel.default.delta_multiplier_z", 100);
pref("mousewheel.with_alt.delta_multiplier_x", 100);
pref("mousewheel.with_alt.delta_multiplier_y", 100);
pref("mousewheel.with_alt.delta_multiplier_z", 100);
pref("mousewheel.with_control.delta_multiplier_x", 100);
pref("mousewheel.with_control.delta_multiplier_y", 100);
pref("mousewheel.with_control.delta_multiplier_z", 100);
pref("mousewheel.with_meta.delta_multiplier_x", 100);  // command key on Mac
pref("mousewheel.with_meta.delta_multiplier_y", 100);  // command key on Mac
pref("mousewheel.with_meta.delta_multiplier_z", 100);  // command key on Mac
pref("mousewheel.with_shift.delta_multiplier_x", 100);
pref("mousewheel.with_shift.delta_multiplier_y", 100);
pref("mousewheel.with_shift.delta_multiplier_z", 100);
pref("mousewheel.with_win.delta_multiplier_x", 100);
pref("mousewheel.with_win.delta_multiplier_y", 100);
pref("mousewheel.with_win.delta_multiplier_z", 100);

// If line-height is lower than this value (in device pixels), 1 line scroll
// scrolls this height.
pref("mousewheel.min_line_scroll_amount", 5);

// Auto-dir is a feature which treats any single-wheel scroll as a scroll in the
// only one scrollable direction if the target has only one scrollable
// direction. For example, if the user scrolls a vertical wheel inside a target
// which is horizontally scrollable but vertical unscrollable, then the vertical
// scroll is converted to a horizontal scroll for that target.
// Note that auto-dir only takes effect for |mousewheel.*.action|s and
// |mousewheel.*.action.override_x|s whose values are 1.
pref("mousewheel.autodir.enabled", false);
// When a wheel scroll is converted due to auto-dir, which side the converted
// scroll goes towards is decided by one thing called "honoured target". If the
// content of the honoured target horizontally starts from right to left, then
// an upward scroll maps to a rightward scroll and a downward scroll maps to a
// leftward scroll; otherwise, an upward scroll maps to a leftward scroll and a
// downward scroll maps to a rightward scroll.
// If this pref is set to false, then consider the scrolling target as the
// honoured target.
// If set to true, then consider the root element in the document where the
// scrolling target is as the honoured target. But note that there's one
// exception: for targets in an HTML document, the real root element(I.e. the
// <html> element) is typically not considered as a root element, but the <body>
// element is typically considered as a root element. If there is no <body>
// element, then consider the <html> element instead.
pref("mousewheel.autodir.honourroot", false);

// These define the smooth scroll behavior (min ms, max ms) for different triggers
// Some triggers:
// mouseWheel: Discrete mouse wheel events, Synaptics touchpads on windows (generate wheel events)
// Lines:  Up/Down/Left/Right KB arrows
// Pages:  Page up/down, Space
// Scrollbars: Clicking scrollbars arrows, clicking scrollbars tracks
// Note: Currently OS X trackpad and magic mouse don't use our smooth scrolling
// Note: These are relevant only when "general.smoothScroll" is enabled
pref("general.smoothScroll.mouseWheel.durationMinMS", 200);
pref("general.smoothScroll.mouseWheel.durationMaxMS", 400);
pref("general.smoothScroll.pixels.durationMinMS", 150);
pref("general.smoothScroll.pixels.durationMaxMS", 150);
pref("general.smoothScroll.lines.durationMinMS", 150);
pref("general.smoothScroll.lines.durationMaxMS", 150);
pref("general.smoothScroll.pages.durationMinMS", 150);
pref("general.smoothScroll.pages.durationMaxMS", 150);
pref("general.smoothScroll.scrollbars.durationMinMS", 150);
pref("general.smoothScroll.scrollbars.durationMaxMS", 150);
pref("general.smoothScroll.other.durationMinMS", 150);
pref("general.smoothScroll.other.durationMaxMS", 150);
// Enable disable smooth scrolling for different triggers (when "general.smoothScroll" is enabled)
pref("general.smoothScroll.mouseWheel", true);
pref("general.smoothScroll.pixels", true);
pref("general.smoothScroll.lines", true);
pref("general.smoothScroll.pages", true);
pref("general.smoothScroll.scrollbars", true);
pref("general.smoothScroll.other", true);
// To connect consecutive scroll events into a continuous flow, the animation's duration
// should be longer than scroll events intervals (or else the scroll will stop
// before the next event arrives - we're guessing next interval by averaging recent
// intervals).
// This defines how longer is the duration compared to events interval (percentage)
pref("general.smoothScroll.durationToIntervalRatio", 200);
// These two prefs determine the timing function.
pref("general.smoothScroll.currentVelocityWeighting", "0.25");
pref("general.smoothScroll.stopDecelerationWeighting", "0.4");
// Alternative smooth scroll physics ("MSD" = Mass-Spring-Damper)
pref("general.smoothScroll.msdPhysics.enabled", false);
pref("general.smoothScroll.msdPhysics.continuousMotionMaxDeltaMS", 120);
pref("general.smoothScroll.msdPhysics.motionBeginSpringConstant", 1250);
pref("general.smoothScroll.msdPhysics.slowdownMinDeltaMS", 12);
pref("general.smoothScroll.msdPhysics.slowdownMinDeltaRatio", "1.3");
pref("general.smoothScroll.msdPhysics.slowdownSpringConstant", 2000);
pref("general.smoothScroll.msdPhysics.regularSpringConstant", 1000);

// We can show it anytime from menus
pref("profile.manage_only_at_launch", false);

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
// Whether delete and backspace should immediately delete characters not
// visually adjacent to the caret, or adjust the visual position of the caret
// on the first keypress and delete the character on a second keypress
pref("bidi.edit.delete_immediately", true);

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

// Whether underscore should be treated as a word-breaking character for
// word selection/arrow-key movement purposes.
pref("layout.word_select.stop_at_underscore", false);

// controls caret style and word-delete during text selection
// 0 = use platform default
// 1 = caret moves and blinks as when there is no selection; word
//     delete deselects the selection and then deletes word
// 2 = caret moves to selection edge and is not visible during selection;
//     word delete deletes the selection (Mac and Linux default)
// 3 = caret moves and blinks as when there is no selection; word delete
//     deletes the selection
// Windows default is 1 for word delete behavior, the rest as for 2.
pref("layout.selection.caret_style", 0);

// pref to report CSS errors to the error console
pref("layout.css.report_errors", true);

// Override DPI. A value of -1 means use the maximum of 96 and the system DPI.
// A value of 0 means use the system DPI. A positive value is used as the DPI.
// This sets the physical size of a device pixel and thus controls the
// interpretation of physical units such as "pt".
pref("layout.css.dpi", -1);

// Set the number of device pixels per CSS pixel. A value <= 0 means choose
// automatically based on user settings for the platform (e.g., "UI scale factor"
// on Mac). A positive value is used as-is. This effectively controls the size
// of a CSS "px". This is only used for windows on the screen, not for printing.
pref("layout.css.devPixelsPerPx", "-1.0");

// Is support for CSS individual transform enabled?
pref("layout.css.individual-transform.enabled", false);

// Is support for CSS initial-letter property enabled?
pref("layout.css.initial-letter.enabled", false);

// Is support for mix-blend-mode enabled?
pref("layout.css.mix-blend-mode.enabled", true);

// Is support for isolation enabled?
pref("layout.css.isolation.enabled", true);

// Is support for CSS Filters enabled?
pref("layout.css.filters.enabled", true);

// Is support for CSS Scrollbar color properties enabled?
pref("layout.css.scrollbar-colors.enabled", false);

// Set the threshold distance in CSS pixels below which scrolling will snap to
// an edge, when scroll snapping is set to "proximity".
pref("layout.css.scroll-snap.proximity-threshold", 200);

// When selecting the snap point for CSS scroll snapping, the velocity of the
// scroll frame is clamped to this speed, in CSS pixels / s.
pref("layout.css.scroll-snap.prediction-max-velocity", 2000);

// When selecting the snap point for CSS scroll snapping, the velocity of the
// scroll frame is integrated over this duration, in seconds.  The snap point
// best suited for this position is selected, enabling the user to perform fling
// gestures.
pref("layout.css.scroll-snap.prediction-sensitivity", "0.750");

// Is support for DOMPoint enabled?
pref("layout.css.DOMPoint.enabled", true);

// Is support for DOMQuad enabled?
pref("layout.css.DOMQuad.enabled", true);

// Is support for DOMMatrix enabled?
pref("layout.css.DOMMatrix.enabled", true);

// Is support for GeometryUtils.getBoxQuads enabled?
#ifdef RELEASE_OR_BETA
pref("layout.css.getBoxQuads.enabled", false);
#else
pref("layout.css.getBoxQuads.enabled", true);
#endif

// Is support for GeometryUtils.convert*FromNode enabled?
#ifdef RELEASE_OR_BETA
pref("layout.css.convertFromNode.enabled", false);
#else
pref("layout.css.convertFromNode.enabled", true);
#endif

// Is support for CSS "text-align: unsafe X" enabled?
pref("layout.css.text-align-unsafe-value.enabled", false);

// Is support for CSS text-justify property enabled?
pref("layout.css.text-justify.enabled", true);

// Is support for the CSS4 image-orientation property enabled?
pref("layout.css.image-orientation.enabled", true);

// Is the paint-order property supported for HTML text?
// (It is always supported for SVG.)
pref("layout.css.paint-order.enabled", true);

// Are sets of prefixed properties supported?
pref("layout.css.prefixes.border-image", true);
pref("layout.css.prefixes.transforms", true);
pref("layout.css.prefixes.transitions", true);
pref("layout.css.prefixes.animations", true);
pref("layout.css.prefixes.box-sizing", true);
pref("layout.css.prefixes.font-features", true);

// Is support for background-blend-mode enabled?
pref("layout.css.background-blend-mode.enabled", true);

// Is -moz-osx-font-smoothing enabled?
// Only supported in OSX builds
#ifdef XP_MACOSX
pref("layout.css.osx-font-smoothing.enabled", true);
#else
pref("layout.css.osx-font-smoothing.enabled", false);
#endif

// Is support for the CSS-wide "unset" value enabled?
pref("layout.css.unset-value.enabled", true);

// Is support for the "all" shorthand enabled?
pref("layout.css.all-shorthand.enabled", true);

// Is support for CSS overflow-clip-box enabled for non-UA sheets?
pref("layout.css.overflow-clip-box.enabled", false);

// Is support for CSS contain enabled?
pref("layout.css.contain.enabled", false);

// Is support for CSS box-decoration-break enabled?
pref("layout.css.box-decoration-break.enabled", true);

// Is layout of CSS outline-style:auto enabled?
pref("layout.css.outline-style-auto.enabled", false);

// Is CSSOM-View scroll-behavior and its MSD smooth scrolling enabled?
pref("layout.css.scroll-behavior.enabled", true);

// Is the CSSOM-View scroll-behavior CSS property enabled?
pref("layout.css.scroll-behavior.property-enabled", true);

// Tuning of the smooth scroll motion used by CSSOM-View scroll-behavior.
// Spring-constant controls the strength of the simulated MSD
// (Mass-Spring-Damper)
pref("layout.css.scroll-behavior.spring-constant", "250.0");

// Tuning of the smooth scroll motion used by CSSOM-View scroll-behavior.
// Damping-ratio controls the dampening force of the simulated MSD
// (Mass-Spring-Damper).
// When below 1.0, the system is under-damped; it may overshoot the target and
// oscillate.
// When greater than 1.0, the system is over-damped; it will reach the target at
// reduced speed without overshooting.
// When equal to 1.0, the system is critically-damped; it will reach the target
// at the greatest speed without overshooting.
pref("layout.css.scroll-behavior.damping-ratio", "1.0");

// Is support for scroll-snap enabled?
pref("layout.css.scroll-snap.enabled", true);

// Is support for CSS shape-outside enabled?
#ifdef NIGHTLY_BUILD
pref("layout.css.shape-outside.enabled", true);
#else
pref("layout.css.shape-outside.enabled", false);
#endif

// Is support for document.fonts enabled?
pref("layout.css.font-loading-api.enabled", true);

// Is support for column-span enabled?
pref("layout.css.column-span.enabled", false);

// Are inter-character ruby annotations enabled?
pref("layout.css.ruby.intercharacter.enabled", false);

// Is support for overscroll-behavior enabled?
pref("layout.css.overscroll-behavior.enabled", true);

// pref for which side vertical scrollbars should be on
// 0 = end-side in UI direction
// 1 = end-side in document/content direction
// 2 = right
// 3 = left
pref("layout.scrollbar.side", 0);

// pref to stop overlay scrollbars from fading out, for testing purposes
pref("layout.testing.overlay-scrollbars.always-visible", false);

// Enable/disable interruptible reflow, which allows reflows to stop
// before completion (and display the partial results) when user events
// are pending.
pref("layout.interruptible-reflow.enabled", true);

// pref to control browser frame rate, in Hz. A value <= 0 means choose
// automatically based on knowledge of the platform (or 60Hz if no platform-
// specific information is available).
pref("layout.frame_rate", -1);

// pref to dump the display list to the log. Useful for debugging drawing.
pref("layout.display-list.dump", false);
pref("layout.display-list.dump-content", false);
pref("layout.display-list.dump-parent", false);

// Toggle retaining display lists between paints
#if !defined(ANDROID)
pref("layout.display-list.retain", true);
#else
pref("layout.display-list.retain", false);
#endif
pref("layout.display-list.retain.chrome", false);

// Set the maximum amount of modified frames allowed before doing a full
// display list rebuild.
pref("layout.display-list.rebuild-frame-limit", 500);

// pref to control whether layout warnings that are hit quite often are enabled
pref("layout.spammy_warnings.enabled", false);

// Should we fragment floats inside CSS column layout?
pref("layout.float-fragments-inside-column.enabled", true);

// The number of frames times the frame rate is the time required to
// pass without painting used to guess that we'll not paint again soon
pref("layout.idle_period.required_quiescent_frames", 2);

// The amount of time (milliseconds) needed between an idle period's
// end and the start of the next tick to avoid jank.
pref("layout.idle_period.time_limit", 1);

// Is support for the Web Animations API enabled?
// Before enabling this by default, make sure also CSSPseudoElement interface
// has been spec'ed properly, or we should add a separate pref for
// CSSPseudoElement interface. See Bug 1174575 for further details.
#ifdef RELEASE_OR_BETA
pref("dom.animations-api.core.enabled", false);
#else
pref("dom.animations-api.core.enabled", true);
#endif

// Is support for the Element.animate() function (a subset of the Web Animations
// API) enabled?
// Note that if dom.animations-api.core.enabled is true, this preference is
// ignored.
pref("dom.animations-api.element-animate.enabled", true);

// Pref to throttle offsreen animations
pref("dom.animations.offscreen-throttling", true);

// Prefs to control the maximum area to pre-render when animating a large
// element on the compositor.
pref("layout.animation.prerender.partial", false);
pref("layout.animation.prerender.viewport-ratio-limit-x", "1.125");
pref("layout.animation.prerender.viewport-ratio-limit-y", "1.125");
pref("layout.animation.prerender.absolute-limit-x", 4096);
pref("layout.animation.prerender.absolute-limit-y", 4096);

// if true, allow plug-ins to override internal imglib decoder mime types in full-page mode
pref("plugin.override_internal_types", false);

// enable single finger gesture input (win7+ tablets)
pref("gestures.enable_single_finger_input", true);

pref("editor.resizing.preserve_ratio",       true);
pref("editor.positioning.offset",            0);

pref("dom.use_watchdog", true);
pref("dom.max_chrome_script_run_time", 20);
pref("dom.max_script_run_time", 10);
pref("dom.max_ext_content_script_run_time", 5);

// Stop all scripts in a compartment when the "stop script" dialog is used.
pref("dom.global_stop_script", true);

// Support the input event queue on the main thread of content process
pref("input_event_queue.supported", true);

// The maximum and minimum time (milliseconds) we reserve for handling input
// events in each frame.
pref("input_event_queue.duration.max", 8);
pref("input_event_queue.duration.min", 1);

// The default amount of time (milliseconds) required for handling a input
// event.
pref("input_event_queue.default_duration_per_event", 1);

// The number of processed input events we use to predict the amount of time
// required to process the following input events.
pref("input_event_queue.count_for_prediction", 9);

// Hang monitor timeout after which we kill the browser, in seconds
// (0 is disabled)
// Disabled on all platforms per bug 705748 until the found issues are
// resolved.
pref("hangmonitor.timeout", 0);

pref("plugins.load_appdir_plugins", false);
// If true, plugins will be click to play
pref("plugins.click_to_play", false);

// This only supports one hidden ctp plugin, edit nsPluginArray.cpp if adding a second
pref("plugins.navigator.hidden_ctp_plugin", "");

// The default value for nsIPluginTag.enabledState (STATE_ENABLED = 2)
pref("plugin.default.state", 2);

// How long in minutes we will allow a plugin to work after the user has chosen
// to allow it "now"
pref("plugin.sessionPermissionNow.intervalInMinutes", 60);
// How long in days we will allow a plugin to work after the user has chosen
// to allow it persistently.
pref("plugin.persistentPermissionAlways.intervalInDays", 90);

// This pref can take 3 possible string values:
// "always"     - always use favor fallback mode
// "follow-ctp" - activate if ctp is active for the given
//                plugin object (could be due to a plugin-wide
//                setting or a site-specific setting)
// "never"      - never use favor fallback mode
pref("plugins.favorfallback.mode", "never");

// A comma-separated list of rules to follow when deciding
// whether an object has been provided with good fallback content.
// The valid values can be found at nsObjectLoadingContent::HasGoodFallback.
pref("plugins.favorfallback.rules", "");


// Set IPC timeouts for plugins and tabs, except in leak-checking and
// dynamic analysis builds.  (NS_FREE_PERMANENT_DATA is C++ only, so
// approximate its definition here.)
#if !defined(DEBUG) && !defined(MOZ_ASAN) && !defined(MOZ_VALGRIND) && !defined(MOZ_TSAN)
// How long a plugin is allowed to process a synchronous IPC message
// before we consider it "hung".
pref("dom.ipc.plugins.timeoutSecs", 45);
// How long a plugin process will wait for a response from the parent
// to a synchronous request before terminating itself. After this
// point the child assumes the parent is hung. Currently disabled.
pref("dom.ipc.plugins.parentTimeoutSecs", 0);
// How long a plugin in e10s is allowed to process a synchronous IPC
// message before we notify the chrome process of a hang.
pref("dom.ipc.plugins.contentTimeoutSecs", 10);
// How long a plugin launch is allowed to take before
// we consider it failed.
pref("dom.ipc.plugins.processLaunchTimeoutSecs", 45);
#ifdef XP_WIN
// How long a plugin is allowed to process a synchronous IPC message
// before we display the plugin hang UI
pref("dom.ipc.plugins.hangUITimeoutSecs", 11);
// Minimum time that the plugin hang UI will be displayed
pref("dom.ipc.plugins.hangUIMinDisplaySecs", 10);
#endif
// How long a content process can take before closing its IPC channel
// after shutdown is initiated.  If the process exceeds the timeout,
// we fear the worst and kill it.
pref("dom.ipc.tabs.shutdownTimeoutSecs", 5);
#else
// No timeout in leak-checking builds
pref("dom.ipc.plugins.timeoutSecs", 0);
pref("dom.ipc.plugins.contentTimeoutSecs", 0);
pref("dom.ipc.plugins.processLaunchTimeoutSecs", 0);
pref("dom.ipc.plugins.parentTimeoutSecs", 0);
#ifdef XP_WIN
pref("dom.ipc.plugins.hangUITimeoutSecs", 0);
pref("dom.ipc.plugins.hangUIMinDisplaySecs", 0);
#endif
pref("dom.ipc.tabs.shutdownTimeoutSecs", 0);
#endif

pref("dom.ipc.plugins.flash.disable-protected-mode", false);

pref("dom.ipc.plugins.flash.subprocess.crashreporter.enabled", true);
pref("dom.ipc.plugins.reportCrashURL", true);

// How long we wait before unloading an idle plugin process.
// Defaults to 30 seconds.
pref("dom.ipc.plugins.unloadTimeoutSecs", 30);

// Allow Flash async drawing mode in 64-bit release builds
pref("dom.ipc.plugins.asyncdrawing.enabled", true);
// Force the accelerated direct path for a subset of Flash wmode values
pref("dom.ipc.plugins.forcedirect.enabled", true);

// Enable multi by default.
pref("dom.ipc.processCount", 4);

// Default to allow only one file:// URL content process.
pref("dom.ipc.processCount.file", 1);

// WebExtensions only support a single extension process.
pref("dom.ipc.processCount.extension", 1);

// Whether a native event loop should be used in the content process.
#if defined(XP_WIN)
pref("dom.ipc.useNativeEventProcessing.content", false);
#else
pref("dom.ipc.useNativeEventProcessing.content", true);
#endif

// Quantum DOM scheduling:
pref("dom.ipc.scheduler.useMultipleQueues", true);
pref("dom.ipc.scheduler.preemption", false);
pref("dom.ipc.scheduler.threadCount", 2);
pref("dom.ipc.scheduler.chaoticScheduling", false);

// Disable support for SVG
pref("svg.disabled", false);

// Override default dom.ipc.processCount for some remote content process types.
pref("dom.ipc.processCount.webLargeAllocation", 10);

// Enable the Large-Allocation header
pref("dom.largeAllocationHeader.enabled", true);

// Disable e10s for Gecko by default. This is overridden in firefox.js.
pref("browser.tabs.remote.autostart", false);

// Pref to control whether we use separate content processes for top-level load
// of file:// URIs.
pref("browser.tabs.remote.separateFileUriProcess", true);

// Pref that enables top level web content pages that are opened from file://
// URI pages to run in the file content process.
// This has been added in case breaking any window references between these
// sorts of pages, which we have to do when we run them in the normal web
// content process, causes compatibility issues.
pref("browser.tabs.remote.allowLinkedWebInFileUriProcess", true);

// Enable the use of display-lists for SVG hit-testing and painting.
pref("svg.display-lists.hit-testing.enabled", true);
pref("svg.display-lists.painting.enabled", true);

// Is support for the <marker orient="auto-start-reverse"> feature enabled?
pref("svg.marker-improvements.enabled", true);

// Is support for the new getBBox method from SVG 2 enabled?
// See https://svgwg.org/svg2-draft/single-page.html#types-SVGBoundingBoxOptions
pref("svg.new-getBBox.enabled", false);

pref("svg.transform-box.enabled", true);

# This pref controls whether the 'context-fill' and 'context-stroke' keywords
# can be used in SVG-as-an-image in the content processes to use the fill/
# stroke specified on the element that embeds the image.  (These keywords are
# always enabled in the chrome process, regardless of this pref.)
# Also, these keywords are currently not part of any spec, which is partly why
# we disable them for web content.
pref("svg.context-properties.content.enabled", false);

// Default font types and sizes by locale
pref("font.default.ar", "sans-serif");
pref("font.minimum-size.ar", 0);
pref("font.size.variable.ar", 16);
pref("font.size.fixed.ar", 13);

pref("font.default.el", "serif");
pref("font.minimum-size.el", 0);
pref("font.size.variable.el", 16);
pref("font.size.fixed.el", 13);

pref("font.default.he", "sans-serif");
pref("font.minimum-size.he", 0);
pref("font.size.variable.he", 16);
pref("font.size.fixed.he", 13);

pref("font.default.ja", "sans-serif");
pref("font.minimum-size.ja", 0);
pref("font.size.variable.ja", 16);
pref("font.size.fixed.ja", 16);

pref("font.default.ko", "sans-serif");
pref("font.minimum-size.ko", 0);
pref("font.size.variable.ko", 16);
pref("font.size.fixed.ko", 16);

pref("font.default.th", "sans-serif");
pref("font.minimum-size.th", 0);
pref("font.size.variable.th", 16);
pref("font.size.fixed.th", 13);

pref("font.default.x-cyrillic", "serif");
pref("font.minimum-size.x-cyrillic", 0);
pref("font.size.variable.x-cyrillic", 16);
pref("font.size.fixed.x-cyrillic", 13);

pref("font.default.x-devanagari", "serif");
pref("font.minimum-size.x-devanagari", 0);
pref("font.size.variable.x-devanagari", 16);
pref("font.size.fixed.x-devanagari", 13);

pref("font.default.x-tamil", "serif");
pref("font.minimum-size.x-tamil", 0);
pref("font.size.variable.x-tamil", 16);
pref("font.size.fixed.x-tamil", 13);

pref("font.default.x-armn", "serif");
pref("font.minimum-size.x-armn", 0);
pref("font.size.variable.x-armn", 16);
pref("font.size.fixed.x-armn", 13);

pref("font.default.x-beng", "serif");
pref("font.minimum-size.x-beng", 0);
pref("font.size.variable.x-beng", 16);
pref("font.size.fixed.x-beng", 13);

pref("font.default.x-cans", "serif");
pref("font.minimum-size.x-cans", 0);
pref("font.size.variable.x-cans", 16);
pref("font.size.fixed.x-cans", 13);

pref("font.default.x-ethi", "serif");
pref("font.minimum-size.x-ethi", 0);
pref("font.size.variable.x-ethi", 16);
pref("font.size.fixed.x-ethi", 13);

pref("font.default.x-geor", "serif");
pref("font.minimum-size.x-geor", 0);
pref("font.size.variable.x-geor", 16);
pref("font.size.fixed.x-geor", 13);

pref("font.default.x-gujr", "serif");
pref("font.minimum-size.x-gujr", 0);
pref("font.size.variable.x-gujr", 16);
pref("font.size.fixed.x-gujr", 13);

pref("font.default.x-guru", "serif");
pref("font.minimum-size.x-guru", 0);
pref("font.size.variable.x-guru", 16);
pref("font.size.fixed.x-guru", 13);

pref("font.default.x-khmr", "serif");
pref("font.minimum-size.x-khmr", 0);
pref("font.size.variable.x-khmr", 16);
pref("font.size.fixed.x-khmr", 13);

pref("font.default.x-mlym", "serif");
pref("font.minimum-size.x-mlym", 0);
pref("font.size.variable.x-mlym", 16);
pref("font.size.fixed.x-mlym", 13);

pref("font.default.x-orya", "serif");
pref("font.minimum-size.x-orya", 0);
pref("font.size.variable.x-orya", 16);
pref("font.size.fixed.x-orya", 13);

pref("font.default.x-telu", "serif");
pref("font.minimum-size.x-telu", 0);
pref("font.size.variable.x-telu", 16);
pref("font.size.fixed.x-telu", 13);

pref("font.default.x-knda", "serif");
pref("font.minimum-size.x-knda", 0);
pref("font.size.variable.x-knda", 16);
pref("font.size.fixed.x-knda", 13);

pref("font.default.x-sinh", "serif");
pref("font.minimum-size.x-sinh", 0);
pref("font.size.variable.x-sinh", 16);
pref("font.size.fixed.x-sinh", 13);

pref("font.default.x-tibt", "serif");
pref("font.minimum-size.x-tibt", 0);
pref("font.size.variable.x-tibt", 16);
pref("font.size.fixed.x-tibt", 13);

pref("font.default.x-unicode", "serif");
pref("font.minimum-size.x-unicode", 0);
pref("font.size.variable.x-unicode", 16);
pref("font.size.fixed.x-unicode", 13);

pref("font.default.x-western", "serif");
pref("font.minimum-size.x-western", 0);
pref("font.size.variable.x-western", 16);
pref("font.size.fixed.x-western", 13);

pref("font.default.zh-CN", "sans-serif");
pref("font.minimum-size.zh-CN", 0);
pref("font.size.variable.zh-CN", 16);
pref("font.size.fixed.zh-CN", 16);

pref("font.default.zh-HK", "sans-serif");
pref("font.minimum-size.zh-HK", 0);
pref("font.size.variable.zh-HK", 16);
pref("font.size.fixed.zh-HK", 16);

pref("font.default.zh-TW", "sans-serif");
pref("font.minimum-size.zh-TW", 0);
pref("font.size.variable.zh-TW", 16);
pref("font.size.fixed.zh-TW", 16);

// mathml.css sets font-size to "inherit" and font-family to "serif" so only
// font.name.*.x-math and font.minimum-size.x-math are really relevant.
pref("font.default.x-math", "serif");
pref("font.minimum-size.x-math", 0);
pref("font.size.variable.x-math", 16);
pref("font.size.fixed.x-math", 13);

/*
 * A value greater than zero enables font size inflation for
 * pan-and-zoom UIs, so that the fonts in a block are at least the size
 * that, if a block's width is scaled to match the device's width, the
 * fonts in the block are big enough that at most the pref value ems of
 * text fit in *the width of the device*.
 *
 * When both this pref and the next are set, the larger inflation is
 * used.
 */
pref("font.size.inflation.emPerLine", 0);
/*
 * A value greater than zero enables font size inflation for
 * pan-and-zoom UIs, so that if a block's width is scaled to match the
 * device's width, the fonts in a block are at least the font size
 * given.  The value given is in twips, i.e., 1/20 of a point, or 1/1440
 * of an inch.
 *
 * When both this pref and the previous are set, the larger inflation is
 * used.
 */
pref("font.size.inflation.minTwips", 0);
/*
 * In products with multi-mode pan-and-zoom and non-pan-and-zoom UIs,
 * this pref forces font inflation to always be enabled in all modes.
 * That is, any heuristics used to detect pan-and-zoom
 * vs. non-pan-and-zoom modes are disabled and all content is treated
 * as pan-and-zoom mode wrt font inflation.
 *
 * This pref has no effect if font inflation is not enabled through
 * either of the prefs above.  It has no meaning in single-mode UIs.
 */
pref("font.size.inflation.forceEnabled", false);
/*
 * In products with multi-mode pan-and-zoom and non-pan-and-zoom UIs,
 * this pref disables font inflation in master-process contexts where
 * existing heuristics can't be used determine enabled-ness.
 *
 * This pref has no effect if font inflation is not enabled through
 * either of the prefs above.  The "forceEnabled" pref above overrides
 * this pref.
 */
pref("font.size.inflation.disabledInMasterProcess", false);
/*
 * Since the goal of font size inflation is to avoid having to
 * repeatedly scroll side to side to read a block of text, and there are
 * a number of page layouts where a relatively small chunk of text is
 * better of not being inflated according to the same algorithm we use
 * for larger chunks of text, we want a threshold for an amount of text
 * that triggers font size inflation.  This preference controls that
 * threshold.
 *
 * It controls the threshold used within an *approximation* of the
 * number of lines of text we use.  In particular, if we assume that
 * each character (collapsing collapsible whitespace) has a width the
 * same as the em-size of the font (when, normally, it's actually quite
 * a bit smaller on average), this preference gives the percentage of a
 * number of lines of text we'd need to trigger inflation.  This means
 * that a percentage of 100 means that we'd need a number of characters
 * (we know the font size and the width) equivalent to one line of
 * square text (which is actually a lot less than a real line of text).
 *
 * A value of 0 means there's no character length threshold.
 */
pref("font.size.inflation.lineThreshold", 400);

/*
 * Defines the font size inflation mapping intercept parameter.
 *
 * Font size inflation computes a minimum font size, m, based on
 * other preferences (see font.size.inflation.minTwips and
 * font.size.inflation.emPerLine, above) and the width of the
 * frame in which the text resides. Using this minimum, a specified
 * font size, s, is mapped to an inflated font size, i, using an
 * equation that varies depending on the value of the font size
 * inflation mapping intercept parameter, P:
 *
 * If the intercept parameter is negative, then the following mapping
 * function is used:
 *
 * i = m + s
 *
 * If the intercept parameter is non-negative, then the mapping function
 * is a function such that its graph meets the graph of i = s at the
 * point where both i and s are (1 + P/2) * m for values of s that are
 * large enough. This means that when s=0, i is always equal to m.
 */
pref("font.size.inflation.mappingIntercept", 1);

/*
 * This controls the percentage that fonts will be inflated, if font
 * size inflation is enabled. Essentially, if we have a specified font
 * size, s, and an inflated font size, i, this specifies that the ratio
 * i/s * 100 should never exceed the value of this preference.
 *
 * In order for this preference to have any effect, its value must be
 * greater than 100, since font inflation can never decrease the ratio
 * i/s.
 */
pref("font.size.inflation.maxRatio", 0);

/**
 * This setting corresponds to a global text zoom setting affecting
 * all content that is not already subject to font size inflation.
 * It is interpreted as a percentage value that is applied on top
 * of the document's current text zoom setting.
 *
 * The resulting total zoom factor (text zoom * system font scale)
 * will be limited by zoom.minPercent and maxPercent.
 */
pref("font.size.systemFontScale", 100);

/*
 * When enabled, the touch.radius and mouse.radius prefs allow events to be dispatched
 * to nearby elements that are sensitive to the event. See PositionedEventTargeting.cpp.
 * The 'mm' prefs define a rectangle around the nominal event target point within which
 * we will search for suitable elements. 'visitedWeight' is a percentage weight;
 * a value > 100 makes a visited link be treated as further away from the event target
 * than it really is, while a value < 100 makes a visited link be treated as closer
 * to the event target than it really is.
 */
pref("ui.touch.radius.enabled", false);
pref("ui.touch.radius.leftmm", 8);
pref("ui.touch.radius.topmm", 12);
pref("ui.touch.radius.rightmm", 8);
pref("ui.touch.radius.bottommm", 4);
pref("ui.touch.radius.visitedWeight", 120);

pref("ui.mouse.radius.enabled", false);
pref("ui.mouse.radius.leftmm", 8);
pref("ui.mouse.radius.topmm", 12);
pref("ui.mouse.radius.rightmm", 8);
pref("ui.mouse.radius.bottommm", 4);
pref("ui.mouse.radius.visitedWeight", 120);

// When true, the ui.mouse.radius.* prefs will only affect simulated mouse events generated by touch input.
// When false, the prefs will be used for all mouse events.
pref("ui.mouse.radius.inputSource.touchOnly", true);

#ifdef XP_WIN

pref("font.name-list.emoji", "Segoe UI Emoji, Twemoji Mozilla");

pref("font.name-list.serif.ar", "Times New Roman");
pref("font.name-list.sans-serif.ar", "Segoe UI, Tahoma, Arial");
pref("font.name-list.monospace.ar", "Courier New");
pref("font.name-list.cursive.ar", "Comic Sans MS");

pref("font.name-list.serif.el", "Times New Roman");
pref("font.name-list.sans-serif.el", "Arial");
pref("font.name-list.monospace.el", "Courier New");
pref("font.name-list.cursive.el", "Comic Sans MS");

pref("font.name-list.serif.he", "Narkisim, David");
pref("font.name-list.sans-serif.he", "Arial");
pref("font.name-list.monospace.he", "Fixed Miriam Transparent, Miriam Fixed, Rod, Courier New");
pref("font.name-list.cursive.he", "Guttman Yad, Ktav, Arial");

pref("font.name-list.serif.ja", "Yu Mincho, MS PMincho, MS Mincho, Meiryo, Yu Gothic, MS PGothic, MS Gothic");
pref("font.name-list.sans-serif.ja", "Meiryo, Yu Gothic, MS PGothic, MS Gothic, Yu Mincho, MS PMincho, MS Mincho");
pref("font.name-list.monospace.ja", "MS Gothic, MS Mincho, Meiryo, Yu Gothic, Yu Mincho, MS PGothic, MS PMincho");

pref("font.name-list.serif.ko", "Batang, Gulim");
pref("font.name-list.sans-serif.ko", "Malgun Gothic, Gulim");
pref("font.name-list.monospace.ko", "GulimChe");
pref("font.name-list.cursive.ko", "Gungsuh");

pref("font.name-list.serif.th", "Tahoma");
pref("font.name-list.sans-serif.th", "Tahoma");
pref("font.name-list.monospace.th", "Tahoma");
pref("font.name-list.cursive.th", "Tahoma");

pref("font.name-list.serif.x-cyrillic", "Times New Roman");
pref("font.name-list.sans-serif.x-cyrillic", "Arial");
pref("font.name-list.monospace.x-cyrillic", "Courier New");
pref("font.name-list.cursive.x-cyrillic", "Comic Sans MS");

pref("font.name-list.serif.x-unicode", "Times New Roman");
pref("font.name-list.sans-serif.x-unicode", "Arial");
pref("font.name-list.monospace.x-unicode", "Courier New");
pref("font.name-list.cursive.x-unicode", "Comic Sans MS");

pref("font.name-list.serif.x-western", "Times New Roman");
pref("font.name-list.sans-serif.x-western", "Arial");
pref("font.name-list.monospace.x-western", "Courier New");
pref("font.name-list.cursive.x-western", "Comic Sans MS");

pref("font.name-list.serif.zh-CN", "SimSun, MS Song, SimSun-ExtB");
pref("font.name-list.sans-serif.zh-CN", "Microsoft YaHei, SimHei");
pref("font.name-list.monospace.zh-CN", "SimSun, MS Song, SimSun-ExtB");
pref("font.name-list.cursive.zh-CN", "KaiTi, KaiTi_GB2312");

// Per Taiwanese users' demand. They don't want to use TC fonts for
// rendering Latin letters. (bug 88579)
pref("font.name-list.serif.zh-TW", "Times New Roman, PMingLiu, MingLiU, MingLiU-ExtB");
pref("font.name-list.sans-serif.zh-TW", "Arial, PMingLiU, MingLiU, MingLiU-ExtB");
pref("font.name-list.monospace.zh-TW", "MingLiU, MingLiU-ExtB");
pref("font.name-list.cursive.zh-TW", "DFKai-SB");

// hkscsm3u.ttf (HKSCS-2001) :  http://www.microsoft.com/hk/hkscs
// Hong Kong users have the same demand about glyphs for Latin letters (bug 88579)
pref("font.name-list.serif.zh-HK", "Times New Roman, MingLiu_HKSCS, Ming(for ISO10646), MingLiU, MingLiU_HKSCS-ExtB");
pref("font.name-list.sans-serif.zh-HK", "Arial, MingLiU_HKSCS, Ming(for ISO10646), MingLiU, MingLiU_HKSCS-ExtB");
pref("font.name-list.monospace.zh-HK", "MingLiU_HKSCS, Ming(for ISO10646), MingLiU, MingLiU_HKSCS-ExtB");
pref("font.name-list.cursive.zh-HK", "DFKai-SB");

pref("font.name-list.serif.x-devanagari", "Kokila, Raghindi");
pref("font.name-list.sans-serif.x-devanagari", "Nirmala UI, Mangal");
pref("font.name-list.monospace.x-devanagari", "Mangal, Nirmala UI");

pref("font.name-list.serif.x-tamil", "Latha");
pref("font.name-list.monospace.x-tamil", "Latha");

# http://www.alanwood.net/unicode/fonts.html

pref("font.name-list.serif.x-armn", "Sylfaen");
pref("font.name-list.sans-serif.x-armn", "Arial AMU");
pref("font.name-list.monospace.x-armn", "Arial AMU");

pref("font.name-list.serif.x-beng", "Vrinda, Akaash, Likhan, Ekushey Punarbhaba");
pref("font.name-list.sans-serif.x-beng", "Vrinda, Akaash, Likhan, Ekushey Punarbhaba");
pref("font.name-list.monospace.x-beng", "Mitra Mono, Likhan, Mukti Narrow");

pref("font.name-list.serif.x-cans", "Aboriginal Serif, BJCree Uni");
pref("font.name-list.sans-serif.x-cans", "Aboriginal Sans");
pref("font.name-list.monospace.x-cans", "Aboriginal Sans, OskiDakelh, Pigiarniq, Uqammaq");

pref("font.name-list.serif.x-ethi", "Visual Geez Unicode, Visual Geez Unicode Agazian");
pref("font.name-list.sans-serif.x-ethi", "GF Zemen Unicode");
pref("font.name-list.monospace.x-ethi", "Ethiopia Jiret");
pref("font.name-list.cursive.x-ethi", "Visual Geez Unicode Title");

pref("font.name-list.serif.x-geor", "Sylfaen, BPG Paata Khutsuri U, TITUS Cyberbit Basic");
pref("font.name-list.sans-serif.x-geor", "BPG Classic 99U");
pref("font.name-list.monospace.x-geor", "BPG Classic 99U");

pref("font.name-list.serif.x-gujr", "Shruti");
pref("font.name-list.sans-serif.x-gujr", "Shruti");
pref("font.name-list.monospace.x-gujr", "Shruti");

pref("font.name-list.serif.x-guru", "Raavi, Saab");
pref("font.name-list.sans-serif.x-guru", "");
pref("font.name-list.monospace.x-guru", "Raavi, Saab");

pref("font.name-list.serif.x-khmr", "PhnomPenh OT,.Mondulkiri U GR 1.5, Khmer OS");
pref("font.name-list.sans-serif.x-khmr", "Khmer OS");
pref("font.name-list.monospace.x-khmr", "Khmer OS, Khmer OS System");

pref("font.name-list.serif.x-mlym", "Rachana_w01, AnjaliOldLipi, Kartika, ThoolikaUnicode");
pref("font.name-list.sans-serif.x-mlym", "Rachana_w01, AnjaliOldLipi, Kartika, ThoolikaUnicode");
pref("font.name-list.monospace.x-mlym", "Rachana_w01, AnjaliOldLipi, Kartika, ThoolikaUnicode");

pref("font.name-list.serif.x-orya", "ori1Uni, Kalinga");
pref("font.name-list.sans-serif.x-orya", "ori1Uni, Kalinga");
pref("font.name-list.monospace.x-orya", "ori1Uni, Kalinga");

pref("font.name-list.serif.x-telu", "Gautami, Akshar Unicode");
pref("font.name-list.sans-serif.x-telu", "Gautami, Akshar Unicode");
pref("font.name-list.monospace.x-telu", "Gautami, Akshar Unicode");

pref("font.name-list.serif.x-knda", "Tunga, AksharUnicode");
pref("font.name-list.sans-serif.x-knda", "Tunga, AksharUnicode");
pref("font.name-list.monospace.x-knda", "Tunga, AksharUnicode");

pref("font.name-list.serif.x-sinh", "Iskoola Pota, AksharUnicode");
pref("font.name-list.sans-serif.x-sinh", "Iskoola Pota, AksharUnicode");
pref("font.name-list.monospace.x-sinh", "Iskoola Pota, AksharUnicode");

pref("font.name-list.serif.x-tibt", "Tibetan Machine Uni, Jomolhari, Microsoft Himalaya");
pref("font.name-list.sans-serif.x-tibt", "Tibetan Machine Uni, Jomolhari, Microsoft Himalaya");
pref("font.name-list.monospace.x-tibt", "Tibetan Machine Uni, Jomolhari, Microsoft Himalaya");

pref("font.minimum-size.th", 10);

pref("font.default.x-devanagari", "sans-serif");

pref("font.name-list.serif.x-math", "Latin Modern Math, STIX Two Math, XITS Math, Cambria Math, Libertinus Math, DejaVu Math TeX Gyre, TeX Gyre Bonum Math, TeX Gyre Pagella Math, TeX Gyre Schola, TeX Gyre Termes Math, STIX Math, Asana Math, STIXGeneral, DejaVu Serif, DejaVu Sans, Times New Roman");
pref("font.name-list.sans-serif.x-math", "Arial");
pref("font.name-list.monospace.x-math", "Courier New");
pref("font.name-list.cursive.x-math", "Comic Sans MS");

// ClearType tuning parameters for directwrite/d2d.
//
// Allows overriding of underlying registry values in:
//   HKCU/Software/Microsoft/Avalon.Graphics/<display> (contrast and level)
//   HKLM/Software/Microsoft/Avalon.Graphics/<display> (gamma, pixel structure)
// and selection of the ClearType/antialiasing mode.
//
// A value of -1 implies use the default value, otherwise value ranges
// follow registry settings:
//   gamma [1000, 2200]  default: based on screen, typically 2200 (== 2.2)
//   enhanced contrast [0, 1000] default: 50
//   cleartype level [0, 100] default: 100
//   pixel structure [0, 2] default: 0 (flat/RGB/BGR)
//   rendering mode [0, 5] default: 0
//     0 = use default for font & size;
//     1 = aliased;
//     2 = GDI Classic;
//     3 = GDI Natural Widths;
//     4 = Natural;
//     5 = Natural Symmetric
//
// See:
//   http://msdn.microsoft.com/en-us/library/aa970267.aspx
//   http://msdn.microsoft.com/en-us/library/dd368190%28v=VS.85%29.aspx
// Note: DirectWrite uses the "Enhanced Contrast Level" value rather than the
// "Text Contrast Level" value

pref("gfx.font_rendering.cleartype_params.gamma", -1);
pref("gfx.font_rendering.cleartype_params.enhanced_contrast", -1);
pref("gfx.font_rendering.cleartype_params.cleartype_level", -1);
pref("gfx.font_rendering.cleartype_params.pixel_structure", -1);
pref("gfx.font_rendering.cleartype_params.rendering_mode", -1);

// A comma-separated list of font family names. Fonts in these families will
// be forced to use "GDI Classic" ClearType mode, provided the value
// of gfx.font_rendering.cleartype_params.rendering_mode is -1
// (i.e. a specific rendering_mode has not been explicitly set).
// Currently we apply this setting to the sans-serif Microsoft "core Web fonts".
pref("gfx.font_rendering.cleartype_params.force_gdi_classic_for_families",
     "Arial,Consolas,Courier New,Microsoft Sans Serif,Segoe UI,Tahoma,Trebuchet MS,Verdana");
// The maximum size at which we will force GDI classic mode using
// force_gdi_classic_for_families.
pref("gfx.font_rendering.cleartype_params.force_gdi_classic_max_size", 15);

pref("ui.key.menuAccessKeyFocuses", true);

// override double-click word selection behavior.
pref("layout.word_select.eat_space_to_next_word", true);

// scrollbar snapping region
pref("slider.snapMultiplier", 6);

// Whether to extend the native dialog with information on printing frames.
pref("print.extend_native_print_dialog", true);

// Locate plugins by the directories specified in the Windows registry for PLIDs
// Which is currently HKLM\Software\MozillaPlugins\xxxPLIDxxx\Path
pref("plugin.scan.plid.all", true);

// Whether sending WM_MOUSEWHEEL and WM_MOUSEHWHEEL to plugins on Windows.
pref("plugin.mousewheel.enabled", true);

// Switch the keyboard layout per window
pref("intl.keyboard.per_window_layout", false);

// Whether Gecko sets input scope of the URL bar to IS_DEFAULT when black
// listed IMEs are active.  If you use tablet mode mainly and you want to
// use touch keyboard for URL when you set focus to the URL bar, you can
// set this to false.  Then, you'll see, e.g., ".com" key on the keyboard.
// However, if you set this to false, such IMEs set its open state to "closed"
// when you set focus to the URL bar.  I.e., input mode is automatically
// changed to English input mode.
// Black listed IMEs:
//   - Microsoft IME for Japanese
//   - Google Japanese Input
//   - Microsoft Bopomofo
//   - Microsoft ChangJie
//   - Microsoft Phonetic
//   - Microsoft Quick
//   - Microsoft New ChangJie
//   - Microsoft New Phonetic
//   - Microsoft New Quick
//   - Microsoft Pinyin
//   - Microsoft Pinyin New Experience Input Style
//   - Microsoft Wubi
//   - Microsoft IME for Korean (except on Win7)
//   - Microsoft Old Hangul
pref("intl.ime.hack.set_input_scope_of_url_bar_to_default", true);

#ifdef NS_ENABLE_TSF
// Enable/Disable TSF support.
pref("intl.tsf.enable", true);

// Support IMEs implemented with IMM in TSF mode.
pref("intl.tsf.support_imm", true);

// This is referred only when both "intl.tsf.enable" and "intl.tsf.support_imm"
// are true.  When this is true, default IMC is associated with focused window
// only when active keyboard layout is a legacy IMM-IME.
pref("intl.tsf.associate_imc_only_when_imm_ime_is_active", false);

// Enables/Disables hack for specific TIP.

// Whether creates native caret for ATOK or not.
pref("intl.tsf.hack.atok.create_native_caret", true);
// Whether use available composition string rect for result of
// ITextStoreACP::GetTextExt() even if the specified range is same as the
// range of composition string but some character rects of them are not
// available.  Note that this is ignored if active ATOK is or older than 2016
// and create_native_caret is true.
pref("intl.tsf.hack.atok.do_not_return_no_layout_error_of_composition_string", true);
// Whether use available composition string rect for result of
// ITextStoreACP::GetTextExt() even if the specified range is same as or is in
// the range of composition string but some character rects of them are not
// available.
pref("intl.tsf.hack.japanist10.do_not_return_no_layout_error_of_composition_string", true);
// Whether use composition start position for the result of
// ITfContextView::GetTextExt() if the specified range is larger than
// composition start offset.
// For Free ChangJie 2010
pref("intl.tsf.hack.free_chang_jie.do_not_return_no_layout_error", true);
// For Microsoft Pinyin and Microsoft Wubi
pref("intl.tsf.hack.ms_simplified_chinese.do_not_return_no_layout_error", true);
// For Microsoft ChangJie and Microsoft Quick
pref("intl.tsf.hack.ms_traditional_chinese.do_not_return_no_layout_error", true);
// Whether use previous character rect for the result of
// ITfContextView::GetTextExt() if the specified range is the first character
// of selected clause of composition string.
pref("intl.tsf.hack.ms_japanese_ime.do_not_return_no_layout_error_at_first_char", true);
// Whether use previous character rect for the result of
// ITfContextView::GetTextExt() if the specified range is the caret of
// composition string.
pref("intl.tsf.hack.ms_japanese_ime.do_not_return_no_layout_error_at_caret", true);
// Whether hack ITextStoreACP::QueryInsert() or not.  The method should return
// new selection after specified length text is inserted at specified range.
// However, Microsoft's some Chinese TIPs expect that the result is same as
// specified range.  If following prefs are true, ITextStoreACP::QueryInsert()
// returns specified range only when one of the TIPs is active.
// For Microsoft Pinyin and Microsoft Wubi
pref("intl.tsf.hack.ms_simplified_chinese.query_insert_result", true);
// For Microsoft ChangJie and Microsoft Quick
pref("intl.tsf.hack.ms_traditional_chinese.query_insert_result", true);
#endif

// If composition_font is set, Gecko sets the font to IME.  IME may use
// the fonts on their window like candidate window.  If they are empty,
// Gecko uses the system default font which is set to the IM context.
// The font name must not start with '@'.  When the writing mode is vertical,
// Gecko inserts '@' to the start of the font name automatically.
// FYI: Changing these prefs requires to restart.
pref("intl.imm.composition_font", "");

// Japanist 2003's candidate window is broken if the font is "@System" which
// is default composition font for vertical writing mode.
// You can specify font to use on candidate window of Japanist 2003.
// This value must not start with '@'.
// FYI: Changing this pref requires to restart.
pref("intl.imm.composition_font.japanist_2003", "MS PGothic");

// Even if IME claims that they support vertical writing mode but it may not
// support vertical writing mode for its candidate window.  This pref allows
// to ignore the claim.
// FYI: Changing this pref requires to restart.
pref("intl.imm.vertical_writing.always_assume_not_supported", false);

// We cannot retrieve active IME name with IMM32 API if a TIP of TSF is active.
// This pref can specify active IME name when Japanese TIP is active.
// For example:
//   Google Japanese Input: "Google 日本語入力 IMM32 モジュール"
//   ATOK 2011: "ATOK 2011" (similarly, e.g., ATOK 2013 is "ATOK 2013")
pref("intl.imm.japanese.assume_active_tip_name_as", "");

// See bug 448927, on topmost panel, some IMEs are not usable on Windows.
pref("ui.panel.default_level_parent", false);

pref("mousewheel.system_scroll_override_on_root_content.enabled", true);

// Enable system settings cache for mouse wheel message handling.
// Note that even if this pref is set to true, Gecko may not cache the system
// settings if Gecko detects that the cache won't be refreshed properly when
// the settings are changed.
pref("mousewheel.system_settings_cache.enabled", true);

// This is a pref to test system settings cache for mouse wheel message
// handling.  If this is set to true, Gecko forcibly use the cache.
pref("mousewheel.system_settings_cache.force_enabled", false);

// High resolution scrolling with supported mouse drivers on Vista or later.
pref("mousewheel.enable_pixel_scrolling", true);

// If your mouse drive sends WM_*SCROLL messages when you turn your mouse wheel,
// set this to true.  Then, gecko processes them as mouse wheel messages.
pref("mousewheel.emulate_at_wm_scroll", false);

// Some odd touchpad utils give focus to window under cursor when user tries
// to scroll.  If this is true, Gecko tries to emulate such odd behavior.
// Don't make this true unless you want to debug.  Enabling this pref causes
// making damage to the performance.
pref("mousewheel.debug.make_window_under_cursor_foreground", false);

// Enables or disabled the TrackPoint hack, -1 is autodetect, 0 is off,
// and 1 is on.  Set this to 1 if TrackPoint scrolling is not working.
pref("ui.trackpoint_hack.enabled", -1);

// Setting this to a non-empty string overrides the Win32 "window class" used
// for "normal" windows. Setting this to MozillaUIWindowClass might make
// some trackpad drivers behave better.
pref("ui.window_class_override", "");

// Enables or disables the Elantech gesture hacks.  -1 is autodetect, 0 is off,
// and 1 is on.  Set this to 1 if three-finger swipe gestures do not cause
// page back/forward actions, or if pinch-to-zoom does not work.
pref("ui.elantech_gesture_hacks.enabled", -1);

// Show the Windows on-screen keyboard (osk.exe) when a text field is focused.
pref("ui.osk.enabled", true);
// Only show the on-screen keyboard if there are no physical keyboards attached
// to the device.
pref("ui.osk.detect_physical_keyboard", true);
// Path to TabTip.exe on local machine. Cached for performance reasons.
pref("ui.osk.on_screen_keyboard_path", "");
// Only try to show the on-screen keyboard on Windows 10 and later. Setting
// this pref to false will allow the OSK to show on Windows 8 and 8.1.
pref("ui.osk.require_win10", false);
// This pref stores the "reason" that the on-screen keyboard was either
// shown or not shown when focus is moved to an editable text field. It is
// used to help debug why the keyboard is either not appearing when expected
// or appearing when it is not expected.
pref("ui.osk.debug.keyboardDisplayReason", "");

# XP_WIN
#endif

#ifdef XP_MACOSX
// Mac specific preference defaults
pref("browser.drag_out_of_frame_style", 1);
pref("ui.key.saveLink.shift", false); // true = shift, false = meta

// default fonts (in UTF8 and using canonical names)
// to determine canonical font names, use a debug build and
// enable NSPR logging for module fontInfoLog:5
// canonical names immediately follow '(fontinit) family:' in the log

pref("font.name-list.emoji", "Apple Color Emoji");

pref("font.name-list.serif.ar", "Al Bayan");
pref("font.name-list.sans-serif.ar", "Geeza Pro");
pref("font.name-list.monospace.ar", "Geeza Pro");
pref("font.name-list.cursive.ar", "DecoType Naskh");
pref("font.name-list.fantasy.ar", "KufiStandardGK");

pref("font.name-list.serif.el", "Times, Times New Roman");
pref("font.name-list.sans-serif.el", "Helvetica, Lucida Grande");
pref("font.name-list.monospace.el", "Courier New, Lucida Grande");
pref("font.name-list.cursive.el", "Lucida Grande, Times");
pref("font.name-list.fantasy.el", "Lucida Grande, Times");

pref("font.name-list.serif.he", "Times New Roman");
pref("font.name-list.sans-serif.he", "Arial");
pref("font.name-list.monospace.he", "Courier New");
pref("font.name-list.cursive.he", "Times New Roman");
pref("font.name-list.fantasy.he", "Times New Roman");

pref("font.name-list.serif.ja", "Hiragino Mincho ProN, Hiragino Mincho Pro");
pref("font.name-list.sans-serif.ja", "Hiragino Kaku Gothic ProN, Hiragino Kaku Gothic Pro");
pref("font.name-list.monospace.ja", "Osaka-Mono");

pref("font.name-list.serif.ko", "AppleMyungjo");
pref("font.name-list.sans-serif.ko", "Apple SD Gothic Neo, AppleGothic");
pref("font.name-list.monospace.ko", "Apple SD Gothic Neo, AppleGothic");

pref("font.name-list.serif.th", "Thonburi");
pref("font.name-list.sans-serif.th", "Thonburi");
pref("font.name-list.monospace.th", "Ayuthaya");

pref("font.name-list.serif.x-armn", "Mshtakan");
pref("font.name-list.sans-serif.x-armn", "Mshtakan");
pref("font.name-list.monospace.x-armn", "Mshtakan");

// SolaimanLipi, Rupali http://ekushey.org/?page/mac_download
pref("font.name-list.serif.x-beng", "Bangla MN");
pref("font.name-list.sans-serif.x-beng", "Bangla Sangam MN");
pref("font.name-list.monospace.x-beng", "Bangla Sangam MN");

pref("font.name-list.serif.x-cans", "Euphemia UCAS");
pref("font.name-list.sans-serif.x-cans", "Euphemia UCAS");
pref("font.name-list.monospace.x-cans", "Euphemia UCAS");

pref("font.name-list.serif.x-cyrillic", "Times, Times New Roman");
pref("font.name-list.sans-serif.x-cyrillic", "Helvetica, Arial");
pref("font.name-list.monospace.x-cyrillic", "Monaco, Courier New");
pref("font.name-list.cursive.x-cyrillic", "Geneva");
pref("font.name-list.fantasy.x-cyrillic", "Charcoal CY");

pref("font.name-list.serif.x-devanagari", "Devanagari MT");
pref("font.name-list.sans-serif.x-devanagari", "Devanagari Sangam MN, Devanagari MT");
pref("font.name-list.monospace.x-devanagari", "Devanagari Sangam MN, Devanagari MT");

// Abyssinica SIL http://scripts.sil.org/AbyssinicaSIL_Download
pref("font.name-list.serif.x-ethi", "Kefa, Abyssinica SIL");
pref("font.name-list.sans-serif.x-ethi", "Kefa, Abyssinica SIL");
pref("font.name-list.monospace.x-ethi", "Kefa, Abyssinica SIL");

// no suitable fonts for georgian ship with mac os x
// however some can be freely downloaded
// TITUS Cyberbit Basic http://titus.fkidg1.uni-frankfurt.de/unicode/tituut.asp
// Zuzumbo http://homepage.mac.com/rsiradze/FileSharing91.html
pref("font.name-list.serif.x-geor", "TITUS Cyberbit Basic");
pref("font.name-list.sans-serif.x-geor", "Zuzumbo");
pref("font.name-list.monospace.x-geor", "Zuzumbo");

pref("font.name-list.serif.x-gujr", "Gujarati MT");
pref("font.name-list.sans-serif.x-gujr", "Gujarati Sangam MN, Gujarati MT");
pref("font.name-list.monospace.x-gujr", "Gujarati Sangam MN, Gujarati MT");

pref("font.name-list.serif.x-guru", "Gurmukhi MT");
pref("font.name-list.sans-serif.x-guru", "Gurmukhi MT");
pref("font.name-list.monospace.x-guru", "Gurmukhi MT");

pref("font.name-list.serif.x-khmr", "Khmer MN");
pref("font.name-list.sans-serif.x-khmr", "Khmer Sangam MN");
pref("font.name-list.monospace.x-khmr", "Khmer Sangam MN");

pref("font.name-list.serif.x-mlym", "Malayalam MN");
pref("font.name-list.sans-serif.x-mlym", "Malayalam Sangam MN");
pref("font.name-list.monospace.x-mlym", "Malayalam Sangam MN");

pref("font.name-list.serif.x-orya", "Oriya MN");
pref("font.name-list.sans-serif.x-orya", "Oriya Sangam MN");
pref("font.name-list.monospace.x-orya", "Oriya Sangam MN");

// Pothana http://web.nickshanks.com/typography/telugu/
pref("font.name-list.serif.x-telu", "Telugu MN, Pothana");
pref("font.name-list.sans-serif.x-telu", "Telugu Sangam MN, Pothana");
pref("font.name-list.monospace.x-telu", "Telugu Sangam MN, Pothana");

// Kedage http://web.nickshanks.com/typography/kannada/
pref("font.name-list.serif.x-knda", "Kannada MN, Kedage");
pref("font.name-list.sans-serif.x-knda", "Kannada Sangam MN, Kedage");
pref("font.name-list.monospace.x-knda", "Kannada Sangam MN, Kedage");

pref("font.name-list.serif.x-sinh", "Sinhala MN");
pref("font.name-list.sans-serif.x-sinh", "Sinhala Sangam MN");
pref("font.name-list.monospace.x-sinh", "Sinhala Sangam MN");

pref("font.name-list.serif.x-tamil", "InaiMathi");
pref("font.name-list.sans-serif.x-tamil", "InaiMathi");
pref("font.name-list.monospace.x-tamil", "InaiMathi");

// Kailasa ships with mac os x >= 10.5
pref("font.name-list.serif.x-tibt", "Kailasa");
pref("font.name-list.sans-serif.x-tibt", "Kailasa");
pref("font.name-list.monospace.x-tibt", "Kailasa");

pref("font.name-list.serif.x-unicode", "Times");
pref("font.name-list.sans-serif.x-unicode", "Helvetica");
pref("font.name-list.monospace.x-unicode", "Courier");
pref("font.name-list.cursive.x-unicode", "Apple Chancery");
pref("font.name-list.fantasy.x-unicode", "Papyrus");

pref("font.name-list.serif.x-western", "Times, Times New Roman");
pref("font.name-list.sans-serif.x-western", "Helvetica, Arial");
pref("font.name-list.monospace.x-western", "Courier, Courier New");
pref("font.name-list.cursive.x-western", "Apple Chancery");
pref("font.name-list.fantasy.x-western", "Papyrus");

pref("font.name-list.serif.zh-CN", "Times, Songti SC, STSong, Heiti SC");
pref("font.name-list.sans-serif.zh-CN", "Helvetica, PingFang SC, STHeiti, Heiti SC");
pref("font.name-list.monospace.zh-CN", "Courier, PingFang SC, STHeiti, Heiti SC");
pref("font.name-list.cursive.zh-CN", "Kaiti SC");

pref("font.name-list.serif.zh-TW", "Times, Songti TC, LiSong Pro, Heiti TC");
pref("font.name-list.sans-serif.zh-TW", "Helvetica, PingFang TC, Heiti TC, LiHei Pro");
pref("font.name-list.monospace.zh-TW", "Courier, PingFang TC, Heiti TC, LiHei Pro");
pref("font.name-list.cursive.zh-TW", "Kaiti TC");

pref("font.name-list.serif.zh-HK", "Times, Songti TC, LiSong Pro, Heiti TC");
pref("font.name-list.sans-serif.zh-HK", "Helvetica, PingFang TC, Heiti TC, LiHei Pro");
pref("font.name-list.monospace.zh-HK", "Courier, PingFang TC, Heiti TC, LiHei Pro");
pref("font.name-list.cursive.zh-HK", "Kaiti TC");

// XP_MACOSX changes to default font sizes
pref("font.minimum-size.th", 10);

// Apple's Symbol is Unicode so use it
pref("font.name-list.serif.x-math", "Latin Modern Math, STIX Two Math, XITS Math, Cambria Math, Libertinus Math, DejaVu Math TeX Gyre, TeX Gyre Bonum Math, TeX Gyre Pagella Math, TeX Gyre Schola, TeX Gyre Termes Math, STIX Math, Asana Math, STIXGeneral, DejaVu Serif, DejaVu Sans, Symbol, Times");
pref("font.name-list.sans-serif.x-math", "Helvetica");
pref("font.name-list.monospace.x-math", "Courier");
pref("font.name-list.cursive.x-math", "Apple Chancery");
pref("font.name-list.fantasy.x-math", "Papyrus");

// Individual font faces to be treated as independent families,
// listed as <Postscript name of face:Owning family name>
pref("font.single-face-list", "Osaka-Mono:Osaka");

// optimization hint for fonts with localized names to be read in at startup, otherwise read in at lookup miss
// names are canonical family names (typically English names)
pref("font.preload-names-list", "Hiragino Kaku Gothic ProN,Hiragino Mincho ProN,STSong");

// Override font-weight values for some problematic families Apple ships
// (see bug 931426).
// The name here is the font's PostScript name, which can be checked in
// the Font Book utility or other tools.
pref("font.weight-override.AppleSDGothicNeo-Thin", 100); // Ensure Thin < UltraLight < Light
pref("font.weight-override.AppleSDGothicNeo-UltraLight", 200);
pref("font.weight-override.AppleSDGothicNeo-Light", 300);
pref("font.weight-override.AppleSDGothicNeo-Heavy", 900); // Ensure Heavy > ExtraBold (800)

pref("font.weight-override.Avenir-Book", 300); // Ensure Book < Roman (400)
pref("font.weight-override.Avenir-BookOblique", 300);
pref("font.weight-override.Avenir-MediumOblique", 500); // Harmonize MediumOblique with Medium
pref("font.weight-override.Avenir-Black", 900); // Ensure Black > Heavy (800)
pref("font.weight-override.Avenir-BlackOblique", 900);

pref("font.weight-override.AvenirNext-MediumItalic", 500); // Harmonize MediumItalic with Medium
pref("font.weight-override.AvenirNextCondensed-MediumItalic", 500);

pref("font.weight-override.HelveticaNeue-Light", 300); // Ensure Light > Thin (200)
pref("font.weight-override.HelveticaNeue-LightItalic", 300);
pref("font.weight-override.HelveticaNeue-MediumItalic", 500); // Harmonize MediumItalic with Medium

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
pref("ui.key.contentAccess", 6);

// See bug 404131, topmost <panel> element wins to Dashboard on MacOSX.
pref("ui.panel.default_level_parent", false);

pref("ui.plugin.cancel_composition_at_input_source_changed", false);

pref("mousewheel.system_scroll_override_on_root_content.enabled", false);

// Macbook touchpad two finger pixel scrolling
pref("mousewheel.enable_pixel_scrolling", true);

# XP_MACOSX
#endif

#ifdef ANDROID
// Handled differently under Mac/Windows
pref("network.protocol-handler.warn-external.file", false);
pref("browser.drag_out_of_frame_style", 1);

// Middle-mouse handling
pref("middlemouse.paste", true);
pref("middlemouse.openNewWindow", true);
pref("middlemouse.scrollbarPosition", true);

pref("browser.urlbar.clickSelectsAll", false);

// Tab focus model bit field:
// 1 focuses text controls, 2 focuses other form elements, 4 adds links.
// Leave this at the default, 7, to match mozilla1.0-era user expectations.
// pref("accessibility.tabfocus", 1);

pref("helpers.global_mime_types_file", "/etc/mime.types");
pref("helpers.global_mailcap_file", "/etc/mailcap");
pref("helpers.private_mime_types_file", "~/.mime.types");
pref("helpers.private_mailcap_file", "~/.mailcap");
pref("print.printer_list", ""); // list of printers, separated by spaces
pref("print.print_reversed", false);
pref("print.print_in_color", true);

/* PostScript print module prefs */
// pref("print.postscript.enabled",      true);

// Setting default_level_parent to true makes the default level for popup
// windows "top" instead of "parent".  On GTK2 platform, this is implemented
// with override-redirect windows which is the normal way to implement
// temporary popup windows.  Setting this to false would make the default
// level "parent" which is implemented with managed windows.
// A problem with using managed windows is that metacity sometimes deactivates
// the parent window when the managed popup is shown.
pref("ui.panel.default_level_parent", true);

pref("mousewheel.system_scroll_override_on_root_content.enabled", false);

// Forward downloads with known OMA MIME types to Android's download manager
// instead of downloading them in the browser.
pref("browser.download.forward_oma_android_download_manager", false);

# ANDROID
#endif

#ifndef ANDROID
#ifndef XP_MACOSX
#ifdef XP_UNIX
// Handled differently under Mac/Windows
pref("network.protocol-handler.warn-external.file", false);
pref("browser.drag_out_of_frame_style", 1);

// Middle-mouse handling
pref("middlemouse.paste", true);
pref("middlemouse.openNewWindow", true);
pref("middlemouse.scrollbarPosition", true);

// Clipboard behavior
pref("clipboard.autocopy", true);

pref("browser.urlbar.clickSelectsAll", false);

// Tab focus model bit field:
// 1 focuses text controls, 2 focuses other form elements, 4 adds links.
// Leave this at the default, 7, to match mozilla1.0-era user expectations.
// pref("accessibility.tabfocus", 1);

pref("helpers.global_mime_types_file", "/etc/mime.types");
pref("helpers.global_mailcap_file", "/etc/mailcap");
pref("helpers.private_mime_types_file", "~/.mime.types");
pref("helpers.private_mailcap_file", "~/.mailcap");
pref("print.printer_list", ""); // list of printers, separated by spaces
pref("print.print_reversed", false);
pref("print.print_in_color", true);

// font names

// fontconfig doesn't support emoji yet
// https://lists.freedesktop.org/archives/fontconfig/2016-October/005842.html
pref("font.name-list.emoji", "Twemoji Mozilla");

pref("font.name-list.serif.ar", "serif");
pref("font.name-list.sans-serif.ar", "sans-serif");
pref("font.name-list.monospace.ar", "monospace");
pref("font.size.fixed.ar", 12);

pref("font.name-list.serif.el", "serif");
pref("font.name-list.sans-serif.el", "sans-serif");
pref("font.name-list.monospace.el", "monospace");
pref("font.size.fixed.el", 12);

pref("font.name-list.serif.he", "serif");
pref("font.name-list.sans-serif.he", "sans-serif");
pref("font.name-list.monospace.he", "monospace");
pref("font.size.fixed.he", 12);

pref("font.name-list.serif.ja", "serif");
pref("font.name-list.sans-serif.ja", "sans-serif");
pref("font.name-list.monospace.ja", "monospace");

pref("font.name-list.serif.ko", "serif");
pref("font.name-list.sans-serif.ko", "sans-serif");
pref("font.name-list.monospace.ko", "monospace");

pref("font.name-list.serif.th", "serif");
pref("font.name-list.sans-serif.th", "sans-serif");
pref("font.name-list.monospace.th", "monospace");
pref("font.minimum-size.th", 13);

pref("font.name-list.serif.x-armn", "serif");
pref("font.name-list.sans-serif.x-armn", "sans-serif");
pref("font.name-list.monospace.x-armn", "monospace");

pref("font.name-list.serif.x-beng", "serif");
pref("font.name-list.sans-serif.x-beng", "sans-serif");
pref("font.name-list.monospace.x-beng", "monospace");

pref("font.name-list.serif.x-cans", "serif");
pref("font.name-list.sans-serif.x-cans", "sans-serif");
pref("font.name-list.monospace.x-cans", "monospace");

pref("font.name-list.serif.x-cyrillic", "serif");
pref("font.name-list.sans-serif.x-cyrillic", "sans-serif");
pref("font.name-list.monospace.x-cyrillic", "monospace");
pref("font.size.fixed.x-cyrillic", 12);

pref("font.name-list.serif.x-devanagari", "serif");
pref("font.name-list.sans-serif.x-devanagari", "sans-serif");
pref("font.name-list.monospace.x-devanagari", "monospace");

pref("font.name-list.serif.x-ethi", "serif");
pref("font.name-list.sans-serif.x-ethi", "sans-serif");
pref("font.name-list.monospace.x-ethi", "monospace");

pref("font.name-list.serif.x-geor", "serif");
pref("font.name-list.sans-serif.x-geor", "sans-serif");
pref("font.name-list.monospace.x-geor", "monospace");

pref("font.name-list.serif.x-gujr", "serif");
pref("font.name-list.sans-serif.x-gujr", "sans-serif");
pref("font.name-list.monospace.x-gujr", "monospace");

pref("font.name-list.serif.x-guru", "serif");
pref("font.name-list.sans-serif.x-guru", "sans-serif");
pref("font.name-list.monospace.x-guru", "monospace");

pref("font.name-list.serif.x-khmr", "serif");
pref("font.name-list.sans-serif.x-khmr", "sans-serif");
pref("font.name-list.monospace.x-khmr", "monospace");

pref("font.name-list.serif.x-knda", "serif");
pref("font.name-list.sans-serif.x-knda", "sans-serif");
pref("font.name-list.monospace.x-knda", "monospace");

pref("font.name-list.serif.x-mlym", "serif");
pref("font.name-list.sans-serif.x-mlym", "sans-serif");
pref("font.name-list.monospace.x-mlym", "monospace");

pref("font.name-list.serif.x-orya", "serif");
pref("font.name-list.sans-serif.x-orya", "sans-serif");
pref("font.name-list.monospace.x-orya", "monospace");

pref("font.name-list.serif.x-sinh", "serif");
pref("font.name-list.sans-serif.x-sinh", "sans-serif");
pref("font.name-list.monospace.x-sinh", "monospace");

pref("font.name-list.serif.x-tamil", "serif");
pref("font.name-list.sans-serif.x-tamil", "sans-serif");
pref("font.name-list.monospace.x-tamil", "monospace");

pref("font.name-list.serif.x-telu", "serif");
pref("font.name-list.sans-serif.x-telu", "sans-serif");
pref("font.name-list.monospace.x-telu", "monospace");

pref("font.name-list.serif.x-tibt", "serif");
pref("font.name-list.sans-serif.x-tibt", "sans-serif");
pref("font.name-list.monospace.x-tibt", "monospace");

pref("font.name-list.serif.x-unicode", "serif");
pref("font.name-list.sans-serif.x-unicode", "sans-serif");
pref("font.name-list.monospace.x-unicode", "monospace");
pref("font.size.fixed.x-unicode", 12);

pref("font.name-list.serif.x-western", "serif");
pref("font.name-list.sans-serif.x-western", "sans-serif");
pref("font.name-list.monospace.x-western", "monospace");
pref("font.size.fixed.x-western", 12);

pref("font.name-list.serif.zh-CN", "serif");
pref("font.name-list.sans-serif.zh-CN", "sans-serif");
pref("font.name-list.monospace.zh-CN", "monospace");

pref("font.name-list.serif.zh-HK", "serif");
pref("font.name-list.sans-serif.zh-HK", "sans-serif");
pref("font.name-list.monospace.zh-HK", "monospace");

pref("font.name-list.serif.zh-TW", "serif");
pref("font.name-list.sans-serif.zh-TW", "sans-serif");
pref("font.name-list.monospace.zh-TW", "monospace");

/* PostScript print module prefs */
// pref("print.postscript.enabled",      true);

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

pref("intl.ime.use_simple_context_on_password_field", false);

// uim may use key snooper to listen to key events.  Unfortunately, we cannot
// know whether it uses or not since it's a build option.  So, we need to make
// distribution switchable whether we think uim uses key snooper or not with
// this pref.  Debian 9.x still uses uim as their default IM and it uses key
// snooper.  So, let's use true for its default value.
pref("intl.ime.hack.uim.using_key_snooper", true);

#ifdef MOZ_WIDGET_GTK
// maximum number of fonts to substitute for a generic
pref("gfx.font_rendering.fontconfig.max_generic_substitutions", 3);
#endif

# XP_UNIX
#endif
#endif
#endif

#if defined(ANDROID)

pref("font.size.fixed.ar", 12);

pref("font.default.el", "sans-serif");
pref("font.size.fixed.el", 12);

pref("font.size.fixed.he", 12);

pref("font.default.x-cyrillic", "sans-serif");
pref("font.size.fixed.x-cyrillic", 12);

pref("font.default.x-unicode", "sans-serif");
pref("font.size.fixed.x-unicode", 12);

pref("font.default.x-western", "sans-serif");
pref("font.size.fixed.x-western", 12);

# ANDROID
#endif

#if defined(ANDROID)
// We use the bundled Charis SIL Compact as serif font for Firefox for Android

pref("font.name-list.emoji", "Noto Color Emoji");

pref("font.name-list.serif.ar", "Noto Naskh Arabic, Noto Serif, Droid Serif");
pref("font.name-list.sans-serif.ar", "Noto Naskh Arabic, Roboto, Droid Sans");
pref("font.name-list.monospace.ar", "Noto Naskh Arabic");

pref("font.name-list.serif.el", "Droid Serif, Noto Serif"); // not Charis SIL Compact, only has a few Greek chars
pref("font.name-list.sans-serif.el", "Roboto, Droid Sans");
pref("font.name-list.monospace.el", "Droid Sans Mono");

pref("font.name-list.serif.he", "Droid Serif, Noto Serif");
pref("font.name-list.sans-serif.he", "Roboto, Droid Sans Hebrew, Droid Sans, Arial");
pref("font.name-list.monospace.he", "Droid Sans Mono");

pref("font.name-list.serif.ja", "Charis SIL Compact, Noto Serif, Droid Serif");
pref("font.name-list.sans-serif.ja", "Roboto, Droid Sans, MotoyaLMaru, MotoyaLCedar, Noto Sans JP, Noto Sans CJK JP, Droid Sans Japanese");
pref("font.name-list.monospace.ja", "MotoyaLMaru, MotoyaLCedar, Droid Sans Mono CJK JP, Droid Sans Mono");

pref("font.name-list.serif.ko", "Charis SIL Compact, Noto Serif, Droid Serif, HYSerif");
pref("font.name-list.sans-serif.ko", "Roboto, SmartGothic, NanumGothic, Noto Sans KR, Noto Sans CJK KR, DroidSansFallback, Droid Sans Fallback");
pref("font.name-list.monospace.ko", "Droid Sans Mono, Noto Sans Mono CJK KR");

pref("font.name-list.serif.th", "Charis SIL Compact, Noto Serif, Droid Serif");
pref("font.name-list.sans-serif.th", "Roboto, Droid Sans Thai, Droid Sans");
pref("font.name-list.monospace.th", "Droid Sans Mono");

pref("font.name-list.serif.x-cyrillic", "Charis SIL Compact, Noto Serif, Droid Serif");
pref("font.name-list.sans-serif.x-cyrillic", "Roboto, Droid Sans");
pref("font.name-list.monospace.x-cyrillic", "Droid Sans Mono");

pref("font.name-list.serif.x-unicode", "Charis SIL Compact, Noto Serif, Droid Serif");
pref("font.name-list.sans-serif.x-unicode", "Roboto, Droid Sans");
pref("font.name-list.monospace.x-unicode", "Droid Sans Mono");

pref("font.name-list.serif.x-western", "Charis SIL Compact, Noto Serif, Droid Serif");
pref("font.name-list.sans-serif.x-western", "Roboto, Droid Sans");
pref("font.name-list.monospace.x-western", "Droid Sans Mono");

pref("font.name-list.serif.zh-CN", "Charis SIL Compact, Noto Serif, Droid Serif, Droid Sans Fallback");
pref("font.name-list.sans-serif.zh-CN", "Roboto, Droid Sans, Noto Sans SC, Noto Sans CJK SC, Droid Sans Fallback");
pref("font.name-list.monospace.zh-CN", "Droid Sans Mono, Noto Sans Mono CJK SC, Droid Sans Fallback");

pref("font.name-list.serif.zh-HK", "Charis SIL Compact, Noto Serif, Droid Serif, Droid Sans Fallback");
pref("font.name-list.sans-serif.zh-HK", "Roboto, Droid Sans, Noto Sans TC, Noto Sans SC, Noto Sans CJK TC, Droid Sans Fallback");
pref("font.name-list.monospace.zh-HK", "Droid Sans Mono, Noto Sans Mono CJK TC, Droid Sans Fallback");

pref("font.name-list.serif.zh-TW", "Charis SIL Compact, Noto Serif, Droid Serif, Droid Sans Fallback");
pref("font.name-list.sans-serif.zh-TW", "Roboto, Droid Sans, Noto Sans TC, Noto Sans SC, Noto Sans CJK TC, Droid Sans Fallback");
pref("font.name-list.monospace.zh-TW", "Droid Sans Mono, Noto Sans Mono CJK TC, Droid Sans Fallback");

pref("font.name-list.serif.x-math", "Latin Modern Math, STIX Two Math, XITS Math, Cambria Math, Libertinus Math, DejaVu Math TeX Gyre, TeX Gyre Bonum Math, TeX Gyre Pagella Math, TeX Gyre Schola, TeX Gyre Termes Math, STIX Math, Asana Math, STIXGeneral, DejaVu Serif, DejaVu Sans, Charis SIL Compact");
pref("font.name-list.sans-serif.x-math", "Roboto");
pref("font.name-list.monospace.x-math", "Droid Sans Mono");

#endif

#if OS_ARCH==AIX

// Override default Japanese fonts
pref("font.name-list.serif.ja", "dt-interface system-jisx0208.1983-0");
pref("font.name-list.sans-serif.ja", "dt-interface system-jisx0208.1983-0");
pref("font.name-list.monospace.ja", "dt-interface user-jisx0208.1983-0");

// Override default Cyrillic fonts
pref("font.name-list.serif.x-cyrillic", "dt-interface system-iso8859-5");
pref("font.name-list.sans-serif.x-cyrillic", "dt-interface system-iso8859-5");
pref("font.name-list.monospace.x-cyrillic", "dt-interface user-iso8859-5");

// Override default Unicode fonts
pref("font.name-list.serif.x-unicode", "dt-interface system-ucs2.cjk_japan-0");
pref("font.name-list.sans-serif.x-unicode", "dt-interface system-ucs2.cjk_japan-0");
pref("font.name-list.monospace.x-unicode", "dt-interface user-ucs2.cjk_japan-0");

# AIX
#endif

// Login Manager prefs
pref("signon.rememberSignons",              true);
pref("signon.rememberSignons.visibilityToggle", true);
pref("signon.autofillForms",                true);
pref("signon.autofillForms.http",           false);
pref("signon.autologin.proxy",              false);
pref("signon.formlessCapture.enabled",      true);
pref("signon.storeWhenAutocompleteOff",     true);
pref("signon.debug",                        false);
pref("signon.recipes.path",                 "chrome://passwordmgr/content/recipes.json");
pref("signon.schemeUpgrades",               false);
// This temporarily prevents the master password to reprompt for autocomplete.
pref("signon.masterPasswordReprompt.timeout_ms", 900000); // 15 Minutes

// Satchel (Form Manager) prefs
pref("browser.formfill.debug",            false);
pref("browser.formfill.enable",           true);
pref("browser.formfill.expire_days",      180);
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

//
// Image-related prefs
//

// The maximum size (in kB) that the aggregate frames of an animation can use
// before it starts to discard already displayed frames and redecode them as
// necessary.
pref("image.animated.decode-on-demand.threshold-kb", 20480);

// The minimum number of frames we want to have buffered ahead of an
// animation's currently displayed frame.
pref("image.animated.decode-on-demand.batch-size", 6);

// Resume an animated image from the last displayed frame rather than
// advancing when out of view.
pref("image.animated.resume-from-last-displayed", true);

// Maximum number of surfaces for an image before entering "factor of 2" mode.
// This in addition to the number of "native" sizes of an image. A native size
// is a size for which we can decode a frame without up or downscaling. Most
// images only have 1, but some (i.e. ICOs) may have multiple frames for the
// same data at different sizes.
pref("image.cache.factor2.threshold-surfaces", 4);

// The maximum size, in bytes, of the decoded images we cache
pref("image.cache.size", 5242880);

// A weight, from 0-1000, to place on time when comparing to size.
// Size is given a weight of 1000 - timeweight.
pref("image.cache.timeweight", 500);

// Decode all images automatically on load, ignoring our normal heuristics.
pref("image.decode-immediately.enabled", false);

// Whether we attempt to downscale images during decoding.
pref("image.downscale-during-decode.enabled", true);

// The default Accept header sent for images loaded over HTTP(S)
pref("image.http.accept", "*/*");

// The threshold for inferring that changes to an <img> element's |src|
// attribute by JavaScript represent an animation, in milliseconds. If the |src|
// attribute is changing more frequently than this value, then we enter a
// special "animation mode" which is designed to eliminate flicker. Set to 0 to
// disable.
pref("image.infer-src-animation.threshold-ms", 2000);

// Whether the network request priority should be adjusted according
// the layout and view frame position of each particular image.
pref("image.layout_network_priority", true);

//
// Image memory management prefs
//

// Discards inactive image frames and re-decodes them on demand from
// compressed data.
pref("image.mem.discardable", true);

// Discards inactive image frames of _animated_ images and re-decodes them on
// demand from compressed data. Has no effect if image.mem.discardable is false.
pref("image.mem.animated.discardable", true);

// Whether the heap should be used for frames from animated images. On Android,
// volatile memory keeps file handles open for each buffer.
#if defined(ANDROID)
pref("image.mem.animated.use_heap", true);
#else
pref("image.mem.animated.use_heap", false);
#endif

// Decodes images into shared memory to allow direct use in separate
// rendering processes. Only applicable with WebRender.
pref("image.mem.shared", true);

// Allows image locking of decoded image data in content processes.
pref("image.mem.allow_locking_in_content_processes", true);

// Chunk size for calls to the image decoders
pref("image.mem.decode_bytes_at_a_time", 16384);

// Minimum timeout for expiring unused images from the surface cache, in
// milliseconds. This controls how long we store cached temporary surfaces.
pref("image.mem.surfacecache.min_expiration_ms", 60000); // 60s

// Maximum size for the surface cache, in kilobytes.
pref("image.mem.surfacecache.max_size_kb", 1048576); // 1GB

// The surface cache's size, within the constraints of the maximum size set
// above, is determined as a fraction of main memory size. The size factor is
// interpreted as a reciprocal, so a size factor of 4 means to use no more than
// 1/4 of main memory.  The default should be a good balance for most systems.
pref("image.mem.surfacecache.size_factor", 4);

// How much of the data in the surface cache is discarded when we get a memory
// pressure notification, as a fraction. The discard factor is interpreted as a
// reciprocal, so a discard factor of 1 means to discard everything in the
// surface cache on memory pressure, a discard factor of 2 means to discard half
// of the data, and so forth. The default should be a good balance for desktop
// and laptop systems, where we never discard visible images.
pref("image.mem.surfacecache.discard_factor", 1);

// What is the minimum buffer size in KB before using volatile memory over the
// heap. On Android, volatile memory keeps file handles open for each buffer.
#if defined(ANDROID)
pref("image.mem.volatile.min_threshold_kb", 100);
#else
pref("image.mem.volatile.min_threshold_kb", -1);
#endif

// How many threads we'll use for multithreaded decoding. If < 0, will be
// automatically determined based on the system's number of cores.
pref("image.multithreaded_decoding.limit", -1);

// How long in ms before we should start shutting down idle decoder threads.
pref("image.multithreaded_decoding.idle_timeout", 600000);

// Limit for the canvas image cache. 0 means we don't limit the size of the
// cache.
pref("canvas.image.cache.limit", 0);

// WebGL prefs
#ifdef ANDROID
// Disable MSAA on mobile.
pref("gl.msaa-level", 0);
#else
pref("gl.msaa-level", 2);
#endif
pref("gl.require-hardware", false);
#ifdef XP_MACOSX
pref("gl.multithreaded", true);
#endif
pref("gl.ignore-dx-interop2-blacklist", false);
pref("gl.use-tls-is-current", 0);

#ifdef XP_MACOSX
pref("webgl.1.allow-core-profiles", true);
#else
pref("webgl.1.allow-core-profiles", false);
#endif
pref("webgl.force-enabled", false);
pref("webgl.disabled", false);
pref("webgl.disable-angle", false);
pref("webgl.disable-wgl", false);
pref("webgl.min_capability_mode", false);
pref("webgl.disable-extensions", false);
pref("webgl.msaa-force", false);
pref("webgl.prefer-16bpp", false);
pref("webgl.default-no-alpha", false);
pref("webgl.force-layers-readback", false);
pref("webgl.force-index-validation", 0);
pref("webgl.lose-context-on-memory-pressure", false);
pref("webgl.can-lose-context-in-foreground", true);
pref("webgl.restore-context-when-visible", true);
#ifdef ANDROID
pref("webgl.max-contexts", 16);
pref("webgl.max-contexts-per-principal", 8);
#else
pref("webgl.max-contexts", 32);
pref("webgl.max-contexts-per-principal", 16);
#endif
pref("webgl.max-warnings-per-context", 32);
pref("webgl.enable-draft-extensions", false);
pref("webgl.enable-privileged-extensions", false);
pref("webgl.bypass-shader-validation", false);
pref("webgl.disable-fail-if-major-performance-caveat", false);
pref("webgl.disable-DOM-blit-uploads", false);
pref("webgl.allow-fb-invalidation", false);
pref("webgl.webgl2-compat-mode", false);

pref("webgl.perf.max-warnings", 0);
pref("webgl.perf.max-acceptable-fb-status-invals", 0);
pref("webgl.perf.spew-frame-allocs", true);

pref("webgl.enable-webgl2", true);

pref("webgl.enable-debug-renderer-info", true);
pref("webgl.renderer-string-override", "");
pref("webgl.vendor-string-override", "");

#ifdef XP_WIN
pref("webgl.angle.try-d3d11", true);
pref("webgl.angle.force-d3d11", false);
pref("webgl.angle.force-warp", false);
pref("webgl.dxgl.enabled", true);
pref("webgl.dxgl.needs-finish", false);
#endif

pref("gfx.offscreencanvas.enabled", false);

// sendbuffer of 0 means use OS default, sendbuffer unset means use
// gecko default which varies depending on windows version and is OS
// default on non windows
// pref("network.tcp.sendbuffer", 0);

// TCP Keepalive
pref("network.tcp.keepalive.enabled", true);
// Default idle time before first TCP keepalive probe; same time for interval
// between successful probes. Can be overridden in socket transport API.
// Win, Linux and Mac.
pref("network.tcp.keepalive.idle_time", 600); // seconds; 10 mins
// Default timeout for retransmission of unack'd keepalive probes.
// Win and Linux only; not configurable on Mac.
#if defined(XP_UNIX) && !defined(XP_MACOSX) || defined(XP_WIN)
pref("network.tcp.keepalive.retry_interval", 1); // seconds
#endif
// Default maximum probe retransmissions.
// Linux only; not configurable on Win and Mac; fixed at 10 and 8 respectively.
#if defined(XP_UNIX) && !defined(XP_MACOSX)
pref("network.tcp.keepalive.probe_count", 4);
#endif

#if !defined(EARLY_BETA_OR_EARLIER)
pref("network.tcp.tcp_fastopen_enable", false);
#elif  defined(XP_WIN) || defined(XP_MACOSX)
pref("network.tcp.tcp_fastopen_enable", true);
#else
pref("network.tcp.tcp_fastopen_enable", false);
#endif

pref("network.tcp.tcp_fastopen_consecutive_failure_limit", 5);
// We are trying to detect stalled tcp connections that use TFO and TLS
// (bug 1395494).
// This is only happening if a connection is idle for more than 10s, but we
// will make this a pref. If tcp_fastopen_http_stalls_limit of stalls are
// detected the TCP fast open will be disabled.
// If tcp_fastopen_http_check_for_stalls_only_if_idle_for is set to 0 the
// check will not be performed.
pref("network.tcp.tcp_fastopen_http_check_for_stalls_only_if_idle_for", 10);
pref("network.tcp.tcp_fastopen_http_stalls_limit", 3);
pref("network.tcp.tcp_fastopen_http_stalls_timeout", 20);

// Whether to disable acceleration for all widgets.
pref("layers.acceleration.disabled", false);
// Preference that when switched at runtime will run a series of benchmarks
// and output the result to stderr.
pref("layers.bench.enabled", false);

#if defined(XP_WIN)
pref("layers.gpu-process.enabled", true);
pref("layers.gpu-process.allow-software", true);
#ifdef NIGHTLY_BUILD
pref("layers.gpu-process.max_restarts", 3);
#endif
#endif

// Whether to force acceleration on, ignoring blacklists.
#ifdef ANDROID
// bug 838603 -- on Android, accidentally blacklisting OpenGL layers
// means a startup crash for everyone.
// Temporarily force-enable GL compositing.  This is default-disabled
// deep within the bowels of the widgetry system.  Remove me when GL
// compositing isn't default disabled in widget/android.
pref("layers.acceleration.force-enabled", true);
#else
pref("layers.acceleration.force-enabled", false);
#endif

pref("layers.acceleration.draw-fps", false);

// Enable DEAA antialiasing for transformed layers in the compositor
#if !defined(MOZ_WIDGET_ANDROID)
// Desktop prefs
pref("layers.deaa.enabled", true);
#else
// Mobile prefs
pref("layers.deaa.enabled", false);
#endif

pref("layers.dump", false);
#ifdef MOZ_DUMP_PAINTING
// If we're dumping layers, also dump the texture data
pref("layers.dump-texture", false);
pref("layers.dump-decision", false);
pref("layers.dump-client-layers", false);
pref("layers.dump-host-layers", false);
#endif
pref("layers.draw-borders", false);
pref("layers.draw-tile-borders", false);
pref("layers.draw-bigimage-borders", false);
pref("layers.child-process-shutdown", true);
// Max number of layers per container. See Overwrite in mobile prefs.
pref("layers.max-active", -1);

// Compositor target frame rate. NOTE: If vsync is enabled the compositor
// frame rate will still be capped.
// -1 -> default (match layout.frame_rate or 60 FPS)
// 0  -> full-tilt mode: Recomposite even if not transaction occured.
pref("layers.offmainthreadcomposition.frame-rate", -1);

#if defined(XP_MACOSX) || defined (OS_OPENBSD)
pref("layers.enable-tiles", true);
#else
pref("layers.enable-tiles", false);
#endif
#if defined(XP_WIN)
pref("layers.enable-tiles-if-skia-pomtp", true);
#else
pref("layers.enable-tiles-if-skia-pomtp", false);
#endif
pref("layers.single-tile.enabled", true);
pref("layers.low-precision-buffer", false);
pref("layers.progressive-paint", false);
pref("layers.tiles.retain-back-buffer", true);
// If this is set the tile size will only be treated as a suggestion.
// On B2G we will round this to the stride of the underlying allocation.
// On any platform we may later use the screen size and ignore
// tile-width/tile-height entirely. Its recommended to turn this off
// if you change the tile size.
pref("layers.tiles.adjust", true);
pref("layers.tile-width", 512);
pref("layers.tile-height", 512);
#ifdef MOZ_WIDGET_ANDROID
pref("layers.tiles.edge-padding", true);
#else
pref("layers.tiles.edge-padding", false);
#endif

// Whether to animate simple opacity and transforms on the compositor
pref("layers.offmainthreadcomposition.async-animations", true);

// Whether to log information about off main thread animations to stderr
pref("layers.offmainthreadcomposition.log-animations", false);

pref("layers.bufferrotation.enabled", true);

pref("layers.componentalpha.enabled", true);
pref("layers.draw-mask-debug", false);

pref("gfx.content.always-paint", false);

#ifdef ANDROID
pref("gfx.apitrace.enabled",false);
#endif

#ifdef MOZ_X11
#ifdef MOZ_WIDGET_GTK
pref("gfx.xrender.enabled",false);
pref("widget.chrome.allow-gtk-dark-theme", false);
pref("widget.content.allow-gtk-dark-theme", false);
#endif
#endif

pref("widget.window-transforms.disabled", false);

#ifdef XP_WIN
// Whether to disable the automatic detection and use of direct2d.
pref("gfx.direct2d.disabled", false);

// Whether to attempt to enable Direct2D regardless of automatic detection or
// blacklisting
pref("gfx.direct2d.force-enabled", false);

// Whether to defer destruction of Direct2D DrawTargets to the paint thread
// when using OMTP.
pref("gfx.direct2d.destroy-dt-on-paintthread", true);

pref("gfx.direct3d11.enable-debug-layer", false);
pref("gfx.direct3d11.break-on-error", false);

pref("layers.prefer-opengl", false);
#endif

// Copy-on-write canvas
pref("layers.shared-buffer-provider.enabled", true);

#ifdef XP_WIN
pref("layers.shared-buffer-provider.enabled", false);
#endif

#ifdef XP_MACOSX
// cf. Bug 1324908
pref("layers.shared-buffer-provider.enabled", false);
#endif

// Force all possible layers to be always active layers
pref("layers.force-active", false);

// Enable/Disable the geolocation API for content
pref("geo.enabled", true);

// Timeout for outbound network geolocation provider XHR
pref("geo.wifi.xhr.timeout", 60000);

// Enable/Disable the various sensor APIs for content
pref("device.sensors.enabled", true);
pref("device.sensors.orientation.enabled", true);
pref("device.sensors.motion.enabled", true);
pref("device.sensors.proximity.enabled", false);
pref("device.sensors.ambientLight.enabled", false);

// Enable/Disable the device storage API for content
pref("device.storage.enabled", false);

// Push/Pop/Replace State prefs
pref("browser.history.maxStateObjectSize", 655360);

pref("browser.meta_refresh_when_inactive.disabled", false);

// XPInstall prefs
pref("xpinstall.whitelist.required", true);
// Only Firefox requires add-on signatures
pref("xpinstall.signatures.required", false);
pref("extensions.langpacks.signatures.required", false);
pref("extensions.webExtensionsMinPlatformVersion", "42.0a1");
pref("extensions.legacy.enabled", true);

// Other webextensions prefs
pref("extensions.webextensions.keepStorageOnUninstall", false);
pref("extensions.webextensions.keepUuidOnUninstall", false);
// Redirect basedomain used by identity api
pref("extensions.webextensions.identity.redirectDomain", "extensions.allizom.org");
pref("extensions.webextensions.restrictedDomains", "accounts-static.cdn.mozilla.net,accounts.firefox.com,addons.cdn.mozilla.net,addons.mozilla.org,api.accounts.firefox.com,content.cdn.mozilla.net,content.cdn.mozilla.net,discovery.addons.mozilla.org,input.mozilla.org,install.mozilla.org,oauth.accounts.firefox.com,profile.accounts.firefox.com,support.mozilla.org,sync.services.mozilla.com,testpilot.firefox.com");
// Whether or not webextension themes are supported.
pref("extensions.webextensions.themes.enabled", false);
pref("extensions.webextensions.themes.icons.enabled", false);
pref("extensions.webextensions.remote", false);
// Whether or not the moz-extension resource loads are remoted. For debugging
// purposes only. Setting this to false will break moz-extension URI loading
// unless other process sandboxing and extension remoting prefs are changed.
pref("extensions.webextensions.protocol.remote", true);

// Enable tab hiding API by default.
pref("extensions.webextensions.tabhide.enabled", true);

pref("extensions.webextensions.background-delayed-startup", false);

// Whether or not the installed extensions should be migrated to the storage.local IndexedDB backend.
pref("extensions.webextensions.ExtensionStorageIDB.enabled", false);

// Report Site Issue button
pref("extensions.webcompat-reporter.newIssueEndpoint", "https://webcompat.com/issues/new");
#if defined(MOZ_DEV_EDITION) || defined(NIGHTLY_BUILD)
pref("extensions.webcompat-reporter.enabled", true);
#else
pref("extensions.webcompat-reporter.enabled", false);
#endif

pref("network.buffer.cache.count", 24);
pref("network.buffer.cache.size",  32768);

// Web Notification
pref("dom.webnotifications.enabled", true);
pref("dom.webnotifications.serviceworker.enabled", true);
pref("dom.webnotifications.requireinteraction.count", 3);
#ifdef NIGHTLY_BUILD
pref("dom.webnotifications.requireinteraction.enabled", true);
#else
pref("dom.webnotifications.requireinteraction.enabled", false);
#endif

// Show favicons in web notifications.
pref("alerts.showFavicons", false);

// Whether to use platform-specific backends for showing desktop notifications.
// If no such backend is available, or if the pref is false, then XUL
// notifications are used.
pref("alerts.useSystemBackend", true);

// DOM full-screen API.
pref("full-screen-api.enabled", false);
pref("full-screen-api.allow-trusted-requests-only", true);
// whether to prevent the top level widget from going fullscreen
pref("full-screen-api.ignore-widgets", false);
pref("full-screen-api.pointer-lock.enabled", true);
// transition duration of fade-to-black and fade-from-black, unit: ms
#ifndef MOZ_WIDGET_GTK
pref("full-screen-api.transition-duration.enter", "200 200");
pref("full-screen-api.transition-duration.leave", "200 200");
#else
pref("full-screen-api.transition-duration.enter", "0 0");
pref("full-screen-api.transition-duration.leave", "0 0");
#endif
// timeout for black screen in fullscreen transition, unit: ms
pref("full-screen-api.transition.timeout", 1000);
// time for the warning box stays on the screen before sliding out, unit: ms
pref("full-screen-api.warning.timeout", 3000);
// delay for the warning box to show when pointer stays on the top, unit: ms
pref("full-screen-api.warning.delay", 500);

// DOM pointerlock API
// time for the warning box stays on the screen before sliding out, unit: ms
pref("pointer-lock-api.warning.timeout", 3000);

// Time limit, in milliseconds, for EventStateManager::IsHandlingUserInput().
// Used to detect long running handlers of user-generated events.
pref("dom.event.handling-user-input-time-limit", 1000);

// Whether we should layerize all animated images (if otherwise possible).
pref("layout.animated-image-layers.enabled", false);

pref("dom.vibrator.enabled", true);
pref("dom.vibrator.max_vibrate_ms", 10000);
pref("dom.vibrator.max_vibrate_list_len", 128);

// Battery API
pref("dom.battery.enabled", true);

// Streams API
pref("dom.streams.enabled", false);

// Push

pref("dom.push.enabled", false);
pref("dom.push.alwaysConnect", false);

pref("dom.push.loglevel", "Error");

pref("dom.push.serverURL", "wss://push.services.mozilla.com/");
pref("dom.push.userAgentID", "");

// The maximum number of push messages that a service worker can receive
// without user interaction.
pref("dom.push.maxQuotaPerSubscription", 16);

// The maximum number of recent message IDs to store for each push
// subscription, to avoid duplicates for unacknowledged messages.
pref("dom.push.maxRecentMessageIDsPerSubscription", 10);

// The delay between receiving a push message and updating the quota for a
// subscription.
pref("dom.push.quotaUpdateDelay", 3000); // 3 seconds

// Is the network connection allowed to be up?
// This preference should be used in UX to enable/disable push.
pref("dom.push.connection.enabled", true);

// Exponential back-off start is 5 seconds like in HTTP/1.1.
// Maximum back-off is pingInterval.
pref("dom.push.retryBaseInterval", 5000);

// Interval at which to ping PushServer to check connection status. In
// milliseconds. If no reply is received within requestTimeout, the connection
// is considered closed.
pref("dom.push.pingInterval", 1800000); // 30 minutes

// How long before we timeout
pref("dom.push.requestTimeout", 10000);

// WebPush prefs:
pref("dom.push.http2.reset_retry_count_after_ms", 60000);
pref("dom.push.http2.maxRetries", 2);
pref("dom.push.http2.retryInterval", 5000);

// W3C touch events
// 0 - disabled, 1 - enabled, 2 - autodetect
// Autodetection is currently only supported on Windows and GTK3
#if defined(XP_MACOSX)
pref("dom.w3c_touch_events.enabled", 0);
#else
pref("dom.w3c_touch_events.enabled", 2);
#endif

// W3C draft pointer events
#if !defined(ANDROID)
pref("dom.w3c_pointer_events.enabled", true);
#else
pref("dom.w3c_pointer_events.enabled", false);
#endif

// Control firing WidgetMouseEvent by handling Windows pointer messages or mouse
// messages.
#if defined(XP_WIN)
pref("dom.w3c_pointer_events.dispatch_by_pointer_messages", false);
#endif

// W3C pointer events draft
pref("dom.w3c_pointer_events.implicit_capture", false);

// WHATWG promise rejection events. See
// https://html.spec.whatwg.org/multipage/webappapis.html#promiserejectionevent
// TODO: Enable the event interface once actually firing it (bug 1362272).
pref("dom.promise_rejection_events.enabled", false);

// W3C draft ImageCapture API
pref("dom.imagecapture.enabled", false);

// W3C MediaDevices devicechange event
pref("media.ondevicechange.enabled", true);

// W3C MediaDevices devicechange fake event
pref("media.ondevicechange.fakeDeviceChangeEvent.enabled", false);

// W3C touch-action css property (related to touch and pointer events)
// Note that we turn this on even on platforms/configurations where touch
// events are not supported (e.g. OS X, or Windows with e10s disabled). For
// those platforms we don't handle touch events anyway so it's conceptually
// a no-op.
pref("layout.css.touch_action.enabled", true);

#if defined(MOZ_WIDGET_ANDROID)
// Network Information API
pref("dom.netinfo.enabled", true);
#else
pref("dom.netinfo.enabled", false);
#endif

// How long must we wait before declaring that a window is a "ghost" (i.e., a
// likely leak)?  This should be longer than it usually takes for an eligible
// window to be collected via the GC/CC.
pref("memory.ghost_window_timeout_seconds", 60);

// Don't dump memory reports on OOM, by default.
pref("memory.dump_reports_on_oom", false);

// Number of stack frames to capture in createObjectURL for about:memory.
pref("memory.blob_report.stack_frames", 0);

// Disable idle observer fuzz, because only privileged content can access idle
// observers (bug 780507).
pref("dom.idle-observers-api.fuzz_time.disabled", true);

// Minimum delay in milliseconds between network activity notifications (0 means
// no notifications). The delay is the same for both download and upload, though
// they are handled separately. This pref is only read once at startup:
// a restart is required to enable a new value.
pref("network.activity.intervalMilliseconds", 0);

// If true, reuse the same global for (almost) everything loaded by the component
// loader (JS components, JSMs, etc). This saves memory, but makes it possible
// for the scripts to interfere with each other.  A restart is required for this
// to take effect.
pref("jsloader.shareGlobal", true);

// When we're asked to take a screenshot, don't wait more than 2000ms for the
// event loop to become idle before actually taking the screenshot.
pref("dom.browserElement.maxScreenshotDelayMS", 2000);

// Whether we should show the placeholder when the element is focused but empty.
pref("dom.placeholder.show_on_focus", true);

// WebVR is enabled by default in beta and release for Windows and for all
// platforms in nightly and aurora.
#if defined(XP_WIN) || !defined(RELEASE_OR_BETA)
pref("dom.vr.enabled", true);
#else
pref("dom.vr.enabled", false);
#endif
// It is often desirable to automatically start vr presentation when
// a user puts on the VR headset.  This is done by emitting the
// Window.vrdisplayactivate event when the headset's sensors detect it
// being worn.  This can result in WebVR content taking over the headset
// when the user is using it outside the browser or inadvertent start of
// presentation due to the high sensitivity of the proximity sensor in some
// headsets, so it is off by default.
pref("dom.vr.autoactivate.enabled", false);
// The threshold value of trigger inputs for VR controllers
pref("dom.vr.controller_trigger_threshold", "0.1");
// Enable external XR API integrations
pref("dom.vr.external.enabled", false);
// Maximum number of milliseconds the browser will wait for content to call
// VRDisplay.requestPresent after emitting vrdisplayactivate during VR
// link traversal.  This prevents a long running event handler for
// vrdisplayactivate from later calling VRDisplay.requestPresent, which would
// result in a non-responsive browser in the VR headset.
pref("dom.vr.navigation.timeout", 5000);
// Oculus device
#if defined(HAVE_64BIT_BUILD)
// We are only enabling WebVR by default on 64-bit builds (Bug 1384459)
pref("dom.vr.oculus.enabled", true);
#else
pref("dom.vr.oculus.enabled", false);
#endif
// Minimum number of milliseconds after content has stopped VR presentation
// before the Oculus session is re-initialized to an invisible / tracking
// only mode.  If this value is too high, users will need to wait longer
// after stopping WebVR presentation before automatically returning to the
// Oculus home interface.  (They can immediately return to the Oculus Home
// interface through the Oculus HUD without waiting this duration)
// If this value is too low, the Oculus Home interface may be visible
// momentarily during VR link navigation.
pref("dom.vr.oculus.present.timeout", 500);
// Minimum number of milliseconds that the browser will wait before
// reloading the Oculus OVR library after seeing a "ShouldQuit" flag set.
// Oculus requests that we shut down and unload the OVR library, by setting
// a "ShouldQuit" flag.  To ensure that we don't interfere with
// Oculus software auto-updates, we will not attempt to re-load the
// OVR library until this timeout has elapsed.
pref("dom.vr.oculus.quit.timeout", 10000);
// When enabled, Oculus sessions may be created with the ovrInit_Invisible
// flag if a page is using tracking but not presenting.  When a page
// begins presenting VR frames, the session will be re-initialized without
// the flag.  This eliminates the "Firefox not responding" warnings in
// the headset, but might not be compatible with all versions of the Oculus
// runtime.
pref("dom.vr.oculus.invisible.enabled", true);
// OSVR device
pref("dom.vr.osvr.enabled", false);
// OpenVR device
#if !defined(HAVE_64BIT_BUILD)
// We are only enabling WebVR by default on 64-bit builds (Bug 1384459)
pref("dom.vr.openvr.enabled", false);
#elif defined(XP_WIN) || defined(XP_MACOSX)
// We enable OpenVR by default for Windows and macOS
pref("dom.vr.openvr.enabled", true);
#else
// See Bug 1310663 (Linux)
pref("dom.vr.openvr.enabled", false);
#endif
// Minimum number of milliseconds that the browser will wait before
// attempting to poll again for connected VR controllers.  The browser
// will not attempt to poll for VR controllers until it needs to use them.
pref("dom.vr.controller.enumerate.interval", 1000);
// Minimum number of milliseconds that the browser will wait before
// attempting to poll again for connected VR displays.  The browser
// will not attempt to poll for VR displays until it needs to use
// them, such as when detecting a WebVR site.
pref("dom.vr.display.enumerate.interval", 5000);
// Minimum number of milliseconds that the VR session will be kept
// alive after the browser and content no longer are using the
// hardware.  If a VR multitasking environment, this should be set
// very low or set to 0.
pref("dom.vr.inactive.timeout", 5000);
// Pose prediction reduces latency effects by returning future predicted HMD
// poses to callers of the WebVR API.  This currently only has an effect for
// Oculus Rift on SDK 0.8 or greater.
pref("dom.vr.poseprediction.enabled", true);
// Starting VR presentation is only allowed within a user gesture or event such
// as VRDisplayActivate triggered by the system.  dom.vr.require-gesture allows
// this requirement to be disabled for special cases such as during automated
// tests or in a headless kiosk system.
pref("dom.vr.require-gesture", true);
// path to OSVR DLLs
pref("gfx.vr.osvr.utilLibPath", "");
pref("gfx.vr.osvr.commonLibPath", "");
pref("gfx.vr.osvr.clientLibPath", "");
pref("gfx.vr.osvr.clientKitLibPath", "");
// Puppet device, used for simulating VR hardware within tests and dev tools
pref("dom.vr.puppet.enabled", false);
// Allow displaying the result of vr submitframe (0: disable, 1: store the
// result as a base64 image, 2: show it on the screen).
pref("dom.vr.puppet.submitframe", 0);
// The number of milliseconds since last frame start before triggering a new frame.
// When content is failing to submit frames on time or the lower level VR platform API's
// are rejecting frames, it determines the rate at which RAF callbacks will be called.
pref("dom.vr.display.rafMaxDuration", 50);
// VR test system.
pref("dom.vr.test.enabled", false);

// If the user puts a finger down on an element and we think the user
// might be executing a pan gesture, how long do we wait before
// tentatively deciding the gesture is actually a tap and activating
// the target element?
pref("ui.touch_activation.delay_ms", 100);

// If the user has clicked an element, how long do we keep the
// :active state before it is cleared by the mouse sequences
// fired after a touchstart/touchend.
pref("ui.touch_activation.duration_ms", 10);

// nsMemoryInfoDumper can watch a fifo in the temp directory and take various
// actions when the fifo is written to.  Disable this in general.
pref("memory_info_dumper.watch_fifo.enabled", false);

// If minInterval is 0, the check will only happen
// when the service has a strong suspicion we are in a captive portal
pref("network.captive-portal-service.minInterval", 60000); // 60 seconds
pref("network.captive-portal-service.maxInterval", 1500000); // 25 minutes
// Every 10 checks, the delay is increased by a factor of 5
pref("network.captive-portal-service.backoffFactor", "5.0");
pref("network.captive-portal-service.enabled", false);

// DNS Trusted Recursive Resolver
// 0 - default off, 1 - race, 2 TRR first, 3 TRR only, 4 shadow, 5 off by choice
pref("network.trr.mode", 0);
// DNS-over-HTTP service to use, must be HTTPS://
pref("network.trr.uri", "");
// credentials to pass to DOH end-point
pref("network.trr.credentials", "");
// Wait for captive portal confirmation before enabling TRR
#if defined(ANDROID)
// On Android, the captive portal is handled by the OS itself
pref("network.trr.wait-for-portal", false);
#else
pref("network.trr.wait-for-portal", true);
#endif
// Allow RFC1918 address in responses?
pref("network.trr.allow-rfc1918", false);
// Use GET (rather than POST)
pref("network.trr.useGET", false);
// Before TRR is widely used the NS record for this host is fetched
// from the DOH end point to ensure proper configuration
pref("network.trr.confirmationNS", "example.com");
// hardcode the resolution of the hostname in network.trr.uri instead of
// relying on the system resolver to do it for you
pref("network.trr.bootstrapAddress", "");
// TRR blacklist entry expire time (in seconds). Default is 20 minutes.
pref("network.trr.blacklist-duration", 1200);
// Single TRR request timeout, in milliseconds
pref("network.trr.request-timeout", 3000);
// Allow AAAA entries to be used "early", before the A results are in
pref("network.trr.early-AAAA", false);

pref("captivedetect.canonicalURL", "http://detectportal.firefox.com/success.txt");
pref("captivedetect.canonicalContent", "success\n");
pref("captivedetect.maxWaitingTime", 5000);
pref("captivedetect.pollingTime", 3000);
pref("captivedetect.maxRetryCount", 5);

#ifdef RELEASE_OR_BETA
pref("dom.forms.inputmode", false);
#else
pref("dom.forms.inputmode", true);
#endif

// Enable mapped array buffer by default.
pref("dom.mapped_arraybuffer.enabled", true);

// The tables used for Safebrowsing phishing and malware checks
pref("urlclassifier.malwareTable", "goog-malware-proto,goog-unwanted-proto,test-harmful-simple,test-malware-simple,test-unwanted-simple");
#ifdef MOZILLA_OFFICIAL
// In official builds, we are allowed to use Google's private phishing
// list (see bug 1288840).
pref("urlclassifier.phishTable", "goog-phish-proto,test-phish-simple");
#else
pref("urlclassifier.phishTable", "googpub-phish-proto,test-phish-simple");
#endif

// Tables for application reputation
pref("urlclassifier.downloadAllowTable", "goog-downloadwhite-proto");
pref("urlclassifier.downloadBlockTable", "goog-badbinurl-proto");

// Tables for login reputation
pref("urlclassifier.passwordAllowTable", "goog-passwordwhite-proto");

// Tables for tracking protection
pref("urlclassifier.trackingTable", "test-track-simple,base-track-digest256");
pref("urlclassifier.trackingWhitelistTable", "test-trackwhite-simple,mozstd-trackwhite-digest256");

// These tables will never trigger a gethash call.
pref("urlclassifier.disallow_completions", "test-malware-simple,test-harmful-simple,test-phish-simple,test-unwanted-simple,test-track-simple,test-trackwhite-simple,test-block-simple,goog-downloadwhite-digest256,base-track-digest256,mozstd-trackwhite-digest256,content-track-digest256,mozplugin-block-digest256,mozplugin2-block-digest256,block-flash-digest256,except-flash-digest256,allow-flashallow-digest256,except-flashallow-digest256,block-flashsubdoc-digest256,except-flashsubdoc-digest256,except-flashinfobar-digest256,goog-passwordwhite-proto,ads-track-digest256,social-track-digest256,analytics-track-digest256");

// Number of random entries to send with a gethash request
pref("urlclassifier.gethashnoise", 4);

// Gethash timeout for Safe Browsing
pref("urlclassifier.gethash.timeout_ms", 5000);
// Update server response timeout for Safe Browsing
pref("urlclassifier.update.response_timeout_ms", 30000);
// Download update timeout for Safe Browsing
pref("urlclassifier.update.timeout_ms", 90000);

// Name of the about: page to display Safe Browsing warnings (bug 399233)
pref("urlclassifier.alternate_error_page", "blocked");

// Enable phishing & malware protection.
pref("browser.safebrowsing.phishing.enabled", true);
pref("browser.safebrowsing.malware.enabled", true);
pref("browser.safebrowsing.debug", false);

// Allow users to ignore Safe Browsing warnings.
pref("browser.safebrowsing.allowOverride", true);

// These names are approved by the Google Safe Browsing team.
// Any changes must be coordinated with them.
#ifdef MOZILLA_OFFICIAL
pref("browser.safebrowsing.id", "navclient-auto-ffox");
#else
pref("browser.safebrowsing.id", "Firefox");
#endif

// Download protection
pref("browser.safebrowsing.downloads.enabled", true);
pref("browser.safebrowsing.downloads.remote.enabled", true);
pref("browser.safebrowsing.downloads.remote.timeout_ms", 10000);
pref("browser.safebrowsing.downloads.remote.url", "https://sb-ssl.google.com/safebrowsing/clientreport/download?key=%GOOGLE_API_KEY%");
pref("browser.safebrowsing.downloads.remote.block_dangerous",            true);
pref("browser.safebrowsing.downloads.remote.block_dangerous_host",       true);
pref("browser.safebrowsing.downloads.remote.block_potentially_unwanted", true);
pref("browser.safebrowsing.downloads.remote.block_uncommon",             true);

// Password protection
pref("browser.safebrowsing.passwords.enabled", false);

// Google Safe Browsing provider (legacy)
pref("browser.safebrowsing.provider.google.pver", "2.2");
pref("browser.safebrowsing.provider.google.lists", "goog-badbinurl-shavar,goog-downloadwhite-digest256,goog-phish-shavar,googpub-phish-shavar,goog-malware-shavar,goog-unwanted-shavar");
pref("browser.safebrowsing.provider.google.updateURL", "https://safebrowsing.google.com/safebrowsing/downloads?client=SAFEBROWSING_ID&appver=%MAJOR_VERSION%&pver=2.2&key=%GOOGLE_API_KEY%");
pref("browser.safebrowsing.provider.google.gethashURL", "https://safebrowsing.google.com/safebrowsing/gethash?client=SAFEBROWSING_ID&appver=%MAJOR_VERSION%&pver=2.2");
pref("browser.safebrowsing.provider.google.reportURL", "https://safebrowsing.google.com/safebrowsing/diagnostic?client=%NAME%&hl=%LOCALE%&site=");
pref("browser.safebrowsing.provider.google.reportPhishMistakeURL", "https://%LOCALE%.phish-error.mozilla.com/?hl=%LOCALE%&url=");
pref("browser.safebrowsing.provider.google.reportMalwareMistakeURL", "https://%LOCALE%.malware-error.mozilla.com/?hl=%LOCALE%&url=");
pref("browser.safebrowsing.provider.google.advisoryURL", "https://developers.google.com/safe-browsing/v4/advisory");
pref("browser.safebrowsing.provider.google.advisoryName", "Google Safe Browsing");

// Google Safe Browsing provider
pref("browser.safebrowsing.provider.google4.pver", "4");
pref("browser.safebrowsing.provider.google4.lists", "goog-badbinurl-proto,goog-downloadwhite-proto,goog-phish-proto,googpub-phish-proto,goog-malware-proto,goog-unwanted-proto,goog-harmful-proto,goog-passwordwhite-proto");
pref("browser.safebrowsing.provider.google4.updateURL", "https://safebrowsing.googleapis.com/v4/threatListUpdates:fetch?$ct=application/x-protobuf&key=%GOOGLE_API_KEY%&$httpMethod=POST");
pref("browser.safebrowsing.provider.google4.gethashURL", "https://safebrowsing.googleapis.com/v4/fullHashes:find?$ct=application/x-protobuf&key=%GOOGLE_API_KEY%&$httpMethod=POST");
pref("browser.safebrowsing.provider.google4.reportURL", "https://safebrowsing.google.com/safebrowsing/diagnostic?client=%NAME%&hl=%LOCALE%&site=");
pref("browser.safebrowsing.provider.google4.reportPhishMistakeURL", "https://%LOCALE%.phish-error.mozilla.com/?hl=%LOCALE%&url=");
pref("browser.safebrowsing.provider.google4.reportMalwareMistakeURL", "https://%LOCALE%.malware-error.mozilla.com/?hl=%LOCALE%&url=");
pref("browser.safebrowsing.provider.google4.advisoryURL", "https://developers.google.com/safe-browsing/v4/advisory");
pref("browser.safebrowsing.provider.google4.advisoryName", "Google Safe Browsing");
pref("browser.safebrowsing.provider.google4.dataSharingURL", "https://safebrowsing.googleapis.com/v4/threatHits?$ct=application/x-protobuf&key=%GOOGLE_API_KEY%&$httpMethod=POST");
pref("browser.safebrowsing.provider.google4.dataSharing.enabled", false);

pref("browser.safebrowsing.reportPhishURL", "https://%LOCALE%.phish-report.mozilla.com/?hl=%LOCALE%&url=");

// Mozilla Safe Browsing provider (for tracking protection and plugin blocking)
pref("browser.safebrowsing.provider.mozilla.pver", "2.2");
pref("browser.safebrowsing.provider.mozilla.lists", "base-track-digest256,mozstd-trackwhite-digest256,content-track-digest256,mozplugin-block-digest256,mozplugin2-block-digest256,block-flash-digest256,except-flash-digest256,allow-flashallow-digest256,except-flashallow-digest256,block-flashsubdoc-digest256,except-flashsubdoc-digest256,except-flashinfobar-digest256,ads-track-digest256,social-track-digest256,analytics-track-digest256");
pref("browser.safebrowsing.provider.mozilla.updateURL", "https://shavar.services.mozilla.com/downloads?client=SAFEBROWSING_ID&appver=%MAJOR_VERSION%&pver=2.2");
pref("browser.safebrowsing.provider.mozilla.gethashURL", "https://shavar.services.mozilla.com/gethash?client=SAFEBROWSING_ID&appver=%MAJOR_VERSION%&pver=2.2");
// Set to a date in the past to force immediate download in new profiles.
pref("browser.safebrowsing.provider.mozilla.nextupdatetime", "1");
// Block lists for tracking protection. The name values will be used as the keys
// to lookup the localized name in preferences.properties.
pref("browser.safebrowsing.provider.mozilla.lists.base", "moz-std");
pref("browser.safebrowsing.provider.mozilla.lists.content", "moz-full");

// The table and global pref for blocking plugin content
pref("browser.safebrowsing.blockedURIs.enabled", true);
pref("urlclassifier.blockedTable", "test-block-simple,mozplugin-block-digest256");

// Flash blocking tables
pref("urlclassifier.flashAllowTable", "allow-flashallow-digest256");
pref("urlclassifier.flashAllowExceptTable", "except-flashallow-digest256");
pref("urlclassifier.flashTable", "block-flash-digest256");
pref("urlclassifier.flashExceptTable", "except-flash-digest256");
pref("urlclassifier.flashSubDocTable", "block-flashsubdoc-digest256");
pref("urlclassifier.flashSubDocExceptTable", "except-flashsubdoc-digest256");
pref("urlclassifier.flashInfobarTable", "except-flashinfobar-digest256");

pref("plugins.http_https_only", true);
pref("plugins.flashBlock.enabled", false);

// Turn off Spatial navigation by default.
pref("snav.enabled", false);

// Debug-only pref to force enable the AccessibleCaret. If you want to
// control AccessibleCaret by mouse, you'll need to set
// "layout.accessiblecaret.hide_carets_for_mouse_input" to false.
pref("layout.accessiblecaret.enabled", false);

// Enable the accessible caret on platforms/devices
// that we detect have touch support. Note that this pref is an
// additional way to enable the accessible carets, rather than
// overriding the layout.accessiblecaret.enabled pref.
pref("layout.accessiblecaret.enabled_on_touch", true);

// CSS attributes of the AccessibleCaret in CSS pixels.
pref("layout.accessiblecaret.width", "34.0");
pref("layout.accessiblecaret.height", "36.0");
pref("layout.accessiblecaret.margin-left", "-18.5");
pref("layout.accessiblecaret.bar.width", "2.0");

// Show no selection bars at the two ends of the selection highlight.
pref("layout.accessiblecaret.bar.enabled", false);

// Show the caret when long tapping on an empty content.
pref("layout.accessiblecaret.caret_shown_when_long_tapping_on_empty_content", false);

// Simulate long tap to select words on the platforms where APZ is not enabled
// or long tap events does not fired by APZ.
pref("layout.accessiblecaret.use_long_tap_injector", false);

// By default, carets become tilt only when they are overlapping.
pref("layout.accessiblecaret.always_tilt", false);

// 0 = by default, always hide carets for selection changes due to JS calls.
// 1 = update any visible carets for selection changes due to JS calls,
//     but don't show carets if carets are hidden.
// 2 = always show carets for selection changes due to JS calls.
pref("layout.accessiblecaret.script_change_update_mode", 0);

// Allow one caret to be dragged across the other caret without any limitation.
// This matches the built-in convention for all desktop platforms.
pref("layout.accessiblecaret.allow_dragging_across_other_caret", true);

// Optionally provide haptic feedback on longPress selection events.
pref("layout.accessiblecaret.hapticfeedback", false);

// Smart phone-number selection on long-press is not enabled by default.
pref("layout.accessiblecaret.extend_selection_for_phone_number", false);

// Keep the accessible carets hidden when the user is using mouse input.
pref("layout.accessiblecaret.hide_carets_for_mouse_input", true);

// Wakelock is disabled by default.
pref("dom.wakelock.enabled", false);

// The URL of the Firefox Accounts auth server backend
pref("identity.fxaccounts.auth.uri", "https://api.accounts.firefox.com/v1");

pref("beacon.enabled", true);

// SW Cache API
pref("dom.caches.enabled", true);

// UDPSocket API
pref("dom.udpsocket.enabled", false);

// Presentation API
pref("dom.presentation.enabled", false);
pref("dom.presentation.controller.enabled", false);
pref("dom.presentation.receiver.enabled", false);

// Presentation Device
pref("dom.presentation.tcp_server.debug", false);
pref("dom.presentation.discovery.enabled", false);
pref("dom.presentation.discovery.timeout_ms", 10000);
pref("dom.presentation.discoverable", false);
pref("dom.presentation.discoverable.encrypted", true);
pref("dom.presentation.discoverable.retry_ms", 5000);
pref("dom.presentation.session_transport.data_channel.enable", false);

#ifdef XP_MACOSX
#if !defined(RELEASE_OR_BETA) || defined(DEBUG)
// In non-release builds we crash by default on insecure text input (when a
// password editor has focus but secure event input isn't enabled).  The
// following pref, when turned on, disables this behavior.  See bug 1188425.
pref("intl.allow-insecure-text-input", false);
#endif
#endif // XP_MACOSX

// Enable meta-viewport support in remote APZ-enabled frames.
pref("dom.meta-viewport.enabled", false);

// Search service settings
pref("browser.search.log", false);
pref("browser.search.update", true);
pref("browser.search.update.log", false);
pref("browser.search.update.interval", 21600);
pref("browser.search.suggest.enabled", true);
pref("browser.search.reset.enabled", false);
pref("browser.search.reset.whitelist", "");
pref("browser.search.geoSpecificDefaults", false);
pref("browser.search.geoip.url", "https://location.services.mozilla.com/v1/country?key=%MOZILLA_API_KEY%");
pref("browser.search.geoip.timeout", 3000);

#ifdef MOZ_OFFICIAL_BRANDING
// {moz:official} expands to "official"
pref("browser.search.official", true);
#endif

// GMPInstallManager prefs

// User-settable override to media.gmp-manager.url for testing purposes.
//pref("media.gmp-manager.url.override", "");

// Update service URL for GMP install/updates:
pref("media.gmp-manager.url", "https://aus5.mozilla.org/update/3/GMP/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/update.xml");

// When |media.gmp-manager.cert.requireBuiltIn| is true or not specified the
// final certificate and all certificates the connection is redirected to before
// the final certificate for the url specified in the |media.gmp-manager.url|
// preference must be built-in.
pref("media.gmp-manager.cert.requireBuiltIn", true);

// The |media.gmp-manager.certs.| preference branch contains branches that are
// sequentially numbered starting at 1 that contain attribute name / value
// pairs for the certificate used by the server that hosts the update xml file
// as specified in the |media.gmp-manager.url| preference. When these preferences are
// present the following conditions apply for a successful update check:
// 1. the uri scheme must be https
// 2. the preference name must exist as an attribute name on the certificate and
//    the value for the name must be the same as the value for the attribute name
//    on the certificate.
// If these conditions aren't met it will be treated the same as when there is
// no update available. This validation will not be performed when the
// |media.gmp-manager.url.override| user preference has been set for testing updates or
// when the |media.gmp-manager.cert.checkAttributes| preference is set to false. Also,
// the |media.gmp-manager.url.override| preference should ONLY be used for testing.
// IMPORTANT! app.update.certs.* prefs should also be updated if these
// are updated.
pref("media.gmp-manager.cert.checkAttributes", true);
pref("media.gmp-manager.certs.1.issuerName", "CN=DigiCert SHA2 Secure Server CA,O=DigiCert Inc,C=US");
pref("media.gmp-manager.certs.1.commonName", "aus5.mozilla.org");
pref("media.gmp-manager.certs.2.issuerName", "CN=thawte SSL CA - G2,O=\"thawte, Inc.\",C=US");
pref("media.gmp-manager.certs.2.commonName", "aus5.mozilla.org");

// Whether or not to perform reader mode article parsing on page load.
// If this pref is disabled, we will never show a reader mode icon in the toolbar.
pref("reader.parse-on-load.enabled", true);

// After what size document we don't bother running Readability on it
// because it'd slow things down too much
pref("reader.parse-node-limit", 3000);

// Force-enables reader mode parsing, even on low-memory platforms, where it
// is disabled by default.
pref("reader.parse-on-load.force-enabled", false);

// Whether we include full URLs in browser console errors. This is disabled
// by default because some platforms will persist these, leading to privacy issues.
pref("reader.errors.includeURLs", false);

// The default relative font size in reader mode (1-9)
pref("reader.font_size", 5);

// The default relative content width in reader mode (1-9)
pref("reader.content_width", 3);

// The default relative line height in reader mode (1-9)
pref("reader.line_height", 4);

// The default color scheme in reader mode (light, dark, sepia, auto)
// auto = color automatically adjusts according to ambient light level
// (auto only works on platforms where the 'devicelight' event is enabled)
pref("reader.color_scheme", "light");

// Color scheme values available in reader mode UI.
pref("reader.color_scheme.values", "[\"light\",\"dark\",\"sepia\"]");

// The font type in reader (sans-serif, serif)
pref("reader.font_type", "sans-serif");

// Whether or not the user has interacted with the reader mode toolbar.
// This is used to show a first-launch tip in reader mode.
pref("reader.has_used_toolbar", false);

// Whether to use a vertical or horizontal toolbar.
pref("reader.toolbar.vertical", true);

#if !defined(ANDROID)
pref("narrate.enabled", true);
#else
pref("narrate.enabled", false);
#endif

pref("narrate.test", false);
pref("narrate.rate", 0);
pref("narrate.voice", " { \"default\": \"automatic\" }");
// Only make voices that match content language available.
pref("narrate.filter-voices", true);

// HTML <dialog> element
pref("dom.dialog_element.enabled", false);

// Allow control characters appear in composition string.
// When this is false, control characters except
// CHARACTER TABULATION (horizontal tab) are removed from
// both composition string and data attribute of compositionupdate
// and compositionend events.
pref("dom.compositionevent.allow_control_characters", false);

pref("memory.report_concurrency", 10);

// Add Mozilla AudioChannel APIs.
pref("media.useAudioChannelAPI", false);

pref("toolkit.pageThumbs.screenSizeDivisor", 7);
pref("toolkit.pageThumbs.minWidth", 0);
pref("toolkit.pageThumbs.minHeight", 0);

pref("webextensions.tests", false);

// 16MB default non-parseable upload limit for requestBody.raw.bytes
pref("webextensions.webRequest.requestBodyMaxRawBytes", 16777216);

pref("webextensions.storage.sync.enabled", true);
pref("webextensions.storage.sync.serverURL", "https://webextensions.settings.services.mozilla.com/v1");

// Allow customization of the fallback directory for file uploads
pref("dom.input.fallbackUploadDir", "");

// Turn rewriting of youtube embeds on/off
pref("plugins.rewrite_youtube_embeds", true);

// Disable browser frames by default
pref("dom.mozBrowserFramesEnabled", false);

// Is support for 'color-adjust' CSS property enabled?
pref("layout.css.color-adjust.enabled", true);

pref("dom.audiochannel.audioCompeting", false);
pref("dom.audiochannel.audioCompeting.allAgents", false);

// Default media volume
pref("media.default_volume", "1.0");

pref("media.seekToNextFrame.enabled", true);

// return the maximum number of cores that navigator.hardwareCurrency returns
pref("dom.maxHardwareConcurrency", 16);

// Shutdown the osfile worker if its no longer needed.
#if !defined(RELEASE_OR_BETA)
pref("osfile.reset_worker_delay", 30000);
#endif

#if !defined(MOZ_WIDGET_ANDROID)
pref("dom.webkitBlink.dirPicker.enabled", true);
pref("dom.webkitBlink.filesystem.enabled", true);
#endif

pref("media.block-autoplay-until-in-foreground", true);

// TODO: Bug 1324406: Treat 'data:' documents as unique, opaque origins
// If true, data: URIs will be treated as unique opaque origins, hence will use
// a NullPrincipal as the security context.
// Otherwise it will inherit the origin from parent node, this is the legacy
// behavior of Firefox.
pref("security.data_uri.unique_opaque_origin", true);

// If true, all toplevel data: URI navigations will be blocked.
// Please note that manually entering a data: URI in the
// URL-Bar will not be blocked when flipping this pref.
pref("security.data_uri.block_toplevel_data_uri_navigations", true);

// If true, all FTP subresource loads will be blocked.
pref("security.block_ftp_subresources", true);

// Enable Storage API for all platforms except Android.
#if !defined(MOZ_WIDGET_ANDROID)
pref("dom.storageManager.enabled", true);
#else
pref("dom.storageManager.enabled", false);
#endif
pref("dom.storageManager.prompt.testing", false);
pref("dom.storageManager.prompt.testing.allow", false);


pref("browser.storageManager.pressureNotification.minIntervalMS", 1200000);
pref("browser.storageManager.pressureNotification.usageThresholdGB", 5);

// When a user cancels this number of authentication dialogs coming from
// a single web page in a row, all following authentication dialogs will
// be blocked (automatically canceled) for that page. The counter resets
// when the page is reloaded. To turn this feature off, just set the limit to 0.
pref("prompts.authentication_dialog_abuse_limit", 3);

pref("dom.IntersectionObserver.enabled", true);

// Whether module scripts (<script type="module">) are enabled for content.
pref("dom.moduleScripts.enabled", true);

// Maximum amount of time in milliseconds consecutive setTimeout()/setInterval()
// callback are allowed to run before yielding the event loop.
pref("dom.timeout.max_consecutive_callbacks_ms", 4);

// Use this preference to house "Payment Request API" during development
pref("dom.payments.request.enabled", false);
pref("dom.payments.loglevel", "Warn");

#ifdef FUZZING
pref("fuzzing.enabled", false);
#endif

#ifdef MOZ_ASAN_REPORTER
pref("asanreporter.apiurl", "https://anf1.fuzzing.mozilla.org/crashproxy/submit/");
pref("asanreporter.clientid", "unknown");
#endif

#if defined(XP_WIN)
pref("layers.mlgpu.enabled", true);

// Both this and the master "enabled" pref must be on to use Advanced Layers
// on Windows 7.
pref("layers.mlgpu.enable-on-windows7", true);
#endif

// Enable lowercased response header name
pref("dom.xhr.lowercase_header.enabled", false);

// Control whether clients.openWindow() opens windows in the same process
// that called the API vs following our normal multi-process selection
// algorithm.  Restricting openWindow to same process improves service worker
// web compat in the short term.  Once the SW multi-e10s refactor is complete
// this can be removed.
pref("dom.clients.openwindow_favors_same_process", true);

// When a crash happens, whether to include heap regions of the crash context
// in the minidump. Enabled by default on nightly and aurora.
#ifdef RELEASE_OR_BETA
pref("toolkit.crashreporter.include_context_heap", false);
#else
pref("toolkit.crashreporter.include_context_heap", true);
#endif

// Open noopener links in a new process
pref("dom.noopener.newprocess.enabled", true);

#if defined(XP_WIN) || defined(XP_MACOSX)
pref("layers.omtp.enabled", true);
#else
pref("layers.omtp.enabled", false);
#endif
pref("layers.omtp.paint-workers", -1);
pref("layers.omtp.release-capture-on-main-thread", false);
pref("layers.omtp.dump-capture", false);

// Limits the depth of recursive conversion of data when opening
// a content to view.  This is mostly intended to prevent infinite
// loops with faulty converters involved.
pref("general.document_open_conversion_depth_limit", 20);

// If true, touchstart and touchmove listeners on window, document,
// documentElement and document.body are passive by default.
pref("dom.event.default_to_passive_touch_listeners", true);
