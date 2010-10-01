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
 * The Original Code is Mozilla Application Update.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *  Robert Strong <robert.bugzilla@gmail.com>
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
 *  update.manifest
 *  ---------------
 *
 *  contents = 1*( line )
 *  line     = method LWS *( param LWS ) CRLF
 *  method   = "add" | "remove" | "patch"
 *  CRLF     = "\r\n"
 *  LWS      = 1*( " " | "\t" )
 */

#if defined(XP_WIN)
# include <windows.h>
# include <direct.h>
# include <io.h>
# define F_OK 00
# define W_OK 02
# define R_OK 04
# define access _access
#ifndef WINCE
# define putenv _putenv
#endif
# define fchmod(a,b)
# define NS_T(str) L ## str
// On Windows, _snprintf and _snwprintf don't guarantee null termination. These
// macros always leave room in the buffer for null termination and set the end
// of the buffer to null in case the string is larger than the buffer. Having
// multiple nulls in a string is fine and this approach is simpler (possibly
// faster) than calculating the string length to place the null terminator and
// will truncate the string as _snprintf and _snwprintf do on other platforms.
# define snprintf(dest, count, fmt, ...) \
  PR_BEGIN_MACRO \
    int _count = count - 1; \
    _snprintf(dest, _count, fmt, ##__VA_ARGS__); \
    dest[_count] = '\0'; \
  PR_END_MACRO
# define NS_tsnprintf(dest, count, fmt, ...) \
  PR_BEGIN_MACRO \
    int _count = count - 1; \
    _snwprintf(dest, _count, fmt, ##__VA_ARGS__); \
    dest[_count] = L'\0'; \
  PR_END_MACRO
# define NS_tstrrchr wcsrchr
# define NS_taccess _waccess
# define NS_tchdir _wchdir
# define NS_tchmod _wchmod
# define NS_tmkdir(path, perms) _wmkdir(path)
# define NS_tremove _wremove
# define NS_tfopen _wfopen
#ifndef WINCE
# define stat _stat
#endif
# define NS_tstat _wstat
# define BACKUP_EXT L".moz-backup"
# define CALLBACK_BACKUP_EXT L".moz-callback"
# define LOG_S "%S"
#else
# include <sys/wait.h>
# include <unistd.h>
#ifdef XP_MACOSX
# include <sys/time.h>
#endif

# define NS_T(str) str
# define NS_tsnprintf snprintf
# define NS_tstrrchr strrchr
# define NS_taccess access
# define NS_tchdir chdir
# define NS_tchmod chmod
# define NS_tmkdir mkdir
# define NS_tremove remove
# define NS_tfopen fopen
# define NS_tstat stat
# define BACKUP_EXT ".moz-backup"
# define LOG_S "%s"
#endif

#include "bspatch.h"
#include "progressui.h"
#include "archivereader.h"
#include "errors.h"
#include "bzlib.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>

#ifdef WINCE
#include "updater_wince.h"
#endif

// Amount of the progress bar to use in each of the 3 update stages,
// should total 100.0.
#define PROGRESS_PREPARE_SIZE 20.0f
#define PROGRESS_EXECUTE_SIZE 75.0f
#define PROGRESS_FINISH_SIZE   5.0f

#if defined(XP_MACOSX)
// These functions are defined in launchchild_osx.mm
void LaunchChild(int argc, char **argv);
void LaunchMacPostProcess(const char* aAppExe);
#endif

#ifndef _O_BINARY
# define _O_BINARY 0
#endif

#ifndef NULL
# define NULL (0)
#endif

#ifndef SSIZE_MAX
# define SSIZE_MAX LONG_MAX
#endif

#ifndef MAXPATHLEN
# ifdef PATH_MAX
#  define MAXPATHLEN PATH_MAX
# elif defined(MAX_PATH)
#  define MAXPATHLEN MAX_PATH
# elif defined(_MAX_PATH)
#  define MAXPATHLEN _MAX_PATH
# elif defined(CCHMAXPATH)
#  define MAXPATHLEN CCHMAXPATH
# else
#  define MAXPATHLEN 1024
# endif
#endif

// We want to use execv to invoke the callback executable on platforms where
// we were launched using execv.  See nsUpdateDriver.cpp.
#if defined(XP_UNIX) && !defined(XP_MACOSX)
#define USE_EXECV
#endif

#ifdef XP_WIN
#ifdef WINCE
#define EXIT_WHEN_ELEVATED(path, handle, retCode)
#else
// Closes the handle if valid and if the updater is elevated returns with the
// return code specified. This prevents multiple launches of the callback
// application by preventing the elevated process from launching the callback.
#define EXIT_WHEN_ELEVATED(path, handle, retCode) \
  { \
      if (handle != INVALID_HANDLE_VALUE) { \
        CloseHandle(handle); \
      } \
      if (_waccess(path, F_OK) == 0 && NS_tremove(path) != 0) { \
        return retCode; \
      } \
  }
#endif
#endif

char *BigBuffer = NULL;
int BigBufferSize = 262144;

//-----------------------------------------------------------------------------

// This variable lives in libbz2.  It's declared in bzlib_private.h, so we just
// declare it here to avoid including that entire header file.
#if (__GNUC__ >= 4) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)
extern "C"  __attribute__((visibility("default"))) unsigned int BZ2_crc32Table[256];
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
extern "C" __global unsigned int BZ2_crc32Table[256];
#else
extern "C" unsigned int BZ2_crc32Table[256];
#endif

static unsigned int
crc32(const unsigned char *buf, unsigned int len)
{
  unsigned int crc = 0xffffffffL;

  const unsigned char *end = buf + len;
  for (; buf != end; ++buf)
    crc = (crc << 8) ^ BZ2_crc32Table[(crc >> 24) ^ *buf];

  crc = ~crc;
  return crc;
}

//-----------------------------------------------------------------------------

// A simple stack based container for a FILE struct that closes the
// file descriptor from its destructor.
class AutoFile
{
public:
  AutoFile(FILE* file = NULL)
    : mFile(file) {
  }

  ~AutoFile() {
    if (mFile != NULL)
      fclose(mFile);
  }

  AutoFile &operator=(FILE* file) {
    if (mFile != 0)
      fclose(mFile);
    mFile = file;
    return *this;
  }

  operator FILE*() {
    return mFile;
  }
  
  FILE* get() {
    return mFile;
  }

private:
  FILE* mFile;
};

//-----------------------------------------------------------------------------

typedef void (* ThreadFunc)(void *param);

#ifdef XP_WIN
#include <process.h>

class Thread
{
public:
  int Run(ThreadFunc func, void *param)
  {
    mThreadFunc = func;
    mThreadParam = param;

#ifdef WINCE
    DWORD threadID;
#else
    unsigned int threadID;
#endif
    mThread = (HANDLE) _beginthreadex(NULL, 0, ThreadMain, this, 0, &threadID);
    
    return mThread ? 0 : -1;
  }
  int Join()
  {
    WaitForSingleObject(mThread, INFINITE);
    CloseHandle(mThread);
    return 0;
  }
private:
  static unsigned __stdcall ThreadMain(void *p)
  {
    Thread *self = (Thread *) p;
    self->mThreadFunc(self->mThreadParam);
    return 0;
  }
  HANDLE     mThread;
  ThreadFunc mThreadFunc;
  void      *mThreadParam;
};

#elif defined(XP_UNIX)
#include <pthread.h>

class Thread
{
public:
  int Run(ThreadFunc func, void *param)
  {
    return pthread_create(&thr, NULL, (void* (*)(void *)) func, param);
  }
  int Join()
  {
    void *result;
    return pthread_join(thr, &result);
  }
private:
  pthread_t thr;
};

#elif defined(XP_OS2)

class Thread
{
public:
  int Run(ThreadFunc func, void *param)
  {
    mThreadFunc = func;
    mThreadParam = param;

    mThread = _beginthread(ThreadMain, NULL, 16384, (void *)this);
    
    return mThread ? 0 : -1;
  }
  int Join()
  {
    int status;
    waitpid(mThread, &status, 0);
    return 0;
  }
private:
  static void ThreadMain(void *p)
  {
    Thread *self = (Thread *) p;
    self->mThreadFunc(self->mThreadParam);
  }
  int        mThread;
  ThreadFunc mThreadFunc;
  void      *mThreadParam;
};

#else
#error "Unsupported platform"
#endif

//-----------------------------------------------------------------------------

static NS_tchar* gSourcePath;
static ArchiveReader gArchiveReader;
static bool gSucceeded = false;

#ifdef XP_WIN
WIN32_FIND_DATAW gFFData;
#ifdef WINCE
// Since WinCE doesn't have a current working directory store the current
// working directory specified in the command line arguments 
static NS_tchar* gDestPath;
#endif
#endif

static const char kWhitespace[] = " \t";
static const char kNL[] = "\r\n";
static const char kQuote[] = "\"";

//-----------------------------------------------------------------------------
// LOGGING

static FILE *gLogFP = NULL;

static void LogInit()
{
  if (gLogFP)
    return;

  NS_tchar logFile[MAXPATHLEN];
  NS_tsnprintf(logFile, sizeof(logFile)/sizeof(logFile[0]),
               NS_T("%s/update.log"), gSourcePath);

  gLogFP = NS_tfopen(logFile, NS_T("w"));
}

static void LogFinish()
{
  if (!gLogFP)
    return;

  fclose(gLogFP);
  gLogFP = NULL;
}

static void LogPrintf(const char *fmt, ... )
{
  if (!gLogFP)
    return;

  va_list ap;
  va_start(ap, fmt);
  vfprintf(gLogFP, fmt, ap);
  va_end(ap);
}

#define LOG(args) LogPrintf args

//-----------------------------------------------------------------------------

static inline size_t
mmin(size_t a, size_t b)
{
  return (a > b) ? b : a;
}

static char*
mstrtok(const char *delims, char **str)
{
  if (!*str || !**str)
    return NULL;

  // skip leading "whitespace"
  char *ret = *str;
  const char *d;
  do {
    for (d = delims; *d != '\0'; ++d) {
      if (*ret == *d) {
        ++ret;
        break;
      }
    }
  } while (*d);

  if (!*ret) {
    *str = ret;
    return NULL;
  }

  char *i = ret;
  do {
    for (d = delims; *d != '\0'; ++d) {
      if (*i == *d) {
        *i = '\0';
        *str = ++i;
        return ret;
      }
    }
    ++i;
  } while (*i);

  *str = NULL;
  return ret;
}

#ifdef XP_WIN
// Returns a wchar path. The path returned will be absolute on Windows CE /
// Windows Mobile and relative on all other versions of Windows.
static NS_tchar*
get_wide_path(const char *path)
{
  WCHAR *s = (WCHAR*) malloc(MAXPATHLEN * sizeof(WCHAR));
  if (!s)
    return NULL;

  WCHAR *c = s;
  WCHAR wpath[MAXPATHLEN];
  MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, MAXPATHLEN);
#ifndef WINCE
  wcscpy(c, wpath);
  c += wcslen(wpath);
#else
  wcscpy(c, gDestPath);
  c += wcslen(gDestPath);
  wcscat(c, wpath);
  c += wcslen(wpath);
#endif
  if (wcscmp(wpath, gFFData.cFileName) == 0) {
    wcscat(c, CALLBACK_BACKUP_EXT);
    c += wcslen(CALLBACK_BACKUP_EXT);
  }
  *c = NS_T('\0');
  c++;
  return s;
}
#endif

static void ensure_write_permissions(const NS_tchar *path)
{
#ifdef XP_WIN
  (void) _wchmod(path, _S_IREAD | _S_IWRITE);
#else
  struct stat fs;
  if (!stat(path, &fs) && !(fs.st_mode & S_IWUSR)) {
    (void)chmod(path, fs.st_mode | S_IWUSR);
  }
#endif
}

static int ensure_remove(const NS_tchar *path)
{
  ensure_write_permissions(path);
  int rv = NS_tremove(path);
  if (rv)
    LOG(("ensure_remove: failed to remove file: " LOG_S ",%d,%d\n", path, rv,
         errno));
  return rv;
}

static FILE* ensure_open(const NS_tchar *path, const NS_tchar* flags, unsigned int options)
{
  ensure_write_permissions(path);
  FILE* f = NS_tfopen(path, flags);
  if (NS_tchmod(path, options) != 0) {
    if (f != NULL) {
      fclose(f);
    }
    return NULL;
  }
  struct stat ss;
  if (NS_tstat(path, &ss) != 0 || ss.st_mode != options) {
    if (f != NULL) {
      fclose(f);
    }
    return NULL;
  }
  return f;
}

// Ensure that the directory containing this file exists.
static int ensure_parent_dir(const NS_tchar *path)
{
  int rv = OK;

  NS_tchar *slash = (NS_tchar *) NS_tstrrchr(path, NS_T('/'));
  if (slash) {
    *slash = NS_T('\0');
    rv = ensure_parent_dir(path);
    if (rv == OK) {
      rv = NS_tmkdir(path, 0755);
      // If the directory already exists, then ignore the error. On WinCE rv
      // will equal 0 if the directory already exists.
      if (rv < 0 && errno != EEXIST) {
        LOG(("ensure_parent_dir: failed to create directory: " LOG_S ",%d\n",
             path, errno));
        rv = WRITE_ERROR;
      } else {
        rv = OK;
      }
    }
    *slash = NS_T('/');
  }
  return rv;
}

static int copy_file(const NS_tchar *spath, const NS_tchar *dpath)
{
  int rv = ensure_parent_dir(dpath);
  if (rv)
    return rv;

  struct stat ss;

  AutoFile sfile = NS_tfopen(spath, NS_T("rb"));
  if (sfile == NULL || fstat(fileno((FILE*)sfile), &ss)) {
    LOG(("copy_file: failed to open or stat: %p," LOG_S ",%d\n", sfile.get(),
         spath, errno));
    return READ_ERROR;
  }

  AutoFile dfile = ensure_open(dpath, NS_T("wb+"), ss.st_mode); 
  if (dfile == NULL) {
    LOG(("copy_file: failed to open: " LOG_S ",%d\n", dpath, errno));
    return WRITE_ERROR;
  }

  size_t sc;
  while ((sc = fread(BigBuffer, 1, BigBufferSize, sfile)) > 0) {
    size_t dc;
    char *bp = BigBuffer;
    while ((dc = fwrite(bp, 1, (unsigned int) sc, dfile)) > 0) {
      if ((sc -= dc) == 0)
        break;
      bp += dc;
    }
    if (dc < 0) {
      LOG(("copy_file: failed to write: %d\n", errno));
      return WRITE_ERROR;
    }
  }
  if (sc < 0) {
    LOG(("copy_file: failed to read: %d\n", errno));
    return READ_ERROR;
  }

  return OK;
}

//-----------------------------------------------------------------------------

// Create a backup copy of the specified file alongside it.
static int backup_create(const NS_tchar *path)
{
  NS_tchar backup[MAXPATHLEN];
  NS_tsnprintf(backup, sizeof(backup)/sizeof(backup[0]),
               NS_T("%s" BACKUP_EXT), path);

  return copy_file(path, backup);
}

// Copy the backup copy of the specified file back overtop
// the specified file.
// XXX should be a file move instead
static int backup_restore(const NS_tchar *path)
{
  NS_tchar backup[MAXPATHLEN];
  NS_tsnprintf(backup, sizeof(backup)/sizeof(backup[0]),
               NS_T("%s" BACKUP_EXT), path);

  int rv = copy_file(backup, path);
  if (rv) {
    ensure_remove(backup);
    return rv;
  }

  rv = ensure_remove(backup);
  if (rv) {
    LOG(("backup_restore: failed to remove backup file: " LOG_S ",%d\n", backup,
         errno));
    return WRITE_ERROR;
  }

  return OK;
}

// Discard the backup copy of the specified file.
static int backup_discard(const NS_tchar *path)
{
  NS_tchar backup[MAXPATHLEN];
  NS_tsnprintf(backup, sizeof(backup)/sizeof(backup[0]),
               NS_T("%s" BACKUP_EXT), path);

  int rv = ensure_remove(backup);
  if (rv)
    return WRITE_ERROR;

  return OK;
}

// Helper function for post-processing a temporary backup.
static void backup_finish(const NS_tchar *path, int status)
{
  if (status == OK)
    backup_discard(path);
  else
    backup_restore(path);
}

//-----------------------------------------------------------------------------

static int DoUpdate();

static const int ACTION_DESCRIPTION_BUFSIZE = 256;

class Action
{
public:
  Action() : mProgressCost(1), mNext(NULL) { }
  virtual ~Action() { }

  virtual int Parse(char *line) = 0;

  // Do any preprocessing to ensure that the action can be performed.  Execute
  // will be called if this Action and all others return OK from this method.
  virtual int Prepare() = 0;

  // Perform the operation.  Return OK to indicate success.  After all actions
  // have been executed, Finish will be called.  A requirement of Execute is
  // that its operation be reversable from Finish.
  virtual int Execute() = 0;
  
  // Finish is called after execution of all actions.  If status is OK, then
  // all actions were successfully executed.  Otherwise, some action failed.
  virtual void Finish(int status) = 0;

  int mProgressCost;
private:
  Action* mNext;

  friend class ActionList;
};

class RemoveFile : public Action
{
public:
  RemoveFile() : mFile(NULL), mDestFile(NULL), mSkip(0) { }

  int Parse(char *line);
  int Prepare();
  int Execute();
  void Finish(int status);

private:
  const char* mFile;
  const NS_tchar* mDestFile;
  int mSkip;
};

int
RemoveFile::Parse(char *line)
{
  // format "<deadfile>"

  mFile = mstrtok(kQuote, &line);
  if (!mFile)
    return PARSE_ERROR;

#ifdef XP_WIN
  mDestFile = get_wide_path(mFile);
  if (!mDestFile)
    return PARSE_ERROR;
#else
  mDestFile = mFile;
#endif

  return OK;
}

int
RemoveFile::Prepare()
{
  LOG(("PREPARE REMOVE %s\n", mFile));

  // We expect the file to exist if we are to remove it.
  int rv = NS_taccess(mDestFile, F_OK);
  if (rv) {
    LOG(("file does not exist; skipping\n"));
    mSkip = 1;
    mProgressCost = 0;
    return OK;
  }

#ifndef WINCE
  NS_tchar *slash = (NS_tchar *) NS_tstrrchr(mDestFile, NS_T('/'));
  if (slash) {
    *slash = NS_T('\0');
    rv = NS_taccess(mDestFile, W_OK);
    *slash = NS_T('/');
  } else {
    rv = NS_taccess(NS_T("."), W_OK);
  }

  if (rv) {
    LOG(("access failed: %d\n", errno));
    return WRITE_ERROR;
  }
#endif

  return OK;
}

int
RemoveFile::Execute()
{
  if (mSkip)
    return OK;

  LOG(("EXECUTE REMOVE %s\n", mFile));

  // We expect the file to exist if we are to remove it.  We check here as well
  // as in PREPARE since we might have been asked to remove the same file more
  // than once: bug 311099.
  int rv = NS_taccess(mDestFile, F_OK);
  if (rv) {
    LOG(("file cannot be removed because it does not exist; skipping\n"));
    mSkip = 1;
    return OK;
  }

  // save a complete copy of the old file, and then remove the
  // old file.  we'll clean up the copy in Finish.

  rv = backup_create(mDestFile);
  if (rv) {
    LOG(("backup_create failed: %d\n", rv));
    return rv;
  }

  rv = ensure_remove(mDestFile);
  if (rv)
    return WRITE_ERROR;

  return OK;
}

void
RemoveFile::Finish(int status)
{
  if (mSkip)
    return;

  LOG(("FINISH REMOVE %s\n", mFile));

  backup_finish(mDestFile, status);
}

class AddFile : public Action
{
public:
  AddFile() : mFile(NULL), mDestFile(NULL) { }

  virtual int Parse(char *line);
  virtual int Prepare(); // check that the source file exists
  virtual int Execute();
  virtual void Finish(int status);

private:
  const char *mFile;
  const NS_tchar *mDestFile;
};

int
AddFile::Parse(char *line)
{
  // format "<newfile>"

  mFile = mstrtok(kQuote, &line);
  if (!mFile)
    return PARSE_ERROR;

#ifdef XP_WIN
  mDestFile = get_wide_path(mFile);
  if (!mDestFile)
    return PARSE_ERROR;
#else
  mDestFile = mFile;
#endif

  return OK;
}

int
AddFile::Prepare()
{
  LOG(("PREPARE ADD %s\n", mFile));

  return OK;
}

int
AddFile::Execute()
{
  LOG(("EXECUTE ADD %s\n", mFile));

  int rv;

  // First make sure that we can actually get rid of any existing file.
  rv = NS_taccess(mDestFile, F_OK);
  if (rv == 0) {
    rv = backup_create(mDestFile);
    if (rv)
      return rv;

    rv = ensure_remove(mDestFile);
    if (rv)
      return WRITE_ERROR;
  } else {
    rv = ensure_parent_dir(mDestFile);
    if (rv)
      return rv;
  }

  return gArchiveReader.ExtractFile(mFile, mDestFile);
}

void
AddFile::Finish(int status)
{
  LOG(("FINISH ADD %s\n", mFile));

  backup_finish(mDestFile, status);
}

class PatchFile : public Action
{
public:
  PatchFile() : mPatchIndex(-1), buf(NULL) { }
  virtual ~PatchFile();

  virtual int Parse(char *line);
  virtual int Prepare(); // check for the patch file and for checksums
  virtual int Execute();
  virtual void Finish(int status);

private:
  int LoadSourceFile(FILE* ofile);

  static int sPatchIndex;

  const char *mPatchFile;
  const char *mFile;
  const NS_tchar *mDestFile;
  int mPatchIndex;
  MBSPatchHeader header;
  unsigned char *buf;
  NS_tchar spath[MAXPATHLEN];
};

int PatchFile::sPatchIndex = 0;

PatchFile::~PatchFile()
{
  // delete the temporary patch file
  if (spath[0])
    NS_tremove(spath);

  if (buf)
    free(buf);
}

int
PatchFile::LoadSourceFile(FILE* ofile)
{
  struct stat os;
  int rv = fstat(fileno((FILE*)ofile), &os);
  if (rv) {
    LOG(("LoadSourceFile: unable to stat destination file: " LOG_S ",%d\n",
         mDestFile, errno));
    return READ_ERROR;
  }

  if (PRUint32(os.st_size) != header.slen) {
    LOG(("LoadSourceFile: destination file size %d does not match expected size %d\n",
         PRUint32(os.st_size), header.slen));
    return UNEXPECTED_ERROR;
  }

  buf = (unsigned char*) malloc(header.slen);
  if (!buf)
    return MEM_ERROR;

  size_t r = header.slen;
  unsigned char *rb = buf;
  while (r) {
    size_t c = fread(rb, 1, r, ofile);
    if (c < 0) {
      LOG(("LoadSourceFile: error reading destination file: " LOG_S "\n",
           mDestFile));
      return READ_ERROR;
    }

    r -= c;
    rb += c;

    if (c == 0 && r) {
      LOG(("LoadSourceFile: expected %d more bytes in destination file\n", r));
      return UNEXPECTED_ERROR;
    }
  }

  // Verify that the contents of the source file correspond to what we expect.

  unsigned int crc = crc32(buf, header.slen);

  if (crc != header.scrc32) {
    LOG(("CRC check failed\n"));
    return CRC_ERROR;
  }
  
  return OK;
}

int
PatchFile::Parse(char *line)
{
  // format "<patchfile>" "<filetopatch>"

  mPatchFile = mstrtok(kQuote, &line);
  if (!mPatchFile)
    return PARSE_ERROR;

  // consume whitespace between args
  char *q = mstrtok(kQuote, &line);
  if (!q)
    return PARSE_ERROR;

  mFile = mstrtok(kQuote, &line);
  if (!mFile)
    return PARSE_ERROR;

#ifdef XP_WIN
  mDestFile = get_wide_path(mFile);
  if (!mDestFile)
    return PARSE_ERROR;
#else
  mDestFile = mFile;
#endif

  return OK;
}

int
PatchFile::Prepare()
{
  LOG(("PREPARE PATCH %s\n", mFile));

  // extract the patch to a temporary file
  mPatchIndex = sPatchIndex++;

  NS_tsnprintf(spath, sizeof(spath)/sizeof(spath[0]),
               NS_T("%s/%d.patch"), gSourcePath, mPatchIndex);

  NS_tremove(spath);

  FILE *fp = NS_tfopen(spath, NS_T("wb"));
  if (!fp)
    return WRITE_ERROR;

  int rv = gArchiveReader.ExtractFileToStream(mPatchFile, fp);
  fclose(fp);
  return rv;
}

int
PatchFile::Execute()
{
  LOG(("EXECUTE PATCH %s\n", mFile));

  AutoFile pfile = NS_tfopen(spath, NS_T("rb"));
  if (pfile == NULL)
    return READ_ERROR;

  int rv = MBS_ReadHeader(pfile, &header);
  if (rv)
    return rv;

  FILE *origfile = NS_tfopen(mDestFile, NS_T("rb"));
  if (!origfile) {
    LOG(("unable to open destination file: " LOG_S ",%d\n", mDestFile, errno));
    return READ_ERROR;
  }

  rv = LoadSourceFile(origfile);
  fclose(origfile);
  if (rv) {
    LOG(("LoadSourceFile failed\n"));
    return rv;
  }

  // Create backup copy of the destination file before proceeding.

  struct stat ss;
  if (NS_tstat(mDestFile, &ss))
    return READ_ERROR;

  rv = backup_create(mDestFile);
  if (rv)
    return rv;

  rv = ensure_remove(mDestFile);
  if (rv) {
    LOG(("unable to remove original file: " LOG_S ",%d\n", mDestFile, errno));
    return WRITE_ERROR;
  }

  AutoFile ofile = ensure_open(mDestFile, NS_T("wb+"), ss.st_mode);
  if (ofile == NULL) {
    LOG(("unable to create new file: " LOG_S ",%d\n", mDestFile, errno));
    return WRITE_ERROR;
  }

  rv = MBS_ApplyPatch(&header, pfile, buf, ofile);

  // Go ahead and do a bit of cleanup now to minimize runtime overhead.
  // Set pfile to NULL to make AutoFile close the file so it can be deleted on
  // Windows.
  pfile = NULL;
  NS_tremove(spath);
  spath[0] = '\0';
  free(buf);
  buf = NULL;

  return rv;
}

void
PatchFile::Finish(int status)
{
  LOG(("FINISH PATCH %s\n", mFile));

  backup_finish(mDestFile, status);
}

class AddIfFile : public AddFile
{
public:
  AddIfFile() : mTestFile(NULL) { }

  virtual int Parse(char *line);
  virtual int Prepare(); // check that the source file exists
  virtual int Execute();
  virtual void Finish(int status);

protected:
  const char *mTestFile;
};

int
AddIfFile::Parse(char *line)
{
  // format "<testfile>" "<newfile>"

  mTestFile = mstrtok(kQuote, &line);
  if (!mTestFile)
    return PARSE_ERROR;

  // consume whitespace between args
  char *q = mstrtok(kQuote, &line);
  if (!q)
    return PARSE_ERROR;

  return AddFile::Parse(line);
}

int
AddIfFile::Prepare()
{
  // If the test file does not exist, then turn disable this action.
  if (access(mTestFile, F_OK)) {
    mTestFile = NULL;
    return OK;
  }

  return AddFile::Prepare();
}

int
AddIfFile::Execute()
{
  if (!mTestFile)
    return OK;

  return AddFile::Execute();
}

void
AddIfFile::Finish(int status)
{
  if (!mTestFile)
    return;

  AddFile::Finish(status);
}

class PatchIfFile : public PatchFile
{
public:
  PatchIfFile() : mTestFile(NULL) { }

  virtual int Parse(char *line);
  virtual int Prepare(); // check for the patch file and for checksums
  virtual int Execute();
  virtual void Finish(int status);

private:
  const char *mTestFile;
};

int
PatchIfFile::Parse(char *line)
{
  // format "<testfile>" "<patchfile>" "<filetopatch>"

  mTestFile = mstrtok(kQuote, &line);
  if (!mTestFile)
    return PARSE_ERROR;

  // consume whitespace between args
  char *q = mstrtok(kQuote, &line);
  if (!q)
    return PARSE_ERROR;

  return PatchFile::Parse(line);
}

int
PatchIfFile::Prepare()
{
  // If the test file does not exist, then turn disable this action.
  if (access(mTestFile, F_OK)) {
    mTestFile = NULL;
    return OK;
  }

  return PatchFile::Prepare();
}

int
PatchIfFile::Execute()
{
  if (!mTestFile)
    return OK;

  return PatchFile::Execute();
}

void
PatchIfFile::Finish(int status)
{
  if (!mTestFile)
    return;

  PatchFile::Finish(status);
}

//-----------------------------------------------------------------------------

#ifdef XP_WIN
#include "nsWindowsRestart.cpp"

#ifndef WINCE // until we have a replacement for GetPrivateProfileStringW, 
              // it doesn't make sense to use this function
static void
LaunchWinPostProcess(const WCHAR *appExe)
{
  // Launch helper.exe to perform post processing (e.g. registry and log file
  // modifications) for the update.
  WCHAR inifile[MAXPATHLEN];
  wcscpy(inifile, appExe);

  WCHAR *slash = wcsrchr(inifile, '\\');
  if (!slash)
    return;

  wcscpy(slash + 1, L"updater.ini");

  WCHAR exefile[MAXPATHLEN];
  WCHAR exearg[MAXPATHLEN];
  WCHAR exeasync[10];
  PRBool async = PR_TRUE;
  if (!GetPrivateProfileStringW(L"PostUpdateWin", L"ExeRelPath", NULL, exefile,
                                MAXPATHLEN, inifile))
    return;

  if (!GetPrivateProfileStringW(L"PostUpdateWin", L"ExeArg", NULL, exearg,
                                MAXPATHLEN, inifile))
    return;

  if (!GetPrivateProfileStringW(L"PostUpdateWin", L"ExeAsync", L"TRUE", exeasync,
                                sizeof(exeasync)/sizeof(exeasync[0]), inifile))
    return;

  WCHAR exefullpath[MAXPATHLEN];
  wcscpy(exefullpath, appExe);

  slash = wcsrchr(exefullpath, '\\');
  wcscpy(slash + 1, exefile);

  WCHAR dlogFile[MAXPATHLEN];
  wcscpy(dlogFile, exefullpath);

  slash = wcsrchr(dlogFile, '\\');
  wcscpy(slash + 1, L"uninstall.update");

  WCHAR slogFile[MAXPATHLEN];
  NS_tsnprintf(slogFile, sizeof(slogFile)/sizeof(slogFile[0]),
               NS_T("%s/update.log"), gSourcePath);

  WCHAR dummyArg[13];
  wcscpy(dummyArg, L"argv0ignored ");

  size_t len = wcslen(exearg) + wcslen(dummyArg);
  WCHAR *cmdline = (WCHAR *) malloc((len + 1) * sizeof(WCHAR));
  if (!cmdline)
    return;

  wcscpy(cmdline, dummyArg);
  wcscat(cmdline, exearg);

  if (!_wcsnicmp(exeasync, L"false", 6) || !_wcsnicmp(exeasync, L"0", 2))
    async = PR_FALSE;

  // We want to launch the post update helper app to update the Windows
  // registry even if there is a failure with removing the uninstall.update
  // file or copying the update.log file.
  NS_tremove(dlogFile);
  CopyFile(slogFile, dlogFile, FALSE);

  STARTUPINFOW si = {sizeof(si), 0};
  PROCESS_INFORMATION pi = {0};

  BOOL ok = CreateProcessW(exefullpath,
                           cmdline,
                           NULL,  // no special security attributes
                           NULL,  // no special thread attributes
                           FALSE, // don't inherit filehandles
                           0,     // No special process creation flags
                           NULL,  // inherit my environment
                           NULL,  // use my current directory
                           &si,
                           &pi);
  free(cmdline);

  if (ok) {
    if (!async)
      WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
}
#endif // WINCE
#endif

#ifdef WINCE
static char*
AllocConvertUTF16toUTF8(const WCHAR *arg)
{
  // be generous... UTF16 units can expand up to 3 UTF8 units
  int len = wcslen(arg);
  char *s = new char[len * 3 + 1];
  if (!s) {
    return NULL;
  }
  ConvertUTF16toUTF8 convert(s);
  convert.write(arg, len);
  convert.write_terminator();
  return s;
}
#endif

static void
LaunchCallbackApp(const NS_tchar *workingDir, int argc, NS_tchar **argv)
{
  putenv(const_cast<char*>("NO_EM_RESTART="));
  putenv(const_cast<char*>("MOZ_LAUNCHED_CHILD=1"));

#ifndef WINCE
  // Run from the specified working directory (see bug 312360). This is not
  // necessary on Windows CE since the application that launches the updater
  // passes the working directory as an --environ: command line argument.
  if(NS_tchdir(workingDir) != 0)
    LOG(("Warning: chdir failed\n"));
#endif

#if defined(USE_EXECV)
  execv(argv[0], argv);
#elif defined(XP_MACOSX)
  LaunchChild(argc, argv);
#elif defined(XP_WIN) && !defined(WINCE)
  WinLaunchChild(argv[0], argc, argv);
#elif defined(WINCE)
  // Support the mock environment used on Windows CE.
  int i;
  int winceArgc = 0;
  WCHAR **winceArgv;
  for (i = 0; i < argc; ++i) {
    if (wcsncmp(argv[i], L"--environ:", 10) != 0) {
      winceArgc++;
    }
  }

  winceArgv = (WCHAR**) malloc(sizeof(WCHAR*) * (winceArgc + 1));
  if (!winceArgv) {
    return;
  }

  int c = 0;
  for (i = 0; i < argc; ++i) {
    if (wcsncmp(argv[i], L"--environ:", 10) == 0) {
      char* key_val = AllocConvertUTF16toUTF8(wcsdup(argv[i] + 10));
      putenv(key_val);
      delete [] key_val;
    } else {
      winceArgv[c] = argv[i];
      c++;
    }
  }
  winceArgv[winceArgc + 1] = NULL;
  WinLaunchChild(argv[0], winceArgc, winceArgv);
  free(winceArgv);
#else
# warning "Need implementaton of LaunchCallbackApp"
#endif
}

static void
WriteStatusFile(int status)
{
  // This is how we communicate our completion status to the main application.

  NS_tchar filename[MAXPATHLEN];
  NS_tsnprintf(filename, sizeof(filename)/sizeof(filename[0]),
               NS_T("%s/update.status"), gSourcePath);

  AutoFile file = NS_tfopen(filename, NS_T("wb+"));
  if (file == NULL)
    return;

  const char *text;

  char buf[32];
  if (status == OK) {
    text = "succeeded\n";
  } else {
    snprintf(buf, sizeof(buf)/sizeof(buf[0]), "failed: %d\n", status);
    text = buf;
  }
  fwrite(text, strlen(text), 1, file);
}

static void
UpdateThreadFunc(void *param)
{
  // open ZIP archive and process...

  NS_tchar dataFile[MAXPATHLEN];
  NS_tsnprintf(dataFile, sizeof(dataFile)/sizeof(dataFile[0]),
               NS_T("%s/update.mar"), gSourcePath);

  int rv = gArchiveReader.Open(dataFile);
  if (rv == OK) {
    rv = DoUpdate();
    gArchiveReader.Close();
  }

  if (rv) {
    LOG(("failed: %d\n", rv));
  }
  else {
#ifdef XP_MACOSX
    // If the update was successful we need to update the timestamp
    // on the top-level Mac OS X bundle directory so that Mac OS X's
    // Launch Services picks up any major changes. Here we assume that
    // the current working directory is the top-level bundle directory.
    char* cwd = getcwd(NULL, 0);
    if (cwd) {
      if (utimes(cwd, NULL) != 0) {
        LOG(("Couldn't set access/modification time on application bundle.\n"));
      }
      free(cwd);
    }
    else {
      LOG(("Couldn't get current working directory for setting "
           "access/modification time on application bundle.\n"));
    }
#endif

    LOG(("succeeded\n"));
  }
  WriteStatusFile(rv);

  LOG(("calling QuitProgressUI\n"));
  QuitProgressUI();
}

int NS_main(int argc, NS_tchar **argv)
{
  InitProgressUI(&argc, &argv);
  // The updater command line consists of the directory path containing the
  // updater.mar file to process followed by the PID of the calling process.
  // The updater will wait on the parent process to exit if the PID is non-
  // zero.  This is leveraged on platforms such as Windows where it is
  // necessary for the parent process to exit before its executable image may
  // be altered.

#ifndef WINCE
  if (argc < 2) {
    fprintf(stderr, "Usage: updater <dir-path> [parent-pid [working-dir callback args...]]\n");
    return 1;
  }
#else
  if (argc < 4) {
    fprintf(stderr, "Usage: updater <dir-path> parent-pid <working-dir> [callback args...]]\n");
    return 1;
  }
#endif

  if (argc > 2 ) {
#ifdef XP_WIN
    __int64 pid = _wtoi64(argv[2]);
#else
    int pid = atoi(argv[2]);
#endif
    if (pid) {
#ifdef XP_WIN
      HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, (DWORD) pid);
      // May return NULL if the parent process has already gone away.
      // Otherwise, wait for the parent process to exit before starting the
      // update.
      if (parent) {
        DWORD result = WaitForSingleObject(parent, 5000);
        CloseHandle(parent);
        if (result != WAIT_OBJECT_0)
          return 1;
      }

      // The process may be signaled before it releases the executable image.
      // This is a terrible hack, but it'll have to do for now :-(
      Sleep(50);
#else
      int status;
      waitpid(pid, &status, 0);
#endif
    }
  }

  // The callback is the last N command line arguments starting from argOffset.
  // The argument specified by argOffset is the callback executable and the
  // argument prior to argOffset is the working directory.
  const int argOffset = 4;

  gSourcePath = argv[1];

#if defined(XP_WIN) && !defined(WINCE)
  // Launch a second instance of the updater with the runas verb on Windows
  // when write access is denied to the installation directory.
  HANDLE updateLockFileHandle;
  NS_tchar elevatedLockFilePath[MAXPATHLEN];
  if (argc > argOffset) {
    NS_tchar updateLockFilePath[MAXPATHLEN];
    NS_tsnprintf(updateLockFilePath,
                 sizeof(updateLockFilePath)/sizeof(updateLockFilePath[0]),
                 NS_T("%s.update_in_progress.lock"), argv[argOffset]);

    // The update_in_progress.lock file should only exist during an update. In
    // case it exists attempt to remove it and exit if that fails to prevent
    // simultaneous updates occurring.
    if (!_waccess(updateLockFilePath, F_OK) &&
        NS_tremove(updateLockFilePath) != 0) {
      fprintf(stderr, "Update already in progress! Exiting\n");
      return 1;
    }

    updateLockFileHandle = CreateFileW(updateLockFilePath,
                                       GENERIC_READ | GENERIC_WRITE,
                                       0,
                                       NULL,
                                       OPEN_ALWAYS,
                                       FILE_FLAG_DELETE_ON_CLOSE,
                                       NULL);

    NS_tsnprintf(elevatedLockFilePath,
                 sizeof(elevatedLockFilePath)/sizeof(elevatedLockFilePath[0]),
                 NS_T("%s/update_elevated.lock"), argv[1]);

    if (updateLockFileHandle == INVALID_HANDLE_VALUE) {
      if (!_waccess(elevatedLockFilePath, F_OK) &&
          NS_tremove(elevatedLockFilePath) != 0) {
        fprintf(stderr, "Update already elevated! Exiting\n");
        return 1;
      }

      HANDLE elevatedFileHandle;
      elevatedFileHandle = CreateFileW(elevatedLockFilePath,
                                       GENERIC_READ | GENERIC_WRITE,
                                       0,
                                       NULL,
                                       OPEN_ALWAYS,
                                       FILE_FLAG_DELETE_ON_CLOSE,
                                       NULL);

      if (elevatedFileHandle == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Unable to create elevated lock file! Exiting\n");
        return 1;
      }

      PRUnichar *cmdLine = MakeCommandLine(argc - 1, argv + 1);
      if (!cmdLine) {
        CloseHandle(elevatedFileHandle);
        return 1;
      }

      SHELLEXECUTEINFO sinfo;
      memset(&sinfo, 0, sizeof(SHELLEXECUTEINFO));
      sinfo.cbSize       = sizeof(SHELLEXECUTEINFO);
      sinfo.fMask        = SEE_MASK_FLAG_NO_UI |
                           SEE_MASK_FLAG_DDEWAIT |
                           SEE_MASK_NOCLOSEPROCESS;
      sinfo.hwnd         = NULL;
      sinfo.lpFile       = argv[0];
      sinfo.lpParameters = cmdLine;
      sinfo.lpVerb       = L"runas";
      sinfo.nShow        = SW_SHOWNORMAL;

      BOOL result = ShellExecuteEx(&sinfo);
      free(cmdLine);

      if (result) {
        WaitForSingleObject(sinfo.hProcess, INFINITE);
        CloseHandle(sinfo.hProcess);
      } else {
        WriteStatusFile(ELEVATION_CANCELED);
      }

      if (argc > argOffset) {
        LaunchCallbackApp(argv[3], argc - argOffset, argv + argOffset);
      }

      CloseHandle(elevatedFileHandle);
      return 0;
    }
  }
#endif

  LogInit();
  LOG(("SOURCE DIRECTORY " LOG_S "\n", gSourcePath));

  // The destination directory (the same as the working-dir argument) does not
  // have to be specified when updating manually.
  if (argc > argOffset - 1) {
    LOG(("DESTINATION DIRECTORY " LOG_S "\n", argv[3]));
  }

#ifdef WINCE
  // This is the working directory to apply the update and is required on WinCE
  // since it doesn't have the concept of a working directory.
  gDestPath = argv[3];
#endif

#ifdef XP_WIN
  HANDLE callbackFile = INVALID_HANDLE_VALUE;
  NS_tchar callbackBackupPath[MAXPATHLEN];
  if (argc > argOffset) {
    // FindFirstFileW is used to get the callback's filename for comparison
    // with the callback's patch since it will return the correct case and the
    // long name instead of the 8.3 format name.
    HANDLE hFindFile;
    hFindFile = FindFirstFileW(argv[argOffset], &gFFData);
    if (hFindFile == INVALID_HANDLE_VALUE) {
      LOG(("NS_main: unable to find callback file: " LOG_S "\n", argv[argOffset]));
      LogFinish();
      WriteStatusFile(WRITE_ERROR);
      EXIT_WHEN_ELEVATED(elevatedLockFilePath, updateLockFileHandle, 1);
      LaunchCallbackApp(argv[3], argc - argOffset, argv + argOffset);
      return 1;
    }
    FindClose(hFindFile);

    // Make a copy of the callback executable.
    NS_tsnprintf(callbackBackupPath,
                 sizeof(callbackBackupPath)/sizeof(callbackBackupPath[0]),
                 NS_T("%s" CALLBACK_BACKUP_EXT), argv[argOffset]);
    NS_tremove(callbackBackupPath);
    CopyFileW(argv[argOffset], callbackBackupPath, FALSE);

    // By opening a file handle to the callback executable, the OS will prevent
    // launching the process while it is being updated. 
    callbackFile = CreateFileW(argv[argOffset],
#ifdef WINCE
                               GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
#else
                               DELETE | GENERIC_WRITE,
                               0, // no sharing!
#endif
                               NULL, OPEN_EXISTING, 0, NULL);
    // CreateFileW will fail if the callback executable is already in use. Since
    // it isn't possible to update write the status file and return.
    if (callbackFile == INVALID_HANDLE_VALUE) {
      LOG(("NS_main: file in use - failed to exclusively open executable " \
           "file: " LOG_S "\n", argv[argOffset]));
      LogFinish();
      WriteStatusFile(WRITE_ERROR);
      NS_tremove(callbackBackupPath);
      EXIT_WHEN_ELEVATED(elevatedLockFilePath, updateLockFileHandle, 1);
      LaunchCallbackApp(argv[3], argc - argOffset, argv + argOffset);
      return 1;
    }
  }
#endif

  BigBuffer = (char *)malloc(BigBufferSize);
  if (!BigBuffer) {
    LOG(("NS_main: failed to allocate default buffer of %i. Trying 1K " \
         "buffer\n", BigBufferSize));

    // Try again with a smaller size.
    BigBufferSize = 1024;
    BigBuffer = (char *)malloc(BigBufferSize);
    if (!BigBuffer) {
      LOG(("NS_main: failed to allocate 1K buffer - exiting\n"));
      LogFinish();
      WriteStatusFile(MEM_ERROR);
#ifdef XP_WIN
      CloseHandle(callbackFile);
      NS_tremove(callbackBackupPath);
      EXIT_WHEN_ELEVATED(elevatedLockFilePath, updateLockFileHandle, 1);
#endif
      LaunchCallbackApp(argv[3], argc - argOffset, argv + argOffset);
      return 1;
    }
  }

  // Run update process on a background thread.  ShowProgressUI may return
  // before QuitProgressUI has been called, so wait for UpdateThreadFunc to
  // terminate.
  Thread t;
  if (t.Run(UpdateThreadFunc, NULL) == 0) {
    ShowProgressUI();
  }
  t.Join();

#ifdef XP_WIN
  if (argc > argOffset) {
    CloseHandle(callbackFile);
    // CopyFile will preserve the case of the destination file if it already
    // exists.
    if (CopyFileW(callbackBackupPath, argv[argOffset], FALSE) != 0) {
      NS_tremove(callbackBackupPath);
    }
  }
#endif

  LogFinish();
  free(BigBuffer);
  BigBuffer = NULL;

  if (argc > argOffset) {
#if defined(XP_WIN) && !defined(WINCE)
    if (gSucceeded) {
      LaunchWinPostProcess(argv[argOffset]);
    }
    EXIT_WHEN_ELEVATED(elevatedLockFilePath, updateLockFileHandle, 0);
#endif
#ifdef XP_MACOSX
    if (gSucceeded) {
      LaunchMacPostProcess(argv[argOffset]);
    }
#endif /* XP_MACOSX */
    LaunchCallbackApp(argv[3], argc - argOffset, argv + argOffset);
  }

  return 0;
}

class ActionList
{
public:
  ActionList() : mFirst(NULL), mLast(NULL), mCount(0) { }
  ~ActionList();

  void Append(Action* action);
  int Prepare();
  int Execute();
  void Finish(int status);

private:
  Action *mFirst;
  Action *mLast;
  int     mCount;
};

ActionList::~ActionList()
{
  Action* a = mFirst;
  while (a) {
    Action *b = a;
    a = a->mNext;
    delete b;
  }
}

void
ActionList::Append(Action *action)
{
  if (mLast)
    mLast->mNext = action;
  else
    mFirst = action;

  mLast = action;
  mCount++;
}

int
ActionList::Prepare()
{
  // If the action list is empty then we should fail in order to signal that
  // something has gone wrong. Otherwise we report success when nothing is
  // actually done. See bug 327140.
  if (mCount == 0) {
    LOG(("empty action list\n"));
    return UNEXPECTED_ERROR;
  }

  Action *a = mFirst;
  int i = 0;
  while (a) {
    int rv = a->Prepare();
    if (rv)
      return rv;

    float percent = float(++i) / float(mCount);
    UpdateProgressUI(PROGRESS_PREPARE_SIZE * percent);

    a = a->mNext;
  }

  return OK;
}

int
ActionList::Execute()
{
  int currentProgress = 0, maxProgress = 0;
  Action *a = mFirst;
  while (a) {
    maxProgress += a->mProgressCost;
    a = a->mNext;
  }

  a = mFirst;
  while (a) {
    int rv = a->Execute();
    if (rv) {
      LOG(("### execution failed\n"));
      return rv;
    }

    currentProgress += a->mProgressCost;
    float percent = float(currentProgress) / float(maxProgress);
    UpdateProgressUI(PROGRESS_PREPARE_SIZE +
                     PROGRESS_EXECUTE_SIZE * percent);

    a = a->mNext;
  }

  return OK;
}

void
ActionList::Finish(int status)
{
  Action *a = mFirst;
  int i = 0;
  while (a) {
    a->Finish(status);

    float percent = float(++i) / float(mCount);
    UpdateProgressUI(PROGRESS_PREPARE_SIZE +
                     PROGRESS_EXECUTE_SIZE +
                     PROGRESS_FINISH_SIZE * percent);

    a = a->mNext;
  }

#ifdef XP_WIN
  if (status == OK)
    gSucceeded = true;
#endif
}

int DoUpdate()
{
  NS_tchar manifest[MAXPATHLEN];
  NS_tsnprintf(manifest, sizeof(manifest)/sizeof(manifest[0]),
               NS_T("%s/update.manifest"), gSourcePath);

  // extract the manifest
  FILE *fp = NS_tfopen(manifest, NS_T("wb"));
  if (!fp) {
    LOG(("DoUpdate: error opening manifest file: " LOG_S "\n", manifest));
    return READ_ERROR;
  }

  int rv = gArchiveReader.ExtractFileToStream("update.manifest", fp);
  fclose(fp);
  if (rv) {
    LOG(("DoUpdate: error extracting manifest file\n"));
    return rv;
  }

  AutoFile mfile = NS_tfopen(manifest, NS_T("rb"));
  if (mfile == NULL) {
    LOG(("DoUpdate: error opening manifest file: " LOG_S "\n", manifest));
    return READ_ERROR;
  }

  struct stat ms;
  rv = fstat(fileno((FILE*)mfile), &ms);
  if (rv) {
    LOG(("DoUpdate: error stating manifest file: " LOG_S "\n", manifest));
    return READ_ERROR;
  }

  char *mbuf = (char*) malloc(ms.st_size + 1);
  if (!mbuf)
    return MEM_ERROR;

  size_t r = ms.st_size;
  char *rb = mbuf;
  while (r) {
    size_t c = fread(rb, 1, mmin(SSIZE_MAX,r), mfile);
    if (c < 0) {
      LOG(("DoUpdate: error reading manifest file: " LOG_S "\n", manifest));
      return READ_ERROR;
    }

    r -= c;
    rb += c;

    if (c == 0 && r)
      return UNEXPECTED_ERROR;
  }
  mbuf[ms.st_size] = '\0';

  ActionList list;

  rb = mbuf;
  char *line;
  while((line = mstrtok(kNL, &rb)) != 0) {
    // skip comments
    if (*line == '#')
      continue;

    char *token = mstrtok(kWhitespace, &line);
    if (!token) {
      LOG(("DoUpdate: token not found in manifest\n"));
      return PARSE_ERROR;
    }

    Action *action = NULL;
    if (strcmp(token, "remove") == 0) {
      action = new RemoveFile();
    }
    else if (strcmp(token, "add") == 0) {
      action = new AddFile();
    }
    else if (strcmp(token, "patch") == 0) {
      action = new PatchFile();
    }
    else if (strcmp(token, "add-if") == 0) {
      action = new AddIfFile();
    }
    else if (strcmp(token, "patch-if") == 0) {
      action = new PatchIfFile();
    }
    else {
      LOG(("DoUpdate: unknown token: %s\n", token));
      return PARSE_ERROR;
    }

    if (!action)
      return MEM_ERROR;

    rv = action->Parse(line);
    if (rv)
      return rv;

    list.Append(action);
  }

  rv = list.Prepare();
  if (rv)
    return rv;

  rv = list.Execute();

  list.Finish(rv);
  return rv;
}
