/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 *   Brendan Eich <brendan@mozilla.org>
 *   Colin Blake <colin@theblakes.com>
 *   Javier Pedemonte <pedemont@us.ibm.com>
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

#include "nsProfileStringTypes.h"
#include "nsProfileLock.h"

#if defined(XP_MAC) || defined(XP_MACOSX)
#include <Processes.h>
#include <CFBundle.h>
#endif

#ifdef XP_UNIX
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include "prnetdb.h"
#include "prsystem.h"
#include "prprf.h"
#endif

#ifdef VMS
#include <rmsdef.h>
#endif

// **********************************************************************
// class nsProfileLock
//
// This code was moved from profile/src/nsProfileAccess.
// **********************************************************************

nsProfileLock::nsProfileLock() :
    mHaveLock(PR_FALSE)
#if defined (XP_WIN)
    ,mLockFileHandle(INVALID_HANDLE_VALUE)
#elif defined (XP_OS2)
    ,mLockFileHandle(-1)
#elif defined (XP_UNIX)
    ,mPidLockFileName(nsnull)
    ,mLockFileDesc(-1)
#endif
{
#if defined (XP_UNIX)
    next = prev = this;
#endif
}


nsProfileLock::nsProfileLock(nsProfileLock& src)
{
    *this = src;
}


nsProfileLock& nsProfileLock::operator=(nsProfileLock& rhs)
{
    Unlock();

    mHaveLock = rhs.mHaveLock;
    rhs.mHaveLock = PR_FALSE;

#if defined (XP_WIN)
    mLockFileHandle = rhs.mLockFileHandle;
    rhs.mLockFileHandle = INVALID_HANDLE_VALUE;
#elif defined (XP_OS2)
    mLockFileHandle = rhs.mLockFileHandle;
    rhs.mLockFileHandle = -1;
#elif defined (XP_UNIX)
    mLockFileDesc = rhs.mLockFileDesc;
    rhs.mLockFileDesc = -1;
    mPidLockFileName = rhs.mPidLockFileName;
    rhs.mPidLockFileName = nsnull;
    if (mPidLockFileName)
    {
        // rhs had a symlink lock, therefore it was on the list.
        PR_REMOVE_LINK(&rhs);
        PR_APPEND_LINK(this, &mPidLockList);
    }
#endif

    return *this;
}


nsProfileLock::~nsProfileLock()
{
    Unlock();
}


#if defined (XP_UNIX)

static int setupPidLockCleanup;

PRCList nsProfileLock::mPidLockList =
    PR_INIT_STATIC_CLIST(&nsProfileLock::mPidLockList);

void nsProfileLock::RemovePidLockFiles()
{
    while (!PR_CLIST_IS_EMPTY(&mPidLockList))
    {
        nsProfileLock *lock = NS_STATIC_CAST(nsProfileLock*, mPidLockList.next);
        lock->Unlock();
    }
}

static struct sigaction SIGHUP_oldact;
static struct sigaction SIGINT_oldact;
static struct sigaction SIGQUIT_oldact;
static struct sigaction SIGILL_oldact;
static struct sigaction SIGABRT_oldact;
static struct sigaction SIGSEGV_oldact;
static struct sigaction SIGTERM_oldact;

void nsProfileLock::FatalSignalHandler(int signo)
{
    // Remove any locks still held.
    RemovePidLockFiles();

    // Chain to the old handler, which may exit.
    struct sigaction *oldact = nsnull;

    switch (signo) {
      case SIGHUP:
        oldact = &SIGHUP_oldact;
        break;
      case SIGINT:
        oldact = &SIGINT_oldact;
        break;
      case SIGQUIT:
        oldact = &SIGQUIT_oldact;
        break;
      case SIGILL:
        oldact = &SIGILL_oldact;
        break;
      case SIGABRT:
        oldact = &SIGABRT_oldact;
        break;
      case SIGSEGV:
        oldact = &SIGSEGV_oldact;
        break;
      case SIGTERM:
        oldact = &SIGTERM_oldact;
        break;
      default:
        NS_NOTREACHED("bad signo");
        break;
    }

    if (oldact) {
        if (oldact->sa_handler == SIG_DFL) {
            // Make sure the default sig handler is executed
            // We need it to get Mozilla to dump core.
            sigaction(signo,oldact,NULL);

            // Now that we've restored the default handler, unmask the
            // signal and invoke it.

            sigset_t unblock_sigs;
            sigemptyset(&unblock_sigs);
            sigaddset(&unblock_sigs, signo);

            sigprocmask(SIG_UNBLOCK, &unblock_sigs, NULL);

            raise(signo);
        }
        else if (oldact->sa_handler && oldact->sa_handler != SIG_IGN)
        {
            oldact->sa_handler(signo);
        }
    }

    // Backstop exit call, just in case.
    _exit(signo);
}

