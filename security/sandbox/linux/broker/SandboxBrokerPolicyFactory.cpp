/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxBrokerPolicyFactory.h"
#include "SandboxInfo.h"
#include "SandboxLogging.h"

#include "mozilla/Array.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/SandboxSettings.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/SandboxLaunch.h"
#include "mozilla/dom/ContentChild.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "SpecialSystemDirectory.h"
#include "nsReadableUtils.h"
#include "nsIFileStreams.h"
#include "nsILineInputStream.h"
#include "nsNetCID.h"

#ifdef ANDROID
#include "cutils/properties.h"
#endif

#ifdef MOZ_WIDGET_GTK
#include <glib.h>
#endif

#include <dirent.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#ifndef ANDROID
#include <glob.h>
#endif

namespace mozilla {

#if defined(MOZ_CONTENT_SANDBOX)
namespace {
static const int rdonly = SandboxBroker::MAY_READ;
static const int wronly = SandboxBroker::MAY_WRITE;
static const int rdwr = rdonly | wronly;
static const int rdwrcr = rdwr | SandboxBroker::MAY_CREATE;
static const int access = SandboxBroker::MAY_ACCESS;
}
#endif

static void
AddMesaSysfsPaths(SandboxBroker::Policy* aPolicy)
{
  // Bug 1384178: Mesa driver loader
  aPolicy->AddPrefix(rdonly, "/sys/dev/char/226:");

  // Bug 1401666: Mesa driver loader part 2: Mesa <= 12 using libudev
  if (auto dir = opendir("/dev/dri")) {
    while (auto entry = readdir(dir)) {
      if (entry->d_name[0] != '.') {
        nsPrintfCString devPath("/dev/dri/%s", entry->d_name);
        struct stat sb;
        if (stat(devPath.get(), &sb) == 0 && S_ISCHR(sb.st_mode)) {
          // For both the DRI node and its parent (the physical
          // device), allow reading the "uevent" file.
          static const Array<const char*, 2> kSuffixes = { "", "/device" };
          for (const auto suffix : kSuffixes) {
            nsPrintfCString sysPath("/sys/dev/char/%u:%u%s",
                                    major(sb.st_rdev),
                                    minor(sb.st_rdev),
                                    suffix);
            // libudev will expand the symlink but not do full
            // canonicalization, so it will leave in ".." path
            // components that will be realpath()ed in the
            // broker.  To match this, allow the canonical paths.
            UniqueFreePtr<char[]> realSysPath(realpath(sysPath.get(), nullptr));
            if (realSysPath) {
              nsPrintfCString ueventPath("%s/uevent", realSysPath.get());
              nsPrintfCString configPath("%s/config", realSysPath.get());
              aPolicy->AddPath(rdonly, ueventPath.get());
              aPolicy->AddPath(rdonly, configPath.get());
            }
          }
        }
      }
    }
    closedir(dir);
  }
}

static void
AddPathsFromFile(SandboxBroker::Policy* aPolicy, nsACString& aPath)
{
  nsresult rv;
  nsCOMPtr<nsIFile> ldconfig(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    return;
  }
  rv = ldconfig->InitWithNativePath(aPath);
  if (NS_FAILED(rv)) {
    return;
  }
  nsCOMPtr<nsIFileInputStream> fileStream(
    do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    return;
  }
  rv = fileStream->Init(ldconfig, -1, -1, 0);
  if (NS_FAILED(rv)) {
    return;
  }
  nsCOMPtr<nsILineInputStream> lineStream(do_QueryInterface(fileStream, &rv));
  if (NS_FAILED(rv)) {
    return;
  }
  nsAutoCString line;
  bool more = true;
  do {
    rv = lineStream->ReadLine(line, &more);
    if (NS_FAILED(rv)) {
      break;
    }
    // Cut off any comments at the end of the line, also catches lines
    // that are entirely a comment
    int32_t hash = line.FindChar('#');
    if (hash >= 0) {
      line = Substring(line, 0, hash);
    }
    // Simplify our following parsing by trimming whitespace
    line.CompressWhitespace(true, true);
    if (line.IsEmpty()) {
      // Skip comment lines
      continue;
    }
    // Check for any included files and recursively process
    nsACString::const_iterator start, end, token_end;

    line.BeginReading(start);
    line.EndReading(end);
    token_end = end;

    if (FindInReadable(NS_LITERAL_CSTRING("include "), start, token_end)) {
      nsAutoCString includes(Substring(token_end, end));
      for (const nsACString& includeGlob : includes.Split(' ')) {
        glob_t globbuf;
        if (!glob(PromiseFlatCString(includeGlob).get(), GLOB_NOSORT, nullptr, &globbuf)) {
          for (size_t fileIdx = 0; fileIdx < globbuf.gl_pathc; fileIdx++) {
            nsAutoCString filePath(globbuf.gl_pathv[fileIdx]);
            AddPathsFromFile(aPolicy, filePath);
          }
          globfree(&globbuf);
        }
      }
    }
    // Skip anything left over that isn't an absolute path
    if (line.First() != '/') {
      continue;
    }
    // Cut off anything behind an = sign, used by dirname=TYPE directives
    int32_t equals = line.FindChar('=');
    if (equals >= 0) {
      line = Substring(line, 0, equals);
    }
    char* resolvedPath = realpath(line.get(), nullptr);
    if (resolvedPath) {
      aPolicy->AddDir(rdonly, resolvedPath);
      free(resolvedPath);
    }
  } while (more);
}

static void
AddLdconfigPaths(SandboxBroker::Policy* aPolicy)
{
  nsAutoCString ldconfigPath(NS_LITERAL_CSTRING("/etc/ld.so.conf"));
  AddPathsFromFile(aPolicy, ldconfigPath);
}

SandboxBrokerPolicyFactory::SandboxBrokerPolicyFactory()
{
  // Policy entries that are the same in every process go here, and
  // are cached over the lifetime of the factory.
#if defined(MOZ_CONTENT_SANDBOX)
  SandboxBroker::Policy* policy = new SandboxBroker::Policy;
  policy->AddDir(rdwrcr, "/dev/shm");
  // Write permssions
  //
  // Bug 1308851: NVIDIA proprietary driver when using WebGL
  policy->AddFilePrefix(rdwr, "/dev", "nvidia");

  // Bug 1312678: radeonsi/Intel with DRI when using WebGL
  policy->AddDir(rdwr, "/dev/dri");

  // Read permissions
  policy->AddPath(rdonly, "/dev/urandom");
  policy->AddPath(rdonly, "/proc/cpuinfo");
  policy->AddPath(rdonly, "/proc/meminfo");
  policy->AddDir(rdonly, "/sys/devices/cpu");
  policy->AddDir(rdonly, "/sys/devices/system/cpu");
  policy->AddDir(rdonly, "/lib");
  policy->AddDir(rdonly, "/lib64");
  policy->AddDir(rdonly, "/usr/lib");
  policy->AddDir(rdonly, "/usr/lib32");
  policy->AddDir(rdonly, "/usr/lib64");
  policy->AddDir(rdonly, "/etc");
  policy->AddDir(rdonly, "/usr/share");
  policy->AddDir(rdonly, "/usr/local/share");
  // Various places where fonts reside
  policy->AddDir(rdonly, "/usr/X11R6/lib/X11/fonts");
  policy->AddDir(rdonly, "/nix/store");
  policy->AddDir(rdonly, "/run/host/fonts");
  policy->AddDir(rdonly, "/run/host/user-fonts");

  AddMesaSysfsPaths(policy);
  AddLdconfigPaths(policy);

  // Bug 1385715: NVIDIA PRIME support
  policy->AddPath(rdonly, "/proc/modules");

  // Allow access to XDG_CONFIG_PATH and XDG_CONFIG_DIRS
  if (const auto xdgConfigPath = PR_GetEnv("XDG_CONFIG_PATH")) {
    policy->AddDir(rdonly, xdgConfigPath);
  }

  nsAutoCString xdgConfigDirs(PR_GetEnv("XDG_CONFIG_DIRS"));
  for (const auto& path : xdgConfigDirs.Split(':')) {
    policy->AddDir(rdonly, PromiseFlatCString(path).get());
  }

  // Allow fonts subdir in XDG_DATA_HOME
  nsAutoCString xdgDataHome(PR_GetEnv("XDG_DATA_HOME"));
  if (!xdgDataHome.IsEmpty()) {
    nsAutoCString fontPath(xdgDataHome);
    fontPath.Append("/fonts");
    policy->AddDir(rdonly, PromiseFlatCString(fontPath).get());
  }

  // Any font subdirs in XDG_DATA_DIRS
  nsAutoCString xdgDataDirs(PR_GetEnv("XDG_DATA_DIRS"));
  for (const auto& path : xdgDataDirs.Split(':')) {
    nsAutoCString fontPath(path);
    fontPath.Append("/fonts");
    policy->AddDir(rdonly, PromiseFlatCString(fontPath).get());
  }

  // Extra configuration dirs in the homedir that we want to allow read
  // access to.
  mozilla::Array<const char*, 3> extraConfDirs = {
    ".config",   // Fallback if XDG_CONFIG_PATH isn't set
    ".themes",
    ".fonts",
  };

  nsCOMPtr<nsIFile> homeDir;
  nsresult rv = GetSpecialSystemDirectory(Unix_HomeDirectory,
                                          getter_AddRefs(homeDir));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIFile> confDir;

    for (const auto& dir : extraConfDirs) {
      rv = homeDir->Clone(getter_AddRefs(confDir));
      if (NS_SUCCEEDED(rv)) {
        rv = confDir->AppendNative(nsDependentCString(dir));
        if (NS_SUCCEEDED(rv)) {
          nsAutoCString tmpPath;
          rv = confDir->GetNativePath(tmpPath);
          if (NS_SUCCEEDED(rv)) {
            policy->AddDir(rdonly, tmpPath.get());
          }
        }
      }
    }

    // ~/.local/share (for themes)
    rv = homeDir->Clone(getter_AddRefs(confDir));
    if (NS_SUCCEEDED(rv)) {
      rv = confDir->AppendNative(NS_LITERAL_CSTRING(".local"));
      if (NS_SUCCEEDED(rv)) {
        rv = confDir->AppendNative(NS_LITERAL_CSTRING("share"));
      }
      if (NS_SUCCEEDED(rv)) {
        nsAutoCString tmpPath;
        rv = confDir->GetNativePath(tmpPath);
        if (NS_SUCCEEDED(rv)) {
          policy->AddDir(rdonly, tmpPath.get());
        }
      }
    }

    // ~/.fonts.conf (Fontconfig)
    rv = homeDir->Clone(getter_AddRefs(confDir));
    if (NS_SUCCEEDED(rv)) {
      rv = confDir->AppendNative(NS_LITERAL_CSTRING(".fonts.conf"));
      if (NS_SUCCEEDED(rv)) {
        nsAutoCString tmpPath;
        rv = confDir->GetNativePath(tmpPath);
        if (NS_SUCCEEDED(rv)) {
          policy->AddPath(rdonly, tmpPath.get());
        }
      }
    }

    // .pangorc
    rv = homeDir->Clone(getter_AddRefs(confDir));
    if (NS_SUCCEEDED(rv)) {
      rv = confDir->AppendNative(NS_LITERAL_CSTRING(".pangorc"));
      if (NS_SUCCEEDED(rv)) {
        nsAutoCString tmpPath;
        rv = confDir->GetNativePath(tmpPath);
        if (NS_SUCCEEDED(rv)) {
          policy->AddPath(rdonly, tmpPath.get());
        }
      }
    }
  }

