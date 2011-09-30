/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Taras Glek <tglek@mozilla.com>
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

/**
 * a11y telemetry
 */
HISTOGRAM(A11Y_INSTANTIATED, 0, 1, 2, BOOLEAN, "has accessibility support been instantiated")
HISTOGRAM(ISIMPLE_DOM_USAGE, 0, 1, 2, BOOLEAN, "have the ISimpleDOM* accessibility interfaces been used")
HISTOGRAM(IACCESSIBLE_TABLE_USAGE, 0, 1, 2, BOOLEAN, "has the IAccessibleTable accessibility interface been used")

HISTOGRAM(CYCLE_COLLECTOR, 1, 10000, 50, EXPONENTIAL, "Time spent on one cycle collection (ms)")
HISTOGRAM(CYCLE_COLLECTOR_VISITED_REF_COUNTED, 1, 300000, 50, EXPONENTIAL, "Number of ref counted objects visited by the cycle collector")
HISTOGRAM(CYCLE_COLLECTOR_VISITED_GCED, 1, 300000, 50, EXPONENTIAL, "Number of JS objects visited by the cycle collector")
HISTOGRAM(CYCLE_COLLECTOR_COLLECTED, 1, 100000, 50, EXPONENTIAL, "Number of objects collected by the cycle collector")
HISTOGRAM(TELEMETRY_PING, 1, 3000, 10, EXPONENTIAL, "Time taken to submit telemetry info (ms)")
HISTOGRAM(TELEMETRY_SUCCESS, 0, 1, 2, BOOLEAN,  "Successful telemetry submission")
HISTOGRAM(MEMORY_JS_COMPARTMENTS_SYSTEM, 1, 1000, 10, EXPONENTIAL, "Total JavaScript compartments used for add-ons and internals.")
HISTOGRAM(MEMORY_JS_COMPARTMENTS_USER, 1, 1000, 10, EXPONENTIAL, "Total JavaScript compartments used for web pages")
HISTOGRAM(MEMORY_JS_GC_HEAP, 1024, 512 * 1024, 10, EXPONENTIAL, "Memory used by the garbage-collected JavaScript heap (KB)")
HISTOGRAM(MEMORY_RESIDENT, 32 * 1024, 1024 * 1024, 10, EXPONENTIAL, "Resident memory size (KB)")
HISTOGRAM(MEMORY_STORAGE_SQLITE, 1024, 512 * 1024, 10, EXPONENTIAL, "Memory used by SQLite (KB)")
HISTOGRAM(MEMORY_IMAGES_CONTENT_USED_UNCOMPRESSED, 1024, 1024 * 1024, 10, EXPONENTIAL, "Memory used for uncompressed, in-use content images (KB)")
HISTOGRAM(MEMORY_HEAP_ALLOCATED, 1024, 1024 * 1024, 10, EXPONENTIAL, "Heap memory allocated (KB)")
HISTOGRAM(MEMORY_EXPLICIT, 1024, 1024 * 1024, 10, EXPONENTIAL, "Explicit memory allocations (KB)")
#if defined(XP_WIN)
HISTOGRAM(EARLY_GLUESTARTUP_READ_OPS, 1, 100, 12, LINEAR, "ProcessIoCounters.ReadOperationCount before glue startup")
HISTOGRAM(EARLY_GLUESTARTUP_READ_TRANSFER, 1, 50 * 1024, 12, EXPONENTIAL, "ProcessIoCounters.ReadTransferCount before glue startup (KB)")
HISTOGRAM(GLUESTARTUP_READ_OPS, 1, 100, 12, LINEAR, "ProcessIoCounters.ReadOperationCount after glue startup")
HISTOGRAM(GLUESTARTUP_READ_TRANSFER, 1, 50 * 1024, 12, EXPONENTIAL, "ProcessIoCounters.ReadTransferCount after glue startup (KB)")
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
HISTOGRAM(DWRITEFONT_DELAYEDINITFONTLIST_GDI_TABLE, 0, 1, 2, BOOLEAN, "gfxDWriteFontList::DelayedInitFontList GDI Table Access")
HISTOGRAM(DWRITEFONT_DELAYEDINITFONTLIST_COLLECT, 1, 30000, 10, EXPONENTIAL, "gfxDWriteFontList::DelayedInitFontList GetSystemFontCollection (ms)")
HISTOGRAM(DWRITEFONT_DELAYEDINITFONTLIST_ITERATE, 1, 30000, 10, EXPONENTIAL, "gfxDWriteFontList::DelayedInitFontList iterate over families (ms)")
HISTOGRAM(GDI_INITFONTLIST_TOTAL, 1, 30000, 10, EXPONENTIAL, "gfxGDIFontList::InitFontList Total (ms)")
#elif defined(XP_MACOSX)
HISTOGRAM(MAC_INITFONTLIST_TOTAL, 1, 30000, 10, EXPONENTIAL, "gfxMacPlatformFontList::InitFontList Total (ms)")
#endif
HISTOGRAM(SHUTDOWN_OK, 0, 1, 2, BOOLEAN, "Did the browser start after a successful shutdown")

