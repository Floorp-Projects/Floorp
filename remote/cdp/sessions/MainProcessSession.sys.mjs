/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Session } from "chrome://remote/content/cdp/sessions/Session.sys.mjs";

/**
 * A session, dedicated to the main process target.
 * For some reason, it doesn't need any specific code and can share the base Session class
 * aside TabSession.
 */
export class MainProcessSession extends Session {}
