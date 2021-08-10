/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxBrokerPolicyFactory.h"
#include "SandboxInfo.h"
#include "SandboxLogging.h"

#include "base/shared_memory.h"
#include "mozilla/Array.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/SandboxLaunch.h"
#include "mozilla/SandboxSettings.h"
#include "mozilla/StaticPrefs_security.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/dom/ContentChild.h"
#include "nsComponentManagerUtils.h"
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
#include "nsIFile.h"

#include "nsNetCID.h"

#ifdef ANDROID
#  include "cutils/properties.h"
#endif

#ifdef MOZ_WIDGET_GTK
#  include "mozilla/WidgetUtilsGtk.h"
#  include <glib.h>
#endif

#include <dirent.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#ifndef ANDROID
#  include <glob.h>
#endif

namespace mozilla {

namespace {
static const int rdonly = SandboxBroker::MAY_READ;
static const int wronly = SandboxBroker::MAY_WRITE;
static const int rdwr = rdonly | wronly;
static const int rdwrcr = rdwr | SandboxBroker::MAY_CREATE;
static const int access = SandboxBroker::MAY_ACCESS;
static const int deny = SandboxBroker::FORCE_DENY;
}  // namespace

static void AddMesaSysfsPaths(SandboxBroker::Policy* aPolicy) {
  // Bug 1384178: Mesa driver loader
  aPolicy->AddPrefix(rdonly, "/sys/dev/char/226:");

  // Bug 1480755: Mesa tries to probe /sys paths in turn
  aPolicy->AddAncestors("/sys/dev/char/");

  // Bug 1401666: Mesa driver loader part 2: Mesa <= 12 using libudev
  if (auto dir = opendir("/dev/dri")) {
    while (auto entry = readdir(dir)) {
      if (entry->d_name[0] != '.') {
        nsPrintfCString devPath("/dev/dri/%s", entry->d_name);
        struct stat sb;
        if (stat(devPath.get(), &sb) == 0 && S_ISCHR(sb.st_mode)) {
          // For both the DRI node and its parent (the physical
          // device), allow reading the "uevent" file.
          static const Array<const char*, 2> kSuffixes = {"", "/device"};
          for (const auto suffix : kSuffixes) {
            nsPrintfCString sysPath("/sys/dev/char/%u:%u%s", major(sb.st_rdev),
                                    minor(sb.st_rdev), suffix);
            // libudev will expand the symlink but not do full
            // canonicalization, so it will leave in ".." path
            // components that will be realpath()ed in the
            // broker.  To match this, allow the canonical paths.
            UniqueFreePtr<char[]> realSysPath(realpath(sysPath.get(), nullptr));
            if (realSysPath) {
              constexpr const char* kMesaAttrSuffixes[] = {
                  "config",    "device",           "revision",
                  "subsystem", "subsystem_device", "subsystem_vendor",
                  "uevent",    "vendor",
              };
              for (const auto& attrSuffix : kMesaAttrSuffixes) {
                nsPrintfCString attrPath("%s/%s", realSysPath.get(),
                                         attrSuffix);
                aPolicy->AddPath(rdonly, attrPath.get());
              }
              // Allowing stat-ing the parent dirs
              nsPrintfCString basePath("%s/", realSysPath.get());
              aPolicy->AddAncestors(basePath.get());
            }
          }
        }
      }
    }
    closedir(dir);
  }
}

static void JoinPathIfRelative(const nsACString& aCwd, const nsACString& inPath,
                               nsACString& outPath) {
  if (inPath.Length() < 1) {
    outPath.Assign(aCwd);
    SANDBOX_LOG_ERROR("Unjoinable path: %s", PromiseFlatCString(aCwd).get());
    return;
  }
  const char* startChar = inPath.BeginReading();
  if (*startChar != '/') {
    // Relative path, copy basepath in front
    outPath.Assign(aCwd);
    outPath.Append("/");
    outPath.Append(inPath);
  } else {
    // Absolute path, it's ok like this
    outPath.Assign(inPath);
  }
}

static void AddPathsFromFile(SandboxBroker::Policy* aPolicy,
                             const nsACString& aPath);

static void AddPathsFromFileInternal(SandboxBroker::Policy* aPolicy,
                                     const nsACString& aCwd,
                                     const nsACString& aPath) {
  nsresult rv;
  nsCOMPtr<nsIFile> ldconfig(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    return;
  }
  rv = ldconfig->InitWithNativePath(aPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  nsCOMPtr<nsIFileInputStream> fileStream(
      do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  rv = fileStream->Init(ldconfig, -1, -1, 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  nsCOMPtr<nsILineInputStream> lineStream(do_QueryInterface(fileStream, &rv));
  if (NS_WARN_IF(NS_FAILED(rv))) {
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

    if (FindInReadable("include "_ns, start, token_end)) {
      nsAutoCString includes(Substring(token_end, end));
      for (const nsACString& includeGlob : includes.Split(' ')) {
        // Glob path might be relative, so add cwd if so.
        nsAutoCString includeFile;
        JoinPathIfRelative(aCwd, includeGlob, includeFile);
        glob_t globbuf;
        if (!glob(PromiseFlatCString(includeFile).get(), GLOB_NOSORT, nullptr,
                  &globbuf)) {
          for (size_t fileIdx = 0; fileIdx < globbuf.gl_pathc; fileIdx++) {
            nsAutoCString filePath(globbuf.gl_pathv[fileIdx]);
            AddPathsFromFile(aPolicy, filePath);
          }
          globfree(&globbuf);
        }
      }
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

static void AddPathsFromFile(SandboxBroker::Policy* aPolicy,
                             const nsACString& aPath) {
  // Find the new base path where that file sits in.
  nsresult rv;
  nsCOMPtr<nsIFile> includeFile(
      do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    return;
  }
  rv = includeFile->InitWithNativePath(aPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
    SANDBOX_LOG_ERROR("Adding paths from %s to policy.",
                      PromiseFlatCString(aPath).get());
  }

  // Find the parent dir where this file sits in.
  nsCOMPtr<nsIFile> parentDir;
  rv = includeFile->GetParent(getter_AddRefs(parentDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  nsAutoCString parentPath;
  rv = parentDir->GetNativePath(parentPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
    SANDBOX_LOG_ERROR("Parent path is %s",
                      PromiseFlatCString(parentPath).get());
  }
  AddPathsFromFileInternal(aPolicy, parentPath, aPath);
}

static void AddLdconfigPaths(SandboxBroker::Policy* aPolicy) {
  nsAutoCString ldConfig("/etc/ld.so.conf"_ns);
  AddPathsFromFile(aPolicy, ldConfig);
}

static void AddLdLibraryEnvPaths(SandboxBroker::Policy* aPolicy) {
  nsAutoCString LdLibraryEnv(PR_GetEnv("LD_LIBRARY_PATH"));
  // The items in LD_LIBRARY_PATH can be separated by either colons or
  // semicolons, according to the ld.so(8) man page, and empirically it
  // seems to be allowed to mix them (i.e., a:b;c is a list with 3 elements).
  // There is no support for escaping the delimiters, fortunately (for us).
  LdLibraryEnv.ReplaceChar(';', ':');
  for (const nsACString& libPath : LdLibraryEnv.Split(':')) {
    char* resolvedPath = realpath(PromiseFlatCString(libPath).get(), nullptr);
    if (resolvedPath) {
      aPolicy->AddDir(rdonly, resolvedPath);
      free(resolvedPath);
    }
  }
}

static void AddSharedMemoryPaths(SandboxBroker::Policy* aPolicy, pid_t aPid) {
  std::string shmPath("/dev/shm");
  if (base::SharedMemory::AppendPosixShmPrefix(&shmPath, aPid)) {
    aPolicy->AddPrefix(rdwrcr, shmPath.c_str());
  }
}

static void AddMemoryReporting(SandboxBroker::Policy* aPolicy, pid_t aPid) {
  // Bug 1198552: memory reporting.
  // Bug 1647957: memory reporting.
  aPolicy->AddPath(rdonly, nsPrintfCString("/proc/%d/statm", aPid).get());
  aPolicy->AddPath(rdonly, nsPrintfCString("/proc/%d/smaps", aPid).get());
}

static void AddDynamicPathList(SandboxBroker::Policy* policy,
                               const char* aPathListPref, int perms) {
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

void SandboxBrokerPolicyFactory::InitContentPolicy() {
  const bool headless =
      StaticPrefs::security_sandbox_content_headless_AtStartup();

  // Policy entries that are the same in every process go here, and
  // are cached over the lifetime of the factory.
  SandboxBroker::Policy* policy = new SandboxBroker::Policy;
  // Write permssions
  //
  if (!headless) {
    // Bug 1308851: NVIDIA proprietary driver when using WebGL
    policy->AddFilePrefix(rdwr, "/dev", "nvidia");

    // Bug 1312678: Mesa with DRI when using WebGL
    policy->AddDir(rdwr, "/dev/dri");
  }

  // Bug 1575985: WASM library sandbox needs RW access to /dev/null
  policy->AddPath(rdwr, "/dev/null");

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
  policy->AddDir(rdonly, "/var/cache/fontconfig");

  if (!headless) {
    AddMesaSysfsPaths(policy);
  }
  AddLdconfigPaths(policy);
  AddLdLibraryEnvPaths(policy);

  if (!headless) {
    // Bug 1385715: NVIDIA PRIME support
    policy->AddPath(rdonly, "/proc/modules");
  }

  // XDG directories might be non existent according to specs:
  // https://specifications.freedesktop.org/basedir-spec/0.8/ar01s04.html
  //
  // > If, when attempting to write a file, the destination directory is
  // > non-existent an attempt should be made to create it with permission 0700.
  //
  // For that we use AddPath(, SandboxBroker::Policy::AddCondition::AddAlways).
  //
  // Allow access to XDG_CONFIG_HOME and XDG_CONFIG_DIRS
  nsAutoCString xdgConfigHome(PR_GetEnv("XDG_CONFIG_HOME"));
  if (!xdgConfigHome.IsEmpty()) {  // AddPath will fail on empty strings
    policy->AddFutureDir(rdonly, xdgConfigHome.get());
  }

  nsAutoCString xdgConfigDirs(PR_GetEnv("XDG_CONFIG_DIRS"));
  for (const auto& path : xdgConfigDirs.Split(':')) {
    if (!path.IsEmpty()) {  // AddPath will fail on empty strings
      policy->AddFutureDir(rdonly, PromiseFlatCString(path).get());
    }
  }

  // Allow fonts subdir in XDG_DATA_HOME
  nsAutoCString xdgDataHome(PR_GetEnv("XDG_DATA_HOME"));
  if (!xdgDataHome.IsEmpty()) {
    nsAutoCString fontPath(xdgDataHome);
    fontPath.Append("/fonts");
    policy->AddFutureDir(rdonly, PromiseFlatCString(fontPath).get());
  }

  // Any font subdirs in XDG_DATA_DIRS
  nsAutoCString xdgDataDirs(PR_GetEnv("XDG_DATA_DIRS"));
  for (const auto& path : xdgDataDirs.Split(':')) {
    nsAutoCString fontPath(path);
    fontPath.Append("/fonts");
    policy->AddFutureDir(rdonly, PromiseFlatCString(fontPath).get());
  }

  // Extra configuration/cache dirs in the homedir that we want to allow read
  // access to.
  std::vector<const char*> extraConfDirsAllow = {
      ".themes",
      ".fonts",
      ".cache/fontconfig",
  };

  // Fallback if XDG_CONFIG_HOME isn't set
  if (xdgConfigHome.IsEmpty()) {
    extraConfDirsAllow.emplace_back(".config");
  }

  nsCOMPtr<nsIFile> homeDir;
  nsresult rv =
      GetSpecialSystemDirectory(Unix_HomeDirectory, getter_AddRefs(homeDir));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIFile> confDir;

    for (const auto& dir : extraConfDirsAllow) {
      rv = homeDir->Clone(getter_AddRefs(confDir));
      if (NS_SUCCEEDED(rv)) {
        rv = confDir->AppendRelativeNativePath(nsDependentCString(dir));
        if (NS_SUCCEEDED(rv)) {
          nsAutoCString tmpPath;
          rv = confDir->GetNativePath(tmpPath);
          if (NS_SUCCEEDED(rv)) {
            policy->AddDir(rdonly, tmpPath.get());
          }
        }
      }
    }

    // ~/.config/mozilla/ needs to be manually blocked, because the previous
    // loop will allow for ~/.config/ access.
    {
      // If $XDG_CONFIG_HOME is set, we need to account for it.
      // FIXME: Bug 1722272: Maybe this should just be handled with
      // GetSpecialSystemDirectory(Unix_XDG_ConfigHome) ?
      nsCOMPtr<nsIFile> confDirOrXDGConfigHomeDir;
      if (!xdgConfigHome.IsEmpty()) {
        rv = NS_NewNativeLocalFile(xdgConfigHome, true,
                                   getter_AddRefs(confDirOrXDGConfigHomeDir));
        // confDirOrXDGConfigHomeDir = nsIFile($XDG_CONFIG_HOME)
      } else {
        rv = homeDir->Clone(getter_AddRefs(confDirOrXDGConfigHomeDir));
        if (NS_SUCCEEDED(rv)) {
          // since we will use that later, we dont need to care about trailing
          // slash
          rv = confDirOrXDGConfigHomeDir->AppendNative(".config"_ns);
          // confDirOrXDGConfigHomeDir = nsIFile($HOME/.config/)
        }
      }

      if (NS_SUCCEEDED(rv)) {
        rv = confDirOrXDGConfigHomeDir->AppendNative("mozilla"_ns);
        if (NS_SUCCEEDED(rv)) {
          nsAutoCString tmpPath;
          rv = confDirOrXDGConfigHomeDir->GetNativePath(tmpPath);
          if (NS_SUCCEEDED(rv)) {
            policy->AddFutureDir(deny, tmpPath.get());
          }
        }
      }
    }

    // ~/.local/share (for themes)
    rv = homeDir->Clone(getter_AddRefs(confDir));
    if (NS_SUCCEEDED(rv)) {
      rv = confDir->AppendNative(".local"_ns);
      if (NS_SUCCEEDED(rv)) {
        rv = confDir->AppendNative("share"_ns);
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
      rv = confDir->AppendNative(".fonts.conf"_ns);
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
      rv = confDir->AppendNative(".pangorc"_ns);
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

  if (mozilla::IsDevelopmentBuild()) {
    // If this is a developer build the resources are symlinks to outside the
    // binary dir. Therefore in non-release builds we allow reads from the whole
    // repository. MOZ_DEVELOPER_REPO_DIR is set by mach run.
    const char* developer_repo_dir = PR_GetEnv("MOZ_DEVELOPER_REPO_DIR");
    if (developer_repo_dir) {
      policy->AddDir(rdonly, developer_repo_dir);
    }
  }

#ifdef DEBUG
  char* bloatLog = PR_GetEnv("XPCOM_MEM_BLOAT_LOG");
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

  if (!headless) {
    // Allow Primus to contact the Bumblebee daemon to manage GPU
    // switching on NVIDIA Optimus systems.
    const char* bumblebeeSocket = PR_GetEnv("BUMBLEBEE_SOCKET");
    if (bumblebeeSocket == nullptr) {
      bumblebeeSocket = "/var/run/bumblebee.socket";
    }
    policy->AddPath(SandboxBroker::MAY_CONNECT, bumblebeeSocket);

#if defined(MOZ_WIDGET_GTK) && defined(MOZ_X11)
    // Allow local X11 connections, for several purposes:
    //
    // * for content processes to use WebGL when the browser is in headless
    //   mode, by opening the X display if/when needed
    //
    // * if Primus or VirtualGL is used, to contact the secondary X server
    static const bool kIsX11 =
        !mozilla::widget::GdkIsWaylandDisplay() && PR_GetEnv("DISPLAY");
    if (kIsX11) {
      policy->AddPrefix(SandboxBroker::MAY_CONNECT, "/tmp/.X11-unix/X");
      if (auto* const xauth = PR_GetEnv("XAUTHORITY")) {
        policy->AddPath(rdonly, xauth);
      }
    }
#endif
  }

  // Read any extra paths that will get write permissions,
  // configured by the user or distro
  AddDynamicPathList(policy, "security.sandbox.content.write_path_whitelist",
                     rdwr);

  // Whitelisted for reading by the user/distro
  AddDynamicPathList(policy, "security.sandbox.content.read_path_whitelist",
                     rdonly);

  // Add write permissions on the content process specific temporary dir.
  nsCOMPtr<nsIFile> tmpDir;
  rv = NS_GetSpecialDirectory(NS_APP_CONTENT_PROCESS_TEMP_DIR,
                              getter_AddRefs(tmpDir));
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString tmpPath;
    rv = tmpDir->GetNativePath(tmpPath);
    if (NS_SUCCEEDED(rv)) {
      policy->AddDir(rdwrcr, tmpPath.get());
    }
  }

  // userContent.css and the extensions dir sit in the profile, which is
  // normally blocked.
  nsCOMPtr<nsIFile> profileDir;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(profileDir));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIFile> workDir;
    rv = profileDir->Clone(getter_AddRefs(workDir));
    if (NS_SUCCEEDED(rv)) {
      rv = workDir->AppendNative("chrome"_ns);
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
      rv = workDir->AppendNative("extensions"_ns);
      if (NS_SUCCEEDED(rv)) {
        nsAutoCString tmpPath;
        rv = workDir->GetNativePath(tmpPath);
        if (NS_SUCCEEDED(rv)) {
          bool exists;
          rv = workDir->Exists(&exists);
          if (NS_SUCCEEDED(rv)) {
            if (!exists) {
              policy->AddPrefix(rdonly, tmpPath.get());
              policy->AddPath(rdonly, tmpPath.get());
            } else {
              policy->AddDir(rdonly, tmpPath.get());
            }
          }
        }
      }
    }
  }

  const int level = GetEffectiveContentSandboxLevel();
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

  if (allowPulse) {
    policy->AddDir(rdwrcr, "/dev/shm");
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
#endif  // MOZ_WIDGET_GTK

  if (allowPulse) {
    // PulseAudio also needs access to read the $XAUTHORITY file (see
    // bug 1384986 comment #1), but that's already allowed for hybrid
    // GPU drivers (see above).
    policy->AddPath(rdonly, "/var/lib/dbus/machine-id");
  }

  // Bug 1434711 - AMDGPU-PRO crashes if it can't read it's marketing ids
  // and various other things
  if (!headless && HasAtiDrivers()) {
    policy->AddDir(rdonly, "/opt/amdgpu/share");
    policy->AddPath(rdonly, "/sys/module/amdgpu");
    // AMDGPU-PRO's MESA version likes to readlink a lot of things here
    policy->AddDir(access, "/sys");
  }

  mCommonContentPolicy.reset(policy);
}

UniquePtr<SandboxBroker::Policy> SandboxBrokerPolicyFactory::GetContentPolicy(
    int aPid, bool aFileProcess) {
  // Policy entries that vary per-process (because they depend on the
  // pid or content subtype) are added here.

  MOZ_ASSERT(NS_IsMainThread());

  const int level = GetEffectiveContentSandboxLevel();
  // The file broker is used at level 2 and up.
  if (level <= 1) {
    return nullptr;
  }

  std::call_once(mContentInited, [this] { InitContentPolicy(); });
  MOZ_ASSERT(mCommonContentPolicy);
  UniquePtr<SandboxBroker::Policy> policy(
      new SandboxBroker::Policy(*mCommonContentPolicy));

  // No read blocking at level 2 and below.
  // file:// processes also get global read permissions
  if (level <= 2 || aFileProcess) {
    policy->AddDir(rdonly, "/");
    // Any other read-only rules will be removed as redundant by
    // Policy::FixRecursivePermissions, so there's no need to
    // early-return here.
  }

  // Access to /dev/shm is restricted to a per-process prefix to
  // prevent interfering with other processes or with services outside
  // the browser (e.g., PulseAudio).
  AddSharedMemoryPaths(policy.get(), aPid);

  // Bug 1198550: the profiler's replacement for dl_iterate_phdr
  policy->AddPath(rdonly, nsPrintfCString("/proc/%d/maps", aPid).get());

  // Bug 1198552: memory reporting.
  AddMemoryReporting(policy.get(), aPid);

  // Bug 1384804, notably comment 15
  // Used by libnuma, included by x265/ffmpeg, who falls back
  // to get_mempolicy if this fails
  policy->AddPath(rdonly, nsPrintfCString("/proc/%d/status", aPid).get());

  // Finalize the policy.
  policy->FixRecursivePermissions();
  return policy;
}

/* static */ UniquePtr<SandboxBroker::Policy>
SandboxBrokerPolicyFactory::GetRDDPolicy(int aPid) {
  auto policy = MakeUnique<SandboxBroker::Policy>();

  AddSharedMemoryPaths(policy.get(), aPid);

  // FIXME (bug 1662321): we should fix nsSystemInfo so that every
  // child process doesn't need to re-read these files to get the info
  // the parent process already has.
  policy->AddPath(rdonly, "/proc/cpuinfo");
  policy->AddPath(rdonly,
                  "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
  policy->AddPath(rdonly, "/sys/devices/system/cpu/cpu0/cache/index2/size");
  policy->AddPath(rdonly, "/sys/devices/system/cpu/cpu0/cache/index3/size");
  policy->AddDir(rdonly, "/sys/devices/cpu");
  policy->AddDir(rdonly, "/sys/devices/system/cpu");
  policy->AddDir(rdonly, "/sys/devices/system/node");
  policy->AddDir(rdonly, "/lib");
  policy->AddDir(rdonly, "/lib64");
  policy->AddDir(rdonly, "/usr/lib");
  policy->AddDir(rdonly, "/usr/lib32");
  policy->AddDir(rdonly, "/usr/lib64");

  // Bug 1647957: memory reporting.
  AddMemoryReporting(policy.get(), aPid);

  // Firefox binary dir.
  // Note that unlike the previous cases, we use NS_GetSpecialDirectory
  // instead of GetSpecialSystemDirectory. The former requires a working XPCOM
  // system, which may not be the case for some tests. For querying for the
  // location of XPCOM things, we can use it anyway.
  nsCOMPtr<nsIFile> ffDir;
  nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(ffDir));
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString tmpPath;
    rv = ffDir->GetNativePath(tmpPath);
    if (NS_SUCCEEDED(rv)) {
      policy->AddDir(rdonly, tmpPath.get());
    }
  }

  if (mozilla::IsDevelopmentBuild()) {
    // If this is a developer build the resources are symlinks to outside the
    // binary dir. Therefore in non-release builds we allow reads from the whole
    // repository. MOZ_DEVELOPER_REPO_DIR is set by mach run.
    const char* developer_repo_dir = PR_GetEnv("MOZ_DEVELOPER_REPO_DIR");
    if (developer_repo_dir) {
      policy->AddDir(rdonly, developer_repo_dir);
    }
  }

  if (policy->IsEmpty()) {
    policy = nullptr;
  }
  return policy;
}

/* static */ UniquePtr<SandboxBroker::Policy>
SandboxBrokerPolicyFactory::GetSocketProcessPolicy(int aPid) {
  auto policy = MakeUnique<SandboxBroker::Policy>();

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
  policy->AddDir(rdonly, "/usr/share");
  policy->AddDir(rdonly, "/usr/local/share");
  policy->AddDir(rdonly, "/etc");

  // glibc will try to stat64("/") while populating nsswitch database
  // https://sourceware.org/git/?p=glibc.git;a=blob;f=nss/nss_database.c;h=cf0306adc47f12d9bc761ab1b013629f4482b7e6;hb=9826b03b747b841f5fc6de2054bf1ef3f5c4bdf3#l396
  // denying will make getaddrinfo() return ENONAME
  policy->AddDir(access, "/");

  AddLdconfigPaths(policy.get());

  // Socket process sandbox needs to allow shmem in order to support
  // profiling.  See Bug 1626385.
  AddSharedMemoryPaths(policy.get(), aPid);

  // Bug 1647957: memory reporting.
  AddMemoryReporting(policy.get(), aPid);

  // Firefox binary dir.
  // Note that unlike the previous cases, we use NS_GetSpecialDirectory
  // instead of GetSpecialSystemDirectory. The former requires a working XPCOM
  // system, which may not be the case for some tests. For querying for the
  // location of XPCOM things, we can use it anyway.
  nsCOMPtr<nsIFile> ffDir;
  nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(ffDir));
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString tmpPath;
    rv = ffDir->GetNativePath(tmpPath);
    if (NS_SUCCEEDED(rv)) {
      policy->AddDir(rdonly, tmpPath.get());
    }
  }

  if (policy->IsEmpty()) {
    policy = nullptr;
  }
  return policy;
}

}  // namespace mozilla
