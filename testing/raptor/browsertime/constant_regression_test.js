/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

module.exports = async function (context, commands) {
  context.log.info("Starting constant value regression test");
  await commands.measure.start("regression-test");
  await commands.measure.stop();
  await commands.measure.addObject({
    data: { metric: context.options.browsertime.constant_value },
  });
};
