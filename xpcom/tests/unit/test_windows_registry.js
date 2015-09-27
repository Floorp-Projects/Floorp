/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cr = Components.results;
const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const CC = Components.Constructor;

const nsIWindowsRegKey = Ci.nsIWindowsRegKey;
let regKeyComponent = Cc["@mozilla.org/windows-registry-key;1"];

function run_test()
{
    //* create a key structure in a spot that's normally writable (somewhere under HKCU). 
    let testKey = regKeyComponent.createInstance(nsIWindowsRegKey);

    // If it's already present because a previous test crashed or didn't clean up properly, clean it up first.
    let keyName = BASE_PATH + "\\" + TESTDATA_KEYNAME;
    setup_test_run(testKey, keyName);

    //* test that the write* functions write stuff
    test_writing_functions(testKey);

    //* check that the valueCount/getValueName functions work for the values we just wrote
    test_value_functions(testKey);
    
    //* check that the get* functions work for the values we just wrote.
    test_reading_functions(testKey);
    
    //* check that the get* functions fail with the right exception codes if we ask for the wrong type or if the value name doesn't exist at all
    test_invalidread_functions(testKey);

    //* check that creating/enumerating/deleting child keys works
    test_childkey_functions(testKey);

    test_watching_functions(testKey);

    //* clean up
    cleanup_test_run(testKey, keyName);
}

function setup_test_run(testKey, keyName)
{
    do_print("Setup test run");
    try {
        testKey.open(nsIWindowsRegKey.ROOT_KEY_CURRENT_USER, keyName, nsIWindowsRegKey.ACCESS_READ);
        do_print("Test key exists. Needs cleanup.");
        cleanup_test_run(testKey, keyName);
    }
    catch (e if (e instanceof Ci.nsIException && e.result == Cr.NS_ERROR_FAILURE))
    {
    }

    testKey.create(nsIWindowsRegKey.ROOT_KEY_CURRENT_USER, keyName, nsIWindowsRegKey.ACCESS_ALL);
}

function test_writing_functions(testKey)
{
    strictEqual(testKey.valueCount, 0);

    strictEqual(testKey.hasValue(TESTDATA_STRNAME), false);
    testKey.writeStringValue(TESTDATA_STRNAME, TESTDATA_STRVALUE);
    strictEqual(testKey.hasValue(TESTDATA_STRNAME), true);

    strictEqual(testKey.hasValue(TESTDATA_INTNAME), false);
    testKey.writeIntValue(TESTDATA_INTNAME, TESTDATA_INTVALUE);

    strictEqual(testKey.hasValue(TESTDATA_INT64NAME), false);
    testKey.writeInt64Value(TESTDATA_INT64NAME, TESTDATA_INT64VALUE);

    strictEqual(testKey.hasValue(TESTDATA_BINARYNAME), false);
    testKey.writeBinaryValue(TESTDATA_BINARYNAME, TESTDATA_BINARYVALUE);
}

function test_value_functions(testKey)
{
    strictEqual(testKey.valueCount, 4);
    strictEqual(testKey.getValueName(0), TESTDATA_STRNAME);
    strictEqual(testKey.getValueName(1), TESTDATA_INTNAME);
    strictEqual(testKey.getValueName(2), TESTDATA_INT64NAME);
    strictEqual(testKey.getValueName(3), TESTDATA_BINARYNAME);
}

function test_reading_functions(testKey)
{
    strictEqual(testKey.getValueType(TESTDATA_STRNAME), nsIWindowsRegKey.TYPE_STRING);
    strictEqual(testKey.readStringValue(TESTDATA_STRNAME), TESTDATA_STRVALUE);

    strictEqual(testKey.getValueType(TESTDATA_INTNAME), nsIWindowsRegKey.TYPE_INT);
    strictEqual(testKey.readIntValue(TESTDATA_INTNAME), TESTDATA_INTVALUE);
   
    strictEqual(testKey.getValueType(TESTDATA_INT64NAME), nsIWindowsRegKey.TYPE_INT64);
    strictEqual( testKey.readInt64Value(TESTDATA_INT64NAME), TESTDATA_INT64VALUE);
    
    strictEqual(testKey.getValueType(TESTDATA_BINARYNAME), nsIWindowsRegKey.TYPE_BINARY);
    strictEqual( testKey.readBinaryValue(TESTDATA_BINARYNAME), TESTDATA_BINARYVALUE);
}

