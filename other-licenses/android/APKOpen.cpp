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
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Wu <mwu@mozilla.com>
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

#include <jni.h>
#include <android/log.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <endian.h>
#include <unistd.h>
#include <zlib.h>
#include "dlfcn.h"
#include "APKOpen.h"

/* compression methods */
#define STORE    0
#define DEFLATE  8
#define LZMA    14

#define NS_EXPORT __attribute__ ((visibility("default")))

struct local_file_header {
  uint32_t signature;
  uint16_t min_version;
  uint16_t general_flag;
  uint16_t compression;
  uint16_t lastmod_time;
  uint16_t lastmod_date;
  uint32_t crc32;
  uint32_t compressed_size;
  uint32_t uncompressed_size;
  uint16_t filename_size;
  uint16_t extra_field_size;
  char     data[0];
} __attribute__((__packed__));

struct cdir_entry {
  uint32_t signature;
  uint16_t creator_version;
  uint16_t min_version;
  uint16_t general_flag;
  uint16_t compression;
  uint16_t lastmod_time;
  uint16_t lastmod_date;
  uint32_t crc32;
  uint32_t compressed_size;
  uint32_t uncompressed_size;
  uint16_t filename_size;
  uint16_t extra_field_size;
  uint16_t file_comment_size;
  uint16_t disk_num;
  uint16_t internal_attr;
  uint32_t external_attr;
  uint32_t offset;
  char     data[0];
} __attribute__((__packed__));

#define CDIR_END_SIG 0x06054b50

struct cdir_end {
  uint32_t signature;
  uint16_t disk_num;
  uint16_t cdir_disk;
  uint16_t disk_entries;
  uint16_t cdir_entries;
  uint32_t cdir_size;
  uint32_t cdir_offset;
  uint16_t comment_size;
  char     comment[0];
} __attribute__((__packed__));

static size_t zip_size;
static int zip_fd;
NS_EXPORT struct mapping_info * lib_mapping;

static void * map_file (const char *file)
{
  int fd = open(file, O_RDONLY);
  if (fd == -1) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoMapFile", "map_file open %s", strerror(errno));
    return NULL;
  }

  zip_fd = fd;
  struct stat s;
  if (fstat(fd, &s) == -1) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoMapFile", "map_file fstat %s", strerror(errno));
    return NULL;
  }

  zip_size = s.st_size;
  void *addr = mmap(NULL, zip_size, PROT_READ, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoMapFile", "map_file mmap %s", strerror(errno));
    return NULL;
  }

  return addr;
}

static uint32_t cdir_entry_size (struct cdir_entry *entry)
{
  return sizeof(*entry) +
         letoh16(entry->filename_size) +
         letoh16(entry->extra_field_size) +
         letoh16(entry->file_comment_size);
}

static struct cdir_entry *
find_cdir_entry (struct cdir_entry *entry, int count, const char *name)
{
  size_t name_size = strlen(name);
  while (count--) {
    if (letoh16(entry->filename_size) == name_size &&
        !memcmp(entry->data, name, name_size))
      return entry;
    entry = (struct cdir_entry *)((void *)entry + cdir_entry_size(entry));
  }
  return NULL;
}

static uint32_t simple_write(int fd, const void *buf, uint32_t count)
{
  uint32_t out_offset = 0;
  while (out_offset < count) {
    uint32_t written = write(fd, buf + out_offset,
                             count - out_offset);
    if (written == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        continue;
      else {
        __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "simple_write failed");
        break;
      }
    }

    out_offset += written;
  }
  return out_offset;
}

