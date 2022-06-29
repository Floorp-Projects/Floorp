/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GfxInfo.h"

#include <cctype>
#include <errno.h>
#include <unistd.h>
#include <string>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include "mozilla/gfx/Logging.h"
#include "mozilla/SSE.h"
#include "mozilla/Telemetry.h"
#include "nsCRTGlue.h"
#include "nsExceptionHandler.h"
#include "nsPrintfCString.h"
#include "nsUnicharUtils.h"
#include "nsWhitespaceTokenizer.h"
#include "prenv.h"
#include "WidgetUtilsGtk.h"

#define EXIT_STATUS_BUFFER_TOO_SMALL 2
#ifdef DEBUG
bool fire_glxtest_process();
#endif

namespace mozilla::widget {

#ifdef DEBUG
NS_IMPL_ISUPPORTS_INHERITED(GfxInfo, GfxInfoBase, nsIGfxInfoDebug)
#endif

// these global variables will be set when firing the glxtest process
int glxtest_pipe = -1;
pid_t glxtest_pid = 0;

nsresult GfxInfo::Init() {
  mGLMajorVersion = 0;
  mGLMinorVersion = 0;
  mHasTextureFromPixmap = false;
  mIsMesa = false;
  mIsAccelerated = true;
  mIsWayland = false;
  mIsXWayland = false;
  mHasMultipleGPUs = false;
  mGlxTestError = false;
  mIsVAAPISupported = false;
  return GfxInfoBase::Init();
}

void GfxInfo::AddCrashReportAnnotations() {
  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::AdapterVendorID,
                                     mVendorId);
  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::AdapterDeviceID,
                                     mDeviceId);
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::AdapterDriverVendor, mDriverVendor);
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::AdapterDriverVersion, mDriverVersion);
  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::IsWayland,
                                     mIsWayland);
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::DesktopEnvironment, mDesktopEnvironment);

  if (mHasMultipleGPUs) {
    nsAutoCString note;
    note.AppendLiteral("Has dual GPUs.");
    if (!mSecondaryVendorId.IsEmpty()) {
      note.AppendLiteral(" GPU #2: AdapterVendorID2: ");
      note.Append(mSecondaryVendorId);
      note.AppendLiteral(", AdapterDeviceID2: ");
      note.Append(mSecondaryDeviceId);
    }
    CrashReporter::AppendAppNotesToCrashReport(note);
  }
}