  // Firefox binary dir.
  // Note that unlike the previous cases, we use NS_GetSpecialDirectory
  // instead of GetSpecialSystemDirectory. The former requires a working XPCOM
  // system, which may not be the case for some tests. For querying for the
  // location of XPCOM things, we can use it anyway.
  nsCOMPtr<nsIFile> ffDir;
  rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(ffDir));
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString tmpPath;
    rv = ffDir->GetNativePath(tmpPath);
    if (NS_SUCCEEDED(rv)) {
      policy->AddDir(rdonly, tmpPath.get());
    }
  }

  // ~/.mozilla/systemextensionsdev (bug 1393805)
  nsCOMPtr<nsIFile> sysExtDevDir;
  rv = NS_GetSpecialDirectory(XRE_USER_SYS_EXTENSION_DEV_DIR,
                              getter_AddRefs(sysExtDevDir));
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString tmpPath;
    rv = sysExtDevDir->GetNativePath(tmpPath);
    if (NS_SUCCEEDED(rv)) {
      policy->AddDir(rdonly, tmpPath.get());
    }
  }

  if (mozilla::IsDevelopmentBuild()) {
    // If this is a developer build the resources are symlinks to outside the binary dir.
    // Therefore in non-release builds we allow reads from the whole repository.
    // MOZ_DEVELOPER_REPO_DIR is set by mach run.
    const char *developer_repo_dir = PR_GetEnv("MOZ_DEVELOPER_REPO_DIR");
    if (developer_repo_dir) {
      policy->AddDir(rdonly, developer_repo_dir);
    }
  }

