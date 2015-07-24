/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* The prefs in this file are shipped with the GRE and should apply to all
 * embedding situations. Application-specific preferences belong somewhere else,
 * for example xpfe/bootstrap/browser-prefs.js
 *
 * Platform-specific #ifdefs at the end of this file override the generic
 * entries at the top.
 */

/*
 * SYNTAX HINTS:
 *
 *  - Dashes are delimiters; use underscores instead.
 *  - The first character after a period must be alphabetic.
 *  - Computed values (e.g. 50 * 1024) don't work.
 */

pref("keyword.enabled", false);
pref("general.useragent.locale", "chrome://global/locale/intl.properties");
pref("general.useragent.compatMode.firefox", false);

// This pref exists only for testing purposes. In order to disable all
// overrides by default, don't initialize UserAgentOverrides.jsm.
pref("general.useragent.site_specific_overrides", true);

pref("general.config.obscure_value", 13); // for MCD .cfg files

pref("general.warnOnAboutConfig", true);

// maximum number of dated backups to keep at any time
pref("browser.bookmarks.max_backups",       5);

// Delete HTTP cache v1 data
pref("browser.cache.auto_delete_cache_version", 0);
// Preference for switching the cache backend, can be changed freely at runtime
// 0 - use the old (Darin's) cache
// 1 - use the new cache back-end (cache v2)
pref("browser.cache.use_new_backend",       0);
pref("browser.cache.use_new_backend_temp",  true);

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
pref("browser.cache.disk.max_chunks_memory_usage", 10240);
pref("browser.cache.disk.max_priority_chunks_memory_usage", 10240);

pref("browser.cache.disk_cache_ssl",        true);
// 0 = once-per-session, 1 = each-time, 2 = never, 3 = when-appropriate/automatically
pref("browser.cache.check_doc_frequency",   3);
// Limit of recent metadata we keep in memory for faster access, in Kb
pref("browser.cache.disk.metadata_memory_limit", 250); // 0.25 MB
// The number of chunks we preload ahead of read.  One chunk has currently 256kB.
pref("browser.cache.disk.preload_chunk_count", 4); // 1 MB of read ahead
// The half life used to re-compute cache entries frecency in hours.
pref("browser.cache.frecency_half_life_hours", 6);

pref("browser.cache.offline.enable",           true);
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

// Whether or not MozAbortablePromise is enabled.
pref("dom.abortablepromise.enabled", false);

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

// Whether or not the Permissions API is enabled.
#ifdef NIGHTLY_BUILD
pref("dom.permissions.enabled", true);
#else
pref("dom.permissions.enabled", false);
#endif

// Whether or not Web Workers are enabled.
pref("dom.workers.enabled", true);
// The number of workers per domain allowed to run concurrently.
pref("dom.workers.maxPerDomain", 20);

// Whether or not Shared Web Workers are enabled.
pref("dom.workers.sharedWorkers.enabled", true);

// Service workers
pref("dom.serviceWorkers.enabled", false);

// Allow service workers to intercept network requests using the fetch event
pref("dom.serviceWorkers.interception.enabled", false);

// Allow service workers to intercept opaque (cross origin) responses
pref("dom.serviceWorkers.interception.opaque.enabled", false);

// Whether nonzero values can be returned from performance.timing.*
pref("dom.enable_performance", true);

// Whether resource timing will be gathered and returned by performance.GetEntries*
pref("dom.enable_resource_timing", true);

// Enable high-resolution timing markers for users
pref("dom.enable_user_timing", true);

// Enable printing performance marks/measures to log
pref("dom.performance.enable_user_timing_logging", false);

// Whether the Gamepad API is enabled
pref("dom.gamepad.enabled", true);
#ifdef RELEASE_BUILD
pref("dom.gamepad.non_standard_events.enabled", false);
#else
pref("dom.gamepad.non_standard_events.enabled", true);
#endif

// Whether the KeyboardEvent.code is enabled
pref("dom.keyboardevent.code.enabled", true);

// If this is true, TextEventDispatcher dispatches keydown and keyup events
// even during composition (keypress events are never fired during composition
// even if this is true).
pref("dom.keyboardevent.dispatch_during_composition", false);

// Whether the UndoManager API is enabled
pref("dom.undo_manager.enabled", false);

// Whether URL,nsLocation,Link::GetHash should be percent encoded
// in setter and percent decoded in getter (old behaviour = true)
pref("dom.url.encode_decode_hash", true);
// Whether ::GetHash should do percent decoding (old behaviour = true)
pref("dom.url.getters_decode_hash", false);

// Whether to run add-on code in different compartments from browser code. This
// causes a separate compartment for each (addon, global) combination, which may
// significantly increase the number of compartments in the system.
#ifdef E10S_TESTING_ONLY
pref("dom.compartment_per_addon", true);
#else
pref("dom.compartment_per_addon", false);
#endif

// Fastback caching - if this pref is negative, then we calculate the number
// of content viewers to cache based on the amount of available memory.
pref("browser.sessionhistory.max_total_viewers", -1);

pref("ui.use_native_colors", true);
pref("ui.click_hold_context_menus", false);
// Duration of timeout of incremental search in menus (ms).  0 means infinite.
pref("ui.menu.incremental_search.timeout", 1000);
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

pref("browser.helperApps.alwaysAsk.force",  false);
pref("browser.helperApps.neverAsk.saveToDisk", "");
pref("browser.helperApps.neverAsk.openFile", "");
pref("browser.helperApps.deleteTempFileOnExit", false);

// xxxbsmedberg: where should prefs for the toolkit go?
pref("browser.chrome.toolbar_tips",         true);
// 0 = Pictures Only, 1 = Text Only, 2 = Pictures and Text
pref("browser.chrome.toolbar_style",        2);
// max image size for which it is placed in the tab icon for tabbrowser.
// if 0, no images are used for tab icons for image documents.
pref("browser.chrome.image_icons.max_size", 1024);

pref("browser.triple_click_selects_paragraph", true);

// Print/Preview Shrink-To-Fit won't shrink below 20% for text-ish documents.
pref("print.shrink-to-fit.scale-limit-percent", 20);

// Enable scale transform for stretchy MathML operators. See bug 414277.
pref("mathml.scale_stretchy_operators.enabled", true);

// Media cache size in kilobytes
pref("media.cache_size", 512000);
// When a network connection is suspended, don't resume it until the
// amount of buffered data falls below this threshold (in seconds).
pref("media.cache_resume_threshold", 999999);
// Stop reading ahead when our buffered data is this many seconds ahead
// of the current playback position. This limit can stop us from using arbitrary
// amounts of network bandwidth prefetching huge videos.
pref("media.cache_readahead_limit", 999999);

// Master HTML5 media volume scale.
pref("media.volume_scale", "1.0");

// Timeout for wakelock release
pref("media.wakelock_timeout", 2000);

// Whether we should play videos opened in a "video document", i.e. videos
// opened as top-level documents, as opposed to inside a media element.
pref("media.play-stand-alone", true);

// Whether we should delay actioning a "play()" JS function call and autoplay
// attribute until the media element's owner document is visible.
pref("media.block-play-until-visible", false);

pref("media.hardware-video-decoding.enabled", true);

pref("media.decoder.heuristic.dormant.enabled", true);
pref("media.decoder.heuristic.dormant.timeout", 60000);

#ifdef MOZ_WMF
pref("media.wmf.decoder.thread-count", -1);
#endif
#ifdef MOZ_DIRECTSHOW
pref("media.directshow.enabled", true);
#endif
#ifdef MOZ_FMP4
pref("media.fragmented-mp4.enabled", true);
pref("media.fragmented-mp4.ffmpeg.enabled", false);
pref("media.fragmented-mp4.gmp.enabled", false);
#if defined(XP_WIN) && defined(MOZ_WMF) || defined(XP_MACOSX) || defined(MOZ_WIDGET_GONK)
// Denotes that the fragmented MP4 parser can be created by <video> elements.
pref("media.fragmented-mp4.exposed", true);
#else
pref("media.fragmented-mp4.exposed", false);
#endif
// Specifies whether the fragmented MP4 parser uses a test decoder that
// just outputs blank frames/audio instead of actually decoding. The blank
// decoder works on all platforms.
pref("media.fragmented-mp4.use-blank-decoder", false);
#endif
#ifdef MOZ_RAW
pref("media.raw.enabled", true);
#endif
pref("media.ogg.enabled", true);
pref("media.opus.enabled", true);
#ifdef MOZ_WAVE
pref("media.wave.enabled", true);
#endif
#ifdef MOZ_WEBM
pref("media.webm.enabled", true);
#if defined(MOZ_FMP4) && defined(MOZ_WMF)
pref("media.webm.intel_decoder.enabled", false);
#endif
#endif
#ifdef MOZ_GSTREAMER
pref("media.gstreamer.enabled", true);
pref("media.gstreamer.enable-blacklist", true);
#endif
#ifdef MOZ_APPLEMEDIA
pref("media.apple.mp3.enabled", true);
pref("media.apple.mp4.enabled", true);
#endif
#ifdef MOZ_WEBRTC
pref("media.navigator.enabled", true);
pref("media.navigator.video.enabled", true);
pref("media.navigator.load_adapt", true);
pref("media.navigator.load_adapt.measure_interval",1000);
pref("media.navigator.load_adapt.avg_seconds",3);
pref("media.navigator.load_adapt.high_load","0.90");
pref("media.navigator.load_adapt.low_load","0.40");
pref("media.navigator.video.default_fps",30);
pref("media.navigator.video.default_minfps",10);

pref("media.webrtc.debug.trace_mask", 0);
pref("media.webrtc.debug.multi_log", false);
pref("media.webrtc.debug.aec_log_dir", "");
pref("media.webrtc.debug.log_file", "");
pref("media.webrtc.debug.aec_dump_max_size", 4194304); // 4MB

#ifdef MOZ_WIDGET_GONK
pref("media.navigator.video.default_width",320);
pref("media.navigator.video.default_height",240);
pref("media.peerconnection.enabled", true);
pref("media.peerconnection.video.enabled", true);
pref("media.navigator.video.max_fs", 1200); // 640x480 == 1200mb
pref("media.navigator.video.max_fr", 30);
pref("media.navigator.video.h264.level", 12); // 0x42E00C - level 1.2
pref("media.navigator.video.h264.max_br", 700); // 8x10
pref("media.navigator.video.h264.max_mbps", 11880); // CIF@30fps
pref("media.peerconnection.video.h264_enabled", false);
pref("media.getusermedia.aec", 4);
// Gonk typically captures at QVGA, and so min resolution is QQVGA or
// 160x120; 100Kbps is plenty for that.
pref("media.peerconnection.video.min_bitrate", 100);
pref("media.peerconnection.video.start_bitrate", 220);
pref("media.peerconnection.video.max_bitrate", 1000);
#else
pref("media.navigator.video.default_width",0);  // adaptive default
pref("media.navigator.video.default_height",0); // adaptive default
pref("media.peerconnection.enabled", true);
pref("media.peerconnection.video.enabled", true);
pref("media.navigator.video.max_fs", 12288); // Enough for 2048x1536
pref("media.navigator.video.max_fr", 60);
pref("media.navigator.video.h264.level", 31); // 0x42E01f - level 3.1
pref("media.navigator.video.h264.max_br", 0);
pref("media.navigator.video.h264.max_mbps", 0);
pref("media.peerconnection.video.h264_enabled", false);
pref("media.getusermedia.aec", 1);
pref("media.getusermedia.browser.enabled", true);
// Desktop is typically VGA capture or more; and qm_select will not drop resolution
// below 1/2 in each dimension (or so), so QVGA (320x200) is the lowest here usually.
pref("media.peerconnection.video.min_bitrate", 200);
pref("media.peerconnection.video.start_bitrate", 300);
pref("media.peerconnection.video.max_bitrate", 2000);
#endif
pref("media.navigator.permission.disabled", false);
pref("media.peerconnection.default_iceservers", "[]");
pref("media.peerconnection.ice.loopback", false); // Set only for testing in offline environments.
pref("media.peerconnection.ice.tcp", false);
pref("media.peerconnection.ice.link_local", false); // Set only for testing IPV6 in networks that don't assign IPV6 addresses
pref("media.peerconnection.use_document_iceservers", true);
pref("media.peerconnection.identity.enabled", true);
pref("media.peerconnection.identity.timeout", 10000);
pref("media.peerconnection.ice.stun_client_maximum_transmits", 7);
pref("media.peerconnection.ice.trickle_grace_period", 5000);
// These values (aec, agc, and noice) are from media/webrtc/trunk/webrtc/common_types.h
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
pref("media.getusermedia.noise", 1);
pref("media.getusermedia.agc_enabled", false);
pref("media.getusermedia.agc", 1);
// Adjustments for OS-specific input delay (lower bound)
// Adjustments for OS-specific AudioStream+cubeb+output delay (lower bound)
#if defined(XP_MACOSX)
pref("media.peerconnection.capture_delay", 50);
pref("media.getusermedia.playout_delay", 10);
#elif defined(XP_WIN)
pref("media.peerconnection.capture_delay", 50);
pref("media.getusermedia.playout_delay", 40);
#elif defined(ANDROID)
pref("media.peerconnection.capture_delay", 100);
pref("media.getusermedia.playout_delay", 100);
// Whether to enable Webrtc Hardware acceleration support
pref("media.navigator.hardware.vp8_encode.acceleration_enabled", false);
pref("media.navigator.hardware.vp8_decode.acceleration_enabled", false);
#elif defined(XP_LINUX)
pref("media.peerconnection.capture_delay", 70);
pref("media.getusermedia.playout_delay", 50);
#else
// *BSD, others - merely a guess for now
pref("media.peerconnection.capture_delay", 50);
pref("media.getusermedia.playout_delay", 50);
#endif
#endif

#if !defined(ANDROID)
pref("media.getusermedia.screensharing.enabled", true);
#endif

#ifdef RELEASE_BUILD
pref("media.getusermedia.screensharing.allowed_domains", "webex.com,*.webex.com,ciscospark.com,*.ciscospark.com,projectsquared.com,*.projectsquared.com,*.room.co,room.co,beta.talky.io,talky.io,*.clearslide.com,appear.in,*.appear.in,tokbox.com,*.tokbox.com,*.sso.francetelecom.fr,*.si.francetelecom.fr,*.sso.infra.ftgroup,*.multimedia-conference.orange-business.com,*.espacecollaboration.orange-business.com,free.gotomeeting.com,g2m.me,*.g2m.me,example.com,*.mypurecloud.com,*.mypurecloud.com.au,spreed.me,*.spreed.me,*.spreed.com");
#else
 // temporary value, not intended for release - bug 1049087
pref("media.getusermedia.screensharing.allowed_domains", "mozilla.github.io,webex.com,*.webex.com,ciscospark.com,*.ciscospark.com,projectsquared.com,*.projectsquared.com,*.room.co,room.co,beta.talky.io,talky.io,*.clearslide.com,appear.in,*.appear.in,tokbox.com,*.tokbox.com,*.sso.francetelecom.fr,*.si.francetelecom.fr,*.sso.infra.ftgroup,*.multimedia-conference.orange-business.com,*.espacecollaboration.orange-business.com,free.gotomeeting.com,g2m.me,*.g2m.me,example.com,*.mypurecloud.com,*.mypurecloud.com.au,spreed.me,*.spreed.me,*.spreed.com");
#endif
// OS/X 10.6 and XP have screen/window sharing off by default due to various issues - Caveat emptor
pref("media.getusermedia.screensharing.allow_on_old_platforms", false);

pref("media.getusermedia.audiocapture.enabled", false);

// TextTrack support
pref("media.webvtt.enabled", true);
pref("media.webvtt.regions.enabled", false);

// AudioTrack and VideoTrack support
pref("media.track.enabled", false);