void GfxInfo::GetData() {
  GfxInfoBase::GetData();

  // to understand this function, see bug 639842. We retrieve the OpenGL driver
  // information in a separate process to protect against bad drivers.

  // if glxtest_pipe == -1, that means that we already read the information
  if (glxtest_pipe == -1) return;

  enum { buf_size = 2048 };
  char buf[buf_size];
  ssize_t bytesread = read(glxtest_pipe, &buf,
                           buf_size - 1);  // -1 because we'll append a zero
  close(glxtest_pipe);
  glxtest_pipe = -1;

  // bytesread < 0 would mean that the above read() call failed.
  // This should never happen. If it did, the outcome would be to blocklist
  // anyway.
  if (bytesread < 0) {
    bytesread = 0;
  } else if (bytesread == buf_size - 1) {
    gfxCriticalNote << "glxtest: read from pipe exceeded buffer size";
  }

  // let buf be a zero-terminated string
  buf[bytesread] = 0;

  // Wait for the glxtest process to finish. This serves 2 purposes:
  // * avoid having a zombie glxtest process laying around
  // * get the glxtest process status info.
  int glxtest_status = 0;
  bool wait_for_glxtest_process = true;
  bool waiting_for_glxtest_process_failed = false;
  int waitpid_errno = 0;
  while (wait_for_glxtest_process) {
    wait_for_glxtest_process = false;
    if (waitpid(glxtest_pid, &glxtest_status, 0) == -1) {
      waitpid_errno = errno;
      if (waitpid_errno == EINTR) {
        wait_for_glxtest_process = true;
      } else {
        // Bug 718629
        // ECHILD happens when the glxtest process got reaped got reaped after a
        // PR_CreateProcess as per bug 227246. This shouldn't matter, as we
        // still seem to get the data from the pipe, and if we didn't, the
        // outcome would be to blocklist anyway.
        waiting_for_glxtest_process_failed = (waitpid_errno != ECHILD);
      }
    }
  }

  int exit_code = EXIT_FAILURE;
  bool exited_with_error_code = false;
  if (!waiting_for_glxtest_process_failed && WIFEXITED(glxtest_status)) {
    exit_code = WEXITSTATUS(glxtest_status);
    exited_with_error_code = exit_code != EXIT_SUCCESS;
  }

  bool received_signal =
      !waiting_for_glxtest_process_failed && WIFSIGNALED(glxtest_status);

  bool error = waiting_for_glxtest_process_failed || exited_with_error_code ||
               received_signal;
  bool errorLog = false;

  nsCString glVendor;
  nsCString glRenderer;
  nsCString glVersion;
  nsCString textureFromPixmap;
  nsCString testType;

  // Available if GLX_MESA_query_renderer is supported.
  nsCString mesaVendor;
  nsCString mesaDevice;
  nsCString mesaAccelerated;
  // Available if using a DRI-based libGL stack.
  nsCString driDriver;
  nsCString adapterRam;

  nsCString drmRenderDevice;
  nsCString isVAAPISupported;

  nsCString ddxDriver;

  AutoTArray<nsCString, 2> pciVendors;
  AutoTArray<nsCString, 2> pciDevices;

  nsCString* stringToFill = nullptr;
  bool logString = false;

  char* bufptr = buf;
  while (true) {
    char* line = NS_strtok("\n", &bufptr);
    if (!line) break;
    if (stringToFill) {
      stringToFill->Assign(line);
      stringToFill = nullptr;
    } else if (logString) {
      gfxCriticalNote << "glxtest: " << line;
      logString = false;
    } else if (!strcmp(line, "VENDOR")) {
      stringToFill = &glVendor;
    } else if (!strcmp(line, "RENDERER")) {
      stringToFill = &glRenderer;
    } else if (!strcmp(line, "VERSION")) {
      stringToFill = &glVersion;
    } else if (!strcmp(line, "TFP")) {
      stringToFill = &textureFromPixmap;
    } else if (!strcmp(line, "MESA_VENDOR_ID")) {
      stringToFill = &mesaVendor;
    } else if (!strcmp(line, "MESA_DEVICE_ID")) {
      stringToFill = &mesaDevice;
    } else if (!strcmp(line, "MESA_ACCELERATED")) {
      stringToFill = &mesaAccelerated;
    } else if (!strcmp(line, "MESA_VRAM")) {
      stringToFill = &adapterRam;
    } else if (!strcmp(line, "DDX_DRIVER")) {
      stringToFill = &ddxDriver;
    } else if (!strcmp(line, "DRI_DRIVER")) {
      stringToFill = &driDriver;
    } else if (!strcmp(line, "PCI_VENDOR_ID")) {
      stringToFill = pciVendors.AppendElement();
    } else if (!strcmp(line, "PCI_DEVICE_ID")) {
      stringToFill = pciDevices.AppendElement();
    } else if (!strcmp(line, "DRM_RENDERDEVICE")) {
      stringToFill = &drmRenderDevice;
    } else if (!strcmp(line, "VAAPI_SUPPORTED")) {
      stringToFill = &isVAAPISupported;
    } else if (!strcmp(line, "TEST_TYPE")) {
      stringToFill = &testType;
    } else if (!strcmp(line, "WARNING")) {
      logString = true;
    } else if (!strcmp(line, "ERROR")) {
      logString = true;
      errorLog = true;
    }
  }

  MOZ_ASSERT(pciDevices.Length() == pciVendors.Length(),
             "Missing PCI vendors/devices");

  size_t pciLen = std::min(pciVendors.Length(), pciDevices.Length());
  mHasMultipleGPUs = pciLen > 1;

  if (!strcmp(textureFromPixmap.get(), "TRUE")) mHasTextureFromPixmap = true;

  // only useful for Linux kernel version check for FGLRX driver.
  // assumes X client == X server, which is sad.
  struct utsname unameobj {};
  if (uname(&unameobj) >= 0) {
    mOS.Assign(unameobj.sysname);
    mOSRelease.Assign(unameobj.release);
  }

  const char* spoofedVendor = PR_GetEnv("MOZ_GFX_SPOOF_GL_VENDOR");
  if (spoofedVendor) glVendor.Assign(spoofedVendor);
  const char* spoofedRenderer = PR_GetEnv("MOZ_GFX_SPOOF_GL_RENDERER");
  if (spoofedRenderer) glRenderer.Assign(spoofedRenderer);
  const char* spoofedVersion = PR_GetEnv("MOZ_GFX_SPOOF_GL_VERSION");
  if (spoofedVersion) glVersion.Assign(spoofedVersion);
  const char* spoofedOS = PR_GetEnv("MOZ_GFX_SPOOF_OS");
  if (spoofedOS) mOS.Assign(spoofedOS);
  const char* spoofedOSRelease = PR_GetEnv("MOZ_GFX_SPOOF_OS_RELEASE");
  if (spoofedOSRelease) mOSRelease.Assign(spoofedOSRelease);

  // Scan the GL_VERSION string for the GL and driver versions.
  nsCWhitespaceTokenizer tokenizer(glVersion);
  while (tokenizer.hasMoreTokens()) {
    nsCString token(tokenizer.nextToken());
    unsigned int major = 0, minor = 0, revision = 0, patch = 0;
    if (sscanf(token.get(), "%u.%u.%u.%u", &major, &minor, &revision, &patch) >=
        2) {
      // A survey of GL_VENDOR strings indicates that the first version is
      // always the GL version, the second is usually the driver version.
      if (mGLMajorVersion == 0) {
        mGLMajorVersion = major;
        mGLMinorVersion = minor;
      } else if (mDriverVersion.IsEmpty()) {  // Not already spoofed.
        mDriverVersion =
            nsPrintfCString("%u.%u.%u.%u", major, minor, revision, patch);
      }
    }
  }

  if (mGLMajorVersion == 0) {
    NS_WARNING("Failed to parse GL version!");
  }

  mDrmRenderDevice = std::move(drmRenderDevice);
  mIsVAAPISupported = isVAAPISupported.Equals("TRUE");
  mTestType = std::move(testType);

  // Mesa always exposes itself in the GL_VERSION string, but not always the
  // GL_VENDOR string.
  mIsMesa = glVersion.Find("Mesa") != -1;

  // We need to use custom driver vendor IDs for mesa so we can treat them
  // differently than the proprietary drivers.
  if (mIsMesa) {
    mIsAccelerated = !mesaAccelerated.Equals("FALSE");
    // Process software rasterizers before the DRI driver string; we may be
    // forcing software rasterization on a DRI-accelerated X server by using
    // LIBGL_ALWAYS_SOFTWARE or a similar restriction.
    if (strcasestr(glRenderer.get(), "llvmpipe")) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDriverVendor(DriverVendor::MesaLLVMPipe),
          mDriverVendor);
      mIsAccelerated = false;
    } else if (strcasestr(glRenderer.get(), "softpipe")) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDriverVendor(DriverVendor::MesaSoftPipe),
          mDriverVendor);
      mIsAccelerated = false;
    } else if (strcasestr(glRenderer.get(), "software rasterizer")) {
      CopyUTF16toUTF8(GfxDriverInfo::GetDriverVendor(DriverVendor::MesaSWRast),
                      mDriverVendor);
      mIsAccelerated = false;
    } else if (!mIsAccelerated) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDriverVendor(DriverVendor::MesaSWUnknown),
          mDriverVendor);
    } else if (!driDriver.IsEmpty()) {
      mDriverVendor = nsPrintfCString("mesa/%s", driDriver.get());
    } else {
      // Some other mesa configuration where we couldn't get enough info.
      NS_WARNING("Failed to detect Mesa driver being used!");
      CopyUTF16toUTF8(GfxDriverInfo::GetDriverVendor(DriverVendor::MesaUnknown),
                      mDriverVendor);
    }

    if (!mesaVendor.IsEmpty()) {
      mVendorId = mesaVendor;
    }

    if (!mesaDevice.IsEmpty()) {
      mDeviceId = mesaDevice;
    }

    if (!mIsAccelerated && mVendorId.IsEmpty()) {
      mVendorId.Assign(glVendor.get());
    }

    if (!mIsAccelerated && mDeviceId.IsEmpty()) {
      mDeviceId.Assign(glRenderer.get());
    }
  } else if (glVendor.EqualsLiteral("NVIDIA Corporation")) {
    CopyUTF16toUTF8(GfxDriverInfo::GetDeviceVendor(DeviceVendor::NVIDIA),
                    mVendorId);
    mDriverVendor.AssignLiteral("nvidia/unknown");
    // TODO: Use NV-CONTROL X11 extension to query Device ID and VRAM.
  } else if (glVendor.EqualsLiteral("ATI Technologies Inc.")) {
    CopyUTF16toUTF8(GfxDriverInfo::GetDeviceVendor(DeviceVendor::ATI),
                    mVendorId);
    mDriverVendor.AssignLiteral("ati/unknown");
    // TODO: Look into ways to find the device ID on FGLRX.
  } else {
    NS_WARNING("Failed to detect GL vendor!");
  }

  if (!adapterRam.IsEmpty()) {
    mAdapterRAM = (uint32_t)atoi(adapterRam.get());
  }

  // If we have the DRI driver, we can derive the vendor ID from that if needed.
  if (mVendorId.IsEmpty() && !driDriver.IsEmpty()) {
    const char* nvidiaDrivers[] = {"nouveau", "tegra", nullptr};
    for (size_t i = 0; nvidiaDrivers[i]; ++i) {
      if (driDriver.Equals(nvidiaDrivers[i])) {
        CopyUTF16toUTF8(GfxDriverInfo::GetDeviceVendor(DeviceVendor::NVIDIA),
                        mVendorId);
        break;
      }
    }

    if (mVendorId.IsEmpty()) {
      const char* intelDrivers[] = {"iris", "crocus", "i915", "i965",
                                    "i810", "intel",  nullptr};
      for (size_t i = 0; intelDrivers[i]; ++i) {
        if (driDriver.Equals(intelDrivers[i])) {
          CopyUTF16toUTF8(GfxDriverInfo::GetDeviceVendor(DeviceVendor::Intel),
                          mVendorId);
          break;
        }
      }
    }

    if (mVendorId.IsEmpty()) {
      const char* amdDrivers[] = {"r600",   "r200",     "r100",
                                  "radeon", "radeonsi", nullptr};
      for (size_t i = 0; amdDrivers[i]; ++i) {
        if (driDriver.Equals(amdDrivers[i])) {
          CopyUTF16toUTF8(GfxDriverInfo::GetDeviceVendor(DeviceVendor::ATI),
                          mVendorId);
          break;
        }
      }
    }

    if (mVendorId.IsEmpty()) {
      if (driDriver.EqualsLiteral("freedreno")) {
        CopyUTF16toUTF8(GfxDriverInfo::GetDeviceVendor(DeviceVendor::Qualcomm),
                        mVendorId);
      }
    }
  }

  // If we still don't have a vendor ID, we can try the PCI vendor list.
  if (mVendorId.IsEmpty()) {
    if (pciVendors.IsEmpty()) {
      gfxCriticalNote << "No GPUs detected via PCI";
    } else {
      for (size_t i = 0; i < pciVendors.Length(); ++i) {
        if (mVendorId.IsEmpty()) {
          mVendorId = pciVendors[i];
        } else if (mVendorId != pciVendors[i]) {
          gfxCriticalNote << "More than 1 GPU vendor detected via PCI, cannot "
                             "deduce vendor";
          mVendorId.Truncate();
          break;
        }
      }
    }
  }

  // If we know the vendor ID, but didn't get a device ID, we can guess from the
  // PCI device list.
  if (mDeviceId.IsEmpty() && !mVendorId.IsEmpty()) {
    for (size_t i = 0; i < pciLen; ++i) {
      if (mVendorId.Equals(pciVendors[i])) {
        if (mDeviceId.IsEmpty()) {
          mDeviceId = pciDevices[i];
        } else if (mDeviceId != pciDevices[i]) {
          gfxCriticalNote << "More than 1 GPU from same vendor detected via "
                             "PCI, cannot deduce device";
          mDeviceId.Truncate();
          break;
        }
      }
    }
  }

  // Assuming we know the vendor, we should check for a secondary card.
  if (!mVendorId.IsEmpty()) {
    if (pciLen > 2) {
      gfxCriticalNote
          << "More than 2 GPUs detected via PCI, secondary GPU is arbitrary";
    }
    for (size_t i = 0; i < pciLen; ++i) {
      if (!mVendorId.Equals(pciVendors[i]) ||
          (!mDeviceId.IsEmpty() && !mDeviceId.Equals(pciDevices[i]))) {
        mSecondaryVendorId = pciVendors[i];
        mSecondaryDeviceId = pciDevices[i];
        break;
      }
    }
  }

  // If we couldn't choose, log them.
  if (mVendorId.IsEmpty()) {
    for (size_t i = 0; i < pciLen; ++i) {
      gfxCriticalNote << "PCI candidate " << pciVendors[i].get() << "/"
                      << pciDevices[i].get();
    }
  }

  // Fallback to GL_VENDOR and GL_RENDERER.
  if (mVendorId.IsEmpty()) {
    mVendorId.Assign(glVendor.get());
  }
  if (mDeviceId.IsEmpty()) {
    mDeviceId.Assign(glRenderer.get());
  }

  mAdapterDescription.Assign(glRenderer);

  // Make a best effort guess at whether or not we are using the XWayland compat
  // layer. For all intents and purposes, we should otherwise believe we are
  // using X11.
  mIsWayland = GdkIsWaylandDisplay();
  const char* waylandDisplay = getenv("WAYLAND_DISPLAY");
  mIsXWayland = !mIsWayland && waylandDisplay;

  // Make a best effort guess at the desktop environment in use. Sadly there
  // does not appear to be a standard way to do this, so we check a few
  // different environment variables and search for relevant keywords.
  //
  // Note that some users manually change these values. Some applications check
  // the environment variable like we are here, and either not work or restrict
  // functionality. There may be some heroics we could go through to determine
  // the truth, but for the moment, this is the best we can do. This is
  // something to keep in mind when updating the blocklist.
  const char* desktopEnv = getenv("XDG_CURRENT_DESKTOP");
  if (!desktopEnv) {
    desktopEnv = getenv("DESKTOP_SESSION");
  }

  if (desktopEnv) {
    std::string currentDesktop(desktopEnv);
    for (auto& c : currentDesktop) {
      c = std::tolower(c);
    }

    if (currentDesktop.find("budgie") != std::string::npos) {
      // We need to check for Budgie first, because it might incorporate GNOME
      // into the environment variable value.
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::Budgie),
          mDesktopEnvironment);
    } else if (currentDesktop.find("gnome") != std::string::npos) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::GNOME),
          mDesktopEnvironment);
    } else if (currentDesktop.find("kde") != std::string::npos) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::KDE),
          mDesktopEnvironment);
    } else if (currentDesktop.find("xfce") != std::string::npos) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::XFCE),
          mDesktopEnvironment);
    } else if (currentDesktop.find("cinnamon") != std::string::npos) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::Cinnamon),
          mDesktopEnvironment);
    } else if (currentDesktop.find("enlightenment") != std::string::npos) {
      CopyUTF16toUTF8(GfxDriverInfo::GetDesktopEnvironment(
                          DesktopEnvironment::Enlightenment),
                      mDesktopEnvironment);
    } else if (currentDesktop.find("lxde") != std::string::npos ||
               currentDesktop.find("lubuntu") != std::string::npos) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::LXDE),
          mDesktopEnvironment);
    } else if (currentDesktop.find("openbox") != std::string::npos) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::Openbox),
          mDesktopEnvironment);
    } else if (currentDesktop.find("i3") != std::string::npos) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::i3),
          mDesktopEnvironment);
    } else if (currentDesktop.find("sway") != std::string::npos) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::Sway),
          mDesktopEnvironment);
    } else if (currentDesktop.find("mate") != std::string::npos) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::Mate),
          mDesktopEnvironment);
    } else if (currentDesktop.find("unity") != std::string::npos) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::Unity),
          mDesktopEnvironment);
    } else if (currentDesktop.find("pantheon") != std::string::npos) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::Pantheon),
          mDesktopEnvironment);
    } else if (currentDesktop.find("lxqt") != std::string::npos) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::LXQT),
          mDesktopEnvironment);
    } else if (currentDesktop.find("deepin") != std::string::npos) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::Deepin),
          mDesktopEnvironment);
    } else if (currentDesktop.find("dwm") != std::string::npos) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::Dwm),
          mDesktopEnvironment);
    }
  }

  if (mDesktopEnvironment.IsEmpty()) {
    if (getenv("GNOME_DESKTOP_SESSION_ID")) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::GNOME),
          mDesktopEnvironment);
    } else if (getenv("KDE_FULL_SESSION")) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::KDE),
          mDesktopEnvironment);
    } else if (getenv("MATE_DESKTOP_SESSION_ID")) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::Mate),
          mDesktopEnvironment);
    } else if (getenv("LXQT_SESSION_CONFIG")) {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::LXQT),
          mDesktopEnvironment);
    } else {
      CopyUTF16toUTF8(
          GfxDriverInfo::GetDesktopEnvironment(DesktopEnvironment::Unknown),
          mDesktopEnvironment);
    }
  }

  if (!ddxDriver.IsEmpty()) {
    PRInt32 start = 0;
    PRInt32 loc = ddxDriver.Find(";", PR_FALSE, start);
    while (loc != kNotFound) {
      nsCString line(ddxDriver.get() + start, loc - start);
      mDdxDrivers.AppendElement(std::move(line));

      start = loc + 1;
      loc = ddxDriver.Find(";", PR_FALSE, start);
    }
  }

  if (error || errorLog || mTestType.IsEmpty()) {
    if (!mAdapterDescription.IsEmpty()) {
      mAdapterDescription.AppendLiteral(" (See failure log)");
    } else {
      mAdapterDescription.AppendLiteral("See failure log");
    }

    mGlxTestError = true;
  }

  if (error) {
    nsAutoCString msg("glxtest: process failed");
    if (waiting_for_glxtest_process_failed) {
      msg.AppendPrintf(" (waitpid failed with errno=%d for pid %d)",
                       waitpid_errno, glxtest_pid);
    }

    if (exited_with_error_code) {
      if (exit_code == EXIT_STATUS_BUFFER_TOO_SMALL) {
        msg.AppendLiteral(" (buffer too small)");
      } else {
        msg.AppendPrintf(" (exited with status %d)",
                         WEXITSTATUS(glxtest_status));
      }
    }
    if (received_signal) {
      msg.AppendPrintf(" (received signal %d)", WTERMSIG(glxtest_status));
    }

    gfxCriticalNote << msg.get();
  }

  AddCrashReportAnnotations();
}

