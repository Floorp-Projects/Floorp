/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SIGNAL = "public-suffix-list-updated";

add_task(async () => {
  info("Before fake dafsa reload.");

  let suffix = Services.eTLD.getPublicSuffixFromHost("website.xpcshelltest");
  Assert.equal(
    suffix,
    "xpcshelltest",
    "Fake Suffix does not exist in current PSL."
  );
});

add_task(async () => {
  info("After fake dafsa reload.");

  // reload the PSL with fake data containing .xpcshelltest
  const fakeDafsaFile = do_get_file("data/fake_remote_dafsa.bin");
  Services.obs.notifyObservers(fakeDafsaFile, SIGNAL, fakeDafsaFile.path);

  let suffix = Services.eTLD.getPublicSuffixFromHost("website.xpcshelltest");
  Assert.equal(
    suffix,
    "website.xpcshelltest",
    "Fake Suffix now exists in PSL after DAFSA reload."
  );
});
