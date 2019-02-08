/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#filter substitution

/* Preprocessed constants used by xpcshell tests */

// MOZ_APP_VENDOR is optional.
#ifdef MOZ_APP_VENDOR
const MOZ_APP_VENDOR = "@MOZ_APP_VENDOR@";
#else
const MOZ_APP_VENDOR = "";
#endif

// MOZ_APP_BASENAME is not optional for tests.
const MOZ_APP_BASENAME = "@MOZ_APP_BASENAME@";

#ifdef MOZ_VERIFY_MAR_SIGNATURE
const MOZ_VERIFY_MAR_SIGNATURE = true;
#else
const MOZ_VERIFY_MAR_SIGNATURE = false;
#endif

#ifdef DISABLE_UPDATER_AUTHENTICODE_CHECK
 const IS_AUTHENTICODE_CHECK_ENABLED = false;
#else
 const IS_AUTHENTICODE_CHECK_ENABLED = true;
#endif