#define SHELL_WRAPPER0(name) \
typedef void (*name ## _t)(JNIEnv *, jclass); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT void JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc) \
{ \
  f_ ## name(jenv, jc); \
}

#define SHELL_WRAPPER1(name,type1) \
typedef void (*name ## _t)(JNIEnv *, jclass, type1 one); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT void JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one) \
{ \
  f_ ## name(jenv, jc, one); \
}

#define SHELL_WRAPPER2(name,type1,type2) \
typedef void (*name ## _t)(JNIEnv *, jclass, type1 one, type2 two); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT void JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one, type2 two) \
{ \
  f_ ## name(jenv, jc, one, two); \
}

#define SHELL_WRAPPER3(name,type1,type2,type3) \
typedef void (*name ## _t)(JNIEnv *, jclass, type1 one, type2 two, type3 three); \
static name ## _t f_ ## name; \
extern "C" NS_EXPORT void JNICALL \
Java_org_mozilla_gecko_GeckoAppShell_ ## name(JNIEnv *jenv, jclass jc, type1 one, type2 two, type3 three) \
{ \
  f_ ## name(jenv, jc, one, two, three); \
}

SHELL_WRAPPER0(nativeInit)
SHELL_WRAPPER1(nativeRun, jstring)
SHELL_WRAPPER1(notifyGeckoOfEvent, jobject)
SHELL_WRAPPER1(setSurfaceView, jobject)
SHELL_WRAPPER2(setInitialSize, int, int)
SHELL_WRAPPER0(onResume)
SHELL_WRAPPER0(onLowMemory)
SHELL_WRAPPER3(callObserver, jstring, jstring, jstring)
SHELL_WRAPPER1(removeObserver, jstring)

static void * xul_handle = NULL;
static time_t apk_mtime = 0;
#ifdef DEBUG
extern "C" int extractLibs = 1;
#else
extern "C" int extractLibs = 0;
#endif

static void
extractFile(const char * path, const struct cdir_entry *entry, void * data)
{
  uint32_t size = letoh32(entry->uncompressed_size);

  struct stat status;
  if (!stat(path, &status) &&
      status.st_size == size &&
      apk_mtime < status.st_mtime)
    return;

  int fd = open(path, O_CREAT | O_NOATIME | O_TRUNC | O_RDWR, S_IRWXU);
  if (fd == -1) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't open %s to decompress library", path);
    return;
  }

  void * buf = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (buf == (void *)-1) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't mmap decompression buffer");
    return;
  }

  z_stream strm = {
    next_in: (Bytef *)data,
    avail_in: letoh32(entry->compressed_size),
    total_in: 0,

    next_out: (Bytef *)buf,
    avail_out: 4096,
    total_out: 0
  };

  int ret;
  ret = inflateInit2(&strm, -MAX_WBITS);
  if (ret != Z_OK)
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "inflateInit failed: %s", strm.msg);

  while ((ret = inflate(&strm, Z_SYNC_FLUSH)) != Z_STREAM_END) {
    simple_write(fd, buf, 4096 - strm.avail_out);
    strm.next_out = (Bytef *)buf;
    strm.avail_out = 4096;
  }
  simple_write(fd, buf, 4096 - strm.avail_out);

  if (ret != Z_STREAM_END)
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "inflate failed: %s", strm.msg);

  if (strm.total_out != size)
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "extracted %d, expected %d!", strm.total_out, size);

  ret = inflateEnd(&strm);
  if (ret != Z_OK)
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "inflateEnd failed: %s", strm.msg);

  close(fd);
  munmap(buf, 4096);
}

static void
extractLib(const struct cdir_entry *entry, void * data, void * dest)
{
  z_stream strm = {
    next_in: (Bytef *)data,
    avail_in: letoh32(entry->compressed_size),
    total_in: 0,

    next_out: (Bytef *)dest,
    avail_out: letoh32(entry->uncompressed_size),
    total_out: 0
  };

  int ret;
  ret = inflateInit2(&strm, -MAX_WBITS);
  if (ret != Z_OK)
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "inflateInit failed: %s", strm.msg);

  ret = inflate(&strm, Z_SYNC_FLUSH);
  if (ret != Z_STREAM_END)
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "inflate failed: %s", strm.msg);

  ret = inflateEnd(&strm);
  if (ret != Z_OK)
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "inflateEnd failed: %s", strm.msg);

  if (strm.total_out != letoh32(entry->uncompressed_size))
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "File not fully uncompressed! %d / %d", strm.total_out, letoh32(entry->uncompressed_size));
}

static void * mozload(const char * path, void *zip,
                      struct cdir_entry *cdir_start, uint16_t cdir_entries)
{
#ifdef DEBUG
  struct timeval t0, t1;
  gettimeofday(&t0, 0);
#endif

  struct cdir_entry *entry = find_cdir_entry(cdir_start, cdir_entries, path);
  struct local_file_header *file = (struct local_file_header *)(zip + letoh32(entry->offset));
  void * data = ((void *)&file->data) + letoh16(file->filename_size) + letoh16(file->extra_field_size);
  void * handle;

