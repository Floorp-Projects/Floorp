/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxSettings_h
#define mozilla_SandboxSettings_h

namespace mozilla {

// Return the current sandbox level. This is the
// "security.sandbox.content.level" preference, but rounded up to the current
// minimum allowed level.
int GetEffectiveContentSandboxLevel();

// Returns whether or not the currently running build is a development build -
// where development build means "the files in the .app are symlinks to the src
// directory". This check is implemented by looking for omni.ja in
// .app/Contents/Resources/.
bool IsDevelopmentBuild();

}
#endif // mozilla_SandboxPolicies_h
