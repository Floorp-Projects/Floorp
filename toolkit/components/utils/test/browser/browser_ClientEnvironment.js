/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * This can't be an xpcshell-test because, according to
 * `toolkit/modules/Services.jsm`, "Not all applications implement
 * nsIXULAppInfo (e.g. xpcshell doesn't)."
 */
const { ClientEnvironmentBase } = ChromeUtils.import(
  "resource://gre/modules/components-utils/ClientEnvironment.jsm"
);

add_task(function testBuildId() {
  ok(
    ClientEnvironmentBase.appinfo !== undefined,
    "appinfo should be available in the context"
  );
  ok(
    typeof ClientEnvironmentBase.appinfo === "object",
    "appinfo should be an object"
  );
  ok(
    typeof ClientEnvironmentBase.appinfo.appBuildID === "string",
    "buildId should be a string"
  );
});