const nsTArray<GfxDriverInfo>& GfxInfo::GetGfxDriverInfo() {
  if (!sDriverInfo->Length()) {
    // Mesa 10.0 provides the GLX_MESA_query_renderer extension, which allows us
    // to query device IDs backing a GL context for blocklisting.
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::MesaAll,
        DeviceFamily::All, GfxDriverInfo::allFeatures,
        nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION, DRIVER_LESS_THAN,
        V(10, 0, 0, 0), "FEATURE_FAILURE_OLD_MESA", "Mesa 10.0");

    // NVIDIA Mesa baseline (see bug 1714391).
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::MesaNouveau,
        DeviceFamily::All, GfxDriverInfo::allFeatures,
        nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION, DRIVER_LESS_THAN,
        V(11, 0, 0, 0), "FEATURE_FAILURE_OLD_NV_MESA", "Mesa 11.0");

    // NVIDIA baseline (ported from old blocklist)
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::NonMesaAll,
        DeviceFamily::NvidiaAll, GfxDriverInfo::allFeatures,
        nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION, DRIVER_LESS_THAN,
        V(257, 21, 0, 0), "FEATURE_FAILURE_OLD_NVIDIA", "NVIDIA 257.21");

    // fglrx baseline (chosen arbitrarily as 2013-07-22 release).
    APPEND_TO_DRIVER_BLOCKLIST(
        OperatingSystem::Linux, DeviceFamily::AtiAll,
        GfxDriverInfo::allFeatures, nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION,
        DRIVER_LESS_THAN, V(13, 15, 100, 1), "FEATURE_FAILURE_OLD_FGLRX",
        "fglrx 13.15.100.1");

    ////////////////////////////////////
    // FEATURE_WEBRENDER

    // All Mesa software drivers, they should get Software WebRender instead.
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All,
        DriverVendor::SoftwareMesaAll, DeviceFamily::All,
        nsIGfxInfo::FEATURE_WEBRENDER, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
        DRIVER_COMPARISON_IGNORED, V(0, 0, 0, 0), "FEATURE_FAILURE_SOFTWARE_GL",
        "");

    APPEND_TO_DRIVER_BLOCKLIST(
        OperatingSystem::Linux, DeviceFamily::IntelWebRenderBlocked,
        nsIGfxInfo::FEATURE_WEBRENDER, nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
        DRIVER_COMPARISON_IGNORED, V(0, 0, 0, 0), "INTEL_DEVICE_GEN5_OR_OLDER",
        "");

    // Nvidia Mesa baseline, see bug 1563859.
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::MesaAll,
        DeviceFamily::NvidiaAll, nsIGfxInfo::FEATURE_WEBRENDER,
        nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION, DRIVER_LESS_THAN,
        V(18, 2, 0, 0), "FEATURE_FAILURE_WEBRENDER_OLD_MESA", "Mesa 18.2.0.0");

    // Disable on all older Nvidia drivers due to stability issues.
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::NonMesaAll,
        DeviceFamily::NvidiaAll, nsIGfxInfo::FEATURE_WEBRENDER,
        nsIGfxInfo::FEATURE_BLOCKED_DEVICE, DRIVER_LESS_THAN, V(460, 32, 3, 0),
        "FEATURE_FAILURE_WEBRENDER_OLD_NVIDIA", "460.32.03");

    // ATI Mesa baseline, chosen arbitrarily.
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::MesaAll,
        DeviceFamily::AtiAll, nsIGfxInfo::FEATURE_WEBRENDER,
        nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION, DRIVER_LESS_THAN,
        V(17, 0, 0, 0), "FEATURE_FAILURE_WEBRENDER_OLD_MESA", "Mesa 17.0.0.0");

    // Bug 1690568 / Bug 1393793 - Require Mesa 17.3.0+ for devices using the
    // r600 driver to avoid shader compilation issues.
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::MesaR600,
        DeviceFamily::All, nsIGfxInfo::FEATURE_WEBRENDER,
        nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION, DRIVER_LESS_THAN,
        V(17, 3, 0, 0), "FEATURE_FAILURE_WEBRENDER_OLD_MESA_R600",
        "Mesa 17.3.0.0");

    // Disable on all ATI devices not using Mesa for now.
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::NonMesaAll,
        DeviceFamily::AtiAll, nsIGfxInfo::FEATURE_WEBRENDER,
        nsIGfxInfo::FEATURE_BLOCKED_DEVICE, DRIVER_COMPARISON_IGNORED,
        V(0, 0, 0, 0), "FEATURE_FAILURE_WEBRENDER_NO_LINUX_ATI", "");

    // Bug 1673939 - Garbled text on RS880 GPUs with Mesa drivers.
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::MesaAll,
        DeviceFamily::AmdR600, nsIGfxInfo::FEATURE_WEBRENDER,
        nsIGfxInfo::FEATURE_BLOCKED_DEVICE, DRIVER_COMPARISON_IGNORED,
        V(0, 0, 0, 0), "FEATURE_FAILURE_WEBRENDER_BUG_1673939",
        "https://gitlab.freedesktop.org/mesa/mesa/-/issues/3720");

    // Bug 1635186 - Poor performance with video playing in a background window
    // on XWayland. Keep in sync with FEATURE_X11_EGL below to only enable them
    // together by default. Only Mesa and Nvidia binary drivers are expected
    // on Wayland rigth now.
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::XWayland,
        DriverVendor::MesaAll, DeviceFamily::All, nsIGfxInfo::FEATURE_WEBRENDER,
        nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION, DRIVER_LESS_THAN,
        V(21, 0, 0, 0), "FEATURE_FAILURE_WEBRENDER_BUG_1635186",
        "Mesa 21.0.0.0");

    ////////////////////////////////////
    // FEATURE_WEBRENDER - ALLOWLIST

