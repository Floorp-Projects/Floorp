/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file lists Telemetry histograms collected by Mozilla. The format is
 *
 *    HISTOGRAM(id, minimum, maximum, bucket count, histogram kind,
 *              human-readable description for about:telemetry)
 *
 * This file is the master list of telemetry histograms reported to Mozilla servers.
 * The other data reported by telemetry is listed on https://wiki.mozilla.org/Privacy/Reviews/Telemetry/Measurements
 *
 * Please note that only BOOLEAN histograms are allowed to have a minimum value
 * of zero, and that bucket counts should be >= 3.
 */

/* Convenience macro for BOOLEAN histograms. */
#define HISTOGRAM_BOOLEAN(id, message) HISTOGRAM(id, 0, 1, 2, BOOLEAN, message)
/* Likewise for FLAG histograms. */
#define HISTOGRAM_FLAG(id, message) HISTOGRAM(id, 0, 1, 2, FLAG, message)
/* Convenience macro for LINEAR histograms where we want buckets from 1 to N, inclusive. */
#define HISTOGRAM_ENUMERATED_VALUES(id, n, message) \
  HISTOGRAM(id, 1, n, n+1, LINEAR, message)

/**
 * a11y telemetry
 */
HISTOGRAM_FLAG(A11Y_INSTANTIATED_FLAG, "has accessibility support been instantiated")
HISTOGRAM_ENUMERATED_VALUES(A11Y_CONSUMERS, 11, "Accessibility client by enum id")
HISTOGRAM_FLAG(A11Y_ISIMPLEDOM_USAGE_FLAG, "have the ISimpleDOM* accessibility interfaces been used")
HISTOGRAM_FLAG(A11Y_IATABLE_USAGE_FLAG, "has the IAccessibleTable accessibility interface been used")
HISTOGRAM_FLAG(A11Y_XFORMS_USAGE_FLAG, "has XForms accessibility been instantiated")

/**
 * Cycle collector telemetry
 */
HISTOGRAM(CYCLE_COLLECTOR, 1, 10000, 50, EXPONENTIAL, "Time spent on one cycle collection (ms)")
HISTOGRAM(CYCLE_COLLECTOR_VISITED_REF_COUNTED, 1, 300000, 50, EXPONENTIAL, "Number of ref counted objects visited by the cycle collector")
HISTOGRAM(CYCLE_COLLECTOR_VISITED_GCED, 1, 300000, 50, EXPONENTIAL, "Number of JS objects visited by the cycle collector")
HISTOGRAM(CYCLE_COLLECTOR_COLLECTED, 1, 100000, 50, EXPONENTIAL, "Number of objects collected by the cycle collector")
HISTOGRAM_BOOLEAN(CYCLE_COLLECTOR_NEED_GC, "Needed garbage collection before cycle collection.")
HISTOGRAM(CYCLE_COLLECTOR_TIME_BETWEEN, 1, 120, 50, EXPONENTIAL, "Time spent in between cycle collections (seconds)")
HISTOGRAM(CYCLE_COLLECTOR_CONTENT_UNBIND, 1, 10000, 50, EXPONENTIAL, "Time spent on one ContentUnbinder (ms)")
HISTOGRAM(FORGET_SKIPPABLE_MAX, 1, 10000, 50, EXPONENTIAL, "Max time spent on one forget skippable (ms)")

/**
 * GC telemetry
 */
HISTOGRAM(GC_REASON, 1, 20, 20, LINEAR, "Reason (enum value) for initiating a GC")
HISTOGRAM_BOOLEAN(GC_IS_COMPARTMENTAL, "Is it a compartmental GC?")
HISTOGRAM(GC_MS, 1, 10000, 50, EXPONENTIAL, "Time spent running JS GC (ms)")
HISTOGRAM(GC_MARK_MS, 1, 10000, 50, EXPONENTIAL, "Time spent running JS GC mark phase (ms)")
HISTOGRAM(GC_SWEEP_MS, 1, 10000, 50, EXPONENTIAL, "Time spent running JS GC sweep phase (ms)")
HISTOGRAM(GC_SLICE_MS, 1, 10000, 50, EXPONENTIAL, "Time spent running a JS GC slice (ms)")
HISTOGRAM(GC_MMU_50, 1, 100, 20, LINEAR, "Minimum percentage of time spent outside GC over any 50ms window")
HISTOGRAM_BOOLEAN(GC_RESET, "Was an incremental GC canceled?")
HISTOGRAM_BOOLEAN(GC_INCREMENTAL_DISABLED, "Is incremental GC permanently disabled?")
HISTOGRAM_BOOLEAN(GC_NON_INCREMENTAL, "Was the GC non-incremental?")

