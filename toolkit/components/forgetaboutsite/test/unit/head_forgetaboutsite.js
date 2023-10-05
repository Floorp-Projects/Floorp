/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var profileDir = do_get_profile();

/**
 * Removes any files that could make our tests fail.
 */
async function cleanUp() {
  let files = ["places.sqlite", "cookies.sqlite"];

  for (let i = 0; i < files.length; i++) {
    let file = Services.dirsvc.get("ProfD", Ci.nsIFile);
    file.append(files[i]);
    if (file.exists()) {
      file.remove(false);
    }
  }

  await new Promise(resolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_PERMISSIONS,
      value => resolve()
    );
  });
}
cleanUp();