HISTOGRAM(IMAGE_DECODE_LATENCY, 50,  5000000, 100, EXPONENTIAL, "Time spent decoding an image chunk (us)")
HISTOGRAM(IMAGE_DECODE_TIME,    50, 50000000, 100, EXPONENTIAL, "Time spent decoding an image (us)")
HISTOGRAM(IMAGE_DECODE_ON_DRAW_LATENCY,  50, 50000000, 100, EXPONENTIAL, "Time from starting a decode to it showing up on the screen (us)")
HISTOGRAM(IMAGE_DECODE_CHUNKS, 1,  500, 50, EXPONENTIAL, "Number of chunks per decode attempt")
HISTOGRAM(IMAGE_DECODE_COUNT, 1,  500, 50, EXPONENTIAL, "Decode count")
HISTOGRAM(IMAGE_DECODE_SPEED_JPEG, 500, 50000000,  50, EXPONENTIAL, "JPEG image decode speed (Kbytes/sec)")
HISTOGRAM(IMAGE_DECODE_SPEED_GIF,  500, 50000000,  50, EXPONENTIAL, "GIF image decode speed (Kbytes/sec)")
HISTOGRAM(IMAGE_DECODE_SPEED_PNG,  500, 50000000,  50, EXPONENTIAL, "PNG image decode speed (Kbytes/sec)")

HISTOGRAM(CANVAS_2D_USED, 0, 1, 2, BOOLEAN, "2D canvas used")
HISTOGRAM(CANVAS_WEBGL_USED, 0, 1, 2, BOOLEAN, "WebGL canvas used")

/**
 * Networking telemetry
 */
HISTOGRAM(TOTAL_CONTENT_PAGE_LOAD_TIME, 100, 30000, 100, EXPONENTIAL, "HTTP: Total page load time (ms)")
HISTOGRAM(HTTP_SUBITEM_OPEN_LATENCY_TIME, 1, 30000, 50, EXPONENTIAL, "HTTP subitem: Page start -> subitem open() (ms)")
HISTOGRAM(HTTP_SUBITEM_FIRST_BYTE_LATENCY_TIME, 1, 30000, 50, EXPONENTIAL, "HTTP subitem: Page start -> first byte received for subitem reply (ms)")
HISTOGRAM(HTTP_REQUEST_PER_PAGE, 1, 1000, 50, EXPONENTIAL, "HTTP: Requests per page (count)")
HISTOGRAM(HTTP_REQUEST_PER_PAGE_FROM_CACHE, 1, 101, 102, LINEAR, "HTTP: Requests serviced from cache (%)")

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

#undef _HTTP_HIST
#undef HTTP_HISTOGRAMS

HISTOGRAM(HTTP_CACHE_DISPOSITION, 1, 5, 5, LINEAR, "HTTP Cache Hit, Reval, Failed-Reval, Miss")

HISTOGRAM(FIND_PLUGINS, 1, 3000, 10, EXPONENTIAL, "Time spent scanning filesystem for plugins (ms)")
HISTOGRAM(CHECK_JAVA_ENABLED, 1, 3000, 10, EXPONENTIAL, "Time spent checking if Java is enabled (ms)")

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
  SQLITE_TIME_SPENT(URLCLASSIFIER_ ## NAME, DESC)

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
HISTOGRAM(MOZ_SQLITE_PLACES_WRITE_B, 1, 32768, 3, LINEAR, "SQLite write (bytes)")
HISTOGRAM(MOZ_SQLITE_COOKIES_WRITE_B, 1, 32768, 3, LINEAR, "SQLite write (bytes)")
HISTOGRAM(MOZ_SQLITE_URLCLASSIFIER_WRITE_B, 1, 32768, 3, LINEAR, "SQLite write (bytes)")
HISTOGRAM(MOZ_SQLITE_OTHER_WRITE_B, 1, 32768, 3, LINEAR, "SQLite write (bytes)")
HISTOGRAM(STARTUP_MEASUREMENT_ERRORS, 1, 4, 5, LINEAR, "Flags errors in startup calculation()")
HISTOGRAM(NETWORK_DISK_CACHE_OPEN, 1, 10000, 10, EXPONENTIAL, "Time spent opening disk cache (ms)")
HISTOGRAM(NETWORK_DISK_CACHE_TRASHRENAME, 1, 10000, 10, EXPONENTIAL, "Time spent renaming bad Cache to Cache.Trash (ms)")
HISTOGRAM(NETWORK_DISK_CACHE_DELETEDIR, 1, 10000, 10, EXPONENTIAL, "Time spent deleting disk cache (ms)")

/**
 * Url-Classifier telemetry
 */
#ifdef MOZ_URL_CLASSIFIER
HISTOGRAM(URLCLASSIFIER_PS_FILELOAD_TIME, 1, 1000, 10, EXPONENTIAL, "Time spent loading PrefixSet from file (ms)")
HISTOGRAM(URLCLASSIFIER_PS_CONSTRUCT_TIME, 1, 5000, 15, EXPONENTIAL, "Time spent constructing PrefixSet from DB (ms)")
HISTOGRAM(URLCLASSIFIER_PS_LOOKUP_TIME, 1, 500, 10, EXPONENTIAL, "Time spent per PrefixSet lookup (ms)")
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

/**
 * Thunderbird-specific telemetry.
 */
#ifdef MOZ_THUNDERBIRD
HISTOGRAM(THUNDERBIRD_GLODA_SIZE_MB, 1, 1000, 40, LINEAR, "Gloda: size of global-messages-db.sqlite (MB)")
HISTOGRAM(THUNDERBIRD_CONVERSATIONS_TIME_TO_2ND_GLODA_QUERY_MS, 1, 10000, 30, EXPONENTIAL, "Conversations: time between the moment we click and the second gloda query returns (ms)")
HISTOGRAM(THUNDERBIRD_INDEXING_RATE_MSG_PER_S, 1, 100, 20, LINEAR, "Gloda: indexing rate (message/s)")
#endif