function test_invalidread_functions(testKey)
{
    try {
        testKey.readIntValue(TESTDATA_STRNAME);
        do_throw("Reading an integer from a string registry value should throw.");
    } catch (e if (e instanceof Ci.nsIException && e.result == Cr.NS_ERROR_FAILURE)) {
    }

    try {
        let val = testKey.readStringValue(TESTDATA_INTNAME);
        do_throw("Reading an string from an Int registry value should throw." + val);
    } catch (e if (e instanceof Ci.nsIException && e.result == Cr.NS_ERROR_FAILURE)) {
    }

    try {
        testKey.readStringValue(TESTDATA_INT64NAME);
        do_throw("Reading an string from an Int64 registry value should throw.");
    } catch (e if (e instanceof Ci.nsIException && e.result == Cr.NS_ERROR_FAILURE)) {
    }

    try {
        testKey.readStringValue(TESTDATA_BINARYNAME);
        do_throw("Reading a string from an Binary registry value should throw.");
    } catch (e if (e instanceof Ci.nsIException && e.result == Cr.NS_ERROR_FAILURE)) {
    }

}

function test_childkey_functions(testKey)
{
    strictEqual(testKey.childCount, 0);
    strictEqual(testKey.hasChild(TESTDATA_CHILD_KEY), false);
    
    let childKey = testKey.createChild(TESTDATA_CHILD_KEY, nsIWindowsRegKey.ACCESS_ALL);
    childKey.close();

    strictEqual(testKey.childCount, 1);
    strictEqual(testKey.hasChild(TESTDATA_CHILD_KEY), true);
    strictEqual(testKey.getChildName(0), TESTDATA_CHILD_KEY);

    childKey = testKey.openChild(TESTDATA_CHILD_KEY, nsIWindowsRegKey.ACCESS_ALL);
    testKey.removeChild(TESTDATA_CHILD_KEY);
    strictEqual(testKey.childCount, 0);
    strictEqual(testKey.hasChild(TESTDATA_CHILD_KEY), false);
}

function test_watching_functions(testKey)
{
    strictEqual(testKey.isWatching(), false);
    strictEqual(testKey.hasChanged(), false);
    
    testKey.startWatching(true);
    strictEqual(testKey.isWatching(), true);
    
    testKey.stopWatching();
    strictEqual(testKey.isWatching(), false);

    // Create a child key, and update a value
    let childKey = testKey.createChild(TESTDATA_CHILD_KEY, nsIWindowsRegKey.ACCESS_ALL);
    childKey.writeIntValue(TESTDATA_INTNAME, TESTDATA_INTVALUE);

    // Start a recursive watch, and update the child's value
    testKey.startWatching(true);
    strictEqual(testKey.isWatching(), true);

    childKey.writeIntValue(TESTDATA_INTNAME, 0);
    strictEqual(testKey.hasChanged(), true);
    testKey.stopWatching();
    strictEqual(testKey.isWatching(), false);
        
    childKey.removeValue(TESTDATA_INTNAME);
    childKey.close();
    testKey.removeChild(TESTDATA_CHILD_KEY);
}

function cleanup_test_run(testKey, keyName)
{
    do_print("Cleaning up test.");

    for (var i = 0; i < testKey.childCount; i++) {
        testKey.removeChild(testKey.getChildName(i));
    }
    testKey.close();

    let baseKey = regKeyComponent.createInstance(nsIWindowsRegKey);
    baseKey.open(nsIWindowsRegKey.ROOT_KEY_CURRENT_USER, BASE_PATH, nsIWindowsRegKey.ACCESS_ALL);
    baseKey.removeChild(TESTDATA_KEYNAME);
    baseKey.close();
}

// Test data used above.
const BASE_PATH = "SOFTWARE";
const TESTDATA_KEYNAME = "TestRegXPC";
const TESTDATA_STRNAME = "AString";
const TESTDATA_STRVALUE = "The quick brown fox jumps over the lazy dog.";
const TESTDATA_INTNAME = "AnInteger";
const TESTDATA_INTVALUE = 65536;
const TESTDATA_INT64NAME = "AnInt64";
const TESTDATA_INT64VALUE = 9223372036854775807;
const TESTDATA_BINARYNAME = "ABinary";
const TESTDATA_BINARYVALUE = "She sells seashells by the seashore";
const TESTDATA_CHILD_KEY = "TestChildKey";