#if defined(EARLY_BETA_OR_EARLIER)
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::MesaAll,
        DeviceFamily::All, nsIGfxInfo::FEATURE_WEBRENDER,
        nsIGfxInfo::FEATURE_ALLOW_ALWAYS, DRIVER_GREATER_THAN_OR_EQUAL,
        V(21, 0, 0, 0), "FEATURE_MESA", "Mesa 21.0.0.0");
#endif

    // Intel Mesa baseline, chosen arbitrarily.
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::MesaAll,
        DeviceFamily::IntelAll, nsIGfxInfo::FEATURE_WEBRENDER,
        nsIGfxInfo::FEATURE_ALLOW_ALWAYS, DRIVER_GREATER_THAN_OR_EQUAL,
        V(17, 0, 0, 0), "FEATURE_ROLLOUT_INTEL_MESA", "Mesa 17.0.0.0");

    // Nvidia Mesa baseline, see bug 1563859.
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::MesaAll,
        DeviceFamily::NvidiaRolloutWebRender, nsIGfxInfo::FEATURE_WEBRENDER,
        nsIGfxInfo::FEATURE_ALLOW_QUALIFIED, DRIVER_GREATER_THAN_OR_EQUAL,
        V(18, 2, 0, 0), "FEATURE_ROLLOUT_NVIDIA_MESA", "Mesa 18.2.0.0");

    // Nvidia proprietary driver baseline, see bug 1742994.
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::NonMesaAll,
        DeviceFamily::NvidiaAll, nsIGfxInfo::FEATURE_WEBRENDER,
        nsIGfxInfo::FEATURE_ALLOW_QUALIFIED, DRIVER_GREATER_THAN_OR_EQUAL,
        V(470, 82, 0, 0), "FEATURE_ROLLOUT_NVIDIA_BINARY", "470.82.0");

    // ATI Mesa baseline, chosen arbitrarily.
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::MesaAll,
        DeviceFamily::AtiRolloutWebRender, nsIGfxInfo::FEATURE_WEBRENDER,
        nsIGfxInfo::FEATURE_ALLOW_ALWAYS, DRIVER_GREATER_THAN_OR_EQUAL,
        V(17, 0, 0, 0), "FEATURE_ROLLOUT_ATI_MESA", "Mesa 17.0.0.0");

    ////////////////////////////////////
    // FEATURE_WEBRENDER_COMPOSITOR
    APPEND_TO_DRIVER_BLOCKLIST(
        OperatingSystem::Linux, DeviceFamily::All,
        nsIGfxInfo::FEATURE_WEBRENDER_COMPOSITOR,
        nsIGfxInfo::FEATURE_BLOCKED_DEVICE, DRIVER_COMPARISON_IGNORED,
        V(0, 0, 0, 0), "FEATURE_FAILURE_WEBRENDER_COMPOSITOR_DISABLED", "");

    ////////////////////////////////////
    // FEATURE_X11_EGL
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::MesaAll,
        DeviceFamily::All, nsIGfxInfo::FEATURE_X11_EGL,
        nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION, DRIVER_LESS_THAN,
        V(21, 0, 0, 0), "FEATURE_ROLLOUT_X11_EGL_MESA", "Mesa 21.0.0.0");

    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::NonMesaAll,
        DeviceFamily::NvidiaAll, nsIGfxInfo::FEATURE_X11_EGL,
        nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION, DRIVER_LESS_THAN,
        V(470, 82, 0, 0), "FEATURE_ROLLOUT_X11_EGL_NVIDIA_BINARY", "470.82.0");

    // Disable on all AMD devices not using Mesa.
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::NonMesaAll,
        DeviceFamily::AtiAll, nsIGfxInfo::FEATURE_X11_EGL,
        nsIGfxInfo::FEATURE_BLOCKED_DEVICE, DRIVER_COMPARISON_IGNORED,
        V(0, 0, 0, 0), "FEATURE_FAILURE_X11_EGL_NO_LINUX_ATI", "");

    ////////////////////////////////////
    // FEATURE_DMABUF
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::NonMesaAll,
        DeviceFamily::NvidiaAll, nsIGfxInfo::FEATURE_DMABUF,
        nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION, DRIVER_LESS_THAN,
        V(495, 44, 0, 0), "FEATURE_FAILURE_NO_GBM", "495.44.0");

    ////////////////////////////////////
    // FEATURE_DMABUF_SURFACE_EXPORT
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::MesaAll,
        DeviceFamily::AtiAll, nsIGfxInfo::FEATURE_DMABUF_SURFACE_EXPORT,
        nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION, DRIVER_LESS_THAN,
        V(21, 1, 2, 0), "FEATURE_FAILURE_BROKEN_MESA", "22.1.2");

    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::MesaAll,
        DeviceFamily::IntelAll, nsIGfxInfo::FEATURE_DMABUF_SURFACE_EXPORT,
        nsIGfxInfo::FEATURE_BLOCKED_DEVICE, DRIVER_COMPARISON_IGNORED,
        V(0, 0, 0, 0), "FEATURE_FAILURE_BROKEN_DRIVER", "");

    ////////////////////////////////////
    // FEATURE_HARDWARE_VIDEO_DECODING
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::MesaAll,
        DeviceFamily::All, nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING,
        nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION, DRIVER_LESS_THAN,
        V(21, 0, 0, 0), "FEATURE_HARDWARE_VIDEO_DECODING_MESA",
        "Mesa 21.0.0.0");

    // Disable on all NVIDIA hardware
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::All,
        DeviceFamily::NvidiaAll, nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING,
        nsIGfxInfo::FEATURE_BLOCKED_DEVICE, DRIVER_COMPARISON_IGNORED,
        V(0, 0, 0, 0), "FEATURE_HARDWARE_VIDEO_DECODING_NO_LINUX_NVIDIA", "");

    // Disable on all AMD devices not using Mesa.
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::NonMesaAll,
        DeviceFamily::AtiAll, nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING,
        nsIGfxInfo::FEATURE_BLOCKED_DEVICE, DRIVER_COMPARISON_IGNORED,
        V(0, 0, 0, 0), "FEATURE_HARDWARE_VIDEO_DECODING_NO_LINUX_AMD", "");

    // Disable on Release/late Beta
