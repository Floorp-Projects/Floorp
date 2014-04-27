/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Ensure that each instance of the Dev Tools loader contains its own loader
 * instance, and also returns unique objects.  This ensures there is no sharing
 * in place between loaders.
 */
function run_test() {
  let loader1 = new DevToolsLoader();
  let loader2 = new DevToolsLoader();

  let color1 = loader1.require("devtools/css-color");
  let color2 = loader2.require("devtools/css-color");

  do_check_true(color1 !== color2);

  do_check_true(loader1._provider !== loader2._provider);
  do_check_true(loader1._provider.loader !== loader2._provider.loader);
}
