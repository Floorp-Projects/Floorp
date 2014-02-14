/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __SECURITY_SANDBOX_SANDBOXTARGET_H__
#define __SECURITY_SANDBOX_SANDBOXTARGET_H__

#ifdef TARGET_SANDBOX_EXPORTS
#define TARGET_SANDBOX_EXPORT __declspec(dllexport)
#else
#define TARGET_SANDBOX_EXPORT __declspec(dllimport)
#endif
namespace mozilla {


class TARGET_SANDBOX_EXPORT SandboxTarget
{
public:
  typedef void (*StartSandboxPtr)();

  /**
   * Obtains a pointer to the singleton instance
   */
  static SandboxTarget* Instance()
  {
    static SandboxTarget sb;
    return &sb;
  }

  /**
   * Called by the application that will lower the sandbox token
   *
   * @param aStartSandboxCallback A callback function which will lower privs
   */
  void SetStartSandboxCallback(StartSandboxPtr aStartSandboxCallback)
  {
    mStartSandboxCallback = aStartSandboxCallback;
  }

  /**
   * Called by the library that wants to start the sandbox, which in turn
   * calls into the previously set StartSandboxCallback.
   */
  void StartSandbox()
  {
    if (mStartSandboxCallback) {
      mStartSandboxCallback();
    }
  }

protected:
  SandboxTarget() :
    mStartSandboxCallback(nullptr)
  {
  }

  StartSandboxPtr mStartSandboxCallback;
};


} // mozilla

#endif
