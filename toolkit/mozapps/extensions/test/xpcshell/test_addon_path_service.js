/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var service = Components.classes["@mozilla.org/addon-path-service;1"].getService(Components.interfaces.amIAddonPathService);

function insert(path, value)
{
  service.insertPath("/test/" + path, value);
}

function find(path)
{
  return service.findAddonId("/test/" + path);
}

function run_test()
{
  insert("abc", "10");
  insert("def", "11");
  insert("axy", "12");
  insert("defghij", "13");
  insert("defghi", "14");

  do_check_eq(find("abc"), "10");
  do_check_eq(find("abc123"), "10");
  do_check_eq(find("def"), "11");
  do_check_eq(find("axy"), "12");
  do_check_eq(find("axy1"), "12");
  do_check_eq(find("defghij"), "13");
  do_check_eq(find("abd"), "");
  do_check_eq(find("x"), "");

  insert("file:///home/billm/mozilla/in4/objdir-ff-dbg/dist/bin/browser/extensions/%7B972ce4c6-7e08-4474-a285-3208198ce6fd%7D/", "{972ce4c6-7e08-4474-a285-3208198ce6fd}");
  insert("file:///home/billm/mozilla/addons/dl-helper-workspace/addon/", "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}");

  do_check_eq(find("file:///home/billm/mozilla/addons/dl-helper-workspace/addon/local/modules/medialist-manager.jsm"), "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}");
}
