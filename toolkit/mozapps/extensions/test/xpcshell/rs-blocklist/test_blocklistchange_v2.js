/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// useMLBF=true doesn't support soft blocks, regexps or version ranges.
// Flip the useMLBF preference to make sure that the test_blocklistchange.js
// test works with and without this pref (blocklist v2 and blocklist v3).
enable_blocklist_v2_instead_of_useMLBF();

Services.scriptloader.loadSubScript(
  Services.io.newFileURI(do_get_file("test_blocklistchange.js")).spec,
  this
);