  if (extractLibs) {
    char fullpath[256];
    snprintf(fullpath, 256, "/data/data/org.mozilla.fennec/%s", path + 4);
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "resolved %s to %s", path, fullpath);
    extractFile(fullpath, entry, data);
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "%s: 1", fullpath);
    handle = __wrap_dlopen(fullpath, RTLD_LAZY);
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "%s: 2", fullpath);
    if (!handle)
      __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't load %s because %s", fullpath, __wrap_dlerror());
#ifdef DEBUG
    gettimeofday(&t1, 0);
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "%s: spent %d", path,
                        (((long long)t1.tv_sec * 1000000LL) +
                          (long long)t1.tv_usec) -
                        (((long long)t0.tv_sec * 1000000LL) +
                          (long long)t0.tv_usec));
#endif
    return handle;
  }

  size_t offset = letoh32(entry->offset) + sizeof(*file) + letoh16(file->filename_size) + letoh16(file->extra_field_size);

  int fd = zip_fd;
  void * buf = NULL;
  if (letoh16(file->compression) == DEFLATE) {
    fd = -1;
    buf = mmap(NULL, letoh32(entry->uncompressed_size),
               PROT_READ | PROT_WRITE | PROT_EXEC,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buf == (void *)-1) {
      __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't mmap decompression buffer");
      return NULL;
    }

    offset = 0;

    extractLib(entry, data, buf);
    data = buf;
  }

#ifdef DEBUG
  __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Loading %s with len %d and offset %d", path, letoh32(entry->uncompressed_size), offset);
#endif

  handle = moz_mapped_dlopen(path, RTLD_LAZY, fd, data,
                             letoh32(entry->uncompressed_size), offset);
  if (!handle)
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't load %s because %s", path, __wrap_dlerror());
  if (buf)
    munmap(buf, letoh32(entry->uncompressed_size));

#ifdef DEBUG
  gettimeofday(&t1, 0);
  __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "%s: spent %d", path, (((long long)t1.tv_sec * 1000000LL) + (long long)t1.tv_usec) -
               (((long long)t0.tv_sec * 1000000LL) + (long long)t0.tv_usec));
#endif

  return handle;
}

static void *
extractBuf(const char * path, void *zip,
           struct cdir_entry *cdir_start, uint16_t cdir_entries)
{
  struct cdir_entry *entry = find_cdir_entry(cdir_start, cdir_entries, path);
  struct local_file_header *file = (struct local_file_header *)(zip + letoh32(entry->offset));
  void * data = ((void *)&file->data) + letoh16(file->filename_size) + letoh16(file->extra_field_size);

  void * buf = malloc(letoh32(entry->uncompressed_size));
  if (buf == (void *)-1) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't alloc decompression buffer for %s", path);
    return NULL;
  }
  if (letoh16(file->compression) == DEFLATE)
    extractLib(entry, data, buf);
  else
    memcpy(buf, data, letoh32(entry->uncompressed_size));

  return buf;
}

static int mapping_count = 0;
static char *file_ids = NULL;

#define MAX_MAPPING_INFO 32

extern "C" void
report_mapping(char *name, void *base, uint32_t len, uint32_t offset)
{
  if (!file_ids || mapping_count >= MAX_MAPPING_INFO)
    return;

  struct mapping_info *info = &lib_mapping[mapping_count++];
  info->name = strdup(name);
  info->base = (uintptr_t)base;
  info->len = len;
  info->offset = offset;

  char * entry = strstr(file_ids, name);
  if (entry)
    info->file_id = strndup(entry + strlen(name) + 1, 32);
}

extern "C" void simple_linker_init(void);

static void
loadLibs(const char *apkName)
{
  chdir("/data/data/org.mozilla.fennec");

  simple_linker_init();

  struct stat status;
  if (!stat(apkName, &status))
    apk_mtime = status.st_mtime;

#ifdef DEBUG
  struct timeval t0, t1;
  gettimeofday(&t0, 0);
#endif

  void *zip = map_file(apkName);
  struct cdir_end *dirend = (struct cdir_end *)(zip + zip_size - sizeof(*dirend));
  while ((void *)dirend > zip &&
         letoh32(dirend->signature) != CDIR_END_SIG)
    dirend = (struct cdir_end *)((void *)dirend - 1);
  if (letoh32(dirend->signature) != CDIR_END_SIG) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't find end of central directory record");
    return;
  }

  uint32_t cdir_offset = letoh32(dirend->cdir_offset);
  uint16_t cdir_entries = letoh16(dirend->cdir_entries);

  struct cdir_entry *cdir_start = (struct cdir_entry *)(zip + cdir_offset);

