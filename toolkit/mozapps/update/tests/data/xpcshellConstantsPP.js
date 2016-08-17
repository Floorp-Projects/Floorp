/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Preprocessed constants used by xpcshell tests */

const INSTALL_LOCALE = "@AB_CD@";
const MOZ_APP_NAME = "@MOZ_APP_NAME@";
const BIN_SUFFIX = "@BIN_SUFFIX@";

// MOZ_APP_VENDOR is optional.
#ifdef MOZ_APP_VENDOR
const MOZ_APP_VENDOR = "@MOZ_APP_VENDOR@";
#else
const MOZ_APP_VENDOR = "";
#endif

// MOZ_APP_BASENAME is not optional for tests.
const MOZ_APP_BASENAME = "@MOZ_APP_BASENAME@";
const APP_BIN_SUFFIX = "@BIN_SUFFIX@";

const APP_INFO_NAME = "XPCShell";
const APP_INFO_VENDOR = "Mozilla";

#ifdef XP_WIN
const IS_WIN = true;
#else
const IS_WIN = false;
#endif

#ifdef XP_MACOSX
const IS_MACOSX = true;
#else
const IS_MACOSX = false;
#endif

#ifdef XP_UNIX
const IS_UNIX = true;
#else
const IS_UNIX = false;
#endif

#ifdef ANDROID
const IS_ANDROID = true;
#else
const IS_ANDROID = false;
#endif

#ifdef MOZ_WIDGET_GONK
const IS_TOOLKIT_GONK = true;
#else
const IS_TOOLKIT_GONK = false;
#endif

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
