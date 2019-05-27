/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/utsname.h>
#include "nsCRTGlue.h"
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#include "prenv.h"
#include "nsPrintfCString.h"
#include "nsWhitespaceTokenizer.h"

#include "GfxInfoX11.h"

#include <gdk/gdkx.h>

#ifdef DEBUG
bool fire_glxtest_process();
#endif

namespace mozilla {
namespace widget {

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
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::IsWayland, mIsWayland);
}

void GfxInfo::GetData() {
  // to understand this function, see bug 639842. We retrieve the OpenGL driver
  // information in a separate process to protect against bad drivers.

  // if glxtest_pipe == -1, that means that we already read the information
  if (glxtest_pipe == -1) return;

  enum { buf_size = 1024 };
  char buf[buf_size];
  ssize_t bytesread = read(glxtest_pipe, &buf,
                           buf_size - 1);  // -1 because we'll append a zero
  close(glxtest_pipe);
  glxtest_pipe = -1;

  // bytesread < 0 would mean that the above read() call failed.
  // This should never happen. If it did, the outcome would be to blacklist
  // anyway.
  if (bytesread < 0) bytesread = 0;

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
        // outcome would be to blacklist anyway.
        waiting_for_glxtest_process_failed = (waitpid_errno != ECHILD);
      }
    }
  }

  bool exited_with_error_code = !waiting_for_glxtest_process_failed &&
                                WIFEXITED(glxtest_status) &&
                                WEXITSTATUS(glxtest_status) != EXIT_SUCCESS;
  bool received_signal =
      !waiting_for_glxtest_process_failed && WIFSIGNALED(glxtest_status);

  bool error = waiting_for_glxtest_process_failed || exited_with_error_code ||
               received_signal;

  nsCString glVendor;
  nsCString glRenderer;
  nsCString glVersion;
  nsCString textureFromPixmap;

  // Available if GLX_MESA_query_renderer is supported.
  nsCString mesaVendor;
  nsCString mesaDevice;
  nsCString mesaAccelerated;
  // Available if using a DRI-based libGL stack.
  nsCString driDriver;

  nsCString* stringToFill = nullptr;
  char* bufptr = buf;
  if (!error) {
    while (true) {
      char* line = NS_strtok("\n", &bufptr);
      if (!line) break;
      if (stringToFill) {
        stringToFill->Assign(line);
        stringToFill = nullptr;
      } else if (!strcmp(line, "VENDOR"))
        stringToFill = &glVendor;
      else if (!strcmp(line, "RENDERER"))
        stringToFill = &glRenderer;
      else if (!strcmp(line, "VERSION"))
        stringToFill = &glVersion;
      else if (!strcmp(line, "TFP"))
        stringToFill = &textureFromPixmap;
      else if (!strcmp(line, "MESA_VENDOR_ID"))
        stringToFill = &mesaVendor;
      else if (!strcmp(line, "MESA_DEVICE_ID"))
        stringToFill = &mesaDevice;
      else if (!strcmp(line, "MESA_ACCELERATED"))
        stringToFill = &mesaAccelerated;
      else if (!strcmp(line, "MESA_VRAM"))
        stringToFill = &mAdapterRAM;
      else if (!strcmp(line, "DRI_DRIVER"))
        stringToFill = &driDriver;
    }
  }

  if (!strcmp(textureFromPixmap.get(), "TRUE")) mHasTextureFromPixmap = true;

  // only useful for Linux kernel version check for FGLRX driver.
  // assumes X client == X server, which is sad.
  struct utsname unameobj;
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

  if (error || glVendor.IsEmpty() || glRenderer.IsEmpty() ||
      glVersion.IsEmpty() || mOS.IsEmpty() || mOSRelease.IsEmpty()) {
    mAdapterDescription.AppendLiteral("GLXtest process failed");
    if (waiting_for_glxtest_process_failed)
      mAdapterDescription.AppendPrintf(
          " (waitpid failed with errno=%d for pid %d)", waitpid_errno,
          glxtest_pid);
    if (exited_with_error_code)
      mAdapterDescription.AppendPrintf(" (exited with status %d)",
                                       WEXITSTATUS(glxtest_status));
    if (received_signal)
      mAdapterDescription.AppendPrintf(" (received signal %d)",
                                       WTERMSIG(glxtest_status));
    if (bytesread) {
      mAdapterDescription.AppendLiteral(": ");
      mAdapterDescription.Append(nsDependentCString(buf));
      mAdapterDescription.Append('\n');
    }

    CrashReporter::AppendAppNotesToCrashReport(mAdapterDescription);
    return;
  }

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
    return;
  }

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
      CopyUTF16toUTF8(GfxDriverInfo::GetDriverVendor(DriverMesaLLVMPipe),
                      mDriverVendor);
      mIsAccelerated = false;
    } else if (strcasestr(glRenderer.get(), "softpipe")) {
      CopyUTF16toUTF8(GfxDriverInfo::GetDriverVendor(DriverMesaSoftPipe),
                      mDriverVendor);
      mIsAccelerated = false;
    } else if (strcasestr(glRenderer.get(), "software rasterizer") ||
               !mIsAccelerated) {
      // Fallback to reporting swrast if GLX_MESA_query_renderer tells us
      // we're using an unaccelerated context.
      CopyUTF16toUTF8(GfxDriverInfo::GetDriverVendor(DriverMesaSWRast),
                      mDriverVendor);
      mIsAccelerated = false;
    } else if (!driDriver.IsEmpty()) {
      mDriverVendor = nsPrintfCString("mesa/%s", driDriver.get());
    } else {
      // Some other mesa configuration where we couldn't get enough info.
      NS_WARNING("Failed to detect Mesa driver being used!");
      CopyUTF16toUTF8(GfxDriverInfo::GetDriverVendor(DriverMesaUnknown),
                      mDriverVendor);
    }

    if (!mesaVendor.IsEmpty()) {
      mVendorId = mesaVendor;
    } else {
      NS_WARNING(
          "Failed to get Mesa vendor ID! GLX_MESA_query_renderer unsupported?");
    }

    if (!mesaDevice.IsEmpty()) {
      mDeviceId = mesaDevice;
    } else {
      NS_WARNING(
          "Failed to get Mesa device ID! GLX_MESA_query_renderer unsupported?");
    }
  } else if (glVendor.EqualsLiteral("NVIDIA Corporation")) {
    CopyUTF16toUTF8(GfxDriverInfo::GetDeviceVendor(VendorNVIDIA), mVendorId);
    // TODO: Use NV-CONTROL X11 extension to query Device ID and VRAM.
  } else if (glVendor.EqualsLiteral("ATI Technologies Inc.")) {
    CopyUTF16toUTF8(GfxDriverInfo::GetDeviceVendor(VendorATI), mVendorId);
    // TODO: Look into ways to find the device ID on FGLRX.
  } else {
    NS_WARNING("Failed to detect GL vendor!");
  }

  // Fallback to GL_VENDOR and GL_RENDERER.
  if (mVendorId.IsEmpty()) {
    mVendorId.Assign(glVendor.get());
  }
  if (mDeviceId.IsEmpty()) {
    mDeviceId.Assign(glRenderer.get());
  }

  mAdapterDescription.Assign(glRenderer);
  mIsWayland = !GDK_IS_X11_DISPLAY(gdk_display_get_default());

  AddCrashReportAnnotations();
}

