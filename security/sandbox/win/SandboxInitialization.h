/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_sandboxing_SandboxInitialization_h
#define mozilla_sandboxing_SandboxInitialization_h

namespace sandbox {
class BrokerServices;
class TargetServices;
}

// Things that use this file will probably want access to the IsSandboxedProcess
// function defined in one of the Chromium sandbox cc files.
extern "C" bool IsSandboxedProcess();

namespace mozilla {
// Note the Chromium code just uses a bare sandbox namespace, which makes using
// sandbox for our namespace painful.
namespace sandboxing {

class PermissionsService;

/**
 * Initializes (if required) and returns the Chromium sandbox TargetServices.
 *
 * @return the TargetServices or null if the creation or initialization failed.
 */
sandbox::TargetServices* GetInitializedTargetServices();

/**
 * Lowers the permissions on the process sandbox.
 * Provided because the GMP sandbox needs to be lowered from the executable.
 */
void LowerSandbox();

/**
 * Initializes (if required) and returns the Chromium sandbox BrokerServices.
 *
 * @return the BrokerServices or null if the creation or initialization failed.
 */
sandbox::BrokerServices* GetInitializedBrokerServices();

/**
 * Checks to see if we are running from a network drive and sets a flag in
 * sandbox code to disable the use of restricting SIDs.
 * Using restricting SIDs blocks access to network drives and prevents DLL
 * loading during initial sandboxed child process start-up.
 */
void NetworkDriveCheck();

PermissionsService* GetPermissionsService();

} // sandboxing
} // mozilla

#endif // mozilla_sandboxing_SandboxInitialization_h
