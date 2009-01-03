/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is httpd.js code.
 *
 * The Initial Developer of the Original Code is
 * Jeff Walden <jwalden+code@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  do_check_eq(c, "text/html");

  headers.setHeader("test", "FOO", false);
  var c = headers.getHeader("test");
  do_check_eq(c, "FOO");

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
    do_check_true(it.toLowerCase() in heads);
    delete heads[it.toLowerCase()];
  }

  for (var i in heads)
    do_throw("still have properties in heads!?!?");

}

function testHasHeader()
{
  var headers = new nsHttpHeaders();

  headers.setHeader("foo", "bar", false);
  do_check_true(headers.hasHeader("foo"));
  do_check_true(headers.hasHeader("fOo"));
  do_check_false(headers.hasHeader("not-there"));

  headers.setHeader("f`'~", "bar", false);
  do_check_true(headers.hasHeader("F`'~"));

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