// Whether to enable MediaSource support.
// We want to enable on non-release  builds and on release windows and mac
// but on release builds restrict to YouTube. We don't enable for other
// configurations because code for those platforms isn't ready yet.
#if defined(XP_WIN) || defined(XP_MACOSX) || defined(MOZ_WIDGET_GONK)
pref("media.mediasource.enabled", true);
#else
pref("media.mediasource.enabled", false);
#endif

#ifdef RELEASE_BUILD
pref("media.mediasource.whitelist", true);
#else
pref("media.mediasource.whitelist", false);
#endif // RELEASE_BUILD

pref("media.mediasource.mp4.enabled", true);
pref("media.mediasource.webm.enabled", false);

#if defined(MOZ_WIDGET_GONK)
pref("media.mediasource.format-reader", false);
#else
// Enable new MediaSource architecture.
pref("media.mediasource.format-reader", true);
#endif

// Enable new MediaFormatReader architecture for webm in MSE
pref("media.mediasource.format-reader.webm", false);
// Enable new MediaFormatReader architecture for plain webm.
pref("media.format-reader.webm", false);

#ifdef MOZ_WEBSPEECH
pref("media.webspeech.recognition.enable", false);
pref("media.webspeech.synth.enabled", false);
#endif
#ifdef MOZ_WEBM_ENCODER
pref("media.encoder.webm.enabled", true);
#endif
#ifdef MOZ_OMX_ENCODER
pref("media.encoder.omx.enabled", true);
#endif

// Whether to autostart a media element with an |autoplay| attribute
pref("media.autoplay.enabled", true);

// The default number of decoded video frames that are enqueued in
// MediaDecoderReader's mVideoQueue.
pref("media.video-queue.default-size", 10);

// The maximum number of queued frames to send to the compositor.
// By default, send all of them.
pref("media.video-queue.send-to-compositor-size", 9999);

// Whether to disable the video stats to prevent fingerprinting
pref("media.video_stats.enabled", true);

// Whether to enable the audio writing APIs on the audio element
pref("media.audio_data.enabled", false);

// Whether to use async panning and zooming
pref("layers.async-pan-zoom.enabled", false);

// Whether to enable event region building during painting
pref("layout.event-regions.enabled", false);

// APZ preferences. For documentation/details on what these prefs do, check
// gfx/layers/apz/src/AsyncPanZoomController.cpp.
pref("apz.allow_checkerboarding", true);
pref("apz.allow_zooming", false);
pref("apz.asyncscroll.throttle", 100);
pref("apz.asyncscroll.timeout", 300);

// Whether to lock touch scrolling to one axis at a time
// 0 = FREE (No locking at all)
// 1 = STANDARD (Once locked, remain locked until scrolling ends)
// 2 = STICKY (Allow lock to be broken, with hysteresis)
pref("apz.axis_lock.mode", 0);
pref("apz.axis_lock.lock_angle", "0.5235987");        // PI / 6 (30 degrees)
pref("apz.axis_lock.breakout_threshold", "0.03125");  // 1/32 inches
pref("apz.axis_lock.breakout_angle", "0.3926991");    // PI / 8 (22.5 degrees)
pref("apz.axis_lock.direct_pan_angle", "1.047197");   // PI / 3 (60 degrees)
pref("apz.content_response_timeout", 300);
pref("apz.cross_slide.enabled", false);
pref("apz.danger_zone_x", 50);
pref("apz.danger_zone_y", 100);
pref("apz.enlarge_displayport_when_clipped", false);
pref("apz.fling_accel_base_mult", "1.0");
pref("apz.fling_accel_interval_ms", 500);
pref("apz.fling_accel_supplemental_mult", "1.0");
pref("apz.fling_curve_function_x1", "0.0");
pref("apz.fling_curve_function_y1", "0.0");
pref("apz.fling_curve_function_x2", "1.0");
pref("apz.fling_curve_function_y2", "1.0");
pref("apz.fling_curve_threshold_inches_per_ms", "-1.0");
pref("apz.fling_friction", "0.002");
pref("apz.fling_stop_on_tap_threshold", "0.05");
pref("apz.fling_stopped_threshold", "0.01");
pref("apz.max_velocity_inches_per_ms", "-1.0");
pref("apz.max_velocity_queue_size", 5);
pref("apz.min_skate_speed", "1.0");
pref("apz.minimap.enabled", false);
pref("apz.num_paint_duration_samples", 3);
pref("apz.overscroll.enabled", false);
pref("apz.overscroll.min_pan_distance_ratio", "1.0");
pref("apz.overscroll.stretch_factor", "0.5");
pref("apz.overscroll.spring_stiffness", "0.001");
pref("apz.overscroll.spring_friction", "0.015");
pref("apz.overscroll.stop_distance_threshold", "5.0");
pref("apz.overscroll.stop_velocity_threshold", "0.01");

// Whether to print the APZC tree for debugging
pref("apz.printtree", false);

pref("apz.test.logging_enabled", false);
pref("apz.touch_start_tolerance", "0.2222222");  // 0.2222222 came from 1.0/4.5
pref("apz.use_paint_duration", true);
pref("apz.velocity_bias", "1.0");
pref("apz.velocity_relevance_time_ms", 150);
pref("apz.x_stationary_size_multiplier", "3.0");
pref("apz.y_stationary_size_multiplier", "3.5");
pref("apz.zoom_animation_duration_ms", 250);

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
// Desktop prefs
pref("apz.fling_repaint_interval", 16);
pref("apz.smooth_scroll_repaint_interval", 16);
pref("apz.pan_repaint_interval", 16);
pref("apz.x_skate_size_multiplier", "2.5");
pref("apz.y_skate_size_multiplier", "3.5");
#else
// Mobile prefs
pref("apz.fling_repaint_interval", 75);
pref("apz.smooth_scroll_repaint_interval", 75);
pref("apz.pan_repaint_interval", 250);
pref("apz.x_skate_size_multiplier", "1.5");
pref("apz.y_skate_size_multiplier", "2.5");
#endif

// APZ testing (bug 961289)
pref("apz.test.logging_enabled", false);

#ifdef XP_MACOSX
// Whether to run in native HiDPI mode on machines with "Retina"/HiDPI display;
//   <= 0 : hidpi mode disabled, display will just use pixel-based upscaling
//   == 1 : hidpi supported if all screens share the same backingScaleFactor
//   >= 2 : hidpi supported even with mixed backingScaleFactors (somewhat broken)
pref("gfx.hidpi.enabled", 2);
#endif

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
// Use containerless scrolling for now on desktop.
pref("layout.scroll.root-frame-containers", false);
#endif

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

// disable downloadable font cache so that behavior is consistently
// the uncached load behavior across pages (useful for testing reflow problems)
pref("gfx.downloadable_fonts.disable_cache", false);

pref("gfx.downloadable_fonts.woff2.enabled", true);

#ifdef ANDROID
pref("gfx.bundled_fonts.enabled", true);
pref("gfx.bundled_fonts.force-enabled", false);
#endif

// Do we fire a notification about missing fonts, so the front-end can decide
// whether to try and do something about it (e.g. download additional fonts)?
pref("gfx.missing_fonts.notify", false);

pref("gfx.filter.nearest.force-enabled", false);

// prefs controlling the font (name/cmap) loader that runs shortly after startup
pref("gfx.font_loader.families_per_slice", 3); // read in info 3 families at a time
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
pref("gfx.font_rendering.directwrite.enabled", false);
pref("gfx.font_rendering.directwrite.use_gdi_table_loading", true);
#endif

pref("gfx.font_rendering.opentype_svg.enabled", true);

#ifdef XP_WIN
// comma separated list of backends to use in order of preference
// e.g., pref("gfx.canvas.azure.backends", "direct2d,skia,cairo");
pref("gfx.canvas.azure.backends", "direct2d1.1,direct2d,skia,cairo");
pref("gfx.content.azure.backends", "direct2d1.1,direct2d,cairo");
#else
#ifdef XP_MACOSX
pref("gfx.content.azure.backends", "cg");
pref("gfx.canvas.azure.backends", "skia");
// Accelerated cg canvas where available (10.7+)
pref("gfx.canvas.azure.accelerated", false);
#else
pref("gfx.canvas.azure.backends", "cairo");
pref("gfx.content.azure.backends", "cairo");
#endif
#endif

#ifdef MOZ_WIDGET_GTK2
pref("gfx.content.azure.backends", "cairo");
#endif
#ifdef ANDROID
pref("gfx.content.azure.backends", "cairo");
#endif

pref("gfx.work-around-driver-bugs", true);
pref("gfx.prefer-mesa-llvmpipe", false);

pref("gfx.draw-color-bars", false);

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
pref("canvas.filters.enabled", false);
// Add support for canvas path objects
pref("canvas.path.enabled", true);
// captureStream() on canvas disabled by default
pref("canvas.capturestream.enabled", false);

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

pref("accessibility.ipc_architecture.enabled", true);

#ifdef XP_WIN
// Some accessibility tools poke at windows in the plugin process during setup
// which can cause hangs.  To hack around this set accessibility.delay_plugins
// to true, you can also try increasing accessibility.delay_plugin_time if your
// machine is slow and you still experience hangs.
// See bug 781791.
pref("accessibility.delay_plugins", false);
pref("accessibility.delay_plugin_time", 10000);
#endif

pref("focusmanager.testmode", false);

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
#ifdef XP_MACOSX
pref("accessibility.typeaheadfind.prefillwithselection", false);
#else
pref("accessibility.typeaheadfind.prefillwithselection", true);
#endif
pref("accessibility.typeaheadfind.matchesCountTimeout", 250);
pref("accessibility.typeaheadfind.matchesCountLimit", 100);

// use Mac OS X Appearance panel text smoothing setting when rendering text, disabled by default
pref("gfx.use_text_smoothing_setting", false);

// Number of characters to consider emphasizing for rich autocomplete results
pref("toolkit.autocomplete.richBoundaryCutoff", 200);

// Variable controlling logging for osfile.
pref("toolkit.osfile.log", false);

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
// Information page about telemetry (temporary ; will be about:telemetry in the end)
pref("toolkit.telemetry.infoURL", "https://www.mozilla.org/legal/privacy/firefox.html#telemetry");
// Determines whether full SQL strings are returned when they might contain sensitive info
// i.e. dynamically constructed SQL strings or SQL executed by addons against addon DBs
pref("toolkit.telemetry.debugSlowSql", false);
// Whether to use the unified telemetry behavior, requires a restart.
pref("toolkit.telemetry.unified", true);

// Identity module
pref("toolkit.identity.enabled", false);
pref("toolkit.identity.debug", false);

// AsyncShutdown delay before crashing in case of shutdown freeze
pref("toolkit.asyncshutdown.timeout.crash", 60000);
// Extra logging for AsyncShutdown barriers and phases
pref("toolkit.asyncshutdown.log", false);

// Enable deprecation warnings.
pref("devtools.errorconsole.deprecation_warnings", true);

// Disable debugging chrome
pref("devtools.chrome.enabled", false);

// Disable remote debugging protocol logging
pref("devtools.debugger.log", false);
pref("devtools.debugger.log.verbose", false);

// Disable remote debugging connections
pref("devtools.debugger.remote-enabled", false);

pref("devtools.debugger.remote-port", 6000);
// Force debugger server binding on the loopback interface
pref("devtools.debugger.force-local", true);
// Display a prompt when a new connection starts to accept/reject it
pref("devtools.debugger.prompt-connection", true);
// Block tools from seeing / interacting with certified apps
pref("devtools.debugger.forbid-certified-apps", true);
// List of permissions that a sideloaded app can't ask for
pref("devtools.apps.forbidden-permissions", "embed-apps,engineering-mode,embed-widgets");

// DevTools default color unit
pref("devtools.defaultColorUnit", "hex");

// Used for devtools debugging
pref("devtools.dump.emit", false);

// Disable device discovery logging
pref("devtools.discovery.log", false);
// Whether to scan for DevTools devices via WiFi
pref("devtools.remote.wifi.scan", true);
// Whether UI options for controlling device visibility over WiFi are shown
// N.B.: This does not set whether the device can be discovered via WiFi, only
// whether the UI control to make such a choice is shown to the user
pref("devtools.remote.wifi.visible", true);
// Client must complete TLS handshake within this window (ms)
pref("devtools.remote.tls-handshake-timeout", 10000);

// URL of the remote JSON catalog used for device simulation
pref("devtools.devices.url", "https://code.cdn.mozilla.net/devices/devices.json");

// Display the introductory text
pref("devtools.gcli.hideIntro", false);

// How eager are we to show help: never=1, sometimes=2, always=3
pref("devtools.gcli.eagerHelper", 2);

// Alias to the script URLs for inject command.
pref("devtools.gcli.jquerySrc", "https://ajax.googleapis.com/ajax/libs/jquery/2.1.1/jquery.min.js");
pref("devtools.gcli.lodashSrc", "https://cdnjs.cloudflare.com/ajax/libs/lodash.js/2.4.1/lodash.min.js");
pref("devtools.gcli.underscoreSrc", "https://cdnjs.cloudflare.com/ajax/libs/underscore.js/1.7.0/underscore-min.js");

// Set imgur upload client ID
pref("devtools.gcli.imgurClientID", '0df414e888d7240');
// Imgur's upload URL
pref("devtools.gcli.imgurUploadURL", "https://api.imgur.com/3/image");

// GCLI commands directory
pref("devtools.commands.dir", "");

// view source
pref("view_source.syntax_highlight", true);
pref("view_source.wrap_long_lines", false);
pref("view_source.editor.external", false);
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

// Whether image visibility is enabled globally (ie we will try to unlock images
// that are not visible).
pref("layout.imagevisibility.enabled", true);
// Whether image visibility is enabled in documents that are within a browser
// element as defined by nsDocShell::FrameType and GetInheritedFrameType. This
// pref only has an effect if layout.imagevisibility.enabled is false.
pref("layout.imagevisibility.enabled_for_browser_elements_only", false);
pref("layout.imagevisibility.numscrollportwidths", 0);
pref("layout.imagevisibility.numscrollportheights", 1);

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
pref("browser.fixup.dns_first_for_single_words", false);
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
pref("editor.use_css",                       false);
pref("editor.css.default_length_unit",       "px");
pref("editor.resizing.preserve_ratio",       true);
pref("editor.positioning.offset",            0);

// Scripts & Windows prefs
pref("dom.disable_beforeunload",            false);
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
pref("dom.popup_allowed_events", "change click dblclick mouseup notificationclick reset submit touchend");
pref("dom.disable_open_click_delay", 1000);

pref("dom.storage.enabled", true);
pref("dom.storage.default_quota",      5120);

pref("dom.send_after_paint_to_content", false);

// Timeout clamp in ms for timeouts we clamp
pref("dom.min_timeout_value", 4);
// And for background windows
pref("dom.min_background_timeout_value", 1000);

// Don't use new input types
pref("dom.experimental_forms", false);

// Enable <input type=number>:
pref("dom.forms.number", true);

// Enable <input type=color> by default. It will be turned off for remaining
// platforms which don't have a color picker implemented yet.
pref("dom.forms.color", true);

// Support for new @autocomplete values
pref("dom.forms.autocomplete.experimental", false);

// Enables requestAutocomplete DOM API on forms.
pref("dom.forms.requestAutocomplete", false);

#ifdef NIGHTLY_BUILD
pref("dom.input.dirpicker", true);
#endif

// Enables system messages and activities
pref("dom.sysmsg.enabled", false);

// Enable pre-installed applications.
pref("dom.webapps.useCurrentProfile", false);

pref("dom.cycle_collector.incremental", true);

// Parsing perf prefs. For now just mimic what the old code did.
#ifndef XP_WIN
pref("content.sink.pending_event_mode", 0);
#endif

// Disable popups from plugins by default
//   0 = openAllowed
//   1 = openControlled
//   2 = openAbused
pref("privacy.popups.disable_from_plugins", 2);

