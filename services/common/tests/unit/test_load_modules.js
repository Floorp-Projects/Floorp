/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const modules = [
  "async.js",
  "log4moz.js",
  "preferences.js",
  "rest.js",
  "stringbundle.js",
  "tokenserverclient.js",
  "utils.js",
];

function run_test() {
  for each (let m in modules) {
    let resource = "resource://services-common/" + m;
    Components.utils.import(resource, {});
  }
}
