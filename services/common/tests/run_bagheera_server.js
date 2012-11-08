/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file runs a stub Bagheera server.
 *
 * It is meant to be executed with an xpcshell.
 *
 * The Makefile in this directory contains a target to run it:
 *
 *   $ make bagheera-server
 */

Cu.import("resource://testing-common/services-common/bagheeraserver.js");

initTestLogging();

let server = new BagheeraServer();
server.allowAllNamespaces = true;
server.start(SERVER_PORT);
_("Bagheera server started on port " + SERVER_PORT);

// Launch the thread manager.
_do_main();