#ifdef DEBUG
  char *bloatLog = PR_GetEnv("XPCOM_MEM_BLOAT_LOG");
  // XPCOM_MEM_BLOAT_LOG has the format
  // /tmp/tmpd0YzFZ.mozrunner/runtests_leaks.log
  // but stores into /tmp/tmpd0YzFZ.mozrunner/runtests_leaks_tab_pid3411.log
  // So cut the .log part and whitelist the prefix.
  if (bloatLog != nullptr) {
    size_t bloatLen = strlen(bloatLog);
    if (bloatLen >= 4) {
      nsAutoCString bloatStr(bloatLog);
      bloatStr.Truncate(bloatLen - 4);
      policy->AddPrefix(rdwrcr, bloatStr.get());
    }
  }
#endif

  // Allow Primus to contact the Bumblebee daemon to manage GPU
  // switching on NVIDIA Optimus systems.
  const char* bumblebeeSocket = PR_GetEnv("BUMBLEBEE_SOCKET");
  if (bumblebeeSocket == nullptr) {
    bumblebeeSocket = "/var/run/bumblebee.socket";
  }
  policy->AddPath(SandboxBroker::MAY_CONNECT, bumblebeeSocket);

  // Allow local X11 connections, for Primus and VirtualGL to contact
  // the secondary X server.
  policy->AddPrefix(SandboxBroker::MAY_CONNECT, "/tmp/.X11-unix/X");
  if (const auto xauth = PR_GetEnv("XAUTHORITY")) {
    policy->AddPath(rdonly, xauth);
  }

  mCommonContentPolicy.reset(policy);