nsresult nsProfileLock::LockWithFcntl(const nsACString& lockFilePath)
{
  nsresult rv = NS_OK;
  
  mLockFileDesc = open(PromiseFlatCString(lockFilePath).get(),
                        O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (mLockFileDesc != -1)
  {
      struct flock lock;
      lock.l_start = 0;
      lock.l_len = 0; // len = 0 means entire file
      lock.l_type = F_WRLCK;
      lock.l_whence = SEEK_SET;
      if (fcntl(mLockFileDesc, F_SETLK, &lock) == -1)
      {
          close(mLockFileDesc);
          mLockFileDesc = -1;

          // With OS X, on NFS, errno == ENOTSUP
          // XXX Check for that and return specific rv for it?
#ifdef DEBUG
          printf("fcntl(F_SETLK) failed. errno = %d\n", errno);
#endif
          if (errno == EAGAIN || errno == EACCES)
              rv = NS_ERROR_FILE_ACCESS_DENIED;
          else
              rv = NS_ERROR_FAILURE;                
      }
      else
          mHaveLock = PR_TRUE;
  }
  else
  {
      NS_ERROR("Failed to open lock file.");
      rv = NS_ERROR_FAILURE;
  }
  return rv;
}

nsresult nsProfileLock::LockWithSymlink(const nsACString& lockFilePath)
{
  nsresult rv;
  
  struct in_addr inaddr;
  inaddr.s_addr = INADDR_LOOPBACK;

  char hostname[256];
  PRStatus status = PR_GetSystemInfo(PR_SI_HOSTNAME, hostname, sizeof hostname);
  if (status == PR_SUCCESS)
  {
      char netdbbuf[PR_NETDB_BUF_SIZE];
      PRHostEnt hostent;
      status = PR_GetHostByName(hostname, netdbbuf, sizeof netdbbuf, &hostent);
      if (status == PR_SUCCESS)
          memcpy(&inaddr, hostent.h_addr, sizeof inaddr);
  }

  char *signature =
      PR_smprintf("%s:%lu", inet_ntoa(inaddr), (unsigned long)getpid());
  const nsPromiseFlatCString& flat = PromiseFlatCString(lockFilePath);
  const char *fileName = flat.get();
  int symlink_rv, symlink_errno, tries = 0;

  // use ns4.x-compatible symlinks if the FS supports them
  while ((symlink_rv = symlink(signature, fileName)) < 0)
  {
      symlink_errno = errno;
      if (symlink_errno != EEXIST)
          break;

      // the link exists; see if it's from this machine, and if
      // so if the process is still active
      char buf[1024];
      int len = readlink(fileName, buf, sizeof buf - 1);
      if (len > 0)
      {
          buf[len] = '\0';
          char *colon = strchr(buf, ':');
          if (colon)
          {
              *colon++ = '\0';
              unsigned long addr = inet_addr(buf);
              if (addr != (unsigned long) -1)
              {
                  char *after = nsnull;
                  pid_t pid = strtol(colon, &after, 0);
                  if (pid != 0 && *after == '\0')
                  {
                      if (addr != inaddr.s_addr)
                      {
                          // Remote lock: give up even if stuck.
                          break;
                      }

                      // kill(pid,0) is a neat trick to check if a
                      // process exists
                      if (kill(pid, 0) == 0 || errno != ESRCH)
                      {
                          // Local process appears to be alive, ass-u-me it
                          // is another Mozilla instance, or a compatible
                          // derivative, that's currently using the profile.
                          // XXX need an "are you Mozilla?" protocol
                          break;
                      }
                  }
              }
          }
      }

      // Lock seems to be bogus: try to claim it.  Give up after a large
      // number of attempts (100 comes from the 4.x codebase).
      (void) unlink(fileName);
      if (++tries > 100)
          break;
  }

  PR_smprintf_free(signature);
  signature = nsnull;

  if (symlink_rv == 0)
  {
      // We exclusively created the symlink: record its name for eventual
      // unlock-via-unlink.
      rv = NS_OK;
      mHaveLock = PR_TRUE;
      mPidLockFileName = strdup(fileName);
      if (mPidLockFileName)
      {
          PR_APPEND_LINK(this, &mPidLockList);
          if (!setupPidLockCleanup++)
          {
              // Clean up on normal termination.
              atexit(RemovePidLockFiles);

              // Clean up on abnormal termination, using POSIX sigaction.
              // Don't arm a handler if the signal is being ignored, e.g.,
              // because mozilla is run via nohup.
              struct sigaction act, oldact;
              act.sa_handler = FatalSignalHandler;
              act.sa_flags = 0;
              sigfillset(&act.sa_mask);

#define CATCH_SIGNAL(signame)                                           \
PR_BEGIN_MACRO                                                          \
if (sigaction(signame, NULL, &oldact) == 0 &&                         \
    oldact.sa_handler != SIG_IGN)                                     \
{                                                                     \
    sigaction(signame, &act, &signame##_oldact);                      \
}                                                                     \
PR_END_MACRO

              CATCH_SIGNAL(SIGHUP);
              CATCH_SIGNAL(SIGINT);
              CATCH_SIGNAL(SIGQUIT);
              CATCH_SIGNAL(SIGILL);
              CATCH_SIGNAL(SIGABRT);
              CATCH_SIGNAL(SIGSEGV);
              CATCH_SIGNAL(SIGTERM);

#undef CATCH_SIGNAL
          }
      }
  }
  else if (symlink_errno == EEXIST)
      rv = NS_ERROR_FILE_ACCESS_DENIED;
  else
  {
#ifdef DEBUG
      printf("symlink() failed. errno = %d\n", errno);
#endif
      rv = NS_ERROR_FAILURE;
  }
  return rv;
}
#endif /* XP_UNIX */

nsresult nsProfileLock::Lock(nsILocalFile* aFile)
{
#if defined (XP_MACOSX)
    NS_NAMED_LITERAL_STRING(LOCKFILE_NAME, ".parentlock");
    NS_NAMED_LITERAL_STRING(OLD_LOCKFILE_NAME, "parent.lock");
#elif defined (XP_UNIX)
    NS_NAMED_LITERAL_STRING(OLD_LOCKFILE_NAME, "lock");
    NS_NAMED_LITERAL_STRING(LOCKFILE_NAME, ".parentlock");
#else
    NS_NAMED_LITERAL_STRING(LOCKFILE_NAME, "parent.lock");
#endif

    nsresult rv;
    NS_ENSURE_STATE(!mHaveLock);

    PRBool isDir;
    rv = aFile->IsDirectory(&isDir);
    if (NS_FAILED(rv))
        return rv;
    if (!isDir)
        return NS_ERROR_FILE_NOT_DIRECTORY;

    nsCOMPtr<nsILocalFile> lockFile;
    rv = aFile->Clone((nsIFile **)((void **)getter_AddRefs(lockFile)));
    if (NS_FAILED(rv))
        return rv;

    rv = lockFile->Append(LOCKFILE_NAME);
    if (NS_FAILED(rv))
        return rv;

#if defined(XP_MACOSX)
    // First, try locking using fcntl. It is more reliable on
    // a local machine, but may not be supported by an NFS server.
    nsCAutoString filePath;
    rv = lockFile->GetNativePath(filePath);
    if (NS_FAILED(rv))
        return rv;    
    rv = LockWithFcntl(filePath);
    if (NS_FAILED(rv) && (rv != NS_ERROR_FILE_ACCESS_DENIED))
    {
        // If that failed for any reason other than NS_ERROR_FILE_ACCESS_DENIED,
        // assume we tried an NFS that does not support it. Now, try with symlink.
        rv = LockWithSymlink(filePath);
    }
    if (NS_SUCCEEDED(rv))
    {        
        // Check for the old-style lock used by pre-mozilla 1.3 builds.
        // Those builds used an earlier check to prevent the application
        // from launching if another instance was already running. Because
        // of that, we don't need to create an old-style lock as well.
        struct LockProcessInfo
        {
            ProcessSerialNumber psn;
            unsigned long launchDate;
        };
    
        PRFileDesc *fd = nsnull;
        PRInt32 ioBytes;
        ProcessInfoRec processInfo;
        LockProcessInfo lockProcessInfo;
    
        rv = lockFile->SetLeafName(OLD_LOCKFILE_NAME);
        if (NS_FAILED(rv))
            return rv;
        rv = lockFile->OpenNSPRFileDesc(PR_RDONLY, 0, &fd);
        if (NS_SUCCEEDED(rv))
        {
            ioBytes = PR_Read(fd, &lockProcessInfo, sizeof(LockProcessInfo));
            PR_Close(fd);
    
            if (ioBytes == sizeof(LockProcessInfo))
            {
                processInfo.processAppSpec = nsnull;
                processInfo.processName = nsnull;
                processInfo.processInfoLength = sizeof(ProcessInfoRec);
                if (::GetProcessInformation(&lockProcessInfo.psn, &processInfo) == noErr &&
                    processInfo.processLaunchDate == lockProcessInfo.launchDate)
                {
                    return NS_ERROR_FILE_ACCESS_DENIED;
                }
            }
            else
            {
                NS_WARNING("Could not read lock file - ignoring lock");
            }
        }
        rv = NS_OK; // Don't propagate error from OpenNSPRFileDesc.
    }
#elif defined(XP_WIN)
    nsCAutoString filePath;
    rv = lockFile->GetNativePath(filePath);
    if (NS_FAILED(rv))
        return rv;
    mLockFileHandle = CreateFile(filePath.get(),
                                 GENERIC_READ | GENERIC_WRITE,
                                 0, // no sharing - of course
                                 nsnull,
                                 OPEN_ALWAYS,
                                 FILE_FLAG_DELETE_ON_CLOSE,
                                 nsnull);
    if (mLockFileHandle == INVALID_HANDLE_VALUE)
        return NS_ERROR_FILE_ACCESS_DENIED;
#elif defined(XP_OS2)
    nsCAutoString filePath;
    rv = lockFile->GetNativePath(filePath);
    if (NS_FAILED(rv))
        return rv;

    ULONG   ulAction = 0;
    APIRET  rc;
    rc = DosOpen(filePath.get(),
                  &mLockFileHandle,
                  &ulAction,
                  0,
                  FILE_NORMAL,
                  OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                  OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYREADWRITE | OPEN_FLAGS_NOINHERIT,
                  0 );
    if (rc != NO_ERROR)
    {
        mLockFileHandle = -1;
        return NS_ERROR_FILE_ACCESS_DENIED;
    }
#elif defined(VMS)
    nsCAutoString filePath;
    rv = lockFile->GetNativePath(filePath);
    if (NS_FAILED(rv))
        return rv;

    mLockFileDesc = open_noshr(filePath.get(), O_CREAT, 0666);
    if (mLockFileDesc == -1)
    {
	if ((errno == EVMSERR) && (vaxc$errno == RMS$_FLK))
	{
	    return NS_ERROR_FILE_ACCESS_DENIED;
	}
	else
	{
	    NS_ERROR("Failed to open lock file.");
	    return NS_ERROR_FAILURE;
	}
    }
#elif defined(XP_UNIX)
    nsCOMPtr<nsIFile> oldLockFile;
    rv = aFile->Clone(getter_AddRefs(oldLockFile));
    if (NS_SUCCEEDED(rv))
    {
        rv = oldLockFile->Append(OLD_LOCKFILE_NAME);
        if (NS_SUCCEEDED(rv))
        {
            nsCAutoString filePath;
            rv = oldLockFile->GetNativePath(filePath);
            if (NS_SUCCEEDED(rv))
            {
                // First, try the 4.x-compatible symlink technique, which works
                // with NFS without depending on (broken or missing, too often)
                // lockd.
                rv = LockWithSymlink(filePath);
                if (NS_FAILED(rv) && rv != NS_ERROR_FILE_ACCESS_DENIED)
                {
                    // If that failed with an error other than
                    // NS_ERROR_FILE_ACCESS_DENIED, symlinks aren't
                    // supported (for example, on Win32 SAMBA servers).
                    // Try locking with fcntl.
                    rv = lockFile->GetNativePath(filePath);
                    if (NS_SUCCEEDED(rv))
                        rv = LockWithFcntl(filePath);
                }
            }
        }
    }
    if (NS_FAILED(rv))
        return rv;
#endif /* XP_UNIX */

    mHaveLock = PR_TRUE;

    return rv;
}


nsresult nsProfileLock::Unlock()
{
    nsresult rv = NS_OK;

    if (mHaveLock)
    {
#if defined (XP_WIN)
        if (mLockFileHandle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(mLockFileHandle);
            mLockFileHandle = INVALID_HANDLE_VALUE;
        }
#elif defined (XP_OS2)
        if (mLockFileHandle != -1)
        {
            DosClose(mLockFileHandle);
            mLockFileHandle = -1;
        }
#elif defined (XP_UNIX)
        if (mPidLockFileName)
        {
            PR_REMOVE_LINK(this);
            (void) unlink(mPidLockFileName);
            free(mPidLockFileName);
            mPidLockFileName = nsnull;
        }
        else if (mLockFileDesc != -1)
        {
            close(mLockFileDesc);
            mLockFileDesc = -1;
            // Don't remove it
        }
#endif

        mHaveLock = PR_FALSE;
    }

    return rv;
}