const nsTArray<GfxDriverInfo>& GfxInfo::GetGfxDriverInfo() {
  if (!sDriverInfo->Length()) {
    // Mesa 10.0 provides the GLX_MESA_query_renderer extension, which allows us
    // to query device IDs backing a GL context for blacklisting.
    APPEND_TO_DRIVER_BLOCKLIST(
        OperatingSystem::Linux,
        (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorAll),
        (nsAString&)GfxDriverInfo::GetDriverVendor(DriverMesaAll),
        GfxDriverInfo::allDevices, GfxDriverInfo::allFeatures,
        nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION, DRIVER_LESS_THAN,
        V(10, 0, 0, 0), "FEATURE_FAILURE_OLD_MESA", "Mesa 10.0");

    // NVIDIA baseline (ported from old blocklist)
    APPEND_TO_DRIVER_BLOCKLIST(
        OperatingSystem::Linux,
        (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorNVIDIA),
        (nsAString&)GfxDriverInfo::GetDriverVendor(DriverVendorAll),
        GfxDriverInfo::allDevices, GfxDriverInfo::allFeatures,
        nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION, DRIVER_LESS_THAN,
        V(257, 21, 0, 0), "FEATURE_FAILURE_OLD_NVIDIA", "NVIDIA 257.21");

    // fglrx baseline (chosen arbitrarily as 2013-07-22 release).
    APPEND_TO_DRIVER_BLOCKLIST(
        OperatingSystem::Linux,
        (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorATI),
        (nsAString&)GfxDriverInfo::GetDriverVendor(DriverVendorAll),
        GfxDriverInfo::allDevices, GfxDriverInfo::allFeatures,
        nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION, DRIVER_LESS_THAN,
        V(13, 15, 100, 1), "FEATURE_FAILURE_OLD_FGLRX", "fglrx 13.15.100.1");

    ////////////////////////////////////
    // FEATURE_WEBRENDER

    // Intel Mesa baseline, chosen arbitrarily.
    APPEND_TO_DRIVER_BLOCKLIST(
        OperatingSystem::Linux,
        (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorIntel),
        (nsAString&)GfxDriverInfo::GetDriverVendor(DriverVendorAll),
        GfxDriverInfo::allDevices, nsIGfxInfo::FEATURE_WEBRENDER,
        nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION, DRIVER_LESS_THAN,
        V(18, 0, 0, 0), "FEATURE_FAILURE_WEBRENDER_OLD_MESA", "Mesa 18.0.0.0");

    // Disable on all NVIDIA devices for now.
    APPEND_TO_DRIVER_BLOCKLIST(
        OperatingSystem::Linux,
        (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorNVIDIA),
        (nsAString&)GfxDriverInfo::GetDriverVendor(DriverVendorAll),
        GfxDriverInfo::allDevices, nsIGfxInfo::FEATURE_WEBRENDER,
        nsIGfxInfo::FEATURE_BLOCKED_DEVICE, DRIVER_COMPARISON_IGNORED,
        V(0, 0, 0, 0), "FEATURE_FAILURE_WEBRENDER_NO_LINUX_NVIDIA", "");

    // Disable on all ATI devices for now.
    APPEND_TO_DRIVER_BLOCKLIST(
        OperatingSystem::Linux,
        (nsAString&)GfxDriverInfo::GetDeviceVendor(VendorATI),
        (nsAString&)GfxDriverInfo::GetDriverVendor(DriverVendorAll),
        GfxDriverInfo::allDevices, nsIGfxInfo::FEATURE_WEBRENDER,
        nsIGfxInfo::FEATURE_BLOCKED_DEVICE, DRIVER_COMPARISON_IGNORED,
        V(0, 0, 0, 0), "FEATURE_FAILURE_WEBRENDER_NO_LINUX_ATI", "");
  }
  return *sDriverInfo;
}

