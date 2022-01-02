/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { jsesc } = ChromeUtils.import(
  "resource://gre/modules/third_party/jsesc/jsesc.js"
);

function run_test() {
  Assert.equal(jsesc("teééést", { lowercaseHex: true }), "te\\xe9\\xe9\\xe9st");
  Assert.equal(
    jsesc("teééést", { lowercaseHex: false }),
    "te\\xE9\\xE9\\xE9st"
  );
}