HISTOGRAM(TELEMETRY_PING, 1, 3000, 10, EXPONENTIAL, "Time taken to submit telemetry info (ms)")
HISTOGRAM_BOOLEAN(TELEMETRY_SUCCESS,  "Successful telemetry submission")
HISTOGRAM(MEMORY_JS_COMPARTMENTS_SYSTEM, 1, 1000, 50, EXPONENTIAL, "Total JavaScript compartments used for add-ons and internals.")
HISTOGRAM(MEMORY_JS_COMPARTMENTS_USER, 1, 1000, 50, EXPONENTIAL, "Total JavaScript compartments used for web pages")
HISTOGRAM(MEMORY_JS_GC_HEAP, 1024, 512 * 1024, 50, EXPONENTIAL, "Memory used by the garbage-collected JavaScript heap (KB)")
HISTOGRAM(MEMORY_RESIDENT, 32 * 1024, 1024 * 1024, 50, EXPONENTIAL, "Resident memory size (KB)")
HISTOGRAM(MEMORY_STORAGE_SQLITE, 1024, 512 * 1024, 50, EXPONENTIAL, "Memory used by SQLite (KB)")
HISTOGRAM(MEMORY_IMAGES_CONTENT_USED_UNCOMPRESSED, 1024, 1024 * 1024, 50, EXPONENTIAL, "Memory used for uncompressed, in-use content images (KB)")
HISTOGRAM(MEMORY_HEAP_ALLOCATED, 1024, 1024 * 1024, 50, EXPONENTIAL, "Heap memory allocated (KB)")
HISTOGRAM(MEMORY_HEAP_COMMITTED_UNUSED, 1024, 512 * 1024, 50, EXPONENTIAL, "Committed, unused heap memory (KB)")
HISTOGRAM(MEMORY_HEAP_COMMITTED_UNUSED_RATIO, 1, 100, 25, LINEAR, "Ratio of committed, unused memory to allocated memory in the heap (percentage).")
HISTOGRAM(MEMORY_EXPLICIT, 1024, 1024 * 1024, 50, EXPONENTIAL, "Explicit memory allocations (KB)")
HISTOGRAM(GHOST_WINDOWS, 1, 128, 8, EXPONENTIAL, "Number of ghost windows")
#if defined(XP_MACOSX)
HISTOGRAM(MEMORY_FREE_PURGED_PAGES_MS, 1, 1024, 10, EXPONENTIAL, "Time(ms) to purge MADV_FREE'd heap pages.")
#elif defined(XP_WIN)
HISTOGRAM(LOW_MEMORY_EVENTS_VIRTUAL, 1, 1024, 21, EXPONENTIAL, "Number of low-virtual-memory events fired since last ping")
HISTOGRAM(LOW_MEMORY_EVENTS_PHYSICAL, 1, 1024, 21, EXPONENTIAL, "Number of low-physical-memory events fired since last ping")
HISTOGRAM(LOW_MEMORY_EVENTS_COMMIT_SPACE, 1, 1024, 21, EXPONENTIAL, "Number of low-commit-space events fired since last ping")
#endif

#if defined(XP_WIN)
HISTOGRAM(EARLY_GLUESTARTUP_READ_OPS, 1, 100, 12, LINEAR, "ProcessIoCounters.ReadOperationCount before glue startup")
HISTOGRAM(EARLY_GLUESTARTUP_READ_TRANSFER, 1, 50 * 1024, 12, EXPONENTIAL, "ProcessIoCounters.ReadTransferCount before glue startup (KB)")
HISTOGRAM(GLUESTARTUP_READ_OPS, 1, 100, 12, LINEAR, "ProcessIoCounters.ReadOperationCount after glue startup")
HISTOGRAM(GLUESTARTUP_READ_TRANSFER, 1, 50 * 1024, 12, EXPONENTIAL, "ProcessIoCounters.ReadTransferCount after glue startup (KB)")
HISTOGRAM(STARTUP_USING_PRELOAD, 1, 2, 3, LINEAR, "Preload heuristic. 0: none, 1: preload from service, 2: preload from ioCounters.ReadOperationCount")
#elif defined(XP_UNIX)
HISTOGRAM(EARLY_GLUESTARTUP_HARD_FAULTS, 1, 100, 12, LINEAR, "Hard faults count before glue startup")
HISTOGRAM(GLUESTARTUP_HARD_FAULTS, 1, 500, 12, EXPONENTIAL, "Hard faults count after glue startup")
HISTOGRAM(PAGE_FAULTS_HARD, 8, 64 * 1024, 13, EXPONENTIAL, "Hard page faults (since last telemetry ping)")
#endif
HISTOGRAM(FONTLIST_INITOTHERFAMILYNAMES, 1, 30000, 50, EXPONENTIAL, "Time(ms) spent on reading other family names from all fonts")
HISTOGRAM(FONTLIST_INITFACENAMELISTS, 1, 30000, 50, EXPONENTIAL, "Time(ms) spent on reading family names from all fonts")
#if defined(XP_WIN)
HISTOGRAM(DWRITEFONT_INITFONTLIST_TOTAL, 1, 30000, 10, EXPONENTIAL, "gfxDWriteFontList::InitFontList Total (ms)")
HISTOGRAM(DWRITEFONT_INITFONTLIST_INIT, 1, 30000, 10, EXPONENTIAL, "gfxDWriteFontList::InitFontList init (ms)")
HISTOGRAM(DWRITEFONT_INITFONTLIST_GDI, 1, 30000, 10, EXPONENTIAL, "gfxDWriteFontList::InitFontList GdiInterop object (ms)")
HISTOGRAM(DWRITEFONT_DELAYEDINITFONTLIST_TOTAL, 1, 30000, 10, EXPONENTIAL, "gfxDWriteFontList::DelayedInitFontList Total (ms)")
HISTOGRAM(DWRITEFONT_DELAYEDINITFONTLIST_COUNT, 1, 10000, 10, EXPONENTIAL, "gfxDWriteFontList::DelayedInitFontList Font Family Count")
HISTOGRAM_BOOLEAN(DWRITEFONT_DELAYEDINITFONTLIST_GDI_TABLE, "gfxDWriteFontList::DelayedInitFontList GDI Table Access")
HISTOGRAM(DWRITEFONT_DELAYEDINITFONTLIST_COLLECT, 1, 30000, 10, EXPONENTIAL, "gfxDWriteFontList::DelayedInitFontList GetSystemFontCollection (ms)")
HISTOGRAM(DWRITEFONT_DELAYEDINITFONTLIST_ITERATE, 1, 30000, 10, EXPONENTIAL, "gfxDWriteFontList::DelayedInitFontList iterate over families (ms)")
HISTOGRAM(GDI_INITFONTLIST_TOTAL, 1, 30000, 10, EXPONENTIAL, "gfxGDIFontList::InitFontList Total (ms)")
#elif defined(XP_MACOSX)
HISTOGRAM(MAC_INITFONTLIST_TOTAL, 1, 30000, 10, EXPONENTIAL, "gfxMacPlatformFontList::InitFontList Total (ms)")
#endif

HISTOGRAM(SYSTEM_FONT_FALLBACK, 1, 100000, 50, EXPONENTIAL, "System font fallback (us)")
HISTOGRAM(SYSTEM_FONT_FALLBACK_FIRST, 1, 40000, 20, EXPONENTIAL, "System font fallback, first call (ms)")
HISTOGRAM(SYSTEM_FONT_FALLBACK_SCRIPT, 1, 110, 111, LINEAR, "System font fallback script")

