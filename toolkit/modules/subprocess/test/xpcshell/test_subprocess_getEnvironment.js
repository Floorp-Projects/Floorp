"use strict";

add_task(async function test_getEnvironment() {
  Services.env.set("FOO", "BAR");

  let environment = Subprocess.getEnvironment();

  equal(environment.FOO, "BAR");
  equal(environment.PATH, Services.env.get("PATH"));

  Services.env.set("FOO", null);

  environment = Subprocess.getEnvironment();
  equal(environment.FOO || "", "");
});
