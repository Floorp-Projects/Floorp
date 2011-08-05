/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include "nsCRTGlue.h"
#include "prenv.h"

#include "GfxInfoX11.h"

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#endif

namespace mozilla {
namespace widget {

// these global variables will be set when firing the glxtest process
int glxtest_pipe = 0;
pid_t glxtest_pid = 0;

nsresult
GfxInfo::Init()
{
    mMajorVersion = 0;
    mMinorVersion = 0;
    mRevisionVersion = 0;
    mIsMesa = false;
    mIsNVIDIA = false;
    mIsFGLRX = false;
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

    // bytesread < 0 would mean that the above read() call failed. This should never happen.
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
    while(wait_for_glxtest_process) {
        wait_for_glxtest_process = false;
        if (waitpid(glxtest_pid, &glxtest_status, 0) == -1) {
            if (errno == EINTR)
                wait_for_glxtest_process = true;
            else
                waiting_for_glxtest_process_failed = true;
        }
    }

    bool exited_with_error_code = !waiting_for_glxtest_process_failed &&
                                  WIFEXITED(glxtest_status) && 
                                  WEXITSTATUS(glxtest_status) != EXIT_SUCCESS;
    bool received_signal = !waiting_for_glxtest_process_failed &&
                           WIFSIGNALED(glxtest_status);

    bool error = waiting_for_glxtest_process_failed || exited_with_error_code || received_signal;

    nsCString textureFromPixmap; 
    nsCString *stringToFill = nsnull;
    char *bufptr = buf;
    if (!error) {
        while(true) {
            char *line = NS_strtok("\n", &bufptr);
            if (!line)
                break;
            if (stringToFill) {
                stringToFill->Assign(line);
                stringToFill = nsnull;
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

    const char *spoofedVendor = PR_GetEnv("MOZ_GFX_SPOOF_GL_VENDOR");
    if (spoofedVendor)
        mVendor.Assign(spoofedVendor);
    const char *spoofedRenderer = PR_GetEnv("MOZ_GFX_SPOOF_GL_RENDERER");
    if (spoofedRenderer)
        mRenderer.Assign(spoofedRenderer);
    const char *spoofedVersion = PR_GetEnv("MOZ_GFX_SPOOF_GL_VERSION");
    if (spoofedVersion)
        mVersion.Assign(spoofedVersion);

    if (error ||
        mVendor.IsEmpty() ||
        mRenderer.IsEmpty() ||
        mVersion.IsEmpty())
    {
        mAdapterDescription.AppendLiteral("GLXtest process failed");
        if (waiting_for_glxtest_process_failed)
            mAdapterDescription.AppendLiteral(" (waitpid failed)");
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

    // determine driver type (vendor) and where in the version string
    // the actual driver version numbers should be expected to be found (whereToReadVersionNumbers)
    const char *whereToReadVersionNumbers = nsnull;
    const char *Mesa_in_version_string = strstr(mVersion.get(), "Mesa");
    if (Mesa_in_version_string) {
        mIsMesa = true;
        // with Mesa, the version string contains "Mesa major.minor" and that's all the version information we get:
        // there is no actual driver version info.
        whereToReadVersionNumbers = Mesa_in_version_string + strlen("Mesa");
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

    // read major.minor version numbers
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

nsresult
GfxInfo::GetFeatureStatusImpl(PRInt32 aFeature, PRInt32 *aStatus, nsAString & aSuggestedDriverVersion, GfxDriverInfo* aDriverInfo /* = nsnull */)
{
    GetData();
    *aStatus = nsIGfxInfo::FEATURE_NO_INFO;
    aSuggestedDriverVersion.SetIsVoid(PR_TRUE);

#ifdef MOZ_PLATFORM_MAEMO
    // on Maemo, the glxtest probe doesn't build, and we don't really need GfxInfo anyway
    return NS_OK;
#endif

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
    // We do the same thing on Windows XP, see in widget/src/windows/GfxInfo.cpp
    if (mIsNVIDIA &&
        !strcmp(mRenderer.get(), "GeForce 9400/PCI/SSE2") &&
        !strcmp(mVersion.get(), "3.2.0 NVIDIA 190.42"))
    {
        return NS_OK;
    }

    if (mIsMesa) {
        if (version(mMajorVersion, mMinorVersion, mRevisionVersion) < version(7,10,3)) {
            *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DRIVER_VERSION;
            aSuggestedDriverVersion.AssignLiteral("Mesa 7.10.3");
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
        }
    } else {
        // like on windows, let's block unknown vendors. Think of virtual machines.
        // Also, this case is hit whenever the GLXtest probe failed to get driver info or crashed.
        *aStatus = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
    }
  return NS_OK;
}


NS_IMETHODIMP
GfxInfo::GetD2DEnabled(PRBool *aEnabled)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetDWriteEnabled(PRBool *aEnabled)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAzureEnabled(PRBool *aEnabled)
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

/* readonly attribute DOMString adapterRAM; */
NS_IMETHODIMP
GfxInfo::GetAdapterRAM(nsAString & aAdapterRAM)
{
  aAdapterRAM.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriver; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver(nsAString & aAdapterDriver)
{
  aAdapterDriver.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverVersion; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion(nsAString & aAdapterDriverVersion)
{
  GetData();
  CopyASCIItoUTF16(mVersion, aAdapterDriverVersion);
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverDate; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate(nsAString & aAdapterDriverDate)
{
  aAdapterDriverDate.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute unsigned long adapterVendorID; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID(PRUint32 *aAdapterVendorID)
{
  *aAdapterVendorID = 0;
  return NS_OK;
}

/* readonly attribute unsigned long adapterDeviceID; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID(PRUint32 *aAdapterDeviceID)
{
  *aAdapterDeviceID = 0;
  return NS_OK;
}


} // end namespace widget
} // end namespace mozilla
