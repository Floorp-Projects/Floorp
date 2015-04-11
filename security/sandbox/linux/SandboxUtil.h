/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxUtil_h
#define mozilla_SandboxUtil_h

namespace mozilla {

bool IsSingleThreaded();

// Unshare the user namespace, and set up id mappings so that the
// process's subjective uid and gid are unchanged.  This will always
// fail if the process is multithreaded.
bool UnshareUserNamespace();

} // namespace mozilla

#endif // mozilla_SandboxUtil_h