HISTOGRAM(STARTUP_CACHE_AGE_HOURS, 1, 3000, 20, EXPONENTIAL, "Startup cache age (hours)")

/**
 * Word cache - one count for overall lookups, the other for the number of times a word is found
 * Note: range and number of buckets must match
 */
HISTOGRAM(WORD_CACHE_LOOKUP_LEN, 1, 256, 30, EXPONENTIAL, "Word cache lookup (chars)")
HISTOGRAM(WORD_CACHE_HIT_LEN, 1, 256, 30, EXPONENTIAL, "Word cache hit (chars)")
HISTOGRAM(WORD_CACHE_LOOKUP_SCRIPT, 1, 110, 111, LINEAR, "Word cache lookup (script)")
HISTOGRAM(WORD_CACHE_HIT_SCRIPT, 1, 110, 111, LINEAR, "Word cache hit (script)")

HISTOGRAM_BOOLEAN(FONT_CACHE_HIT, "font cache hit")
HISTOGRAM_BOOLEAN(BAD_FALLBACK_FONT, "system fallback font can't be used")

HISTOGRAM_BOOLEAN(SHUTDOWN_OK, "Did the browser start after a successful shutdown")

HISTOGRAM(IMAGE_DECODE_LATENCY, 50,  5000000, 100, EXPONENTIAL, "Time spent decoding an image chunk (us)")
HISTOGRAM(IMAGE_DECODE_TIME,    50, 50000000, 100, EXPONENTIAL, "Time spent decoding an image (us)")
HISTOGRAM(IMAGE_DECODE_ON_DRAW_LATENCY,  50, 50000000, 100, EXPONENTIAL, "Time from starting a decode to it showing up on the screen (us)")
HISTOGRAM(IMAGE_DECODE_CHUNKS, 1,  500, 50, EXPONENTIAL, "Number of chunks per decode attempt")
HISTOGRAM(IMAGE_DECODE_COUNT, 1,  500, 50, EXPONENTIAL, "Decode count")
HISTOGRAM(IMAGE_DECODE_SPEED_JPEG, 500, 50000000,  50, EXPONENTIAL, "JPEG image decode speed (Kbytes/sec)")
HISTOGRAM(IMAGE_DECODE_SPEED_GIF,  500, 50000000,  50, EXPONENTIAL, "GIF image decode speed (Kbytes/sec)")
HISTOGRAM(IMAGE_DECODE_SPEED_PNG,  500, 50000000,  50, EXPONENTIAL, "PNG image decode speed (Kbytes/sec)")

HISTOGRAM_BOOLEAN(CANVAS_2D_USED, "2D canvas used")
HISTOGRAM_BOOLEAN(CANVAS_WEBGL_USED, "WebGL canvas used")

/**
 * Networking telemetry
 */
HISTOGRAM(TOTAL_CONTENT_PAGE_LOAD_TIME, 100, 30000, 100, EXPONENTIAL, "HTTP: Total page load time (ms)")
HISTOGRAM(HTTP_SUBITEM_OPEN_LATENCY_TIME, 1, 30000, 50, EXPONENTIAL, "HTTP subitem: Page start -> subitem open() (ms)")
HISTOGRAM(HTTP_SUBITEM_FIRST_BYTE_LATENCY_TIME, 1, 30000, 50, EXPONENTIAL, "HTTP subitem: Page start -> first byte received for subitem reply (ms)")
HISTOGRAM(HTTP_REQUEST_PER_PAGE, 1, 1000, 50, EXPONENTIAL, "HTTP: Requests per page (count)")
HISTOGRAM(HTTP_REQUEST_PER_PAGE_FROM_CACHE, 1, 101, 102, LINEAR, "HTTP: Requests serviced from cache (%)")
HISTOGRAM(HTTP_REQUEST_PER_CONN, 1, 1000, 50, EXPONENTIAL, "HTTP: requests per connection")
HISTOGRAM(HTTP_KBREAD_PER_CONN, 1, 3000, 50, EXPONENTIAL, "HTTP: KB read per connection")

#define _HTTP_HIST(name, label) \
  HISTOGRAM(name, 1, 30000, 50, EXPONENTIAL, "HTTP " label) \

