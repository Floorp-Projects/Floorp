/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ContentProcessSession } = ChromeUtils.import(
  "chrome://remote/content/sessions/ContentProcessSession.jsm"
);

/* global content, docShell */
new ContentProcessSession(this, docShell.browsingContext, content, docShell);
