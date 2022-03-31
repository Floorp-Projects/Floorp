/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * This test exists solely to ensure that channel-prefs.js is not changed.
 * If it does get changed, it will cause a variation of Bug 1431342.
 * To summarize, our updater doesn't update that file. But, on macOS, it is
 * still used to compute the application's signature. This means that if Firefox
 * updates and that file has been changed, the signature no will no longer
 * validate.
 */

const expectedChannelPrefsContents = `/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
//
// This pref is in its own file for complex reasons. See the comment in
// browser/app/Makefile.in, bug 756325, and bug 1431342 for details. Do not add
// other prefs to this file.

pref("app.update.channel", "${UpdateUtils.UpdateChannel}");
`;

async function run_test() {
  let channelPrefsFile = Services.dirsvc.get("GreD", Ci.nsIFile);
  channelPrefsFile.append("defaults");
  channelPrefsFile.append("pref");
  channelPrefsFile.append("channel-prefs.js");

  const contents = await IOUtils.readUTF8(channelPrefsFile.path);
  Assert.equal(
    contents,
    expectedChannelPrefsContents,
    "Channel Prefs file should should not change"
  );
}