// send "do not track" HTTP header, disabled by default
pref("privacy.donottrackheader.enabled",    false);
// Enforce tracking protection in all modes
pref("privacy.trackingprotection.enabled",  false);
// Enforce tracking protection in Private Browsing mode
pref("privacy.trackingprotection.pbmode.enabled",  false);

pref("dom.event.contextmenu.enabled",       true);
pref("dom.event.clipboardevents.enabled",   true);
#if defined(XP_WIN) && !defined(RELEASE_BUILD)
pref("dom.event.highrestimestamp.enabled",  true);
#else
pref("dom.event.highrestimestamp.enabled",  false);
#endif

pref("dom.webcomponents.enabled",           false);

pref("javascript.enabled",                  true);
pref("javascript.options.strict",           false);
#ifdef DEBUG
pref("javascript.options.strict.debug",     false);
#endif
pref("javascript.options.baselinejit",      true);
pref("javascript.options.ion",              true);
pref("javascript.options.asmjs",            true);
pref("javascript.options.native_regexp",    true);
pref("javascript.options.parallel_parsing", true);
#if !defined(RELEASE_BUILD) && !defined(ANDROID) && !defined(MOZ_B2G) && !defined(XP_IOS)
pref("javascript.options.asyncstack",       true);
#else
pref("javascript.options.asyncstack",       false);
#endif
pref("javascript.options.ion.offthread_compilation", true);
// This preference instructs the JS engine to discard the
// source of any privileged JS after compilation. This saves
// memory, but makes things like Function.prototype.toSource()
// fail.
pref("javascript.options.discardSystemSource", false);
// This preference limits the memory usage of javascript.
// If you want to change these values for your device,
// please find Bug 417052 comment 17 and Bug 456721
// Comment 32 and Bug 613551.
pref("javascript.options.mem.high_water_mark", 128);
pref("javascript.options.mem.max", -1);
pref("javascript.options.mem.gc_per_compartment", true);
pref("javascript.options.mem.gc_incremental", true);
pref("javascript.options.mem.gc_incremental_slice_ms", 10);
pref("javascript.options.mem.gc_compacting", true);
pref("javascript.options.mem.log", false);
pref("javascript.options.mem.notify", false);
pref("javascript.options.gc_on_memory_pressure", true);
pref("javascript.options.compact_on_user_inactive", true);

pref("javascript.options.mem.gc_high_frequency_time_limit_ms", 1000);
pref("javascript.options.mem.gc_high_frequency_low_limit_mb", 100);
pref("javascript.options.mem.gc_high_frequency_high_limit_mb", 500);
pref("javascript.options.mem.gc_high_frequency_heap_growth_max", 300);
pref("javascript.options.mem.gc_high_frequency_heap_growth_min", 150);
pref("javascript.options.mem.gc_low_frequency_heap_growth", 150);
pref("javascript.options.mem.gc_dynamic_heap_growth", true);
pref("javascript.options.mem.gc_dynamic_mark_slice", true);
pref("javascript.options.mem.gc_allocation_threshold_mb", 30);
pref("javascript.options.mem.gc_decommit_threshold_mb", 32);
pref("javascript.options.mem.gc_min_empty_chunk_count", 1);
pref("javascript.options.mem.gc_max_empty_chunk_count", 30);

pref("javascript.options.showInConsole", false);

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

// Allow necko to do A/B testing. Will generally only happen if
// telemetry is also enabled as otherwise there is no way to report
// the results
pref("network.allow-experiments", true);

// Allow the network changed event to get sent when a network topology or
// setup change is noticed while running.
pref("network.notify.changed", true);

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
pref("network.protocol-handler.external.ttps", false); // https
pref("network.protocol-handler.external.tps", false);  // https
pref("network.protocol-handler.external.ps", false);   // https
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
pref("network.offline-mirrors-connectivity", true);

// <http>
pref("network.http.version", "1.1");      // default
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

// There is a problem with some IIS7 servers that don't close the connection
// properly after it times out (bug #491541). Default timeout on IIS7 is
// 120 seconds. We need to reuse or drop the connection within this time.
// We set the timeout a little shorter to keep a reserve for cases when
// the packet is lost or delayed on the route.
pref("network.http.keep-alive.timeout", 115);

// Timeout connections if an initial response is not received after 5 mins.
pref("network.http.response.timeout", 300);

// Limit the absolute number of http connections.
// Note: the socket transport service will clamp the number below 256 if the OS
// cannot allocate that many FDs, and it also always tries to reserve up to 250
// file descriptors for things other than sockets.
pref("network.http.max-connections", 256);

// If NOT connecting via a proxy, then
// a new connection will only be attempted if the number of active persistent
// connections to the server is less then max-persistent-connections-per-server.
pref("network.http.max-persistent-connections-per-server", 6);

// If connecting via a proxy, then a
// new connection will only be attempted if the number of active persistent
// connections to the proxy is less then max-persistent-connections-per-proxy.
pref("network.http.max-persistent-connections-per-proxy", 32);

// amount of time (in seconds) to suspend pending requests, before spawning a
// new connection, once the limit on the number of persistent connections per
// host has been reached.  however, a new connection will not be created if
// max-connections or max-connections-per-server has also been reached.
pref("network.http.request.max-start-delay", 10);