#if !defined(EARLY_BETA_OR_EARLIER)
    APPEND_TO_DRIVER_BLOCKLIST(OperatingSystem::Linux, DeviceFamily::All,
                               nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING,
                               nsIGfxInfo::FEATURE_BLOCKED_DEVICE,
                               DRIVER_COMPARISON_IGNORED, V(0, 0, 0, 0),
                               "FEATURE_HARDWARE_VIDEO_DECODING_DISABLE", "");
#endif

    ////////////////////////////////////
    // FEATURE_WEBRENDER_PARTIAL_PRESENT
    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::X11, DriverVendor::NonMesaAll,
        DeviceFamily::NvidiaAll, nsIGfxInfo::FEATURE_WEBRENDER_PARTIAL_PRESENT,
        nsIGfxInfo::FEATURE_BLOCKED_DEVICE, DRIVER_COMPARISON_IGNORED,
        V(0, 0, 0, 0), "FEATURE_ROLLOUT_WR_PARTIAL_PRESENT_NVIDIA_BINARY", "");

    ////////////////////////////////////

    APPEND_TO_DRIVER_BLOCKLIST_EXT(
        OperatingSystem::Linux, ScreenSizeStatus::All, BatteryStatus::All,
        DesktopEnvironment::All, WindowProtocol::All, DriverVendor::MesaNouveau,
        DeviceFamily::All, nsIGfxInfo::FEATURE_THREADSAFE_GL,
        nsIGfxInfo::FEATURE_BLOCKED_DEVICE, DRIVER_COMPARISON_IGNORED,
        V(0, 0, 0, 0), "FEATURE_FAILURE_THREADSAFE_GL", "");
  }
  return *sDriverInfo;
}

