/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

async function doOne(resource, id) {
  let l10n = new Localization([resource], true);
  let value = await l10n.formatValue(id);
  Assert.ok(value, `${id} from ${resource} is not null: ${value}`);

  let exitCode = await do_backgroundtask("localization", {
    extraArgs: [resource, id, value],
  });
  Assert.equal(0, exitCode);
}

add_task(async function test_localization() {
  // Verify that the `l10n-registry` category is processed and that localization
  // works as expected in background tasks.  We can use any FTL resource and
  // string identifier here as long as the value is short and can be passed as a
  // command line argument safely (i.e., is ASCII).

  // One from toolkit/.
  await doOne("toolkit/global/commonDialog.ftl", "common-dialog-title-system");
  if (AppConstants.MOZ_APP_NAME == "thunderbird") {
    // And one from messenger/.
    await doOne("messenger/messenger.ftl", "no-reply-title");
  } else {
    // And one from browser/.
    await doOne("browser/pageInfo.ftl", "not-set-date");
  }
});