// Headers
pref("network.http.accept.default", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");

// Prefs allowing granular control of referers
// 0=don't send any, 1=send only on clicks, 2=send on image requests as well
pref("network.http.sendRefererHeader",      2);
// false=real referer, true=spoof referer (use target URI as referer)
pref("network.http.referer.spoofSource", false);
// 0=full URI, 1=scheme+host+port+path, 2=scheme+host+port
pref("network.http.referer.trimmingPolicy", 0);
// 0=always send, 1=send iff base domains match, 2=send iff hosts match
pref("network.http.referer.XOriginPolicy", 0);

// Controls whether we send HTTPS referres to other HTTPS sites.
// By default this is enabled for compatibility (see bug 141641)
pref("network.http.sendSecureXSiteReferrer", true);

// Controls whether referrer attributes in <a>, <img>, <area>, and <iframe> are honoured
pref("network.http.enablePerElementReferrer", false);

// Maximum number of consecutive redirects before aborting.
pref("network.http.redirection-limit", 20);

// Enable http compression: comment this out in case of problems with 1.1
// NOTE: support for "compress" has been disabled per bug 196406.
// NOTE: separate values with comma+space (", "): see bug 576033
pref("network.http.accept-encoding", "gzip, deflate");

pref("network.http.pipelining"      , false);
pref("network.http.pipelining.ssl"  , false); // disable pipelining over SSL
pref("network.http.pipelining.abtest", false);
pref("network.http.proxy.pipelining", false);

// Max number of requests in the pipeline
pref("network.http.pipelining.maxrequests" , 32);

// An optimistic request is one pipelined when policy might allow a new
// connection instead
pref("network.http.pipelining.max-optimistic-requests" , 4);

pref("network.http.pipelining.aggressive", false);
pref("network.http.pipelining.maxsize" , 300000);
pref("network.http.pipelining.reschedule-on-timeout", true);
pref("network.http.pipelining.reschedule-timeout", 1500);

// The read-timeout is a ms timer that causes the transaction to be completely
// restarted without pipelining.
pref("network.http.pipelining.read-timeout", 30000);

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

// The maximum amount of time the cache session lock can be held
// before a new transaction bypasses the cache. In milliseconds.
#ifdef RELEASE_BUILD
pref("network.http.bypass-cachelock-threshold", 200000);
#else
pref("network.http.bypass-cachelock-threshold", 250);
#endif

// Try and use SPDY when using SSL
pref("network.http.spdy.enabled", true);
pref("network.http.spdy.enabled.v3-1", true);
pref("network.http.spdy.enabled.http2", true);
pref("network.http.spdy.enabled.deps", true);
pref("network.http.spdy.enforce-tls-profile", true);
pref("network.http.spdy.chunk-size", 16000);
pref("network.http.spdy.timeout", 180);
pref("network.http.spdy.coalesce-hostnames", true);
pref("network.http.spdy.persistent-settings", false);
pref("network.http.spdy.ping-threshold", 58);
pref("network.http.spdy.ping-timeout", 8);
pref("network.http.spdy.send-buffer-size", 131072);
pref("network.http.spdy.allow-push", true);
pref("network.http.spdy.push-allowance", 131072);
pref("network.http.spdy.default-concurrent", 100);

// alt-svc allows separation of transport routing from
// the origin host without using a proxy.
pref("network.http.altsvc.enabled", true);
pref("network.http.altsvc.oe", true);

pref("network.http.diagnostics", false);

pref("network.http.pacing.requests.enabled", true);
pref("network.http.pacing.requests.min-parallelism", 6);
pref("network.http.pacing.requests.hz", 100);
pref("network.http.pacing.requests.burst", 32);

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

// Whether nsHttpChannel should use the PackagedAppService to load
// resources from a package when directed to a URL
// such as http://domain.com/package.pak!//resource.html
// See http://www.w3.org/TR/web-packaging/#streamable-package-format
pref("network.http.enable-packaged-apps", false);

// default values for FTP
// in a DSCP environment this should be 40 (0x28, or AF11), per RFC-4594,
// Section 4.8 "High-Throughput Data Service Class", and 80 (0x50, or AF22)
// per Section 4.7 "Low-Latency Data Service Class".
pref("network.ftp.data.qos", 0);
pref("network.ftp.control.qos", 0);

// If this pref is false only one xpcom event will be served per poll
// iteration. This is the original behavior.
// If it is true multiple events will be served.
pref("network.sts.serve_multiple_events_per_poll_iteration", true);
// The max time to spend on xpcom events between two polls in ms.
pref("network.sts.max_time_for_events_between_two_polls", 100);
// </http>

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

pref("dom.server-events.enabled", true);
// Equal to the DEFAULT_RECONNECTION_TIME_VALUE value in nsEventSource.cpp
pref("dom.server-events.default-reconnection-time", 5000); // in milliseconds

// If false, remote JAR files that are served with a content type other than
// application/java-archive or application/x-jar will not be opened
// by the jar channel.
pref("network.jar.open-unsafe-types", false);

// This preference, if true, causes all UTF-8 domain names to be normalized to
// punycode.  The intention is to allow UTF-8 domain names as input, but never
// generate them from punycode.
pref("network.IDN_show_punycode", false);

// If "network.IDN.use_whitelist" is set to true, TLDs with
// "network.IDN.whitelist.tld" explicitly set to true are treated as
// IDN-safe. Otherwise, they're treated as unsafe and punycode will be used
// for displaying them in the UI (e.g. URL bar), unless they conform to one of
// the profiles specified in
// http://www.unicode.org/reports/tr36/proposed.html#Security_Levels_and_Alerts
// If "network.IDN.restriction_profile" is "high", the Highly Restrictive
// profile is used.
// If "network.IDN.restriction_profile" is "moderate", the Moderately
// Restrictive profile is used.
// In all other cases, the ASCII-Only profile is used.
// Note that these preferences are referred to ONLY when
// "network.IDN_show_punycode" is false. In other words, all IDNs will be shown
// in punycode if "network.IDN_show_punycode" is true.
pref("network.IDN.restriction_profile", "moderate");
pref("network.IDN.use_whitelist", true);

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
pref("network.IDN.blacklist_chars", "\u0020\u00A0\u00BC\u00BD\u00BE\u01C3\u02D0\u0337\u0338\u0589\u05C3\u05F4\u0609\u060A\u066A\u06D4\u0701\u0702\u0703\u0704\u115F\u1160\u1735\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A\u200B\u200E\u200F\u2024\u2027\u2028\u2029\u202A\u202B\u202C\u202D\u202E\u202F\u2039\u203A\u2041\u2044\u2052\u205F\u2153\u2154\u2155\u2156\u2157\u2158\u2159\u215A\u215B\u215C\u215D\u215E\u215F\u2215\u2236\u23AE\u2571\u29F6\u29F8\u2AFB\u2AFD\u2FF0\u2FF1\u2FF2\u2FF3\u2FF4\u2FF5\u2FF6\u2FF7\u2FF8\u2FF9\u2FFA\u2FFB\u3000\u3002\u3014\u3015\u3033\u3164\u321D\u321E\u33AE\u33AF\u33C6\u33DF\uA789\uFE14\uFE15\uFE3F\uFE5D\uFE5E\uFEFF\uFF0E\uFF0F\uFF61\uFFA0\uFFF9\uFFFA\uFFFB\uFFFC\uFFFD");

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

// The grace period allows the DNS cache to use expired entries, while kicking off
// a revalidation in the background.
pref("network.dnsCacheExpirationGracePeriod", 60);

// This preference can be used to turn off DNS prefetch.
pref("network.dns.disablePrefetch", false);

// Contols whether or not "localhost" should resolve when offline
pref("network.dns.offline-localhost", true);

// This preference controls whether or not URLs with UTF-8 characters are
// escaped.  Set this preference to TRUE for strict RFC2396 conformance.
pref("network.standard-url.escape-utf8", true);

// This preference controls whether or not URLs are always encoded and sent as
// UTF-8.
pref("network.standard-url.encode-utf8", true);

// The maximum allowed length for a URL - 1MB default
pref("network.standard-url.max-length", 1048576);

// Idle timeout for ftp control connections - 5 minute default
pref("network.ftp.idleConnectionTimeout", 300);

// directory listing format
// 2: HTML
// 3: XUL directory viewer
// all other values are treated like 2
pref("network.dir.format", 2);

// enables the prefetch service (i.e., prefetching of <link rel="next"> URLs).
pref("network.prefetch-next", true);

// enables the predictive service
pref("network.predictor.enabled", true);
pref("network.predictor.enable-hover-on-ssl", false);
pref("network.predictor.page-degradation.day", 0);
pref("network.predictor.page-degradation.week", 5);
pref("network.predictor.page-degradation.month", 10);
pref("network.predictor.page-degradation.year", 25);
pref("network.predictor.page-degradation.max", 50);
pref("network.predictor.subresource-degradation.day", 1);
pref("network.predictor.subresource-degradation.week", 10);
pref("network.predictor.subresource-degradation.month", 25);
pref("network.predictor.subresource-degradation.year", 50);
pref("network.predictor.subresource-degradation.max", 100);
pref("network.predictor.preconnect-min-confidence", 90);
pref("network.predictor.preresolve-min-confidence", 60);
pref("network.predictor.redirect-likely-confidence", 75);
pref("network.predictor.max-resources-per-entry", 100);
pref("network.predictor.max-uri-length", 500);
pref("network.predictor.cleaned-up", false);

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

// Sub-resources HTTP-authentication:
//   0 - don't allow sub-resources to open HTTP authentication credentials
//       dialogs
//   1 - allow sub-resources to open HTTP authentication credentials dialogs,
//       but don't allow it for cross-origin sub-resources
//   2 - allow the cross-origin authentication as well.
pref("network.auth.allow-subresource-auth", 1);

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
pref("network.cookie.cookieBehavior",       0); // 0-Accept, 1-dontAcceptForeign, 2-dontUse, 3-limitForeign
#ifdef ANDROID
pref("network.cookie.cookieBehavior",       0); // Keep the old default of accepting all cookies
#endif
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

// Use the HSTS preload list by default
pref("network.stricttransportsecurity.preloadlist", true);

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
pref("intl.ellipsis",                       "chrome://global-platform/locale/intl.properties");
pref("intl.locale.matchOS",                 false);
// fallback charset list for Unicode conversion (converting from Unicode)
// currently used for mail send only to handle symbol characters (e.g Euro, trademark, smartquotes)
// for ISO-8859-1
pref("intl.fallbackCharsetList.ISO-8859-1", "windows-1252");
pref("font.language.group",                 "chrome://global/locale/intl.properties");

// Android-specific pref to use key-events-only mode for IME-unaware webapps.
#ifdef MOZ_WIDGET_ANDROID
pref("intl.ime.hack.on_ime_unaware_apps.fire_key_events_for_composition", true);
#else
pref("intl.ime.hack.on_ime_unaware_apps.fire_key_events_for_composition", false);
#endif

// these locales have right-to-left UI
pref("intl.uidirection.ar", "rtl");
pref("intl.uidirection.he", "rtl");
pref("intl.uidirection.fa", "rtl");
pref("intl.uidirection.ug", "rtl");
pref("intl.uidirection.ur", "rtl");

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

pref("font.name.serif.x-math", "Latin Modern Math");
pref("font.name-list.serif.x-math", "Latin Modern Math, XITS Math, Cambria Math, TeX Gyre Bonum Math, TeX Gyre Pagella Math, TeX Gyre Schola, TeX Gyre Termes Math, STIX Math, Asana Math, STIXGeneral, DejaVu Serif, DejaVu Sans, serif");
pref("font.name.sans-serif.x-math", "sans-serif");
pref("font.name.monospace.x-math", "monospace");

// Some CJK fonts have bad underline offset, their CJK character glyphs are overlapped (or adjoined)  to its underline.
// These fonts are ignored the underline offset, instead of it, the underline is lowered to bottom of its em descent.
pref("font.blacklist.underline_offset", "FangSong,Gulim,GulimChe,MingLiU,MingLiU-ExtB,MingLiU_HKSCS,MingLiU-HKSCS-ExtB,MS Gothic,MS Mincho,MS PGothic,MS PMincho,MS UI Gothic,PMingLiU,PMingLiU-ExtB,SimHei,SimSun,SimSun-ExtB,Hei,Kai,Apple LiGothic,Apple LiSung,Osaka");

#ifdef MOZ_B2G
// Whitelist of fonts that ship with B2G that do not include space lookups in
// default features. This allows us to skip analyzing the GSUB/GPOS tables
// unless features are explicitly enabled.
// Use NSPR_LOG_MODULES=fontinit:5 to dump out details of space lookups
pref("font.whitelist.skip_default_features_space_check", "Fira Sans,Fira Mono");
#endif

pref("images.dither", "auto");
pref("security.directory",              "");

pref("signed.applets.codebase_principal_support", false);
pref("security.checkloaduri", true);
pref("security.xpconnect.plugin.unrestricted", true);
// security-sensitive dialogs should delay button enabling. In milliseconds.
pref("security.dialog_enable_delay", 1000);
pref("security.notification_enable_delay", 500);

pref("security.csp.enable", true);
pref("security.csp.debug", false);
pref("security.csp.experimentalEnabled", false);

// Default Content Security Policy to apply to privileged apps.
pref("security.apps.privileged.CSP.default", "default-src * data: blob:; script-src 'self'; object-src 'none'; style-src 'self' 'unsafe-inline'");

// Mixed content blocking
pref("security.mixed_content.block_active_content", false);
pref("security.mixed_content.block_display_content", false);

// Disable pinning checks by default.
pref("security.cert_pinning.enforcement_level", 0);
// Do not process hpkp headers rooted by not built in roots by default.
// This is to prevent accidental pinning from MITM devices and is used
// for tests.
pref("security.cert_pinning.process_headers_from_non_builtin_roots", false);

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

// mousewheel.*.action can specify the action when you use mosue wheel.
// When no modifier keys are pressed or two or more modifires are pressed,
// .default is used.
// 0: Nothing happens
// 1: Scrolling contents
// 2: Go back or go forward, in your history
// 3: Zoom in or out.
pref("mousewheel.default.action", 1);
pref("mousewheel.with_alt.action", 2);
pref("mousewheel.with_control.action", 3);
pref("mousewheel.with_meta.action", 1);  // command key on Mac
pref("mousewheel.with_shift.action", 1);
pref("mousewheel.with_win.action", 1);

// mousewheel.*.action.override_x will override the action
// when the mouse wheel is rotated along the x direction.
// -1: Don't override the action.
// 0 to 3: Override the action with the specified value.
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
pref("layout.css.devPixelsPerPx", "-1.0");

// Is support for CSS Masking features enabled?
pref("layout.css.masking.enabled", true);

// Is support for mix-blend-mode enabled?
pref("layout.css.mix-blend-mode.enabled", true);

// Is support for isolation enabled?
pref("layout.css.isolation.enabled", true);

// Is support for CSS Filters enabled?
pref("layout.css.filters.enabled", true);

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

// Is support for basic shapes in clip-path enabled?
pref("layout.css.clip-path-shapes.enabled", false);

// Is support for CSS sticky positioning enabled?
pref("layout.css.sticky.enabled", true);

// Is support for CSS "will-change" enabled?
pref("layout.css.will-change.enabled", true);

// Is support for DOMPoint enabled?
pref("layout.css.DOMPoint.enabled", true);

// Is support for DOMQuad enabled?
pref("layout.css.DOMQuad.enabled", true);

// Is support for DOMMatrix enabled?
pref("layout.css.DOMMatrix.enabled", true);

// Is support for GeometryUtils.getBoxQuads enabled?
#ifdef RELEASE_BUILD
pref("layout.css.getBoxQuads.enabled", false);
#else
pref("layout.css.getBoxQuads.enabled", true);
#endif

// Is support for GeometryUtils.getBoxQuads enabled?
#ifdef RELEASE_BUILD
pref("layout.css.convertFromNode.enabled", false);
#else
pref("layout.css.convertFromNode.enabled", true);
#endif

// Is support for unicode-range enabled?
#ifdef RELEASE_BUILD
pref("layout.css.unicode-range.enabled", false);
#else
pref("layout.css.unicode-range.enabled", true);
#endif

// Is support for CSS "text-align: true X" enabled?
pref("layout.css.text-align-true-value.enabled", false);

// Is support for the CSS4 image-orientation property enabled?
pref("layout.css.image-orientation.enabled", true);

// Are sets of prefixed properties supported?
pref("layout.css.prefixes.border-image", true);
pref("layout.css.prefixes.transforms", true);
pref("layout.css.prefixes.transitions", true);
pref("layout.css.prefixes.animations", true);
pref("layout.css.prefixes.box-sizing", true);
pref("layout.css.prefixes.font-features", true);
pref("layout.css.prefixes.gradients", true);

// Is the CSS Unprefixing Service enabled? (This service emulates support
// for certain vendor-prefixed properties & values, for sites on a "fixlist".)
pref("layout.css.unprefixing-service.enabled", true);

// Is support for the :scope selector enabled?
pref("layout.css.scope-pseudo.enabled", true);

// Is support for background-blend-mode enabled?
pref("layout.css.background-blend-mode.enabled", true);

// Is support for CSS vertical text enabled?
pref("layout.css.vertical-text.enabled", true);

// Is support for CSS text-combine-upright (tate-chu-yoko) enabled?
pref("layout.css.text-combine-upright.enabled", false);

// Is support for object-fit and object-position enabled?
pref("layout.css.object-fit-and-position.enabled", true);

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

// Is support for CSS variables enabled?
pref("layout.css.variables.enabled", true);

// Is support for CSS overflow-clip-box enabled for non-UA sheets?
pref("layout.css.overflow-clip-box.enabled", false);

// Is support for CSS grid enabled?
pref("layout.css.grid.enabled", false);

// Is support for CSS contain enabled?
pref("layout.css.contain.enabled", false);

// Is support for CSS Ruby enabled?
//
// When this pref is removed, make sure that the pref callback registration
// in nsLayoutStylesheetCache::EnsureGlobal and the invalidation of
// mUASheet in nsLayoutStylesheetCache::DependentPrefChanged (if it's not
// otherwise needed) are removed.
pref("layout.css.ruby.enabled", true);

// Is support for CSS display:contents enabled?
pref("layout.css.display-contents.enabled", true);

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

// Is support for document.fonts enabled?
pref("layout.css.font-loading-api.enabled", true);

// Are the MouseEvent.offsetX/Y properties enabled?
pref("dom.mouseEvent.offsetXY.enabled", true);

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

// pref to control whether layout warnings that are hit quite often are enabled
pref("layout.spammy_warnings.enabled", true);

// Should we fragment floats inside CSS column layout?
pref("layout.float-fragments-inside-column.enabled", true);

// Is support for the Web Animations API enabled?
#ifdef RELEASE_BUILD
pref("dom.animations-api.core.enabled", false);
#else
pref("dom.animations-api.core.enabled", true);
#endif

// pref to permit users to make verified SOAP calls by default
pref("capability.policy.default.SOAPCall.invokeVerifySourceHeader", "allAccess");

// if true, allow plug-ins to override internal imglib decoder mime types in full-page mode
pref("plugin.override_internal_types", false);

// See bug 136985.  Gives embedders a pref to hook into to show
// a popup blocker if they choose.
pref("browser.popups.showPopupBlocker", true);

// Pref to control whether the viewmanager code does double-buffering or not
// See http://bugzilla.mozilla.org/show_bug.cgi?id=169483 for further details...
pref("viewmanager.do_doublebuffering", true);

// enable single finger gesture input (win7+ tablets)
pref("gestures.enable_single_finger_input", true);

pref("editor.resizing.preserve_ratio",       true);
pref("editor.positioning.offset",            0);

pref("dom.use_watchdog", true);
pref("dom.max_chrome_script_run_time", 20);
pref("dom.max_child_script_run_time", 10);
pref("dom.max_script_run_time", 10);

// If true, ArchiveReader will be enabled
pref("dom.archivereader.enabled", false);

// Hang monitor timeout after which we kill the browser, in seconds
// (0 is disabled)
// Disabled on all platforms per bug 705748 until the found issues are
// resolved.
pref("hangmonitor.timeout", 0);

pref("plugins.load_appdir_plugins", false);
// If true, plugins will be click to play
pref("plugins.click_to_play", false);

// The default value for nsIPluginTag.enabledState (STATE_ENABLED = 2)
pref("plugin.default.state", 2);

// The MIME type that should bind to legacy java-specific invocations like
// <applet> and <object data="java:foo">. Setting this to a non-java MIME type
// is undefined behavior.
pref("plugin.java.mime", "application/x-java-vm");

// How long in minutes we will allow a plugin to work after the user has chosen
// to allow it "now"
pref("plugin.sessionPermissionNow.intervalInMinutes", 60);
// How long in days we will allow a plugin to work after the user has chosen
// to allow it persistently.
pref("plugin.persistentPermissionAlways.intervalInDays", 90);

#if !defined(DEBUG) && !defined(MOZ_ASAN)
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
// No timeout in DEBUG or ASan builds
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

#ifdef XP_WIN
// Disable oopp for java on windows. They run their own
// process isolation which conflicts with our implementation.
pref("dom.ipc.plugins.java.enabled", false);
#endif

pref("dom.ipc.plugins.flash.disable-protected-mode", false);

pref("dom.ipc.plugins.flash.subprocess.crashreporter.enabled", true);
pref("dom.ipc.plugins.reportCrashURL", true);

// How long we wait before unloading an idle plugin process.
// Defaults to 30 seconds.
pref("dom.ipc.plugins.unloadTimeoutSecs", 30);

// Asynchronous plugin initialization should only be enabled on non-e10s
// channels until some remaining bugs are resolved.
#ifdef E10S_TESTING_ONLY
pref("dom.ipc.plugins.asyncInit", false);
#else
pref("dom.ipc.plugins.asyncInit", true);
#endif

pref("dom.ipc.processCount", 1);

// Enable caching of Moz2D Path objects for SVG geometry elements
pref("svg.path-caching.enabled", true);

// Enable the use of display-lists for SVG hit-testing and painting.
pref("svg.display-lists.hit-testing.enabled", true);
pref("svg.display-lists.painting.enabled", true);

// Is support for the SVG 2 paint-order property enabled?
pref("svg.paint-order.enabled", true);

// Is support for the <marker orient="auto-start-reverse"> feature enabled?
pref("svg.marker-improvements.enabled", true);

// Is support for the new getBBox method from SVG 2 enabled?
// See https://svgwg.org/svg2-draft/single-page.html#types-SVGBoundingBoxOptions
pref("svg.new-getBBox.enabled", false);

#ifdef RELEASE_BUILD
pref("svg.transform-origin.enabled", false);
#else
pref("svg.transform-origin.enabled", true);
#endif // RELEASE_BUILD

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
pref("font.minimum-size.x-math", 0);

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

pref("font.name.serif.ar", "Times New Roman");
pref("font.name.sans-serif.ar", "Segoe UI");
pref("font.name-list.sans-serif.ar", "Segoe UI, Tahoma, Arial");
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

pref("font.name.serif.ja", "MS PMincho");
pref("font.name.sans-serif.ja", "MS PGothic");
pref("font.name.monospace.ja", "MS Gothic");
pref("font.name-list.serif.ja", "MS PMincho, MS Mincho, MS PGothic, MS Gothic,Meiryo");
pref("font.name-list.sans-serif.ja", "MS PGothic, MS Gothic, MS PMincho, MS Mincho,Meiryo");
pref("font.name-list.monospace.ja", "MS Gothic, MS Mincho, MS PGothic, MS PMincho,Meiryo");

pref("font.name.serif.ko", "Batang");
pref("font.name.sans-serif.ko", "Gulim");
pref("font.name.monospace.ko", "GulimChe");
pref("font.name.cursive.ko", "Gungsuh");

pref("font.name-list.serif.ko", "Batang, Gulim");
pref("font.name-list.sans-serif.ko", "Gulim");
pref("font.name-list.monospace.ko", "GulimChe");
pref("font.name-list.cursive.ko", "Gungsuh");

pref("font.name.serif.th", "Tahoma");
pref("font.name.sans-serif.th", "Tahoma");
pref("font.name.monospace.th", "Tahoma");
pref("font.name.cursive.th", "Tahoma");

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

pref("font.name.serif.zh-CN", "SimSun");
pref("font.name.sans-serif.zh-CN", "Microsoft YaHei");
pref("font.name.monospace.zh-CN", "SimSun");
pref("font.name.cursive.zh-CN", "KaiTi");
pref("font.name-list.serif.zh-CN", "MS Song, SimSun, SimSun-ExtB");
pref("font.name-list.sans-serif.zh-CN", "Microsoft YaHei, SimHei, Arial Unicode MS");
pref("font.name-list.monospace.zh-CN", "MS Song, SimSun, SimSun-ExtB");

// Per Taiwanese users' demand. They don't want to use TC fonts for
// rendering Latin letters. (bug 88579)
pref("font.name.serif.zh-TW", "Times New Roman");
pref("font.name.sans-serif.zh-TW", "Arial");
pref("font.name.monospace.zh-TW", "MingLiU");
pref("font.name-list.serif.zh-TW", "PMingLiu, MingLiU, MingLiU-ExtB");
pref("font.name-list.sans-serif.zh-TW", "PMingLiU, MingLiU, MingLiU-ExtB");
pref("font.name-list.monospace.zh-TW", "MingLiU, MingLiU-ExtB");

// hkscsm3u.ttf (HKSCS-2001) :  http://www.microsoft.com/hk/hkscs
// Hong Kong users have the same demand about glyphs for Latin letters (bug 88579)
pref("font.name.serif.zh-HK", "Times New Roman");
pref("font.name.sans-serif.zh-HK", "Arial");
pref("font.name.monospace.zh-HK", "MingLiu_HKSCS");
pref("font.name-list.serif.zh-HK", "MingLiu_HKSCS, Ming(for ISO10646), MingLiU, MingLiU_HKSCS-ExtB");
pref("font.name-list.sans-serif.zh-HK", "MingLiU_HKSCS, Ming(for ISO10646), MingLiU, MingLiU_HKSCS-ExtB");
pref("font.name-list.monospace.zh-HK", "MingLiU_HKSCS, Ming(for ISO10646), MingLiU, MingLiU_HKSCS-ExtB");

pref("font.name.serif.x-devanagari", "Kokila");
pref("font.name.sans-serif.x-devanagari", "Nirmala UI");
pref("font.name.monospace.x-devanagari", "Mangal");
pref("font.name-list.serif.x-devanagari", "Kokila, Raghindi");
pref("font.name-list.sans-serif.x-devanagari", "Nirmala UI, Mangal");
pref("font.name-list.monospace.x-devanagari", "Mangal, Nirmala UI");

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

pref("font.minimum-size.th", 10);

pref("font.default.x-devanagari", "sans-serif");
pref("font.name.serif.x-math", "Latin Modern Math");
// We have special support for Monotype Symbol on Windows.
pref("font.name-list.serif.x-math", "Latin Modern Math, XITS Math, Cambria Math, TeX Gyre Bonum Math, TeX Gyre Pagella Math, TeX Gyre Schola, TeX Gyre Termes Math, STIX Math, Asana Math, STIXGeneral, DejaVu Serif, DejaVu Sans, Symbol, Times New Roman");
pref("font.name.sans-serif.x-math", "Arial");
pref("font.name.monospace.x-math", "Courier New");
pref("font.name.cursive.x-math", "Comic Sans MS");

// cleartype settings - false implies default system settings

// use cleartype rendering for downloadable fonts (win xp only)
pref("gfx.font_rendering.cleartype.use_for_downloadable_fonts", true);

// use cleartype rendering for all fonts always (win xp only)
pref("gfx.font_rendering.cleartype.always_use_for_content", false);

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

// print_extra_margin enables platforms to specify an extra gap or margin
// around the content of the page for Print Preview only
pref("print.print_extra_margin", 90); // twips (90 twips is an eigth of an inch)

// Whether to extend the native dialog with information on printing frames.
pref("print.extend_native_print_dialog", true);

// Locate plugins by scanning the Adobe Acrobat installation directory with a minimum version
pref("plugin.scan.Acrobat", "5.0");

// Locate plugins by scanning the Quicktime installation directory with a minimum version
pref("plugin.scan.Quicktime", "5.0");

// Locate and scan the Window Media Player installation directory for plugins with a minimum version
pref("plugin.scan.WindowsMediaPlayer", "7.0");

// Locate plugins by the directories specified in the Windows registry for PLIDs
// Which is currently HKLM\Software\MozillaPlugins\xxxPLIDxxx\Path
pref("plugin.scan.plid.all", true);

// Allow the new AsyncDrawing mode to be used for plugins.
pref("plugin.allow.asyncdrawing", false);

// Help Windows NT, 2000, and XP dialup a RAS connection
// when a network address is unreachable.
pref("network.autodial-helper.enabled", true);

// Switch the keyboard layout per window
pref("intl.keyboard.per_window_layout", false);

#ifdef NS_ENABLE_TSF
// Enable/Disable TSF support on Vista or later.
pref("intl.tsf.enable", true);

// Force enable TSF even on WinXP or WinServer 2003.
// Be aware, TSF framework on prior to Vista is not enough stable.
pref("intl.tsf.force_enable", false);

// Support IMEs implemented with IMM in TSF mode.
pref("intl.tsf.support_imm", true);

// Enables/Disables hack for specific TIP.

// Whether creates native caret for ATOK or not.
pref("intl.tsf.hack.atok.create_native_caret", true);
// Whether use composition start position for the result of
// ITfContextView::GetTextExt() if the specified range is larger than
// composition start offset.
// For Free ChangJie 2010
pref("intl.tsf.hack.free_chang_jie.do_not_return_no_layout_error", true);
// For Easy Changjei
pref("intl.tsf.hack.easy_changjei.do_not_return_no_layout_error", true);
// Whether use previous character rect for the result of
// ITfContextView::GetTextExt() if the specified range is the first character
// of selected clause of composition string.
pref("intl.tsf.hack.google_ja_input.do_not_return_no_layout_error_at_first_char", true);
// Whether use previous character rect for the result of
// ITfContextView::GetTextExt() if the specified range is the caret of
// composition string.
pref("intl.tsf.hack.google_ja_input.do_not_return_no_layout_error_at_caret", true);
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

// High resolution scrolling with supported mouse drivers on Vista or later.
pref("mousewheel.enable_pixel_scrolling", true);

// If your mouse drive sends WM_*SCROLL messages when you turn your mouse wheel,
// set this to true.  Then, gecko processes them as mouse wheel messages.
pref("mousewheel.emulate_at_wm_scroll", false);

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

pref("font.name.serif.el", "Times");
pref("font.name.sans-serif.el", "Helvetica");
pref("font.name.monospace.el", "Courier New");
pref("font.name.cursive.el", "Lucida Grande");
pref("font.name.fantasy.el", "Lucida Grande");
pref("font.name-list.serif.el", "Times,Times New Roman");
pref("font.name-list.sans-serif.el", "Helvetica,Lucida Grande");
pref("font.name-list.monospace.el", "Courier New,Lucida Grande");
pref("font.name-list.cursive.el", "Times,Lucida Grande");
pref("font.name-list.fantasy.el", "Times,Lucida Grande");

pref("font.name.serif.he", "Times New Roman");
pref("font.name.sans-serif.he", "Arial");
pref("font.name.monospace.he", "Courier New");
pref("font.name.cursive.he", "Times New Roman");
pref("font.name.fantasy.he", "Times New Roman");
pref("font.name-list.serif.he", "Times New Roman");
pref("font.name-list.sans-serif.he", "Arial");
pref("font.name-list.monospace.he", "Courier New");
pref("font.name-list.cursive.he", "Times New Roman");
pref("font.name-list.fantasy.he", "Times New Roman");

pref("font.name.serif.ja", "Hiragino Mincho ProN");
pref("font.name.sans-serif.ja", "Hiragino Kaku Gothic ProN");
pref("font.name.monospace.ja", "Osaka-Mono");
pref("font.name-list.serif.ja", "Hiragino Mincho ProN,Hiragino Mincho Pro");
pref("font.name-list.sans-serif.ja", "Hiragino Kaku Gothic ProN,Hiragino Kaku Gothic Pro");
pref("font.name-list.monospace.ja", "Osaka-Mono");

pref("font.name.serif.ko", "AppleMyungjo");
pref("font.name.sans-serif.ko", "Apple SD Gothic Neo");
pref("font.name.monospace.ko", "Apple SD Gothic Neo");
pref("font.name-list.serif.ko", "AppleMyungjo");
pref("font.name-list.sans-serif.ko", "Apple SD Gothic Neo,AppleGothic");
pref("font.name-list.monospace.ko", "Apple SD Gothic Neo,AppleGothic");

pref("font.name.serif.th", "Thonburi");
pref("font.name.sans-serif.th", "Thonburi");
pref("font.name.monospace.th", "Ayuthaya");
pref("font.name-list.serif.th", "Thonburi");
pref("font.name-list.sans-serif.th", "Thonburi");
pref("font.name-list.monospace.th", "Ayuthaya");

pref("font.name.serif.x-armn", "Mshtakan");
pref("font.name.sans-serif.x-armn", "Mshtakan");
pref("font.name.monospace.x-armn", "Mshtakan");
pref("font.name-list.serif.x-armn", "Mshtakan");
pref("font.name-list.sans-serif.x-armn", "Mshtakan");
pref("font.name-list.monospace.x-armn", "Mshtakan");

// SolaimanLipi, Rupali http://ekushey.org/?page/mac_download
pref("font.name.serif.x-beng", "Bangla MN");
pref("font.name.sans-serif.x-beng", "Bangla Sangam MN");
pref("font.name.monospace.x-beng", "Bangla Sangam MN");
pref("font.name-list.serif.x-beng", "Bangla MN");
pref("font.name-list.sans-serif.x-beng", "Bangla Sangam MN");
pref("font.name-list.monospace.x-beng", "Bangla Sangam MN");

pref("font.name.serif.x-cans", "Euphemia UCAS");
pref("font.name.sans-serif.x-cans", "Euphemia UCAS");
pref("font.name.monospace.x-cans", "Euphemia UCAS");
pref("font.name-list.serif.x-cans", "Euphemia UCAS");
pref("font.name-list.sans-serif.x-cans", "Euphemia UCAS");
pref("font.name-list.monospace.x-cans", "Euphemia UCAS");

pref("font.name.serif.x-cyrillic", "Times");
pref("font.name.sans-serif.x-cyrillic", "Helvetica");
pref("font.name.monospace.x-cyrillic", "Monaco");
pref("font.name.cursive.x-cyrillic", "Geneva");
pref("font.name.fantasy.x-cyrillic", "Charcoal CY");
pref("font.name-list.serif.x-cyrillic", "Times,Times New Roman");
pref("font.name-list.sans-serif.x-cyrillic", "Helvetica,Arial");
pref("font.name-list.monospace.x-cyrillic", "Monaco,Courier New");
pref("font.name-list.cursive.x-cyrillic", "Geneva");
pref("font.name-list.fantasy.x-cyrillic", "Charcoal CY");

pref("font.name.serif.x-devanagari", "Devanagari MT");
pref("font.name.sans-serif.x-devanagari", "Devanagari Sangam MN");
pref("font.name.monospace.x-devanagari", "Devanagari Sangam MN");
pref("font.name-list.serif.x-devanagari", "Devanagari MT");
pref("font.name-list.sans-serif.x-devanagari", "Devanagari Sangam MN,Devanagari MT");
pref("font.name-list.monospace.x-devanagari", "Devanagari Sangam MN,Devanagari MT");

// Abyssinica SIL http://scripts.sil.org/AbyssinicaSIL_Download
pref("font.name.serif.x-ethi", "Kefa");
pref("font.name.sans-serif.x-ethi", "Kefa");
pref("font.name.monospace.x-ethi", "Kefa");
pref("font.name-list.serif.x-ethi", "Kefa,Abyssinica SIL");
pref("font.name-list.sans-serif.x-ethi", "Kefa,Abyssinica SIL");
pref("font.name-list.monospace.x-ethi", "Kefa,Abyssinica SIL");

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
pref("font.name.sans-serif.x-gujr", "Gujarati Sangam MN");
pref("font.name.monospace.x-gujr", "Gujarati Sangam MN");
pref("font.name-list.serif.x-gujr", "Gujarati MT");
pref("font.name-list.sans-serif.x-gujr", "Gujarati Sangam MN,Gujarati MT");
pref("font.name-list.monospace.x-gujr", "Gujarati Sangam MN,Gujarati MT");

pref("font.name.serif.x-guru", "Gurmukhi MT");
pref("font.name.sans-serif.x-guru", "Gurmukhi MT");
pref("font.name.monospace.x-guru", "Gurmukhi MT");
pref("font.name-list.serif.x-guru", "Gurmukhi MT");
pref("font.name-list.sans-serif.x-guru", "Gurmukhi MT");
pref("font.name-list.monospace.x-guru", "Gurmukhi MT");

pref("font.name.serif.x-khmr", "Khmer MN");
pref("font.name.sans-serif.x-khmr", "Khmer Sangam MN");
pref("font.name.monospace.x-khmr", "Khmer Sangam MN");
pref("font.name-list.serif.x-khmr", "Khmer MN");
pref("font.name-list.sans-serif.x-khmr", "Khmer Sangam MN");
pref("font.name-list.monospace.x-khmr", "Khmer Sangam MN");

pref("font.name.serif.x-mlym", "Malayalam MN");
pref("font.name.sans-serif.x-mlym", "Malayalam Sangam MN");
pref("font.name.monospace.x-mlym", "Malayalam Sangam MN");
pref("font.name-list.serif.x-mlym", "Malayalam MN");
pref("font.name-list.sans-serif.x-mlym", "Malayalam Sangam MN");
pref("font.name-list.monospace.x-mlym", "Malayalam Sangam MN");

pref("font.name.serif.x-orya", "Oriya MN");
pref("font.name.sans-serif.x-orya", "Oriya Sangam MN");
pref("font.name.monospace.x-orya", "Oriya Sangam MN");
pref("font.name-list.serif.x-orya", "Oriya MN");
pref("font.name-list.sans-serif.x-orya", "Oriya Sangam MN");
pref("font.name-list.monospace.x-orya", "Oriya Sangam MN");

// Pothana http://web.nickshanks.com/typography/telugu/
pref("font.name.serif.x-telu", "Telugu MN");
pref("font.name.sans-serif.x-telu", "Telugu Sangam MN");
pref("font.name.monospace.x-telu", "Telugu Sangam MN");
pref("font.name-list.serif.x-telu", "Telugu MN,Pothana");
pref("font.name-list.sans-serif.x-telu", "Telugu Sangam MN,Pothana");
pref("font.name-list.monospace.x-telu", "Telugu Sangam MN,Pothana");

// Kedage http://web.nickshanks.com/typography/kannada/
pref("font.name.serif.x-knda", "Kannada MN");
pref("font.name.sans-serif.x-knda", "Kannada Sangam MN");
pref("font.name.monospace.x-knda", "Kannada Sangam MN");
pref("font.name-list.serif.x-knda", "Kannada MN,Kedage");
pref("font.name-list.sans-serif.x-knda", "Kannada Sangam MN,Kedage");
pref("font.name-list.monospace.x-knda", "Kannada Sangam MN,Kedage");

pref("font.name.serif.x-sinh", "Sinhala MN");
pref("font.name.sans-serif.x-sinh", "Sinhala Sangam MN");
pref("font.name.monospace.x-sinh", "Sinhala Sangam MN");
pref("font.name-list.serif.x-sinh", "Sinhala MN");
pref("font.name-list.sans-serif.x-sinh", "Sinhala Sangam MN");
pref("font.name-list.monospace.x-sinh", "Sinhala Sangam MN");

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
pref("font.name-list.serif.x-western", "Times,Times New Roman");
pref("font.name-list.sans-serif.x-western", "Helvetica,Arial");
pref("font.name-list.monospace.x-western", "Courier,Courier New");
pref("font.name-list.cursive.x-western", "Apple Chancery");
pref("font.name-list.fantasy.x-western", "Papyrus");

pref("font.name.serif.zh-CN", "Times");
pref("font.name.sans-serif.zh-CN", "Helvetica");
pref("font.name.monospace.zh-CN", "Courier");
pref("font.name-list.serif.zh-CN", "Times,STSong,Heiti SC");
pref("font.name-list.sans-serif.zh-CN", "Helvetica,PingFang SC,STHeiti,Heiti SC");
pref("font.name-list.monospace.zh-CN", "Courier,PingFang SC,STHeiti,Heiti SC");

pref("font.name.serif.zh-TW", "Times");
pref("font.name.sans-serif.zh-TW", "Helvetica");
pref("font.name.monospace.zh-TW", "Courier");
pref("font.name-list.serif.zh-TW", "Times,LiSong Pro,Heiti TC");
pref("font.name-list.sans-serif.zh-TW", "Helvetica,PingFang TC,Heiti TC,LiHei Pro");
pref("font.name-list.monospace.zh-TW", "Courier,PingFang TC,Heiti TC,LiHei Pro");

pref("font.name.serif.zh-HK", "Times");
pref("font.name.sans-serif.zh-HK", "Helvetica");
pref("font.name.monospace.zh-HK", "Courier");
pref("font.name-list.serif.zh-HK", "Times,LiSong Pro,Heiti TC");
pref("font.name-list.sans-serif.zh-HK", "Helvetica,PingFang TC,Heiti TC,LiHei Pro");
pref("font.name-list.monospace.zh-HK", "Courier,PingFang TC,Heiti TC,LiHei Pro");

// XP_MACOSX changes to default font sizes
pref("font.minimum-size.th", 10);
pref("font.size.variable.zh-CN", 15);
pref("font.size.variable.zh-HK", 15);
pref("font.size.variable.zh-TW", 15);

pref("font.name.serif.x-math", "Latin Modern Math");
// Apple's Symbol is Unicode so use it
pref("font.name-list.serif.x-math", "Latin Modern Math, XITS Math, Cambria Math, TeX Gyre Bonum Math, TeX Gyre Pagella Math, TeX Gyre Schola, TeX Gyre Termes Math, STIX Math, Asana Math, STIXGeneral, DejaVu Serif, DejaVu Sans, Symbol, Times");
pref("font.name.sans-serif.x-math", "Helvetica");
pref("font.name.monospace.x-math", "Courier");
pref("font.name.cursive.x-math", "Apple Chancery");
pref("font.name.fantasy.x-math", "Papyrus");

// individual font faces to be treated as independent families
// names are Postscript names of each face
pref("font.single-face-list", "Osaka-Mono");

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

// print_extra_margin enables platforms to specify an extra gap or margin
// around the content of the page for Print Preview only
pref("print.print_extra_margin", 90); // twips (90 twips is an eigth of an inch)

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
pref("middlemouse.contentLoadURL", true);
pref("middlemouse.openNewWindow", true);
pref("middlemouse.scrollbarPosition", true);

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

pref("helpers.global_mime_types_file", "/etc/mime.types");
pref("helpers.global_mailcap_file", "/etc/mailcap");
pref("helpers.private_mime_types_file", "~/.mime.types");
pref("helpers.private_mailcap_file", "~/.mailcap");
pref("print.printer_list", ""); // list of printers, separated by spaces
pref("print.print_reversed", false);
pref("print.print_color", true);
pref("print.print_landscape", false);
pref("print.print_paper_size", 0);

// print_extra_margin enables platforms to specify an extra gap or margin
// around the content of the page for Print Preview only
pref("print.print_extra_margin", 0); // twips

// CSSOM-View scroll-behavior smooth scrolling requires the C++ APZC
pref("layout.css.scroll-behavior.enabled", false);
pref("layout.css.scroll-behavior.property-enabled", false);

// CSS Scroll Snapping requires the C++ APZC
pref("layout.css.scroll-snap.enabled", false);

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

pref("helpers.global_mime_types_file", "/etc/mime.types");
pref("helpers.global_mailcap_file", "/etc/mailcap");
pref("helpers.private_mime_types_file", "~/.mime.types");
pref("helpers.private_mailcap_file", "~/.mailcap");
pref("print.printer_list", ""); // list of printers, separated by spaces
pref("print.print_reversed", false);
pref("print.print_color", true);
pref("print.print_landscape", false);
pref("print.print_paper_size", 0);

// print_extra_margin enables platforms to specify an extra gap or margin
// around the content of the page for Print Preview only
pref("print.print_extra_margin", 0); // twips

// font names

pref("font.name.serif.ar", "serif");
pref("font.name.sans-serif.ar", "sans-serif");
pref("font.name.monospace.ar", "monospace");
pref("font.size.fixed.ar", 12);

pref("font.name.serif.el", "serif");
pref("font.name.sans-serif.el", "sans-serif");
pref("font.name.monospace.el", "monospace");
pref("font.size.fixed.el", 12);

pref("font.name.serif.he", "serif");
pref("font.name.sans-serif.he", "sans-serif");
pref("font.name.monospace.he", "monospace");
pref("font.size.fixed.he", 12);

pref("font.name.serif.ja", "serif");
pref("font.name.sans-serif.ja", "sans-serif");
pref("font.name.monospace.ja", "monospace");

pref("font.name.serif.ko", "serif");
pref("font.name.sans-serif.ko", "sans-serif");
pref("font.name.monospace.ko", "monospace");

pref("font.name.serif.th", "serif");
pref("font.name.sans-serif.th", "sans-serif");
pref("font.name.monospace.th", "monospace");
pref("font.minimum-size.th", 13);

pref("font.name.serif.x-armn", "serif");
pref("font.name.sans-serif.x-armn", "sans-serif");
pref("font.name.monospace.x-armn", "monospace");

pref("font.name.serif.x-beng", "serif");
pref("font.name.sans-serif.x-beng", "sans-serif");
pref("font.name.monospace.x-beng", "monospace");

pref("font.name.serif.x-cans", "serif");
pref("font.name.sans-serif.x-cans", "sans-serif");
pref("font.name.monospace.x-cans", "monospace");

pref("font.name.serif.x-cyrillic", "serif");
pref("font.name.sans-serif.x-cyrillic", "sans-serif");
pref("font.name.monospace.x-cyrillic", "monospace");
pref("font.size.fixed.x-cyrillic", 12);

pref("font.name.serif.x-devanagari", "serif");
pref("font.name.sans-serif.x-devanagari", "sans-serif");
pref("font.name.monospace.x-devanagari", "monospace");

pref("font.name.serif.x-ethi", "serif");
pref("font.name.sans-serif.x-ethi", "sans-serif");
pref("font.name.monospace.x-ethi", "monospace");

pref("font.name.serif.x-geor", "serif");
pref("font.name.sans-serif.x-geor", "sans-serif");
pref("font.name.monospace.x-geor", "monospace");

pref("font.name.serif.x-gujr", "serif");
pref("font.name.sans-serif.x-gujr", "sans-serif");
pref("font.name.monospace.x-gujr", "monospace");

pref("font.name.serif.x-guru", "serif");
pref("font.name.sans-serif.x-guru", "sans-serif");
pref("font.name.monospace.x-guru", "monospace");

pref("font.name.serif.x-khmr", "serif");
pref("font.name.sans-serif.x-khmr", "sans-serif");
pref("font.name.monospace.x-khmr", "monospace");

pref("font.name.serif.x-knda", "serif");
pref("font.name.sans-serif.x-knda", "sans-serif");
pref("font.name.monospace.x-knda", "monospace");

pref("font.name.serif.x-mlym", "serif");
pref("font.name.sans-serif.x-mlym", "sans-serif");
pref("font.name.monospace.x-mlym", "monospace");

pref("font.name.serif.x-orya", "serif");
pref("font.name.sans-serif.x-orya", "sans-serif");
pref("font.name.monospace.x-orya", "monospace");

pref("font.name.serif.x-sinh", "serif");
pref("font.name.sans-serif.x-sinh", "sans-serif");
pref("font.name.monospace.x-sinh", "monospace");

pref("font.name.serif.x-tamil", "serif");
pref("font.name.sans-serif.x-tamil", "sans-serif");
pref("font.name.monospace.x-tamil", "monospace");

pref("font.name.serif.x-telu", "serif");
pref("font.name.sans-serif.x-telu", "sans-serif");
pref("font.name.monospace.x-telu", "monospace");

pref("font.name.serif.x-tibt", "serif");
pref("font.name.sans-serif.x-tibt", "sans-serif");
pref("font.name.monospace.x-tibt", "monospace");

pref("font.name.serif.x-unicode", "serif");
pref("font.name.sans-serif.x-unicode", "sans-serif");
pref("font.name.monospace.x-unicode", "monospace");
pref("font.size.fixed.x-unicode", 12);

pref("font.name.serif.x-western", "serif");
pref("font.name.sans-serif.x-western", "sans-serif");
pref("font.name.monospace.x-western", "monospace");
pref("font.size.fixed.x-western", 12);

pref("font.name.serif.zh-CN", "serif");
pref("font.name.sans-serif.zh-CN", "sans-serif");
pref("font.name.monospace.zh-CN", "monospace");

pref("font.name.serif.zh-HK", "serif");
pref("font.name.sans-serif.zh-HK", "sans-serif");
pref("font.name.monospace.zh-HK", "monospace");

pref("font.name.serif.zh-TW", "serif");
pref("font.name.sans-serif.zh-TW", "sans-serif");
pref("font.name.monospace.zh-TW", "monospace");

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

#if MOZ_WIDGET_GTK == 2
pref("intl.ime.use_simple_context_on_password_field", true);
#else
pref("intl.ime.use_simple_context_on_password_field", false);
#endif

# enable new platform fontlist for linux on GTK platforms
# temporary pref to allow flipping back to the existing
# gfxPangoFontGroup/gfxFontconfigUtils code for handling system fonts

#ifdef MOZ_WIDGET_GTK
#ifdef RELEASE_BUILD
pref("gfx.font_rendering.fontconfig.fontlist.enabled", false);
#else
pref("gfx.font_rendering.fontconfig.fontlist.enabled", true);
#endif
#endif

# XP_UNIX
#endif
#endif
#endif

#if defined(ANDROID) || defined(MOZ_B2G)

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

# ANDROID || MOZ_B2G
#endif

#if defined(MOZ_B2G)
// Gonk, FxOS Simulator, B2G Desktop and Mulet.

// TODO: some entries could probably be cleaned up.

// ar

pref("font.name.serif.el", "Droid Serif"); // not Charis SIL Compact, only has a few Greek chars
pref("font.name.sans-serif.el", "Fira Sans");
pref("font.name.monospace.el", "Fira Mono");

pref("font.name.serif.he", "Charis SIL Compact");
pref("font.name.sans-serif.he", "Fira Sans");
pref("font.name.monospace.he", "Fira Mono");
pref("font.name-list.sans-serif.he", "Droid Sans Hebrew, Fira Sans");

pref("font.name.serif.ja", "Charis SIL Compact");
pref("font.name.sans-serif.ja", "Fira Sans");
pref("font.name.monospace.ja", "MotoyaLMaru");
pref("font.name-list.sans-serif.ja", "Fira Sans, MotoyaLMaru, MotoyaLCedar, Droid Sans Japanese");
pref("font.name-list.monospace.ja", "MotoyaLMaru, MotoyaLCedar, Fira Mono");

pref("font.name.serif.ko", "Charis SIL Compact");
pref("font.name.sans-serif.ko", "Fira Sans");
pref("font.name.monospace.ko", "Fira Mono");

pref("font.name.serif.th", "Charis SIL Compact");
pref("font.name.sans-serif.th", "Fira Sans");
pref("font.name.monospace.th", "Fira Mono");
pref("font.name-list.sans-serif.th", "Fira Sans, Noto Sans Thai, Droid Sans Thai");

pref("font.name.serif.x-cyrillic", "Charis SIL Compact");
pref("font.name.sans-serif.x-cyrillic", "Fira Sans");
pref("font.name.monospace.x-cyrillic", "Fira Mono");

pref("font.name.serif.x-unicode", "Charis SIL Compact");
pref("font.name.sans-serif.x-unicode", "Fira Sans");
pref("font.name.monospace.x-unicode", "Fira Mono");

pref("font.name.serif.x-western", "Charis SIL Compact");
pref("font.name.sans-serif.x-western", "Fira Sans");
pref("font.name.monospace.x-western", "Fira Mono");

pref("font.name.serif.zh-CN", "Charis SIL Compact");
pref("font.name.sans-serif.zh-CN", "Fira Sans");
pref("font.name.monospace.zh-CN", "Fira Mono");
pref("font.name-list.sans-serif.zh-CN", "Fira Sans,Droid Sans Fallback");

pref("font.name.serif.zh-HK", "Charis SIL Compact");
pref("font.name.sans-serif.zh-HK", "Fira Sans");
pref("font.name.monospace.zh-HK", "Fira Mono");
pref("font.name-list.sans-serif.zh-HK", "Fira Sans,Droid Sans Fallback");

pref("font.name.serif.zh-TW", "Charis SIL Compact");
pref("font.name.sans-serif.zh-TW", "Fira Sans");
pref("font.name.monospace.zh-TW", "Fira Mono");
pref("font.name-list.sans-serif.zh-TW", "Fira Sans,Droid Sans Fallback");

pref("font.name.serif.x-math", "Latin Modern Math");
pref("font.name-list.serif.x-math", "Latin Modern Math, XITS Math, Cambria Math, TeX Gyre Bonum Math, TeX Gyre Pagella Math, TeX Gyre Schola, TeX Gyre Termes Math, STIX Math, Asana Math, STIXGeneral, DejaVu Serif, DejaVu Sans, Charis SIL Compact");
pref("font.name.sans-serif.x-math", "Fira Sans");
pref("font.name.monospace.x-math", "Fira Mono");

#elif defined(ANDROID)
// We use the bundled fonts for Firefox for Android

// ar

pref("font.name.serif.el", "Droid Serif"); // not Charis SIL Compact, only has a few Greek chars
pref("font.name.sans-serif.el", "Clear Sans");
pref("font.name.monospace.el", "Droid Sans Mono");
pref("font.name-list.sans-serif.el", "Clear Sans, Roboto, Droid Sans");

pref("font.name.serif.he", "Droid Serif");
pref("font.name.sans-serif.he", "Clear Sans");
pref("font.name.monospace.he", "Droid Sans Mono");
pref("font.name-list.sans-serif.he", "Droid Sans Hebrew, Clear Sans, Droid Sans");

pref("font.name.serif.ja", "Charis SIL Compact");
pref("font.name.sans-serif.ja", "Clear Sans");
pref("font.name.monospace.ja", "MotoyaLMaru");
pref("font.name-list.serif.ja", "Droid Serif");
pref("font.name-list.sans-serif.ja", "Clear Sans, Roboto, Droid Sans, MotoyaLMaru, MotoyaLCedar, Noto Sans JP, Droid Sans Japanese");
pref("font.name-list.monospace.ja", "MotoyaLMaru, MotoyaLCedar, Droid Sans Mono");

pref("font.name.serif.ko", "Charis SIL Compact");
pref("font.name.sans-serif.ko", "Clear Sans");
pref("font.name.monospace.ko", "Droid Sans Mono");
pref("font.name-list.serif.ko", "Droid Serif, HYSerif");
pref("font.name-list.sans-serif.ko", "SmartGothic, NanumGothic, Noto Sans KR, DroidSansFallback, Droid Sans Fallback");

pref("font.name.serif.th", "Charis SIL Compact");
pref("font.name.sans-serif.th", "Clear Sans");
pref("font.name.monospace.th", "Droid Sans Mono");
pref("font.name-list.serif.th", "Droid Serif");
pref("font.name-list.sans-serif.th", "Droid Sans Thai, Clear Sans, Droid Sans");

pref("font.name.serif.x-cyrillic", "Charis SIL Compact");
pref("font.name.sans-serif.x-cyrillic", "Clear Sans");
pref("font.name.monospace.x-cyrillic", "Droid Sans Mono");
pref("font.name-list.serif.x-cyrillic", "Droid Serif");
pref("font.name-list.sans-serif.x-cyrillic", "Clear Sans, Roboto, Droid Sans");

pref("font.name.serif.x-unicode", "Charis SIL Compact");
pref("font.name.sans-serif.x-unicode", "Clear Sans");
pref("font.name.monospace.x-unicode", "Droid Sans Mono");
pref("font.name-list.serif.x-unicode", "Droid Serif");
pref("font.name-list.sans-serif.x-unicode", "Clear Sans, Roboto, Droid Sans");

pref("font.name.serif.x-western", "Charis SIL Compact");
pref("font.name.sans-serif.x-western", "Clear Sans");
pref("font.name.monospace.x-western", "Droid Sans Mono");
pref("font.name-list.serif.x-western", "Droid Serif");
pref("font.name-list.sans-serif.x-western", "Clear Sans, Roboto, Droid Sans");

pref("font.name.serif.zh-CN", "Charis SIL Compact");
pref("font.name.sans-serif.zh-CN", "Clear Sans");
pref("font.name.monospace.zh-CN", "Droid Sans Mono");
pref("font.name-list.serif.zh-CN", "Droid Serif, Droid Sans Fallback");
pref("font.name-list.sans-serif.zh-CN", "Roboto, Droid Sans, Noto Sans SC, Droid Sans Fallback");
pref("font.name-list.monospace.zh-CN", "Droid Sans Fallback");

pref("font.name.serif.zh-HK", "Charis SIL Compact");
pref("font.name.sans-serif.zh-HK", "Clear Sans");
pref("font.name.monospace.zh-HK", "Droid Sans Mono");
pref("font.name-list.serif.zh-HK", "Droid Serif, Droid Sans Fallback");
pref("font.name-list.sans-serif.zh-HK", "Roboto, Droid Sans, Noto Sans TC, Noto Sans SC, Droid Sans Fallback");
pref("font.name-list.monospace.zh-HK", "Droid Sans Fallback");

pref("font.name.serif.zh-TW", "Charis SIL Compact");
pref("font.name.sans-serif.zh-TW", "Clear Sans");
pref("font.name.monospace.zh-TW", "Droid Sans Mono");
pref("font.name-list.serif.zh-TW", "Droid Serif, Droid Sans Fallback");
pref("font.name-list.sans-serif.zh-TW", "Roboto, Droid Sans, Noto Sans TC, Noto Sans SC, Droid Sans Fallback");
pref("font.name-list.monospace.zh-TW", "Droid Sans Fallback");

pref("font.name.serif.x-math", "Latin Modern Math");
pref("font.name-list.serif.x-math", "Latin Modern Math, XITS Math, Cambria Math, TeX Gyre Bonum Math, TeX Gyre Pagella Math, TeX Gyre Schola, TeX Gyre Termes Math, STIX Math, Asana Math, STIXGeneral, DejaVu Serif, DejaVu Sans, Charis SIL Compact");
pref("font.name.sans-serif.x-math", "Clear Sans");
pref("font.name.monospace.x-math", "Droid Sans Mono");

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

// Login Manager prefs
pref("signon.rememberSignons",              true);
pref("signon.autofillForms",                true);
pref("signon.autologin.proxy",              false);
pref("signon.storeWhenAutocompleteOff",     true);
pref("signon.ui.experimental",              false);
pref("signon.debug",                        false);
pref("signon.recipes.path",                 "chrome://passwordmgr/content/recipes.json");

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

/**
 * Specify whether or not the browser should generate a reflow event on zoom.
 * For a pan-and-zoom ui on mobile, it is sometimes desirable for a zoom event
 * to limit the max line box width of text in order to enable easier reading
 * of large amounts of text.
 *
 * If enabled, this will limit the max line box width of all text on a page to
 * the viewport width (also generating a reflow), after a zoom event occurs.
 *
 * By default, this is not enabled.
 */
pref("browser.zoom.reflowOnZoom", false);

/**
 * Specifies the number of milliseconds to wait after a given reflow-on-zoom
 * operation has completed before allowing another one to be triggered. This
 * is to prevent a buildup of reflow-zoom events.
 */
pref("browser.zoom.reflowZoom.reflowTimeout", 500);

/**
 * Controls whether or not the reflow-on-zoom behavior happens on page load.
 * This can be enabled in conjunction with the above preference (reflowOnZoom),
 * but has no effect if browser.zoom.reflowOnZoom is disabled.
 *
 * Note that this should be turned off only in cases where debugging of the
 * reflow-on-zoom feature is necessary, and enabling the feature during
 * a page load inhbits this debugging.
 */
pref("browser.zoom.reflowZoom.reflowTextOnPageLoad", true);

//
// Image-related prefs
//

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
pref("image.http.accept", "image/png,image/*;q=0.8,*/*;q=0.5");

pref("image.high_quality_downscaling.enabled", true);

// The minimum percent downscaling we'll use high-quality downscaling on,
// interpreted as a floating-point number / 1000.
pref("image.high_quality_downscaling.min_factor", 1000);

// The maximum memory size which we'll use high-quality uspcaling on,
// interpreted as number of decoded bytes.
pref("image.high_quality_upscaling.max_size", 20971520);

// The threshold for inferring that changes to an <img> element's |src|
// attribute by JavaScript represent an animation, in milliseconds. If the |src|
// attribute is changing more frequently than this value, then we enter a
// special "animation mode" which is designed to eliminate flicker. Set to 0 to
// disable.
pref("image.infer-src-animation.threshold-ms", 2000);

// Should we optimize away the surfaces of single-color images?
pref("image.single-color-optimization.enabled", true);

//
// Image memory management prefs
//

// Discards inactive image frames and re-decodes them on demand from
// compressed data.
pref("image.mem.discardable", true);

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

// How many threads we'll use for multithreaded decoding. If < 0, will be
// automatically determined based on the system's number of cores.
pref("image.multithreaded_decoding.limit", -1);

// Limit for the canvas image cache. 0 means we don't limit the size of the
// cache.
pref("canvas.image.cache.limit", 0);

// How many images to eagerly decode on a given page. 0 means "no limit".
pref("image.onload.decode.limit", 0);

// WebGL prefs
#ifdef ANDROID
// Disable MSAA on mobile.
pref("gl.msaa-level", 0);
#else
pref("gl.msaa-level", 2);
#endif
pref("gl.require-hardware", false);

pref("webgl.force-enabled", false);
pref("webgl.disabled", false);
pref("webgl.disable-angle", false);
pref("webgl.min_capability_mode", false);
pref("webgl.disable-extensions", false);
pref("webgl.msaa-force", false);
pref("webgl.prefer-16bpp", false);
pref("webgl.default-no-alpha", false);
pref("webgl.force-layers-readback", false);
pref("webgl.lose-context-on-memory-pressure", false);
pref("webgl.can-lose-context-in-foreground", true);
pref("webgl.restore-context-when-visible", true);
pref("webgl.max-warnings-per-context", 32);
pref("webgl.enable-draft-extensions", false);
pref("webgl.enable-privileged-extensions", false);
pref("webgl.bypass-shader-validation", false);
pref("webgl.enable-prototype-webgl2", false);
pref("webgl.disable-fail-if-major-performance-caveat", false);
pref("webgl.disable-debug-renderer-info", false);
pref("webgl.renderer-string-override", "");
pref("webgl.vendor-string-override", "");

#ifdef XP_WIN
pref("webgl.angle.try-d3d11", true);
pref("webgl.angle.force-d3d11", false);
pref("webgl.angle.force-warp", false);
#endif

#ifdef MOZ_WIDGET_GONK
pref("gfx.gralloc.fence-with-readpixels", false);
#endif

// Stagefright prefs
pref("stagefright.force-enabled", false);
pref("stagefright.disabled", false);

#ifdef XP_WIN
// The default TCP send window on Windows is too small, and autotuning only occurs on receive
pref("network.tcp.sendbuffer", 131072);
#endif
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
#ifdef XP_UNIX && !defined(XP_MACOSX)
pref("network.tcp.keepalive.probe_count", 4);
#endif

// Whether to disable acceleration for all widgets.
pref("layers.acceleration.disabled", false);
// Preference that when switched at runtime will run a series of benchmarks
// and output the result to stderr.
pref("layers.bench.enabled", false);

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
#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
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
pref("layers.frame-counter", false);
pref("layers.enable-tiles", false);
pref("layers.tiled-drawtarget.enabled", false);
pref("layers.low-precision-buffer", false);
pref("layers.progressive-paint", false);
pref("layers.tile-width", 256);
pref("layers.tile-height", 256);
// Max number of layers per container. See Overwrite in mobile prefs.
pref("layers.max-active", -1);
// If this is set the tile size will only be treated as a suggestion.
// On B2G we will round this to the stride of the underlying allocation.
// On any platform we may later use the screen size and ignore
// tile-width/tile-height entirely. Its recommended to turn this off
// if you change the tile size.
pref("layers.tiles.adjust", true);

// Set the default values, and then override per-platform as needed
pref("layers.offmainthreadcomposition.enabled", true);
// Compositor target frame rate. NOTE: If vsync is enabled the compositor
// frame rate will still be capped.
// -1 -> default (match layout.frame_rate or 60 FPS)
// 0  -> full-tilt mode: Recomposite even if not transaction occured.
pref("layers.offmainthreadcomposition.frame-rate", -1);

#ifdef MOZ_WIDGET_UIKIT
pref("layers.async-pan-zoom.enabled", true);
#endif

#ifdef MOZ_WIDGET_UIKIT
pref("layers.async-pan-zoom.enabled", true);
#endif

#ifdef XP_MACOSX
pref("layers.enable-tiles", true);
pref("layers.tiled-drawtarget.enabled", true);
pref("layers.tiles.edge-padding", false);
#endif

#ifdef MOZ_WIDGET_GONK
pref("layers.tiled-drawtarget.enabled", true);
#endif

// same effect as layers.offmainthreadcomposition.enabled, but specifically for
// use with tests.
pref("layers.offmainthreadcomposition.testing.enabled", false);

// whether to allow use of the basic compositor
pref("layers.offmainthreadcomposition.force-basic", false);

// Whether to animate simple opacity and transforms on the compositor
pref("layers.offmainthreadcomposition.async-animations", true);

// Whether to log information about off main thread animations to stderr
pref("layers.offmainthreadcomposition.log-animations", false);

pref("layers.bufferrotation.enabled", true);

pref("layers.componentalpha.enabled", true);

#ifdef ANDROID
pref("gfx.apitrace.enabled",false);
#endif

#ifdef MOZ_X11
#ifdef MOZ_WIDGET_GTK
pref("gfx.xrender.enabled",true);
#endif
#endif

#ifdef XP_WIN
// Whether to disable the automatic detection and use of direct2d.
pref("gfx.direct2d.disabled", false);
pref("gfx.direct2d.use1_1", true);

// Whether to attempt to enable Direct2D regardless of automatic detection or
// blacklisting
pref("gfx.direct2d.force-enabled", false);

pref("layers.prefer-opengl", false);
pref("layers.prefer-d3d9", false);
pref("layers.d3d11.force-warp", false);
pref("layers.d3d11.disable-warp", false);
#endif

// Force all possible layers to be always active layers
pref("layers.force-active", false);

// Never use gralloc surfaces, even when they're available on this
// platform and are the optimal surface type.
pref("layers.gralloc.disable", false);

// Don't use compositor-lru on this platform
pref("layers.compositor-lru-size", 0);

// Enable/Disable the geolocation API for content
pref("geo.enabled", true);

// Enable/Disable the orientation API for content
pref("device.sensors.enabled", true);

// Enable/Disable the device storage API for content
pref("device.storage.enabled", false);

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
// Only Firefox requires add-on signatures
pref("xpinstall.signatures.required", false);
pref("extensions.alwaysUnpack", false);
pref("extensions.minCompatiblePlatformVersion", "2.0");

pref("network.buffer.cache.count", 24);
pref("network.buffer.cache.size",  32768);

// Desktop Notification
pref("notification.feature.enabled", false);

// Web Notification
pref("dom.webnotifications.enabled", true);

// Alert animation effect, name is disableSlidingEffect for backwards-compat.
pref("alerts.disableSlidingEffect", false);

// DOM full-screen API.
pref("full-screen-api.enabled", false);
pref("full-screen-api.allow-trusted-requests-only", true);
pref("full-screen-api.pointer-lock.enabled", true);
// transition duration of fade-to-black and fade-from-black, unit: ms
pref("full-screen-api.transition-duration.enter", "400 400");
pref("full-screen-api.transition-duration.leave", "400 400");

// DOM idle observers API
pref("dom.idle-observers-api.enabled", true);

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

// Image srcset
pref("dom.image.srcset.enabled", true);

// <picture> element and sizes
pref("dom.image.picture.enabled", true);

// WebSMS
pref("dom.sms.enabled", false);
// Enable Latin characters replacement with corresponding ones in GSM SMS
// 7-bit default alphabet.
pref("dom.sms.strict7BitEncoding", false);
pref("dom.sms.requestStatusReport", true);
// Numeric default service id for SMS API calls with |serviceId| parameter
// omitted.
pref("dom.sms.defaultServiceId", 0);
// MobileMessage GetMessages/GetThreads read ahead aggressiveness.
//
// positive: finite read-ahead entries,
// 0: don't read ahead unless explicitly requested, (default)
// negative: read ahead all IDs if possible.
pref("dom.sms.maxReadAheadEntries", 0);

// WebContacts
pref("dom.mozContacts.enabled", false);

// WebAlarms
pref("dom.mozAlarms.enabled", false);

// Push

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID)
// Desktop prefs
#ifdef RELEASE_BUILD
pref("dom.push.enabled", false);
#else
pref("dom.push.enabled", true);
#endif
#else
// Mobile prefs
pref("dom.push.enabled", false);
#endif

