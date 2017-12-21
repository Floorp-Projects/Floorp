/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// tests for header storage in httpd.js; nsHttpHeaders is an *internal* data
// structure and is not to be used directly outside of httpd.js itself except
// for testing purposes


/**
 * Ensures that a fieldname-fieldvalue combination is a valid header.
 *
 * @param fieldName
 *   the name of the header
 * @param fieldValue
 *   the value of the header
 * @param headers
 *   an nsHttpHeaders object to use to check validity
 */
function assertValidHeader(fieldName, fieldValue, headers)
{
  try
  {
    headers.setHeader(fieldName, fieldValue, false);
  }
  catch (e)
  {
    do_throw("Unexpected exception thrown: " + e);
  }
}

/**
 * Ensures that a fieldname-fieldvalue combination is not a valid header.
 *
 * @param fieldName
 *   the name of the header
 * @param fieldValue
 *   the value of the header
 * @param headers
 *   an nsHttpHeaders object to use to check validity
 */
function assertInvalidHeader(fieldName, fieldValue, headers)
{
  try
  {
    headers.setHeader(fieldName, fieldValue, false);
    throw "Setting (" + fieldName + ", " +
          fieldValue + ") as header succeeded!";
  }
  catch (e)
  {
    if (e !== Cr.NS_ERROR_INVALID_ARG)
      do_throw("Unexpected exception thrown: " + e);
  }
}


function run_test()
{
  testHeaderValidity();
  testGetHeader();
  testHeaderEnumerator();
  testHasHeader();
}

function testHeaderValidity()
{
  var headers = new nsHttpHeaders();

  assertInvalidHeader("f o", "bar", headers);
  assertInvalidHeader("f\0n", "bar", headers);
  assertInvalidHeader("foo:", "bar", headers);
  assertInvalidHeader("f\\o", "bar", headers);
  assertInvalidHeader("@xml", "bar", headers);
  assertInvalidHeader("fiz(", "bar", headers);
  assertInvalidHeader("HTTP/1.1", "bar", headers);
  assertInvalidHeader("b\"b", "bar", headers);
  assertInvalidHeader("ascsd\t", "bar", headers);
  assertInvalidHeader("{fds", "bar", headers);
  assertInvalidHeader("baz?", "bar", headers);
  assertInvalidHeader("a\\b\\c", "bar", headers);
  assertInvalidHeader("\0x7F", "bar", headers);
  assertInvalidHeader("\0x1F", "bar", headers);
  assertInvalidHeader("f\n", "bar", headers);
  assertInvalidHeader("foo", "b\nar", headers);
  assertInvalidHeader("foo", "b\rar", headers);
  assertInvalidHeader("foo", "b\0", headers);

  // request splitting, fwiw -- we're actually immune to this type of attack so
  // long as we don't implement persistent connections
  assertInvalidHeader("f\r\nGET /badness HTTP/1.1\r\nFoo", "bar", headers);

  assertValidHeader("f'", "baz", headers);
  assertValidHeader("f`", "baz", headers);
  assertValidHeader("f.", "baz", headers);
  assertValidHeader("f---", "baz", headers);
  assertValidHeader("---", "baz", headers);
  assertValidHeader("~~~", "baz", headers);
  assertValidHeader("~~~", "b\r\n bar", headers);
  assertValidHeader("~~~", "b\r\n\tbar", headers);
}

function testGetHeader()
{
  var headers = new nsHttpHeaders();

  headers.setHeader("Content-Type", "text/html", false);
  var c = headers.getHeader("content-type");
  Assert.equal(c, "text/html");

  headers.setHeader("test", "FOO", false);
  var c = headers.getHeader("test");
  Assert.equal(c, "FOO");

  try
  {
    headers.getHeader(":");
    throw "Failed to throw for invalid header";
  }
  catch (e)
  {
    if (e !== Cr.NS_ERROR_INVALID_ARG)
      do_throw("headers.getHeader(':') must throw invalid arg");
  }

  try
  {
    headers.getHeader("valid");
    throw 'header doesn\'t exist';
  }
  catch (e)
  {
    if (e !== Cr.NS_ERROR_NOT_AVAILABLE)
      do_throw("shouldn't be a header named 'valid' in headers!");
  }
}

function testHeaderEnumerator()
{
  var headers = new nsHttpHeaders();

  var heads =
    {
      "foo": "17",
      "baz": "two six niner",
      "decaf": "class Program { int .7; int main(){ .7 = 5; return 7 - .7; } }"
    };

  for (var i in heads)
    headers.setHeader(i, heads[i], false);

  var en = headers.enumerator;
  while (en.hasMoreElements())
  {
    var it = en.getNext().QueryInterface(Ci.nsISupportsString).data;
    Assert.ok(it.toLowerCase() in heads);
    delete heads[it.toLowerCase()];
  }

  for (var i in heads)
    do_throw("still have properties in heads!?!?");

}

function testHasHeader()
{
  var headers = new nsHttpHeaders();

  headers.setHeader("foo", "bar", false);
  Assert.ok(headers.hasHeader("foo"));
  Assert.ok(headers.hasHeader("fOo"));
  Assert.ok(!headers.hasHeader("not-there"));

  headers.setHeader("f`'~", "bar", false);
  Assert.ok(headers.hasHeader("F`'~"));

  try
  {
    headers.hasHeader(":");
    throw "failed to throw";
  }
  catch (e)
  {
    if (e !== Cr.NS_ERROR_INVALID_ARG)
      do_throw(".hasHeader for an invalid name should throw");
  }
}
