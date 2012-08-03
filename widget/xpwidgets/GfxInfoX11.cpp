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
#include "prenv.h"

#include "GfxInfoX11.h"

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#endif

namespace mozilla {
namespace widget {

#ifdef DEBUG
NS_IMPL_ISUPPORTS_INHERITED1(GfxInfo, GfxInfoBase, nsIGfxInfoDebug)
#endif

// these global variables will be set when firing the glxtest process
int glxtest_pipe = 0;
pid_t glxtest_pid = 0;

nsresult
GfxInfo::Init()
{
    mGLMajorVersion = 0;
    mMajorVersion = 0;
    mMinorVersion = 0;
    mRevisionVersion = 0;
    mIsMesa = false;
    mIsNVIDIA = false;
    mIsFGLRX = false;
    mIsNouveau = false;
    mIsIntel = false;
    mHasTextureFromPixmap = false;
    return GfxInfoBase::Init();
}

void
GfxInfo::GetData()
{
    // to understand this function, see bug 639842. We retrieve the OpenGL driver information in a
    // separate process to protect against bad drivers.

    // if glxtest_pipe == 0, that means that we already read the information
    if (!glxtest_pipe)
        return;

    enum { buf_size = 1024 };
    char buf[buf_size];
    ssize_t bytesread = read(glxtest_pipe,
                             &buf,
                             buf_size-1); // -1 because we'll append a zero
    close(glxtest_pipe);
    glxtest_pipe = 0;

    // bytesread < 0 would mean that the above read() call failed.
    // This should never happen. If it did, the outcome would be to blacklist anyway.
    if (bytesread < 0)
        bytesread = 0;

    // let buf be a zero-terminated string
    buf[bytesread] = 0;

    // Wait for the glxtest process to finish. This serves 2 purposes:
    // * avoid having a zombie glxtest process laying around
    // * get the glxtest process status info.
    int glxtest_status = 0;
    bool wait_for_glxtest_process = true;
    bool waiting_for_glxtest_process_failed = false;
    int waitpid_errno = 0;
    while(wait_for_glxtest_process) {
        wait_for_glxtest_process = false;
        if (waitpid(glxtest_pid, &glxtest_status, 0) == -1) {
            waitpid_errno = errno;
            if (waitpid_errno == EINTR) {
                wait_for_glxtest_process = true;
            } else {
                // Bug 718629
                // ECHILD happens when the glxtest process got reaped got reaped after a PR_CreateProcess
                // as per bug 227246. This shouldn't matter, as we still seem to get the data
                // from the pipe, and if we didn't, the outcome would be to blacklist anyway.
                waiting_for_glxtest_process_failed = (waitpid_errno != ECHILD);
            }
        }
    }

    bool exited_with_error_code = !waiting_for_glxtest_process_failed &&
                                  WIFEXITED(glxtest_status) && 
                                  WEXITSTATUS(glxtest_status) != EXIT_SUCCESS;
    bool received_signal = !waiting_for_glxtest_process_failed &&
                           WIFSIGNALED(glxtest_status);

    bool error = waiting_for_glxtest_process_failed || exited_with_error_code || received_signal;

    nsCString textureFromPixmap; 
    nsCString *stringToFill = nullptr;
    char *bufptr = buf;
    if (!error) {
        while(true) {
            char *line = NS_strtok("\n", &bufptr);
            if (!line)
                break;
            if (stringToFill) {
                stringToFill->Assign(line);
                stringToFill = nullptr;
            }
            else if(!strcmp(line, "VENDOR"))
                stringToFill = &mVendor;
            else if(!strcmp(line, "RENDERER"))
                stringToFill = &mRenderer;
            else if(!strcmp(line, "VERSION"))
                stringToFill = &mVersion;
            else if(!strcmp(line, "TFP"))
                stringToFill = &textureFromPixmap;
        }
    }

    if (!strcmp(textureFromPixmap.get(), "TRUE"))
        mHasTextureFromPixmap = true;

    // only useful for Linux kernel version check for FGLRX driver.
    // assumes X client == X server, which is sad.
    struct utsname unameobj;
    if (!uname(&unameobj))
    {
      mOS.Assign(unameobj.sysname);
      mOSRelease.Assign(unameobj.release);
    }

    const char *spoofedVendor = PR_GetEnv("MOZ_GFX_SPOOF_GL_VENDOR");
    if (spoofedVendor)
        mVendor.Assign(spoofedVendor);
    const char *spoofedRenderer = PR_GetEnv("MOZ_GFX_SPOOF_GL_RENDERER");
    if (spoofedRenderer)
        mRenderer.Assign(spoofedRenderer);
    const char *spoofedVersion = PR_GetEnv("MOZ_GFX_SPOOF_GL_VERSION");
    if (spoofedVersion)
        mVersion.Assign(spoofedVersion);
    const char *spoofedOS = PR_GetEnv("MOZ_GFX_SPOOF_OS");
    if (spoofedOS)
        mOS.Assign(spoofedOS);
    const char *spoofedOSRelease = PR_GetEnv("MOZ_GFX_SPOOF_OS_RELEASE");
    if (spoofedOSRelease)
        mOSRelease.Assign(spoofedOSRelease);

    if (error ||
        mVendor.IsEmpty() ||
        mRenderer.IsEmpty() ||
        mVersion.IsEmpty() ||
        mOS.IsEmpty() ||
        mOSRelease.IsEmpty())
    {
        mAdapterDescription.AppendLiteral("GLXtest process failed");
        if (waiting_for_glxtest_process_failed)
            mAdapterDescription.AppendPrintf(" (waitpid failed with errno=%d for pid %d)", waitpid_errno, glxtest_pid);
        if (exited_with_error_code)
            mAdapterDescription.AppendPrintf(" (exited with status %d)", WEXITSTATUS(glxtest_status));
        if (received_signal)
            mAdapterDescription.AppendPrintf(" (received signal %d)", WTERMSIG(glxtest_status));
        if (bytesread) {
            mAdapterDescription.AppendLiteral(": ");
            mAdapterDescription.Append(nsDependentCString(buf));
            mAdapterDescription.AppendLiteral("\n");
        }
#ifdef MOZ_CRASHREPORTER
        CrashReporter::AppendAppNotesToCrashReport(mAdapterDescription);
#endif
        return;
    }

    mAdapterDescription.Append(mVendor);
    mAdapterDescription.AppendLiteral(" -- ");
    mAdapterDescription.Append(mRenderer);

    nsCAutoString note;
    note.Append("OpenGL: ");
    note.Append(mAdapterDescription);
    note.Append(" -- ");
    note.Append(mVersion);
    if (mHasTextureFromPixmap)
        note.Append(" -- texture_from_pixmap");
    note.Append("\n");
#ifdef MOZ_CRASHREPORTER
    CrashReporter::AppendAppNotesToCrashReport(note);
#endif

    // determine the major OpenGL version. That's the first integer in the version string.
    mGLMajorVersion = strtol(mVersion.get(), 0, 10);

    // determine driver type (vendor) and where in the version string
    // the actual driver version numbers should be expected to be found (whereToReadVersionNumbers)
    const char *whereToReadVersionNumbers = nullptr;
    const char *Mesa_in_version_string = strstr(mVersion.get(), "Mesa");
    if (Mesa_in_version_string) {
        mIsMesa = true;
        // with Mesa, the version string contains "Mesa major.minor" and that's all the version information we get:
        // there is no actual driver version info.
        whereToReadVersionNumbers = Mesa_in_version_string + strlen("Mesa");
        if (strcasestr(mVendor.get(), "nouveau"))
            mIsNouveau = true;
        if (strcasestr(mRenderer.get(), "intel")) // yes, intel is in the renderer string
            mIsIntel = true;
    } else if (strstr(mVendor.get(), "NVIDIA Corporation")) {
        mIsNVIDIA = true;
        // with the NVIDIA driver, the version string contains "NVIDIA major.minor"
        // note that here the vendor and version strings behave differently, that's why we don't put this above
        // alongside Mesa_in_version_string.
        const char *NVIDIA_in_version_string = strstr(mVersion.get(), "NVIDIA");
        if (NVIDIA_in_version_string)
            whereToReadVersionNumbers = NVIDIA_in_version_string + strlen("NVIDIA");
    } else if (strstr(mVendor.get(), "ATI Technologies Inc")) {
        mIsFGLRX = true;
        // with the FGLRX driver, the version string only gives a OpenGL version :/ so let's return that.
        // that can at least give a rough idea of how old the driver is.
        whereToReadVersionNumbers = mVersion.get();
    }

    // read major.minor version numbers of the driver (not to be confused with the OpenGL version)
    if (whereToReadVersionNumbers) {
        // copy into writable buffer, for tokenization
        strncpy(buf, whereToReadVersionNumbers, buf_size);
        bufptr = buf;

        // now try to read major.minor version numbers. In case of failure, gracefully exit: these numbers have
        // been initialized as 0 anyways
        char *token = NS_strtok(".", &bufptr);
        if (token) {
            mMajorVersion = strtol(token, 0, 10);
            token = NS_strtok(".", &bufptr);
            if (token) {
                mMinorVersion = strtol(token, 0, 10);
                token = NS_strtok(".", &bufptr);
                if (token)
                    mRevisionVersion = strtol(token, 0, 10);
            }
        }
    }
}

static inline PRUint64 version(PRUint32 major, PRUint32 minor, PRUint32 revision = 0)
{
    return (PRUint64(major) << 32) + (PRUint64(minor) << 16) + PRUint64(revision);
}

const nsTArray<GfxDriverInfo>&
GfxInfo::GetGfxDriverInfo()
{
  // Nothing here yet.
  //if (!mDriverInfo->Length()) {
  //
  //}
  return *mDriverInfo;
}

nsresult
GfxInfo::GetFeatureStatusImpl(PRInt32 aFeature, 
                              PRInt32 *aStatus, 
                              nsAString & aSuggestedDriverVersion, 
                              const nsTArray<GfxDriverInfo>& aDriverInfo, 
                              OperatingSystem* aOS /* = nullptr */)

{
  GetData();

  NS_ENSURE_ARG_POINTER(aStatus);
  *aStatus = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;
  aSuggestedDriverVersion.SetIsVoid(true);
  OperatingSystem os = DRIVER_OS_LINUX;
  if (aOS)
    *aOS = os;

  if (mGLMajorVersion == 1) {
    // We're on OpenGL 1. In most cases that indicates really old hardware.
    // We better block them, rather than rely on them to fail gracefully, because they don't!
    // see bug 696636
    *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
    return NS_OK;
  }

#ifdef MOZ_PLATFORM_MAEMO
  *aStatus = nsIGfxInfo::FEATURE_NO_INFO;
  // on Maemo, the glxtest probe doesn't build, and we don't really need GfxInfo anyway
  return NS_OK;
#endif

  // Don't evaluate any special cases if we're checking the downloaded blocklist.
  if (!aDriverInfo.Length()) {
    // Only check features relevant to Linux.
    if (aFeature == nsIGfxInfo::FEATURE_OPENGL_LAYERS ||
        aFeature == nsIGfxInfo::FEATURE_WEBGL_OPENGL ||
        aFeature == nsIGfxInfo::FEATURE_WEBGL_MSAA) {

      // Disable OpenGL layers when we don't have texture_from_pixmap because it regresses performance. 
      if (aFeature == nsIGfxInfo::FEATURE_OPENGL_LAYERS && !mHasTextureFromPixmap) {
        *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION;
        aSuggestedDriverVersion.AssignLiteral("<Anything with EXT_texture_from_pixmap support>");
        return NS_OK;
      }

      // whitelist the linux test slaves' current configuration.
      // this is necessary as they're still using the slightly outdated 190.42 driver.
      // this isn't a huge risk, as at least this is the exact setting in which we do continuous testing,
      // and this only affects GeForce 9400 cards on linux on this precise driver version, which is very few users.
      // We do the same thing on Windows XP, see in widget/windows/GfxInfo.cpp
      if (mIsNVIDIA &&
          !strcmp(mRenderer.get(), "GeForce 9400/PCI/SSE2") &&
          !strcmp(mVersion.get(), "3.2.0 NVIDIA 190.42"))
      {
        *aStatus = nsIGfxInfo::FEATURE_NO_INFO;
        return NS_OK;
      }

      if (mIsMesa) {
        if (mIsNouveau && version(mMajorVersion, mMinorVersion) < version(8,0)) {
          *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION;
          aSuggestedDriverVersion.AssignLiteral("Mesa 8.0");
        } else if (version(mMajorVersion, mMinorVersion, mRevisionVersion) < version(7,10,3)) {
          *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION;
          aSuggestedDriverVersion.AssignLiteral("Mesa 7.10.3");
        }
        if (aFeature == nsIGfxInfo::FEATURE_WEBGL_MSAA)
        {
          if (mIsIntel && version(mMajorVersion, mMinorVersion) < version(8,1))
            *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION;
            aSuggestedDriverVersion.AssignLiteral("Mesa 8.1");
        }

      } else if (mIsNVIDIA) {
        if (version(mMajorVersion, mMinorVersion, mRevisionVersion) < version(257,21)) {
          *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION;
          aSuggestedDriverVersion.AssignLiteral("NVIDIA 257.21");
        }
      } else if (mIsFGLRX) {
        // FGLRX does not report a driver version number, so we have the OpenGL version instead.
        // by requiring OpenGL 3, we effectively require recent drivers.
        if (version(mMajorVersion, mMinorVersion, mRevisionVersion) < version(3, 0)) {
          *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION;
          aSuggestedDriverVersion.AssignLiteral("<Something recent>");
        }
        // Bug 724640: FGLRX + Linux 2.6.32 is a crashy combo
        bool unknownOS = mOS.IsEmpty() || mOSRelease.IsEmpty();
        bool badOS = mOS.Find("Linux", true) != -1 &&
                     mOSRelease.Find("2.6.32") != -1;
        if (unknownOS || badOS) {
          *aStatus = nsIGfxInfo::FEATURE_BLOCKED_OS_VERSION;
        }
      } else {
        // like on windows, let's block unknown vendors. Think of virtual machines.
        // Also, this case is hit whenever the GLXtest probe failed to get driver info or crashed.
        *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
      }
    }
  }

  return GfxInfoBase::GetFeatureStatusImpl(aFeature, aStatus, aSuggestedDriverVersion, aDriverInfo, &os);
}


NS_IMETHODIMP
GfxInfo::GetD2DEnabled(bool *aEnabled)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetDWriteEnabled(bool *aEnabled)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString DWriteVersion; */
NS_IMETHODIMP
GfxInfo::GetDWriteVersion(nsAString & aDwriteVersion)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString cleartypeParameters; */
NS_IMETHODIMP
GfxInfo::GetCleartypeParameters(nsAString & aCleartypeParams)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDescription; */
NS_IMETHODIMP
GfxInfo::GetAdapterDescription(nsAString & aAdapterDescription)
{
  GetData();
  AppendASCIItoUTF16(mAdapterDescription, aAdapterDescription);
  return NS_OK;
}

/* readonly attribute DOMString adapterDescription2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDescription2(nsAString & aAdapterDescription)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterRAM; */
NS_IMETHODIMP
GfxInfo::GetAdapterRAM(nsAString & aAdapterRAM)
{
  aAdapterRAM.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterRAM2; */
NS_IMETHODIMP
GfxInfo::GetAdapterRAM2(nsAString & aAdapterRAM)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDriver; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver(nsAString & aAdapterDriver)
{
  aAdapterDriver.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriver2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver2(nsAString & aAdapterDriver)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDriverVersion; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion(nsAString & aAdapterDriverVersion)
{
  GetData();
  CopyASCIItoUTF16(mVersion, aAdapterDriverVersion);
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverVersion2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion2(nsAString & aAdapterDriverVersion)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDriverDate; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate(nsAString & aAdapterDriverDate)
{
  aAdapterDriverDate.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverDate2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate2(nsAString & aAdapterDriverDate)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterVendorID; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID(nsAString & aAdapterVendorID)
{
  GetData();
  CopyUTF8toUTF16(mVendor, aAdapterVendorID);
  return NS_OK;
}

/* readonly attribute DOMString adapterVendorID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID2(nsAString & aAdapterVendorID)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDeviceID; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID(nsAString & aAdapterDeviceID)
{
  GetData();
  CopyUTF8toUTF16(mRenderer, aAdapterDeviceID);
  return NS_OK;
}

/* readonly attribute DOMString adapterDeviceID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID2(nsAString & aAdapterDeviceID)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute boolean isGPU2Active; */
NS_IMETHODIMP
GfxInfo::GetIsGPU2Active(bool* aIsGPU2Active)
{
  return NS_ERROR_FAILURE;
}

#ifdef DEBUG

// Implement nsIGfxInfoDebug
// We don't support spoofing anything on Linux

/* void spoofVendorID (in DOMString aVendorID); */
NS_IMETHODIMP GfxInfo::SpoofVendorID(const nsAString & aVendorID)
{
  CopyUTF16toUTF8(aVendorID, mVendor);
  return NS_OK;
}

/* void spoofDeviceID (in unsigned long aDeviceID); */
NS_IMETHODIMP GfxInfo::SpoofDeviceID(const nsAString & aDeviceID)
{
  CopyUTF16toUTF8(aDeviceID, mRenderer);
  return NS_OK;
}

/* void spoofDriverVersion (in DOMString aDriverVersion); */
NS_IMETHODIMP GfxInfo::SpoofDriverVersion(const nsAString & aDriverVersion)
{
  CopyUTF16toUTF8(aDriverVersion, mVersion);
  return NS_OK;
}

/* void spoofOSVersion (in unsigned long aVersion); */
NS_IMETHODIMP GfxInfo::SpoofOSVersion(PRUint32 aVersion)
{
  // We don't support OS versioning on Linux. There's just "Linux".
  return NS_OK;
}

#endif

} // end namespace widget
} // end namespace mozilla