pref("dom.push.debug", false);
pref("dom.push.serverURL", "wss://push.services.mozilla.com/");
pref("dom.push.userAgentID", "");

// The maximum number of notifications that a service worker can receive
// without user interaction.
pref("dom.push.maxQuotaPerSubscription", 16);

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
pref("dom.push.pingInterval.default", 180000);// 3 min
pref("dom.push.pingInterval.mobile", 180000); // 3 min
pref("dom.push.pingInterval.wifi", 180000);  // 3 min

// Adaptive ping
pref("dom.push.adaptive.enabled", false);
pref("dom.push.adaptive.lastGoodPingInterval", 180000);// 3 min
pref("dom.push.adaptive.lastGoodPingInterval.mobile", 180000);// 3 min
pref("dom.push.adaptive.lastGoodPingInterval.wifi", 180000);// 3 min
// Valid gap between the biggest good ping and the bad ping
pref("dom.push.adaptive.gap", 60000); // 1 minute
// We limit the ping to this maximum value
pref("dom.push.adaptive.upperLimit", 1740000); // 29 min

// enable udp wakeup support
pref("dom.push.udp.wakeupEnabled", false);

// WebPush prefs:
pref("dom.push.http2.reset_retry_count_after_ms", 60000);
pref("dom.push.http2.maxRetries", 2);
pref("dom.push.http2.retryInterval", 5000);

