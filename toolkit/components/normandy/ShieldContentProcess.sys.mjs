/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Registers about: pages provided by Shield, and listens for a shutdown event
 * from the add-on before un-registering them.
 *
 * This file is loaded as a process script. It is executed once for each
 * process, including the parent one.
 */

import { AboutPages } from "resource://normandy-content/AboutPages.sys.mjs";

export function AboutStudies() {
  return AboutPages.aboutStudies;
}
