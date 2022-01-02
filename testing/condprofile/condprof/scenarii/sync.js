/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { triggerSync } = ChromeUtils.import(
  "resource://gre/modules/services-automation/ServicesAutomation.jsm"
);

let resolve = arguments[3]; // eslint-disable-line
try {
  triggerSync(arguments[0], arguments[1], arguments[2]).then(res => {  // eslint-disable-line
    resolve(res); // eslint-disable-line
  });
} catch (error) {
  let res = { logs: {}, result: 1, result_message: error.toString() };
  resolve(res);
}
