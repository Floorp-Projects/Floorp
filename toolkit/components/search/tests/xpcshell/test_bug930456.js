/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test()
{
  if (isChild) {
    do_check_false("@mozilla.org/browser/search-service;1" in Cc);
  } else {
    do_check_true("@mozilla.org/browser/search-service;1" in Cc);
  }
}