bool GfxInfo::DoesWindowProtocolMatch(const nsAString& aBlocklistWindowProtocol,
                                      const nsAString& aWindowProtocol) {
  if (mIsWayland &&
      aBlocklistWindowProtocol.Equals(
          GfxDriverInfo::GetWindowProtocol(WindowProtocol::WaylandAll),
          nsCaseInsensitiveStringComparator)) {
    return true;
  }
  if (!mIsWayland &&
      aBlocklistWindowProtocol.Equals(
          GfxDriverInfo::GetWindowProtocol(WindowProtocol::X11All),
          nsCaseInsensitiveStringComparator)) {
    return true;
  }
  return GfxInfoBase::DoesWindowProtocolMatch(aBlocklistWindowProtocol,
                                              aWindowProtocol);
}

bool GfxInfo::DoesDriverVendorMatch(const nsAString& aBlocklistVendor,
                                    const nsAString& aDriverVendor) {
  if (mIsMesa) {
    if (aBlocklistVendor.Equals(
            GfxDriverInfo::GetDriverVendor(DriverVendor::MesaAll),
            nsCaseInsensitiveStringComparator)) {
      return true;
    }
    if (mIsAccelerated &&
        aBlocklistVendor.Equals(
            GfxDriverInfo::GetDriverVendor(DriverVendor::HardwareMesaAll),
            nsCaseInsensitiveStringComparator)) {
      return true;
    }
    if (!mIsAccelerated &&
        aBlocklistVendor.Equals(
            GfxDriverInfo::GetDriverVendor(DriverVendor::SoftwareMesaAll),
            nsCaseInsensitiveStringComparator)) {
      return true;
    }
  }
  if (!mIsMesa && aBlocklistVendor.Equals(
                      GfxDriverInfo::GetDriverVendor(DriverVendor::NonMesaAll),
                      nsCaseInsensitiveStringComparator)) {
    return true;
  }
  return GfxInfoBase::DoesDriverVendorMatch(aBlocklistVendor, aDriverVendor);
}

