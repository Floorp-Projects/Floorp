/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This source file is used to build "untrusted-startup-test-dll.dll" on
 * Windows. During xpcshell tests, it's loaded early enough to be detected as a
 * startup module, and hard-coded to be reported in the untrusted modules
 * telemetry ping, which allows for testing some code paths.
 */

void nothing() {}