// WebNetworkStats
pref("dom.mozNetworkStats.enabled", false);

// WebSettings
pref("dom.mozSettings.enabled", false);
pref("dom.mozPermissionSettings.enabled", false);

// W3C touch events
// 0 - disabled, 1 - enabled, 2 - autodetect (win)
#ifdef XP_WIN
pref("dom.w3c_touch_events.enabled", 2);
#endif

// W3C draft pointer events
pref("dom.w3c_pointer_events.enabled", false);
// W3C touch-action css property (related to touch and pointer events)
pref("layout.css.touch_action.enabled", false);

// W3C draft ImageCapture API
pref("dom.imagecapture.enabled", false);

// Enables some assertions in nsStyleContext that are too expensive
// for general use, but might be useful to enable for specific tests.
// This only has an effect in DEBUG-builds.
pref("layout.css.expensive-style-struct-assertions.enabled", false);

// enable JS dump() function.
pref("browser.dom.window.dump.enabled", false);

#if defined(MOZ_WIDGET_GONK) || defined(MOZ_WIDGET_ANDROID)
// Network Information API
pref("dom.netinfo.enabled", true);
#else
pref("dom.netinfo.enabled", false);
#endif

#ifdef XP_WIN
// On 32-bit Windows, fire a low-memory notification if we have less than this
// many mb of virtual address space available.
pref("memory.low_virtual_memory_threshold_mb", 128);