nsresult GfxInfo::GetFeatureStatusImpl(
    int32_t aFeature, int32_t* aStatus, nsAString& aSuggestedDriverVersion,
    const nsTArray<GfxDriverInfo>& aDriverInfo, nsACString& aFailureId,
    OperatingSystem* aOS /* = nullptr */)

{
  NS_ENSURE_ARG_POINTER(aStatus);
  *aStatus = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
  aSuggestedDriverVersion.SetIsVoid(true);
  OperatingSystem os = OperatingSystem::Linux;
  if (aOS) *aOS = os;

  if (sShutdownOccurred) {
    return NS_OK;
  }

  GetData();

  if (mGlxTestError) {
    // If glxtest failed, block all features by default.
    *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
    aFailureId = "FEATURE_FAILURE_GLXTEST_FAILED";
    return NS_OK;
  }

  if (mGLMajorVersion == 1) {
    // We're on OpenGL 1. In most cases that indicates really old hardware.
    // We better block them, rather than rely on them to fail gracefully,
    // because they don't! see bug 696636
    *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
    aFailureId = "FEATURE_FAILURE_OPENGL_1";
    return NS_OK;
  }

  // Blocklist software GL implementations from using layers acceleration.
  // On the test infrastructure, we'll force-enable layers acceleration.
  if (aFeature == nsIGfxInfo::FEATURE_OPENGL_LAYERS && !mIsAccelerated &&
      !PR_GetEnv("MOZ_LAYERS_ALLOW_SOFTWARE_GL")) {
    *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
    aFailureId = "FEATURE_FAILURE_SOFTWARE_GL";
    return NS_OK;
  }

  if (aFeature == nsIGfxInfo::FEATURE_WEBRENDER) {
    // Don't try Webrender on devices where we are guaranteed to fail.
    if (mGLMajorVersion < 3) {
      *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
      aFailureId = "FEATURE_FAILURE_OPENGL_LESS_THAN_3";
      return NS_OK;
    }

    // Bug 1710400: Disable Webrender on the deprecated Intel DDX driver
    for (const nsCString& driver : mDdxDrivers) {
      if (strcasestr(driver.get(), "Intel")) {
        *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
        aFailureId = "FEATURE_FAILURE_DDX_INTEL";
        return NS_OK;
      }
    }
  }

  if (aFeature == nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING &&
      !mIsVAAPISupported) {
    *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
    aFailureId = "FEATURE_FAILURE_VIDEO_DECODING_TEST_FAILED";
    return NS_OK;
  }

  return GfxInfoBase::GetFeatureStatusImpl(
      aFeature, aStatus, aSuggestedDriverVersion, aDriverInfo, aFailureId, &os);
}

