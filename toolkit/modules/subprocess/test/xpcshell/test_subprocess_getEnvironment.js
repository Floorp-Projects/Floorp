"use strict";

let env = Cc["@mozilla.org/process/environment;1"].getService(
  Ci.nsIEnvironment
);

add_task(async function test_getEnvironment() {
  env.set("FOO", "BAR");

  let environment = Subprocess.getEnvironment();

  equal(environment.FOO, "BAR");
  equal(environment.PATH, env.get("PATH"));

  env.set("FOO", null);

  environment = Subprocess.getEnvironment();
  equal(environment.FOO || "", "");
});
