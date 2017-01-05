/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Initialization specific to Form Autofill mochitest-browser tests.
 */

/* import-globals-from loader.js */

"use strict";

// We cannot start initialization from "loader.js" like we do in the xpcshell
// and mochitest-chrome frameworks, thus we load the script here.
Services.scriptloader.loadSubScript(getRootDirectory(gTestPath) + "loader.js",
                                    this);

// The testing framework is fully initialized at this point, you can add
// mochitest-browser specific test initialization here.  If you need shared
// functions or initialization that are not specific to mochitest-browser,
// consider adding them to "head_common.js" in the parent folder instead.