NS_IMETHODIMP
GfxInfo::GetD2DEnabled(bool* aEnabled) { return NS_ERROR_FAILURE; }

NS_IMETHODIMP
GfxInfo::GetDWriteEnabled(bool* aEnabled) { return NS_ERROR_FAILURE; }

NS_IMETHODIMP
GfxInfo::GetDWriteVersion(nsAString& aDwriteVersion) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP GfxInfo::GetHasBattery(bool* aHasBattery) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GfxInfo::GetEmbeddedInFirefoxReality(bool* aEmbeddedInFirefoxReality) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetCleartypeParameters(nsAString& aCleartypeParams) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetWindowProtocol(nsAString& aWindowProtocol) {
  GetData();
  if (mIsWayland) {
    aWindowProtocol = GfxDriverInfo::GetWindowProtocol(WindowProtocol::Wayland);
  } else if (mIsXWayland) {
    aWindowProtocol =
        GfxDriverInfo::GetWindowProtocol(WindowProtocol::XWayland);
  } else {
    aWindowProtocol = GfxDriverInfo::GetWindowProtocol(WindowProtocol::X11);
  }
  Telemetry::ScalarSet(Telemetry::ScalarID::GFX_LINUX_WINDOW_PROTOCOL,
                       aWindowProtocol);
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetDesktopEnvironment(nsAString& aDesktopEnvironment) {
  GetData();
  AppendASCIItoUTF16(mDesktopEnvironment, aDesktopEnvironment);
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetTestType(nsAString& aTestType) {
  GetData();
  AppendASCIItoUTF16(mTestType, aTestType);
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDescription(nsAString& aAdapterDescription) {
  GetData();
  AppendASCIItoUTF16(mAdapterDescription, aAdapterDescription);
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDescription2(nsAString& aAdapterDescription) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterRAM(uint32_t* aAdapterRAM) {
  GetData();
  *aAdapterRAM = mAdapterRAM;
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterRAM2(uint32_t* aAdapterRAM) { return NS_ERROR_FAILURE; }

NS_IMETHODIMP
GfxInfo::GetAdapterDriver(nsAString& aAdapterDriver) {
  aAdapterDriver.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriver2(nsAString& aAdapterDriver) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriverVendor(nsAString& aAdapterDriverVendor) {
  GetData();
  CopyASCIItoUTF16(mDriverVendor, aAdapterDriverVendor);
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriverVendor2(nsAString& aAdapterDriverVendor) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion(nsAString& aAdapterDriverVersion) {
  GetData();
  CopyASCIItoUTF16(mDriverVersion, aAdapterDriverVersion);
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion2(nsAString& aAdapterDriverVersion) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate(nsAString& aAdapterDriverDate) {
  aAdapterDriverDate.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate2(nsAString& aAdapterDriverDate) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterVendorID(nsAString& aAdapterVendorID) {
  GetData();
  CopyUTF8toUTF16(mVendorId, aAdapterVendorID);
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterVendorID2(nsAString& aAdapterVendorID) {
  GetData();
  CopyUTF8toUTF16(mSecondaryVendorId, aAdapterVendorID);
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID(nsAString& aAdapterDeviceID) {
  GetData();
  CopyUTF8toUTF16(mDeviceId, aAdapterDeviceID);
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID2(nsAString& aAdapterDeviceID) {
  GetData();
  CopyUTF8toUTF16(mSecondaryDeviceId, aAdapterDeviceID);
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterSubsysID(nsAString& aAdapterSubsysID) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterSubsysID2(nsAString& aAdapterSubsysID) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetIsGPU2Active(bool* aIsGPU2Active) {
  // This is never the case, as the active GPU should be the primary GPU.
  *aIsGPU2Active = false;
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetDrmRenderDevice(nsACString& aDrmRenderDevice) {
  GetData();
  aDrmRenderDevice.Assign(mDrmRenderDevice);
  return NS_OK;
}

#ifdef DEBUG

// Implement nsIGfxInfoDebug
// We don't support spoofing anything on Linux

NS_IMETHODIMP GfxInfo::SpoofVendorID(const nsAString& aVendorID) {
  GetData();
  CopyUTF16toUTF8(aVendorID, mVendorId);
  mIsAccelerated = true;
  return NS_OK;
}

NS_IMETHODIMP GfxInfo::SpoofDeviceID(const nsAString& aDeviceID) {
  GetData();
  CopyUTF16toUTF8(aDeviceID, mDeviceId);
  return NS_OK;
}

NS_IMETHODIMP GfxInfo::SpoofDriverVersion(const nsAString& aDriverVersion) {
  GetData();
  CopyUTF16toUTF8(aDriverVersion, mDriverVersion);
  return NS_OK;
}

NS_IMETHODIMP GfxInfo::SpoofOSVersion(uint32_t aVersion) {
  // We don't support OS versioning on Linux. There's just "Linux".
  return NS_OK;
}

NS_IMETHODIMP GfxInfo::FireTestProcess() {
  // If the pid is zero, then we have never run the test process to query for
  // driver information. This would normally be run on startup, but we need to
  // manually invoke it for XPC shell tests.
  if (glxtest_pid == 0) {
    fire_glxtest_process();
  }
  return NS_OK;
}

#endif

}  // namespace mozilla::widget