#define HTTP_HISTOGRAMS(prefix, labelprefix) \
  _HTTP_HIST(HTTP_##prefix##_DNS_ISSUE_TIME, labelprefix "open() -> DNS request issued (ms)") \
  _HTTP_HIST(HTTP_##prefix##_DNS_LOOKUP_TIME, labelprefix "DNS lookup time (ms)") \
  _HTTP_HIST(HTTP_##prefix##_TCP_CONNECTION, labelprefix "TCP connection setup (ms)") \
  _HTTP_HIST(HTTP_##prefix##_OPEN_TO_FIRST_SENT, labelprefix "Open -> first byte of request sent (ms)") \
  _HTTP_HIST(HTTP_##prefix##_FIRST_SENT_TO_LAST_RECEIVED, labelprefix "First byte of request sent -> last byte of response received (ms)") \
  _HTTP_HIST(HTTP_##prefix##_OPEN_TO_FIRST_RECEIVED, labelprefix "Open -> first byte of reply received (ms)") \
  _HTTP_HIST(HTTP_##prefix##_OPEN_TO_FIRST_FROM_CACHE, labelprefix "Open -> cache read start (ms)") \
  _HTTP_HIST(HTTP_##prefix##_CACHE_READ_TIME, labelprefix "Cache read time (ms)") \
  _HTTP_HIST(HTTP_##prefix##_REVALIDATION, labelprefix "Positive cache validation time (ms)") \
  _HTTP_HIST(HTTP_##prefix##_COMPLETE_LOAD, labelprefix "Overall load time - all (ms)") \
  _HTTP_HIST(HTTP_##prefix##_COMPLETE_LOAD_CACHED, labelprefix "Overall load time - cache hits (ms)") \
  _HTTP_HIST(HTTP_##prefix##_COMPLETE_LOAD_NET, labelprefix "Overall load time - network (ms)") \

HTTP_HISTOGRAMS(PAGE, "page: ")
HTTP_HISTOGRAMS(SUB, "subitem: ")

HISTOGRAM(SPDY_VERSION, 1, 16, 16, LINEAR, "SPDY: Protocol Version Used")
HISTOGRAM(SPDY_PARALLEL_STREAMS, 1, 1000, 50, EXPONENTIAL, "SPDY: Streams concurrent active per connection")
HISTOGRAM(SPDY_REQUEST_PER_CONN, 1, 1000, 50, EXPONENTIAL,  "SPDY: Streams created per connection")
HISTOGRAM(SPDY_SERVER_INITIATED_STREAMS, 1, 100000, 250, EXPONENTIAL,  "SPDY: Streams recevied per connection")
HISTOGRAM(SPDY_CHUNK_RECVD, 1, 1000, 100, EXPONENTIAL,  "SPDY: Recvd Chunk Size (rounded to KB)")
HISTOGRAM(SPDY_SYN_SIZE, 20, 20000, 50, EXPONENTIAL,  "SPDY: SYN Frame Header Size")
HISTOGRAM(SPDY_SYN_RATIO, 1, 99, 20, LINEAR,  "SPDY: SYN Frame Header Ratio (lower better)")
HISTOGRAM(SPDY_SYN_REPLY_SIZE, 16, 20000, 50, EXPONENTIAL,  "SPDY: SYN Reply Header Size")
HISTOGRAM(SPDY_SYN_REPLY_RATIO, 1, 99, 20, LINEAR,  "SPDY: SYN Reply Header Ratio (lower better)")
HISTOGRAM(SPDY_NPN_CONNECT, 0, 1, 2, BOOLEAN,  "SPDY: NPN Negotiated")
HISTOGRAM(SPDY_NPN_JOIN, 0, 1, 2, BOOLEAN,  "SPDY: Coalesce Succeeded")
HISTOGRAM(SPDY_KBREAD_PER_CONN, 1, 3000, 50, EXPONENTIAL, "SPDY: KB read per connection")
HISTOGRAM(SPDY_PING_EXPERIMENT_PASS, 10, 355, 64, LINEAR, "SPDY: Ping Interval Passed")
HISTOGRAM(SPDY_PING_EXPERIMENT_FAIL, 10, 355, 64, LINEAR, "SPDY: Ping Interval Failed")

HISTOGRAM(SPDY_SETTINGS_UL_BW, 1, 10000, 100, EXPONENTIAL,  "SPDY: Settings Upload Bandwidth")
HISTOGRAM(SPDY_SETTINGS_DL_BW, 1, 10000, 100, EXPONENTIAL,  "SPDY: Settings Download Bandwidth")
HISTOGRAM(SPDY_SETTINGS_RTT, 1, 1000, 100, EXPONENTIAL,  "SPDY: Settings RTT")
HISTOGRAM(SPDY_SETTINGS_MAX_STREAMS, 1, 5000, 100, EXPONENTIAL,  "SPDY: Settings Max Streams parameter")
HISTOGRAM(SPDY_SETTINGS_CWND, 1, 500, 50, EXPONENTIAL,  "SPDY: Settings CWND (packets)")
HISTOGRAM(SPDY_SETTINGS_RETRANS, 1, 100, 50, EXPONENTIAL,  "SPDY: Retransmission Rate")
HISTOGRAM(SPDY_SETTINGS_IW, 1, 1000, 50, EXPONENTIAL,  "SPDY: Settings IW (rounded to KB)")

#undef _HTTP_HIST
#undef HTTP_HISTOGRAMS

HISTOGRAM(HTTP_CACHE_DISPOSITION, 1, 5, 5, LINEAR, "HTTP Cache Hit, Reval, Failed-Reval, Miss")
HISTOGRAM(DISK_CACHE_CORRUPT, 0, 1, 2, BOOLEAN, "Was the HTTP disk cache corrupt at startup?")
HISTOGRAM(HTTP_DISK_CACHE_DISPOSITION, 1, 5, 5, LINEAR, "HTTP Disk Cache Hit, Reval, Failed-Reval, Miss")
HISTOGRAM(HTTP_MEMORY_CACHE_DISPOSITION, 1, 5, 5, LINEAR, "HTTP Memory Cache Hit, Reval, Failed-Reval, Miss")
HISTOGRAM(HTTP_OFFLINE_CACHE_DISPOSITION, 1, 5, 5, LINEAR, "HTTP Offline Cache Hit, Reval, Failed-Reval, Miss")
HISTOGRAM(CACHE_DEVICE_SEARCH, 1, 100, 100, LINEAR, "Time to search cache (ms)")
HISTOGRAM(CACHE_MEMORY_SEARCH, 1, 100, 100, LINEAR, "Time to search memory cache (ms)")
HISTOGRAM(CACHE_DISK_SEARCH, 1, 100, 100, LINEAR, "Time to search disk cache (ms)")
HISTOGRAM(CACHE_OFFLINE_SEARCH, 1, 100, 100, LINEAR, "Time to search offline cache (ms)")
HISTOGRAM(HTTP_DISK_CACHE_OVERHEAD, 1, 32000000, 100, EXPONENTIAL, "HTTP Disk cache memory overhead (bytes)")
HISTOGRAM(CACHE_LM_INCONSISTENT, 0, 1, 2, BOOLEAN,  "Cache discovered inconsistent last-modified entry")
HISTOGRAM(CACHE_SERVICE_LOCK_WAIT, 1, 10000, 10000, LINEAR, "Time spent waiting on the cache service lock (ms)")
HISTOGRAM(CACHE_SERVICE_LOCK_WAIT_MAINTHREAD, 1, 10000, 10000, LINEAR, "Time spent waiting on the cache service lock on the main thread (ms)")

HISTOGRAM(DNS_LOOKUP_METHOD, 1, 7, 7, LINEAR, "DNS Lookup Type (hit, renewal, negative-hit, literal, overflow, network-first, network-shared)")
HISTOGRAM(DNS_CLEANUP_AGE, 1, 1440, 50, EXPONENTIAL, "DNS Cache Entry Age at Removal Time (minutes)")
HISTOGRAM(DNS_LOOKUP_TIME, 1, 60000, 50, EXPONENTIAL, "Time for a successful DNS OS resolution (msec)")
HISTOGRAM(DNS_RENEWAL_TIME, 1, 60000, 50, EXPONENTIAL, "Time for a renewed DNS OS resolution (msec)")
HISTOGRAM(DNS_FAILED_LOOKUP_TIME, 1, 60000, 50, EXPONENTIAL, "Time for an unsuccessful DNS OS resolution (msec)")

HISTOGRAM(FIND_PLUGINS, 1, 3000, 10, EXPONENTIAL, "Time spent scanning filesystem for plugins (ms)")
HISTOGRAM(CHECK_JAVA_ENABLED, 1, 3000, 10, EXPONENTIAL, "Time spent checking if Java is enabled (ms)")
HISTOGRAM(PLUGIN_SHUTDOWN_MS, 1, 5000, 20, EXPONENTIAL, "Time spent shutting down plugins (ms)")

/* Define 2 histograms: MOZ_SQLITE_(NAME)_MS and
 * MOZ_SQLITE_(NAME)_MAIN_THREAD_MS. These are meant to be used by
 * IOThreadAutoTimer which relies on _MAIN_THREAD_MS histogram being
 * "+ 1" away from MOZ_SQLITE_(NAME)_MS.
 */
#define SQLITE_TIME_SPENT(NAME, DESC) \
  HISTOGRAM(MOZ_SQLITE_ ## NAME ## _MS, 1, 3000, 10, EXPONENTIAL, DESC) \
  HISTOGRAM(MOZ_SQLITE_ ## NAME ## _MAIN_THREAD_MS, 1, 3000, 10, EXPONENTIAL, DESC)

#define SQLITE_TIME_PER_FILE(NAME, DESC) \
  SQLITE_TIME_SPENT(OTHER_ ## NAME, DESC) \
  SQLITE_TIME_SPENT(PLACES_ ## NAME, DESC) \
  SQLITE_TIME_SPENT(COOKIES_ ## NAME, DESC) \
  SQLITE_TIME_SPENT(URLCLASSIFIER_ ## NAME, DESC) \
  SQLITE_TIME_SPENT(WEBAPPS_ ## NAME, DESC)

SQLITE_TIME_SPENT(OPEN, "Time spent on SQLite open() (ms)")
SQLITE_TIME_SPENT(TRUNCATE, "Time spent on SQLite truncate() (ms)")
SQLITE_TIME_PER_FILE(READ, "Time spent on SQLite read() (ms)")
SQLITE_TIME_PER_FILE(WRITE, "Time spent on SQLite write() (ms)")
SQLITE_TIME_PER_FILE(SYNC, "Time spent on SQLite fsync() (ms)")
#undef SQLITE_TIME_PER_FILE
#undef SQLITE_TIME_SPENT
HISTOGRAM(MOZ_SQLITE_OTHER_READ_B, 1, 32768, 3, LINEAR, "SQLite read() (bytes)")
HISTOGRAM(MOZ_SQLITE_PLACES_READ_B, 1, 32768, 3, LINEAR, "SQLite read() (bytes)")
HISTOGRAM(MOZ_SQLITE_COOKIES_READ_B, 1, 32768, 3, LINEAR, "SQLite read() (bytes)")
HISTOGRAM(MOZ_SQLITE_URLCLASSIFIER_READ_B, 1, 32768, 3, LINEAR, "SQLite read() (bytes)")
HISTOGRAM(MOZ_SQLITE_WEBAPPS_READ_B, 1, 32768, 3, LINEAR, "SQLite read() (bytes)")
HISTOGRAM(MOZ_SQLITE_PLACES_WRITE_B, 1, 32768, 3, LINEAR, "SQLite write (bytes)")
HISTOGRAM(MOZ_SQLITE_COOKIES_WRITE_B, 1, 32768, 3, LINEAR, "SQLite write (bytes)")
HISTOGRAM(MOZ_SQLITE_URLCLASSIFIER_WRITE_B, 1, 32768, 3, LINEAR, "SQLite write (bytes)")
HISTOGRAM(MOZ_SQLITE_WEBAPPS_WRITE_B, 1, 32768, 3, LINEAR, "SQLite write (bytes)")
HISTOGRAM(MOZ_SQLITE_OTHER_WRITE_B, 1, 32768, 3, LINEAR, "SQLite write (bytes)")
HISTOGRAM(MOZ_STORAGE_ASYNC_REQUESTS_MS, 1, 32768, 20, EXPONENTIAL, "mozStorage async requests completion (ms)")
HISTOGRAM_BOOLEAN(MOZ_STORAGE_ASYNC_REQUESTS_SUCCESS, "mozStorage async requests success")
HISTOGRAM(STARTUP_MEASUREMENT_ERRORS, 1, mozilla::StartupTimeline::MAX_EVENT_ID, mozilla::StartupTimeline::MAX_EVENT_ID + 1, LINEAR, "Flags errors in startup calculation()")
HISTOGRAM(NETWORK_DISK_CACHE_OPEN, 1, 10000, 10, EXPONENTIAL, "Time spent opening disk cache (ms)")
HISTOGRAM(NETWORK_DISK_CACHE_TRASHRENAME, 1, 10000, 10, EXPONENTIAL, "Time spent renaming bad Cache to Cache.Trash (ms)")
HISTOGRAM(NETWORK_DISK_CACHE_DELETEDIR, 1, 10000, 10, EXPONENTIAL, "Time spent deleting disk cache (ms)")
HISTOGRAM(NETWORK_DISK_CACHE_DELETEDIR_SHUTDOWN, 1, 10000, 10, EXPONENTIAL, "Time spent during showdown stopping thread deleting old disk cache (ms)")
HISTOGRAM(NETWORK_DISK_CACHE_SHUTDOWN, 1, 10000, 10, EXPONENTIAL, "Total Time spent (ms) during disk cache showdown")
HISTOGRAM(NETWORK_DISK_CACHE_SHUTDOWN_CLEAR_PRIVATE, 1, 10000, 10, EXPONENTIAL, "Time spent (ms) during showdown deleting disk cache for 'clear private data' option")
HISTOGRAM(NETWORK_DISK_CACHE_OUTPUT_STREAM_CLOSE, 1, 10000, 10, EXPONENTIAL, "Time spent in nsDiskCacheOutputStream::Close() on non-main thread (ms)")
HISTOGRAM(NETWORK_DISK_CACHE_OUTPUT_STREAM_CLOSE_MAIN_THREAD, 1, 10000, 10, EXPONENTIAL, "Time spent in nsDiskCacheOutputStream::Close() on the main thread (ms)")
HISTOGRAM(NETWORK_DISK_CACHE_OUTPUT_STREAM_CLOSE_INTERNAL, 1, 10000, 10, EXPONENTIAL, "Time spent in nsDiskCacheOutputStream::CloseInternal() on non-main thread (ms)")
HISTOGRAM(NETWORK_DISK_CACHE_OUTPUT_STREAM_CLOSE_INTERNAL_MAIN_THREAD, 1, 10000, 10, EXPONENTIAL, "Time spent in nsDiskCacheOutputStream::CloseInternal on the main thread (ms)")

/**
 * Idle service telemetry
 */
HISTOGRAM(IDLE_NOTIFY_BACK_MS, 1, 5000, 10, EXPONENTIAL, "Time spent checking for and notifying listeners that the user is back (ms)")
HISTOGRAM(IDLE_NOTIFY_BACK_LISTENERS, 1, 100, 20, LINEAR, "Number of listeners notified that the user is back")
HISTOGRAM(IDLE_NOTIFY_IDLE_MS, 1, 5000, 10, EXPONENTIAL, "Time spent checking for and notifying listeners that the user is idle (ms)")
HISTOGRAM(IDLE_NOTIFY_IDLE_LISTENERS, 1, 100, 20, LINEAR, "Number of listeners notified that the user is idle")

/**
 * Url-Classifier telemetry
 */
#ifdef MOZ_URL_CLASSIFIER
HISTOGRAM(URLCLASSIFIER_PS_FILELOAD_TIME, 1, 1000, 10, EXPONENTIAL, "Time spent loading PrefixSet from file (ms)")
HISTOGRAM(URLCLASSIFIER_PS_FALLOCATE_TIME, 1, 1000, 10, EXPONENTIAL, "Time spent fallocating PrefixSet (ms)")
HISTOGRAM(URLCLASSIFIER_PS_CONSTRUCT_TIME, 1, 5000, 15, EXPONENTIAL, "Time spent constructing PrefixSet from DB (ms)")
HISTOGRAM(URLCLASSIFIER_PS_LOOKUP_TIME, 1, 500, 10, EXPONENTIAL, "Time spent per PrefixSet lookup (ms)")
HISTOGRAM_BOOLEAN(URLCLASSIFIER_PS_OOM, "Did UrlClassifier run out of memory during PrefixSet construction?")
#endif

/**
 * Places telemetry.
 */
HISTOGRAM(PLACES_PAGES_COUNT, 1000, 150000, 20, EXPONENTIAL, "PLACES: Number of unique pages")
HISTOGRAM(PLACES_BOOKMARKS_COUNT, 100, 8000, 15, EXPONENTIAL, "PLACES: Number of bookmarks")
HISTOGRAM(PLACES_TAGS_COUNT, 1, 200, 10, EXPONENTIAL, "PLACES: Number of tags")
HISTOGRAM(PLACES_FOLDERS_COUNT, 1, 200, 10, EXPONENTIAL, "PLACES: Number of folders")
HISTOGRAM(PLACES_KEYWORDS_COUNT, 1, 200, 10, EXPONENTIAL, "PLACES: Number of keywords")
HISTOGRAM(PLACES_SORTED_BOOKMARKS_PERC, 1, 100, 10, LINEAR, "PLACES: Percentage of bookmarks organized in folders")
HISTOGRAM(PLACES_TAGGED_BOOKMARKS_PERC, 1, 100, 10, LINEAR, "PLACES: Percentage of tagged bookmarks")
HISTOGRAM(PLACES_DATABASE_FILESIZE_MB, 5, 200, 10, EXPONENTIAL, "PLACES: Database filesize (MB)")
HISTOGRAM(PLACES_DATABASE_JOURNALSIZE_MB, 1, 50, 10, EXPONENTIAL, "PLACES: Database journal size (MB)")
HISTOGRAM(PLACES_DATABASE_PAGESIZE_B, 1024, 32768, 10, EXPONENTIAL, "PLACES: Database page size (bytes)")
HISTOGRAM(PLACES_DATABASE_SIZE_PER_PAGE_B, 500, 10240, 20, EXPONENTIAL, "PLACES: Average size of a place in the database (bytes)")
HISTOGRAM(PLACES_EXPIRATION_STEPS_TO_CLEAN, 1, 10, 10, LINEAR, "PLACES: Expiration steps to cleanup the database")
HISTOGRAM(PLACES_AUTOCOMPLETE_1ST_RESULT_TIME_MS, 50, 500, 10, EXPONENTIAL, "PLACES: Time for first autocomplete result if > 50ms (ms)")
HISTOGRAM(PLACES_IDLE_FRECENCY_DECAY_TIME_MS, 50, 10000, 10, EXPONENTIAL, "PLACES: Time to decay all frecencies values on idle (ms)")
HISTOGRAM(PLACES_IDLE_MAINTENANCE_TIME_MS, 1000, 30000, 10, EXPONENTIAL, "PLACES: Time to execute maintenance tasks on idle (ms)")
HISTOGRAM(PLACES_ANNOS_BOOKMARKS_COUNT, 50, 5000, 10, EXPONENTIAL, "PLACES: Number of bookmarks annotations")
HISTOGRAM(PLACES_ANNOS_BOOKMARKS_SIZE_KB, 10, 10000, 10, EXPONENTIAL, "PLACES: Size of bookmarks annotations (KB)")
HISTOGRAM(PLACES_ANNOS_PAGES_COUNT, 50, 5000, 10, EXPONENTIAL, "PLACES: Number of pages annotations")
HISTOGRAM(PLACES_ANNOS_PAGES_SIZE_KB, 10, 10000, 10, EXPONENTIAL, "PLACES: Size of pages annotations (KB)")
HISTOGRAM(PLACES_FRECENCY_CALC_TIME_MS, 1, 100, 10, EXPONENTIAL, "PLACES: Time to calculate frecency of a page (ms)")

/**
 * Updater telemetry.
 */
HISTOGRAM(UPDATER_STATUS_CODES, 1, 50, 51, LINEAR, "Updater: the status of the latest update performed")

/**
 * Thunderbird-specific telemetry.
 */
// Disable this application-specific #ifdef ftb. (See bug 710562)
// #ifdef MOZ_THUNDERBIRD
HISTOGRAM(THUNDERBIRD_GLODA_SIZE_MB, 1, 1000, 40, LINEAR, "Gloda: size of global-messages-db.sqlite (MB)")
// THUNDERBIRD_CONVERSATIONS_TIME_TO_2ND_GLODA_QUERY_MS is used by Thunderbird Conversations add-on.
// https://addons.mozilla.org/en-US/thunderbird/addon/54035/
HISTOGRAM(THUNDERBIRD_CONVERSATIONS_TIME_TO_2ND_GLODA_QUERY_MS, 1, 10000, 30, EXPONENTIAL, "Conversations: time between the moment we click and the second gloda query returns (ms)")
HISTOGRAM(THUNDERBIRD_INDEXING_RATE_MSG_PER_S, 1, 100, 20, LINEAR, "Gloda: indexing rate (message/s)")
// #endif

/**
 * Firefox-specific telemetry.
 */
// Disable this application-specific #ifdef ftb. (See bug 710562)
// #ifdef MOZ_PHOENIX
HISTOGRAM(FX_TAB_ANIM_OPEN_MS, 1, 3000, 10, EXPONENTIAL, "Firefox: Time taken by the tab opening animation in milliseconds")
HISTOGRAM(FX_TAB_ANIM_CLOSE_MS, 1, 3000, 10, EXPONENTIAL, "Firefox: Time taken by the tab closing animation in milliseconds")
HISTOGRAM_BOOLEAN(FX_KEYWORD_URL_USERSET, "Firefox: keyword.URL has a user-set value")
HISTOGRAM(FX_IDENTITY_POPUP_OPEN_MS, 1, 1000, 10, EXPONENTIAL, "Firefox: Time taken by the identity popup to open in milliseconds")
HISTOGRAM(FX_APP_MENU_OPEN_MS, 1, 1000, 10, EXPONENTIAL, "Firefox: Time taken by the app-menu opening in milliseconds")
HISTOGRAM(FX_BOOKMARKS_TOOLBAR_INIT_MS, 50, 5000, 10, EXPONENTIAL, "Firefox: Time to initialize the bookmarks toolbar view (ms)")
HISTOGRAM(FX_NEW_WINDOW_MS, 1, 10000, 20, EXPONENTIAL, "Firefox: Time taken to open a new browser window (ms)")

/**
 * Thumbnail Service telemetry.
 */
HISTOGRAM(FX_THUMBNAILS_CAPTURE_TIME_MS, 1, 500, 15, EXPONENTIAL, "THUMBNAILS: Time (ms) it takes to capture a thumbnail")
HISTOGRAM(FX_THUMBNAILS_STORE_TIME_MS, 1, 500, 15, EXPONENTIAL, "THUMBNAILS: Time (ms) it takes to store a thumbnail in the cache")
HISTOGRAM(FX_THUMBNAILS_HIT_OR_MISS, 0, 1, 2, BOOLEAN, "THUMBNAILS: Thumbnail found")


/*
 * Widget telemetry.
 */
HISTOGRAM(EVENTLOOP_UI_LAG, 50, 30000, 200, LINEAR, "Widget: Time (ms) it takes for the message before a UI message")

/**
 * Session restore telemetry
 */
HISTOGRAM(FX_SESSION_RESTORE_COLLECT_DATA_MS, 1, 30000, 10, EXPONENTIAL, "Session restore: Time to collect all window and tab data (ms)")
HISTOGRAM(FX_SESSION_RESTORE_SERIALIZE_DATA_MS, 1, 1000, 10, EXPONENTIAL, "Session restore: Time to JSON serialize session data (ms)")
HISTOGRAM(FX_SESSION_RESTORE_READ_FILE_MS, 1, 3000, 10, EXPONENTIAL, "Session restore: Time to read the session data from the file on disk (ms)")
HISTOGRAM(FX_SESSION_RESTORE_WRITE_FILE_MS, 1, 3000, 10, EXPONENTIAL, "Session restore: Time to write the session data to the file on disk (ms)")
HISTOGRAM_BOOLEAN(FX_SESSION_RESTORE_CORRUPT_FILE, "Session restore: Whether the file read on startup contained parse-able JSON")
// #endif

HISTOGRAM_BOOLEAN(INNERWINDOWS_WITH_MUTATION_LISTENERS, "Deleted or to-be-reused innerwindow which has had mutation event listeners.")
HISTOGRAM(XUL_FOREGROUND_REFLOW_MS, 1, 3000, 10, EXPONENTIAL, "XUL reflows in foreground windows (ms)")
HISTOGRAM(XUL_BACKGROUND_REFLOW_MS, 1, 3000, 10, EXPONENTIAL, "XUL reflows in background windows (ms)")
HISTOGRAM(HTML_FOREGROUND_REFLOW_MS, 1, 3000, 10, EXPONENTIAL, "HTML reflows in foreground windows (ms)")
HISTOGRAM(HTML_BACKGROUND_REFLOW_MS, 1, 3000, 10, EXPONENTIAL, "HTML reflows in background windows (ms)")
HISTOGRAM(XUL_INITIAL_FRAME_CONSTRUCTION, 1, 3000, 10, EXPONENTIAL, "initial xul frame construction")
HISTOGRAM_BOOLEAN(XMLHTTPREQUEST_ASYNC_OR_SYNC, "Type of XMLHttpRequest, async or sync")
HISTOGRAM_BOOLEAN(MULTIPART_XHR_RESPONSE, "XMLHttpRequest response was of type multipart/x-mixed-replace.")

/**
 * Private browsing transition telemetry.
 */
HISTOGRAM(PRIVATE_BROWSING_TRANSITION_ENTER_PREPARATION_MS, 1, 3000, 10, EXPONENTIAL, "Time spent on private browsing enter transition, excluding session restore (ms)")
HISTOGRAM(PRIVATE_BROWSING_TRANSITION_ENTER_TOTAL_MS, 1, 10000, 50, EXPONENTIAL, "Time spent on private browsing enter transition, including session restore (ms)")
HISTOGRAM(PRIVATE_BROWSING_TRANSITION_EXIT_PREPARATION_MS, 1, 3000, 10, EXPONENTIAL, "Time spent on private browsing exit transition, excluding session restore (ms)")
HISTOGRAM(PRIVATE_BROWSING_TRANSITION_EXIT_TOTAL_MS, 1, 10000, 50, EXPONENTIAL, "Time spent on private browsing exit transition, including session restore (ms)")

/**
 * DOM telemetry.
 */
HISTOGRAM(DOM_TIMERS_FIRED_PER_NATIVE_TIMEOUT, 1, 3000, 10, EXPONENTIAL, "DOM: Timer handlers called per native timer expiration")
HISTOGRAM(DOM_TIMERS_RECENTLY_SET, 1, 3000, 10, EXPONENTIAL, "DOM: setTimeout/setInterval calls recently (last 30s or more)")
HISTOGRAM_BOOLEAN(DOM_RANGE_DETACHED, "DOM: Ranges that are detached on destruction (bug 702948)")

/**
 * DOM Storage telemetry.
 */
#define DOMSTORAGE_HISTOGRAM(PREFIX, TYPE, TYPESTRING, DESCRIPTION) \
  HISTOGRAM(PREFIX ## DOMSTORAGE_ ## TYPE ## _SIZE_BYTES, \
            1024, 32768, 10, EXPONENTIAL, "DOM storage: size of " TYPESTRING "s stored in " DESCRIPTION "Storage")
#define DOMSTORAGE_KEY_VAL_SIZE(PREFIX, DESCRIPTION) \
  DOMSTORAGE_HISTOGRAM(PREFIX, KEY, "key", DESCRIPTION) \
  DOMSTORAGE_HISTOGRAM(PREFIX, VALUE, "value", DESCRIPTION)

DOMSTORAGE_KEY_VAL_SIZE(GLOBAL, "global")
DOMSTORAGE_KEY_VAL_SIZE(LOCAL, "local")
DOMSTORAGE_KEY_VAL_SIZE(SESSION, "session")

#undef DOMSTORAGE_KEY_VAL_SIZE
#undef DOMSTORAGE_HISTOGRAM

/**
 * Telemetry telemetry.
 */
HISTOGRAM(RANGE_CHECKSUM_ERRORS, 1, 3000, 10, EXPONENTIAL, "Number of histograms with range checksum errors")
HISTOGRAM(BUCKET_ORDER_ERRORS, 1, 3000, 10, EXPONENTIAL, "Number of histograms with bucket order errors")
HISTOGRAM(TOTAL_COUNT_HIGH_ERRORS, 1, 3000, 10, EXPONENTIAL, "Number of histograms with total count high errors")
HISTOGRAM(TOTAL_COUNT_LOW_ERRORS, 1, 3000, 10, EXPONENTIAL, "Number of histograms with total count low errors")
HISTOGRAM_FLAG(TELEMETRY_TEST_FLAG, "a testing histogram; not meant to be touched")

/**
 * Startup Crash Detection
 */
HISTOGRAM_FLAG(STARTUP_CRASH_DETECTED, "Whether there was a crash during the last startup")
HISTOGRAM_ENUMERATED_VALUES(SAFE_MODE_USAGE, 3, "Whether the user is in safe mode (No, Yes, Forced)")

/**
 * New Tab Page telemetry.
 */
HISTOGRAM(NEWTAB_PAGE_ENABLED, 0, 1, 2, BOOLEAN, "New tab page is enabled.")
HISTOGRAM(NEWTAB_PAGE_PINNED_SITES_COUNT, 1, 9, 10, EXPONENTIAL, "Number of pinned sites on the new tab page.")
HISTOGRAM(NEWTAB_PAGE_BLOCKED_SITES_COUNT, 1, 100, 10, EXPONENTIAL, "Number of sites blocked from the new tab page.")

/**
 * Panorama telemetry.
 */
HISTOGRAM(PANORAMA_INITIALIZATION_TIME_MS, 1, 10000, 15, EXPONENTIAL, "Time it takes to initialize Panorama (ms)")
HISTOGRAM(PANORAMA_GROUPS_COUNT, 1, 25, 15, EXPONENTIAL, "Number of groups in Panorama")
HISTOGRAM(PANORAMA_STACKED_GROUPS_COUNT, 1, 25, 15, EXPONENTIAL, "Number of stacked groups in Panorama")
HISTOGRAM(PANORAMA_MEDIAN_TABS_IN_GROUPS_COUNT, 1, 100, 15, EXPONENTIAL, "Median of tabs in groups in Panorama")

/**
 * Native Fennec Telemetry
 */
#if defined(ANDROID)
HISTOGRAM(BROWSERPROVIDER_XUL_IMPORT_TIME, 20, 600000, 20, EXPONENTIAL, "Time for the initial conversion of a XUL places database (ms)")
HISTOGRAM(BROWSERPROVIDER_XUL_IMPORT_BOOKMARKS, 1, 50000, 20, EXPONENTIAL, "Number of bookmarks in the original XUL places database")
HISTOGRAM(BROWSERPROVIDER_XUL_IMPORT_HISTORY, 1, 1000000, 20, EXPONENTIAL, "Number of history entries in the original XUL places database")
#endif

#undef HISTOGRAM_ENUMERATED_VALUES
#undef HISTOGRAM_BOOLEAN
#undef HISTOGRAM_FLAG