#ifdef MOZ_CRASHREPORTER
  lib_mapping = (struct mapping_info *)calloc(MAX_MAPPING_INFO, sizeof(*lib_mapping));
  file_ids = (char *)extractBuf("lib.id", zip, cdir_start, cdir_entries);
#endif

#define MOZLOAD(name) mozload("lib/lib" name ".so", zip, cdir_start, cdir_entries)
  MOZLOAD("mozalloc");
  MOZLOAD("nspr4");
  MOZLOAD("plc4");
  MOZLOAD("plds4");
  MOZLOAD("mozsqlite3");
  MOZLOAD("nssutil3");
  MOZLOAD("nss3");
  MOZLOAD("ssl3");
  MOZLOAD("smime3");
  xul_handle = MOZLOAD("xul");
  MOZLOAD("xpcom");
  MOZLOAD("nssckbi");
  MOZLOAD("freebl3");
  MOZLOAD("softokn3");
#undef MOZLOAD

  close(zip_fd);

#ifdef MOZ_CRASHREPORTER
  free(file_ids);
  file_ids = NULL;
#endif

  if (!xul_handle)
    __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "Couldn't get a handle to libxul!");

#define GETFUNC(name) f_ ## name = (name ## _t) __wrap_dlsym(xul_handle, "Java_org_mozilla_gecko_GeckoAppShell_" #name)
  GETFUNC(nativeInit);
  GETFUNC(nativeRun);
  GETFUNC(notifyGeckoOfEvent);
  GETFUNC(setSurfaceView);
  GETFUNC(setInitialSize);
  GETFUNC(onResume);
  GETFUNC(onLowMemory);
  GETFUNC(callObserver);
  GETFUNC(removeObserver);
#undef GETFUNC
#ifdef DEBUG
  gettimeofday(&t1, 0);
  __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "spent %d total",
             (((long long)t1.tv_sec * 1000000LL) + (long long)t1.tv_usec) -
             (((long long)t0.tv_sec * 1000000LL) + (long long)t0.tv_usec));
  __android_log_print(ANDROID_LOG_ERROR, "GeckoLibLoad", "All libraries loaded!");
#endif
}

extern "C" NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_loadLibs(JNIEnv *jenv, jclass jGeckoAppShellClass, jstring jApkName)
{
  const char* str;
  // XXX: java doesn't give us true UTF8, we should figure out something 
  // better to do here
  str = jenv->GetStringUTFChars(jApkName, NULL);
  if (str == NULL)
    return;

  loadLibs(str);
  jenv->ReleaseStringUTFChars(jApkName, str);
}

typedef int GeckoProcessType;
typedef int nsresult;

extern "C" NS_EXPORT int
ChildProcessInit(int argc, char* argv[])
{
  int i;
  for (i = 0; i < (argc - 1); i++) {
    if (strcmp(argv[i], "-omnijar"))
      continue;

    i = i + 1;
    break;
  }

  loadLibs(argv[i]);

  typedef GeckoProcessType (*XRE_StringToChildProcessType_t)(char*);
  typedef nsresult (*XRE_InitChildProcess_t)(int, char**, GeckoProcessType);
  XRE_StringToChildProcessType_t fXRE_StringToChildProcessType =
    (XRE_StringToChildProcessType_t)__wrap_dlsym(xul_handle, "XRE_StringToChildProcessType");
  XRE_InitChildProcess_t fXRE_InitChildProcess =
    (XRE_InitChildProcess_t)__wrap_dlsym(xul_handle, "XRE_InitChildProcess");

  GeckoProcessType proctype = fXRE_StringToChildProcessType(argv[--argc]);

  nsresult rv = fXRE_InitChildProcess(argc, argv, proctype);
  if (rv != 0)
    return 1;

  return 0;
}