#endif
}

#ifdef MOZ_CONTENT_SANDBOX
UniquePtr<SandboxBroker::Policy>
SandboxBrokerPolicyFactory::GetContentPolicy(int aPid, bool aFileProcess)
{
  // Policy entries that vary per-process (currently the only reason
  // that can happen is because they contain the pid) are added here,
  // as well as entries that depend on preferences or paths not available
  // in early startup.

  MOZ_ASSERT(NS_IsMainThread());
  // File broker usage is controlled through a pref.
  if (!IsContentSandboxEnabled()) {
    return nullptr;
  }

  MOZ_ASSERT(mCommonContentPolicy);
  UniquePtr<SandboxBroker::Policy>
    policy(new SandboxBroker::Policy(*mCommonContentPolicy));

  const int level = GetEffectiveContentSandboxLevel();

  // Read any extra paths that will get write permissions,
  // configured by the user or distro
  AddDynamicPathList(policy.get(),
                     "security.sandbox.content.write_path_whitelist",
                     rdwr);

  // Whitelisted for reading by the user/distro
  AddDynamicPathList(policy.get(),
                    "security.sandbox.content.read_path_whitelist",
                    rdonly);

  // No read blocking at level 2 and below.
  // file:// processes also get global read permissions
  // This requires accessing user preferences so we can only do it now.
  // Our constructor is initialized before user preferences are read in.
  if (level <= 2 || aFileProcess) {
    policy->AddDir(rdonly, "/");
    // Any other read-only rules will be removed as redundant by
    // Policy::FixRecursivePermissions, so there's no need to
    // early-return here.
  }

  // Bug 1198550: the profiler's replacement for dl_iterate_phdr
  policy->AddPath(rdonly, nsPrintfCString("/proc/%d/maps", aPid).get());

  // Bug 1198552: memory reporting.
  policy->AddPath(rdonly, nsPrintfCString("/proc/%d/statm", aPid).get());
  policy->AddPath(rdonly, nsPrintfCString("/proc/%d/smaps", aPid).get());

  // Bug 1384804, notably comment 15
  // Used by libnuma, included by x265/ffmpeg, who falls back
  // to get_mempolicy if this fails
  policy->AddPath(rdonly, nsPrintfCString("/proc/%d/status", aPid).get());

  // Add write permissions on the content process specific temporary dir.
  nsCOMPtr<nsIFile> tmpDir;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_CONTENT_PROCESS_TEMP_DIR,
                                       getter_AddRefs(tmpDir));
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString tmpPath;
    rv = tmpDir->GetNativePath(tmpPath);
    if (NS_SUCCEEDED(rv)) {
      policy->AddDir(rdwrcr, tmpPath.get());
    }
  }

  // userContent.css and the extensions dir sit in the profile, which is
  // normally blocked and we can't get the profile dir earlier in startup,
  // so this must happen here.
  nsCOMPtr<nsIFile> profileDir;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(profileDir));
  if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIFile> workDir;
      rv = profileDir->Clone(getter_AddRefs(workDir));
      if (NS_SUCCEEDED(rv)) {
        rv = workDir->AppendNative(NS_LITERAL_CSTRING("chrome"));
        if (NS_SUCCEEDED(rv)) {
          nsAutoCString tmpPath;
          rv = workDir->GetNativePath(tmpPath);
          if (NS_SUCCEEDED(rv)) {
            policy->AddDir(rdonly, tmpPath.get());
          }
        }
      }
      rv = profileDir->Clone(getter_AddRefs(workDir));
      if (NS_SUCCEEDED(rv)) {
        rv = workDir->AppendNative(NS_LITERAL_CSTRING("extensions"));
        if (NS_SUCCEEDED(rv)) {
          nsAutoCString tmpPath;
          rv = workDir->GetNativePath(tmpPath);
          if (NS_SUCCEEDED(rv)) {
            policy->AddDir(rdonly, tmpPath.get());
          }
        }
      }
  }

  bool allowPulse = false;
  bool allowAlsa = false;
  if (level < 4) {
#ifdef MOZ_PULSEAUDIO
    allowPulse = true;
#endif
#ifdef MOZ_ALSA
    allowAlsa = true;
#endif
  }

  if (allowAlsa) {
    // Bug 1309098: ALSA support
    policy->AddDir(rdwr, "/dev/snd");
  }

