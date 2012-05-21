/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

PR_BEGIN_EXTERN_C

PR_EXPORT(int)  Testy_LogInit(const char* fileName);
PR_EXPORT(void) Testy_LogShutdown();

PR_EXPORT(void) Testy_LogStart(const char* name);
PR_EXPORT(void) Testy_LogComment(const char* name, const char* comment);
PR_EXPORT(void) Testy_LogEnd(const char* name, bool passed);

PR_END_EXTERN_C