// On Windows 32- or 64-bit, fire a low-memory notification if we have less
// than this many mb of commit space (physical memory plus page file) left.
pref("memory.low_commit_space_threshold_mb", 128);

// On Windows 32- or 64-bit, fire a low-memory notification if we have less
// than this many mb of physical memory available on the whole machine.
pref("memory.low_physical_memory_threshold_mb", 0);

// On Windows 32- or 64-bit, don't fire a low-memory notification because of
// low available physical memory or low commit space more than once every
// low_memory_notification_interval_ms.
pref("memory.low_memory_notification_interval_ms", 10000);
#endif

// How long must we wait before declaring that a window is a "ghost" (i.e., a
// likely leak)?  This should be longer than it usually takes for an eligible
// window to be collected via the GC/CC.
pref("memory.ghost_window_timeout_seconds", 60);

// Disable freeing dirty pages when minimizing memory.
pref("memory.free_dirty_pages", false);

// Disable the Linux-specific, system-wide memory reporter.
#ifdef XP_LINUX
pref("memory.system_memory_reporter", false);
#endif

// Don't dump memory reports on OOM, by default.
pref("memory.dump_reports_on_oom", false);

// Number of stack frames to capture in createObjectURL for about:memory.
pref("memory.blob_report.stack_frames", 0);