#ifdef MOZ_WIDGET_GTK
  if (const auto userDir = g_get_user_runtime_dir()) {
    // Bug 1321134: DConf's single bit of shared memory
    // The leaf filename is "user" by default, but is configurable.
    nsPrintfCString shmPath("%s/dconf/", userDir);
    policy->AddPrefix(rdwrcr, shmPath.get());
    policy->AddAncestors(shmPath.get());
    if (allowPulse) {
      // PulseAudio, if it can't get server info from X11, will break
      // unless it can open this directory (or create it, but in our use
      // case we know it already exists).  See bug 1335329.
      nsPrintfCString pulsePath("%s/pulse", userDir);
      policy->AddPath(rdonly, pulsePath.get());
    }
  }
#endif // MOZ_WIDGET_GTK

  if (allowPulse) {
    // PulseAudio also needs access to read the $XAUTHORITY file (see
    // bug 1384986 comment #1), but that's already allowed for hybrid
    // GPU drivers (see above).
    policy->AddPath(rdonly, "/var/lib/dbus/machine-id");
  }

  // Bug 1434711 - AMDGPU-PRO crashes if it can't read it's marketing ids
  // and various other things
  if (HasAtiDrivers()) {
    policy->AddDir(rdonly, "/opt/amdgpu/share");
    policy->AddPath(rdonly, "/sys/module/amdgpu");
    // AMDGPU-PRO's MESA version likes to readlink a lot of things here
    policy->AddDir(access, "/sys");
  }

  // Return the common policy.
  policy->FixRecursivePermissions();
  return policy;
}

void
SandboxBrokerPolicyFactory::AddDynamicPathList(SandboxBroker::Policy *policy,
                                               const char* aPathListPref,
                                               int perms)
{
  nsAutoCString pathList;
  nsresult rv = Preferences::GetCString(aPathListPref, pathList);
  if (NS_SUCCEEDED(rv)) {
    for (const nsACString& path : pathList.Split(',')) {
      nsCString trimPath(path);
      trimPath.Trim(" ", true, true);
      policy->AddDynamic(perms, trimPath.get());
    }
  }
}

#endif // MOZ_CONTENT_SANDBOX
} // namespace mozilla