bool GfxInfo::DoesDriverVendorMatch(const nsAString& aBlocklistVendor,
                                    const nsAString& aDriverVendor) {
  if (mIsMesa &&
      aBlocklistVendor.Equals(GfxDriverInfo::GetDriverVendor(DriverMesaAll),
                              nsCaseInsensitiveStringComparator())) {
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

  if (mGLMajorVersion == 0) {
    // If we failed to get a GL version, glxtest failed.
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

  // Blacklist software GL implementations from using layers acceleration.
  // On the test infrastructure, we'll force-enable layers acceleration.
  if (aFeature == nsIGfxInfo::FEATURE_OPENGL_LAYERS && !mIsAccelerated &&
      !PR_GetEnv("MOZ_LAYERS_ALLOW_SOFTWARE_GL")) {
    *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
    aFailureId = "FEATURE_FAILURE_SOFTWARE_GL";
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

NS_IMETHODIMP
GfxInfo::GetCleartypeParameters(nsAString& aCleartypeParams) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetWindowProtocol(nsAString& aWindowProtocol) {
  if (mIsWayland) {
    aWindowProtocol.AssignLiteral("wayland");
    return NS_OK;
  }

  aWindowProtocol.AssignLiteral("x11");
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
GfxInfo::GetAdapterRAM(nsAString& aAdapterRAM) {
  GetData();
  CopyUTF8toUTF16(mAdapterRAM, aAdapterRAM);
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterRAM2(nsAString& aAdapterRAM) { return NS_ERROR_FAILURE; }

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
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID(nsAString& aAdapterDeviceID) {
  GetData();
  CopyUTF8toUTF16(mDeviceId, aAdapterDeviceID);
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID2(nsAString& aAdapterDeviceID) {
  return NS_ERROR_FAILURE;
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
GfxInfo::GetIsGPU2Active(bool* aIsGPU2Active) { return NS_ERROR_FAILURE; }

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

}  // end namespace widget
}  // end namespace mozilla