// comma separated list of domain origins (e.g. https://domain.com) that still
// need localStorage in the frameworker
pref("social.whitelist", "https://mozsocial.cliqz.com");
// comma separated list of domain origins (e.g. https://domain.com) for
// directory websites (e.g. AMO) that can install providers for other sites
pref("social.directories", "https://activations.cdn.mozilla.net");
// remote-install allows any website to activate a provider, with extended UI
// notifying user of installation. we can later pref off remote install if
// necessary. This does not affect whitelisted and directory installs.
pref("social.remote-install.enabled", true);
pref("social.toast-notifications.enabled", true);

// Disable idle observer fuzz, because only privileged content can access idle
// observers (bug 780507).
pref("dom.idle-observers-api.fuzz_time.disabled", true);

// Lowest localId for apps.
pref("dom.mozApps.maxLocalId", 1000);

// Reset apps permissions
pref("dom.apps.reset-permissions", false);

// XXX Security: You CANNOT safely add a new app store for
// installing privileged apps just by modifying this pref and
// adding the signing cert for that store to the cert trust
// database. *Any* origin listed can install apps signed with
// *any* certificate trusted; we don't try to maintain a strong
// association between certificate with installOrign. The
// expectation here is that in production builds the pref will
// contain exactly one origin. However, in custom development
// builds it may contain more than one origin so we can test
// different stages (dev, staging, prod) of the same app store.
pref("dom.mozApps.signed_apps_installable_from", "https://marketplace.firefox.com");

// Whether or not to dump mozApps debug messages to the console.
// Only checked on startup, so restart after changing this pref.
// Ignored on Android, where we always report debug messages because it's
// unnecessary to restrict reporting, per bug 1003469.
pref("dom.mozApps.debug", false);

// Minimum delay in milliseconds between network activity notifications (0 means
// no notifications). The delay is the same for both download and upload, though
// they are handled separately. This pref is only read once at startup:
// a restart is required to enable a new value.
pref("network.activity.blipIntervalMilliseconds", 0);

// If true, reuse the same global for everything loaded by the component loader
// (JS components, JSMs, etc).  This saves memory, but makes it possible for
// the scripts to interfere with each other.  A restart is required for this
// to take effect.
pref("jsloader.reuseGlobal", false);

// When we're asked to take a screenshot, don't wait more than 2000ms for the
// event loop to become idle before actually taking the screenshot.
pref("dom.browserElement.maxScreenshotDelayMS", 2000);

// Whether we should show the placeholder when the element is focused but empty.
pref("dom.placeholder.show_on_focus", true);

pref("dom.vr.enabled", false);
// 0 = never; 1 = only if real devices aren't there; 2 = always
pref("dom.vr.add-test-devices", 1);

// MMS UA Profile settings
pref("wap.UAProf.url", "");
pref("wap.UAProf.tagname", "x-wap-profile");

// MMS version 1.1 = 0x11 (or decimal 17)
// MMS version 1.3 = 0x13 (or decimal 19)
// @see OMA-TS-MMS_ENC-V1_3-20110913-A clause 7.3.34
pref("dom.mms.version", 19);

pref("dom.mms.requestStatusReport", true);

// Retrieval mode for MMS
// manual: Manual retrieval mode.
// automatic: Automatic retrieval mode even in roaming.
// automatic-home: Automatic retrieval mode in home network.
// never: Never retrieval mode.
pref("dom.mms.retrieval_mode", "manual");

pref("dom.mms.sendRetryCount", 3);
pref("dom.mms.sendRetryInterval", "10000,60000,180000");

pref("dom.mms.retrievalRetryCount", 4);
pref("dom.mms.retrievalRetryIntervals", "60000,300000,600000,1800000");
// Numeric default service id for MMS API calls with |serviceId| parameter
// omitted.
pref("dom.mms.defaultServiceId", 0);
// Debug enabler for MMS.
pref("mms.debugging.enabled", false);

// Request read report while sending MMS.
pref("dom.mms.requestReadReport", true);

// Number of RadioInterface instances to create.
pref("ril.numRadioInterfaces", 0);

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

pref("captivedetect.canonicalURL", "http://detectportal.firefox.com/success.txt");
pref("captivedetect.canonicalContent", "success\n");
pref("captivedetect.maxWaitingTime", 5000);
pref("captivedetect.pollingTime", 3000);
pref("captivedetect.maxRetryCount", 5);

#ifdef RELEASE_BUILD
pref("dom.forms.inputmode", false);
#else
pref("dom.forms.inputmode", true);
#endif

// InputMethods for soft keyboards in B2G
pref("dom.mozInputMethod.enabled", false);

// DataStore is disabled by default
pref("dom.datastore.enabled", false);

// Telephony API
#ifdef MOZ_B2G_RIL
pref("dom.telephony.enabled", true);
#else
pref("dom.telephony.enabled", false);
#endif
// Numeric default service id for WebTelephony API calls with |serviceId|
// parameter omitted.
pref("dom.telephony.defaultServiceId", 0);

// Cell Broadcast API
#ifdef MOZ_B2G_RIL
pref("dom.cellbroadcast.enabled", true);
#else
pref("dom.cellbroadcast.enabled", false);
#endif

// ICC API
#ifdef MOZ_B2G_RIL
pref("dom.icc.enabled", true);
#else
pref("dom.icc.enabled", false);
#endif

// Mobile Connection API
#ifdef MOZ_B2G_RIL
pref("dom.mobileconnection.enabled", true);
#else
pref("dom.mobileconnection.enabled", false);
#endif

// Voice Mail API
#ifdef MOZ_B2G_RIL
pref("dom.voicemail.enabled", true);
#else
pref("dom.voicemail.enabled", false);
#endif
// Numeric default service id for Voice Mail API calls with |serviceId|
// parameter omitted.
pref("dom.voicemail.defaultServiceId", 0);

// DOM Inter-App Communication API.
pref("dom.inter-app-communication-api.enabled", false);

// Disable mapped array buffer by default.
pref("dom.mapped_arraybuffer.enabled", false);

// The tables used for Safebrowsing phishing and malware checks.
pref("urlclassifier.malwareTable", "goog-malware-shavar,goog-unwanted-shavar,test-malware-simple,test-unwanted-simple");
pref("urlclassifier.phishTable", "goog-phish-shavar,test-phish-simple");
pref("urlclassifier.downloadBlockTable", "");
pref("urlclassifier.downloadAllowTable", "");
pref("urlclassifier.disallow_completions", "test-malware-simple,test-phish-simple,test-unwanted-simple,test-track-simple,goog-downloadwhite-digest256,mozpub-track-digest256");

// The table and update/gethash URLs for Safebrowsing phishing and malware
// checks.
pref("urlclassifier.trackingTable", "test-track-simple,mozpub-track-digest256");
pref("browser.trackingprotection.updateURL", "https://tracking.services.mozilla.com/downloads?client=SAFEBROWSING_ID&appver=%VERSION%&pver=2.2");
pref("browser.trackingprotection.gethashURL", "https://tracking.services.mozilla.com/gethash?client=SAFEBROWSING_ID&appver=%VERSION%&pver=2.2");

// Turn off Spatial navigation by default.
pref("snav.enabled", false);

// Original caret implementation on collapsed selection.
pref("touchcaret.enabled", false);

// This will inflate the size of the touch caret frame when checking if user
// clicks on the caret or not. In app units.
pref("touchcaret.inflatesize.threshold", 40);

// We'll start to increment time when user release the control of touch caret.
// When time exceed this expiration time, we'll hide touch caret.
// In milliseconds. (0 means disable this feature)
pref("touchcaret.expiration.time", 3000);

// Original caret implementation on non-collapsed selection.
pref("selectioncaret.enabled", false);

// This will inflate size of selection caret frame when we checking if
// user click on selection caret or not. In app units.
pref("selectioncaret.inflatesize.threshold", 40);

// Selection carets will fall-back to internal LongTap detector.
pref("selectioncaret.detects.longtap", true);

// Selection carets do not affect caret visibility.
pref("selectioncaret.visibility.affectscaret", false);

// Selection caret visibility does not observe composition
// selections generated by soft keyboard managers.
pref("selectioncaret.observes.compositions", false);

// The Touch caret by default observes the b2g visibility rules, and
// not the extended Android visibility rules that allow for touchcaret
// display in empty editable fields, for example.
pref("touchcaret.extendedvisibility", false);

// Desktop and b2g don't need to open or close the Android
// TextSelection (Actionbar) utility.
pref("caret.manages-android-actionbar", false);

// New implementation to unify touch-caret and selection-carets.
pref("layout.accessiblecaret.enabled", false);

// CSS attributes of the AccessibleCaret in CSS pixels.
pref("layout.accessiblecaret.width", "44.0");
pref("layout.accessiblecaret.height", "47.0");
pref("layout.accessiblecaret.margin-left", "-23.5");
pref("layout.accessiblecaret.bar.width", "2.0");

// Timeout in milliseconds to hide the accessiblecaret under cursor mode while
// no one touches it. Set the value to 0 to disable this feature.
pref("layout.accessiblecaret.timeout_ms", 3000);

// Wakelock is disabled by default.
pref("dom.wakelock.enabled", false);

// The URL of the Firefox Accounts auth server backend
pref("identity.fxaccounts.auth.uri", "https://api.accounts.firefox.com/v1");

// disable mozsample size for now
pref("image.mozsamplesize.enabled", false);

// Enable navigator.sendBeacon on all platforms except b2g because it doesn't
// play nicely with Firefox OS apps yet.
#ifndef MOZ_WIDGET_GONK
pref("beacon.enabled", true);
#endif

// Camera prefs
pref("camera.control.face_detection.enabled", true);


// SW Cache API
pref("dom.caches.enabled", true);

#ifdef MOZ_WIDGET_GONK
// Empirically, this is the value returned by hal::GetTotalSystemMemory()
// when Flame's memory is limited to 512MiB. If the camera stack determines
// it is running on a low memory platform, features that can be reliably
// supported will be disabled. This threshold can be adjusted to suit other
// platforms; and set to 0 to disable the low-memory check altogether.
pref("camera.control.low_memory_thresholdMB", 404);
#endif

// SystemUpdate API
pref("dom.system_update.enabled", false);
pref("dom.system_update.debug", false);

// UDPSocket API
pref("dom.udpsocket.enabled", false);

// MessageChannel enabled by default.
pref("dom.messageChannel.enabled", true);

// Disable before keyboard events and after keyboard events by default.
pref("dom.beforeAfterKeyboardEvent.enabled", false);

// Presentation API
pref("dom.presentation.enabled", false);
pref("dom.presentation.tcp_server.debug", false);

#ifdef XP_MACOSX
// Use raw ICU instead of CoreServices API in Unicode collation
pref("intl.collation.mac.use_icu", true);

// Enable NSTextInput protocol for use with IMEs that have not
// been updated to use the NSTextInputClient protocol.
pref("intl.ime.nstextinput.enable", false);
#endif

// Enable meta-viewport support in remote APZ-enabled frames.
pref("dom.meta-viewport.enabled", false);

// MozSettings debugging prefs for each component
pref("dom.mozSettings.SettingsDB.debug.enabled", false);
pref("dom.mozSettings.SettingsManager.debug.enabled", false);
pref("dom.mozSettings.SettingsRequestManager.debug.enabled", false);
pref("dom.mozSettings.SettingsService.debug.enabled", false);

// MozSettings verbose mode to track everything
pref("dom.mozSettings.SettingsDB.verbose.enabled", false);
pref("dom.mozSettings.SettingsManager.verbose.enabled", false);
pref("dom.mozSettings.SettingsRequestManager.verbose.enabled", false);
pref("dom.mozSettings.SettingsService.verbose.enabled", false);

// Controlling whether we want to allow forcing some Settings
// IndexedDB transactions to be opened as readonly or keep everything as
// readwrite.
pref("dom.mozSettings.allowForceReadOnly", false);

// The interval at which to check for slow running addons
#ifdef NIGHTLY_BUILD
pref("browser.addon-watch.interval", 15000);
#else
pref("browser.addon-watch.interval", -1);
#endif
pref("browser.addon-watch.ignore", "[\"mochikit@mozilla.org\",\"special-powers@mozilla.org\",\"fxdevtools-adapters@mozilla.org\",\"fx-devtools\"]");
// the percentage of time addons are allowed to use without being labeled slow
pref("browser.addon-watch.percentage-limit", 5);

// RequestSync API is disabled by default.
pref("dom.requestSync.enabled", false);

// Search service settings
pref("browser.search.log", false);
pref("browser.search.update", true);
pref("browser.search.update.log", false);
pref("browser.search.update.interval", 21600);
pref("browser.search.suggest.enabled", true);
pref("browser.search.geoSpecificDefaults", false);
pref("browser.search.geoip.url", "https://location.services.mozilla.com/v1/country?key=%MOZILLA_API_KEY%");
// NOTE: this timeout figure is also the "high" value for the telemetry probe
// SEARCH_SERVICE_COUNTRY_FETCH_MS - if you change this also change that probe.
pref("browser.search.geoip.timeout", 2000);

#ifdef MOZ_OFFICIAL_BRANDING
// {moz:official} expands to "official"
pref("browser.search.official", true);
#endif

#ifndef MOZ_WIDGET_GONK
// GMPInstallManager prefs

// User-settable override to media.gmp-manager.url for testing purposes.
//pref("media.gmp-manager.url.override", "");

// Update service URL for GMP install/updates:
pref("media.gmp-manager.url", "https://aus4.mozilla.org/update/3/GMP/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/update.xml");

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
pref("media.gmp-manager.certs.1.issuerName", "CN=DigiCert Secure Server CA,O=DigiCert Inc,C=US");
pref("media.gmp-manager.certs.1.commonName", "aus4.mozilla.org");
pref("media.gmp-manager.certs.2.issuerName", "CN=Thawte SSL CA,O=\"Thawte, Inc.\",C=US");
pref("media.gmp-manager.certs.2.commonName", "aus4.mozilla.org");
#endif

// Whether or not to perform reader mode article parsing on page load.
// If this pref is disabled, we will never show a reader mode icon in the toolbar.
pref("reader.parse-on-load.enabled", true);

// After what size document we don't bother running Readability on it
// because it'd slow things down too much
pref("reader.parse-node-limit", 3000);

// Force-enables reader mode parsing, even on low-memory platforms, where it
// is disabled by default.
pref("reader.parse-on-load.force-enabled", false);

// The default relative font size in reader mode (1-9)
pref("reader.font_size", 5);

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

#if defined(XP_LINUX) && defined(MOZ_GMP_SANDBOX)
// Whether to allow, on a Linux system that doesn't support the necessary sandboxing
// features, loading Gecko Media Plugins unsandboxed.  However, EME CDMs will not be
// loaded without sandboxing even if this pref is changed.
pref("media.gmp.insecure.allow", false);
#endif

pref("dom.audiochannel.mutedByDefault", false);

// Use vsync aligned rendering. b2g prefs are in b2g.js.
// Hardware vsync supported on windows, os x, and b2g.
// Linux and fennec will use software vsync.
pref("gfx.vsync.hw-vsync.enabled", true);
pref("gfx.vsync.compositor", true);
pref("gfx.vsync.refreshdriver", true);

// Secure Element API
#ifdef MOZ_SECUREELEMENT
pref("dom.secureelement.enabled", false);
#endif

// Allow control characters appear in composition string.
// When this is false, control characters except
// CHARACTER TABULATION (horizontal tab) are removed from
// both composition string and data attribute of compositionupdate
// and compositionend events.
pref("dom.compositionevent.allow_control_characters", false);

#ifdef MOZ_WIDGET_GONK
// Bug 1154053: Serialize B2G memory reports; smaller devices are
// usually overcommitted on memory by using zRAM, so memory reporting
// causes memory pressure from uncompressing cold heap memory.
pref("memory.report_concurrency", 1);
#else
// Desktop probably doesn't have swapped-out children like that.
pref("memory.report_concurrency", 10);
#endif

// Make <audio>, <video>, NPAPI plugins and webAudio talk to the AudioChannelService.
pref("media.useAudioChannelService", true);
// Add Mozilla AudioChannel APIs.
pref("media.useAudioChannelAPI", false);
