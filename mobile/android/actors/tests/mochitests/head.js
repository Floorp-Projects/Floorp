/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Bug 1799977: Using workaround to test telemtry in plain mochitests
const GleanTest = new Proxy(
  {},
  {
    get(target, categoryName, receiver) {
      return new Proxy(
        {},
        {
          // eslint-disable-next-line no-shadow
          get(target, metricName, receiver) {
            return {
              // The only API we actually implement right now.
              async testGetValue() {
                return SpecialPowers.spawnChrome(
                  [categoryName, metricName],
                  // eslint-disable-next-line no-shadow
                  async (categoryName, metricName) => {
                    await Services.fog.testFlushAllChildren();
                    const window = this.browsingContext.topChromeWindow;
                    return window.Glean[categoryName][
                      metricName
                    ].testGetValue();
                  }
                );
              },
              async testGetValueLabel(label) {
                return SpecialPowers.spawnChrome(
                  [categoryName, metricName, label],
                  // eslint-disable-next-line no-shadow
                  async (categoryName, metricName, label) => {
                    await Services.fog.testFlushAllChildren();
                    const window = this.browsingContext.topChromeWindow;
                    return window.Glean[categoryName][metricName][
                      label
                    ].testGetValue();
                  }
                );
              },
            };
          },
        }
      );
    },
  }
);
