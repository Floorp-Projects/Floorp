/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef MOZ_BUILDID_SECTION_NAME

#if defined(XP_DARWIN)
#  define MOZ_BUILDID_SECTION_NAME "__DATA,mozbuildid"
#elif defined(XP_WIN)
#  define MOZ_BUILDID_SECTION_NAME "mozbldid"
#else
#  define MOZ_BUILDID_SECTION_NAME ".note.moz.toolkit-build-id"
#endif

#endif  // MOZ_BUILDID_SECTION_NAME
