/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

Components.utils.import("resource://gre/modules/PropertyListUtils.jsm");

function checkValue(aPropertyListObject, aType, aValue) {
  Assert.equal(PropertyListUtils.getObjectType(aPropertyListObject), aType);
  if (aValue !== undefined) {
    // Perform strict equality checks until Bug 714467 is fixed.
    let strictEqualityCheck = function(a, b) {
      Assert.equal(typeof(a), typeof(b));
      Assert.equal(a, b);
    };

    if (typeof(aPropertyListObject) == "object")
      strictEqualityCheck(aPropertyListObject.valueOf(), aValue.valueOf());
    else
      strictEqualityCheck(aPropertyListObject, aValue);
  }
}

function checkLazyGetterValue(aObject, aPropertyName, aType, aValue) {
  let descriptor = Object.getOwnPropertyDescriptor(aObject, aPropertyName);
  Assert.equal(typeof(descriptor.get), "function");
  Assert.equal(typeof(descriptor.value), "undefined");
  checkValue(aObject[aPropertyName], aType, aValue);
  descriptor = Object.getOwnPropertyDescriptor(aObject, aPropertyName);
  Assert.equal(typeof(descriptor.get), "undefined");
  Assert.notEqual(typeof(descriptor.value), "undefined");
}

function checkMainPropertyList(aPropertyListRoot) {
  const PRIMITIVE = PropertyListUtils.TYPE_PRIMITIVE;

  checkValue(aPropertyListRoot, PropertyListUtils.TYPE_DICTIONARY);

  // Check .has()
  Assert.ok(aPropertyListRoot.has("Boolean"));
  Assert.ok(!aPropertyListRoot.has("Nonexistent"));

  checkValue(aPropertyListRoot.get("Boolean"), PRIMITIVE, false);

  let array = aPropertyListRoot.get("Array");
  checkValue(array, PropertyListUtils.TYPE_ARRAY);
  Assert.equal(array.length, 8);

  // Test both long and short values, since binary property lists store
  // long values a little bit differently (see readDataLengthAndOffset).

  // Short ASCII string
  checkLazyGetterValue(array, 0, PRIMITIVE, "abc");
  // Long ASCII string
  checkLazyGetterValue(array, 1, PRIMITIVE, new Array(1001).join("a"));
  // Short unicode string
  checkLazyGetterValue(array, 2, PRIMITIVE, "\u05D0\u05D0\u05D0");
  // Long unicode string
  checkLazyGetterValue(array, 3, PRIMITIVE, new Array(1001).join("\u05D0"));
  // Unicode surrogate pair
  checkLazyGetterValue(array, 4, PRIMITIVE,
                       "\uD800\uDC00\uD800\uDC00\uD800\uDC00");

  // Date
  checkLazyGetterValue(array, 5, PropertyListUtils.TYPE_DATE,
                       new Date("2011-12-31T11:15:23Z"));

  // Data
  checkLazyGetterValue(array, 6, PropertyListUtils.TYPE_UINT8_ARRAY);
  let dataAsString = Array.from(array[6]).map(b => String.fromCharCode(b)).join("");
  Assert.equal(dataAsString, "2011-12-31T11:15:33Z");

  // Dict
  let dict = array[7];
  checkValue(dict, PropertyListUtils.TYPE_DICTIONARY);
  checkValue(dict.get("Negative Number"), PRIMITIVE, -400);
  checkValue(dict.get("Real Number"), PRIMITIVE, 2.71828183);
  checkValue(dict.get("Big Int"),
             PropertyListUtils.TYPE_INT64,
             "9007199254740993");
  checkValue(dict.get("Negative Big Int"),
             PropertyListUtils.TYPE_INT64,
             "-9007199254740993");
}

function readPropertyList(aFile, aCallback) {
  PropertyListUtils.read(aFile, function(aPropertyListRoot) {
    // Null root indicates failure to read property list.
    // Note: It is important not to run do_check_n/eq directly on Dict and array
    // objects, because it cases their toString to get invoked, doing away with
    // all the lazy getter we'd like to test later.
    Assert.ok(aPropertyListRoot !== null);
    aCallback(aPropertyListRoot);
    run_next_test();
  });
}

function run_test() {
  add_test(readPropertyList.bind(this,
    do_get_file("propertyLists/bug710259_propertyListBinary.plist", false),
    checkMainPropertyList));
  add_test(readPropertyList.bind(this,
    do_get_file("propertyLists/bug710259_propertyListXML.plist", false),
    checkMainPropertyList));
  run_next_test();
}
