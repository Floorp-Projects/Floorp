var jum = {}; Components.utils.import('resource://mozmill/modules/jum.js', jum);
var mozmill = {}; Components.utils.import('resource://mozmill/modules/mozmill.js', mozmill);


var testAsserts = function() {
  jum.assert(true);
  jum.assertTrue(true);
  jum.assertFalse(false);
  jum.assertEquals('asdf', 'asdf');
  jum.assertNotEquals('asdf', 'fdsa');
  jum.assertNull(null);
  jum.assertNotNull(true);
  jum.assertUndefined({}.asdf);
  jum.assertNotUndefined('asdf');
  jum.assertNaN('a');
  jum.assertNotNaN(4);
  jum.pass();
}
testAsserts.meta = {'litmusids':[2345678]}

var testAsyncPass = new mozmill.MozMillAsyncTest();

testAsyncPass.testOnePasses = function () {
  jum.assert(true);
  jum.assertTrue(true);
  jum.assertFalse(false);
  jum.assertEquals('asdf', 'asdf');
  jum.assertNotEquals('asdf', 'fdsa');
  jum.assertNull(null);
  jum.assertNotNull(true);
  jum.assertUndefined({}.asdf);
  jum.assertNotUndefined('asdf');
  jum.assertNaN('a');
  jum.assertNotNaN(4);
  jum.pass();
  testAsyncPass.finish();
}

var testAsyncTimeout = new mozmill.MozMillAsyncTest(1000)

var testNothing = {};

var testNotAsserts = function() {
  // All of these calls should fail
  jum.assert(false);
  jum.assertTrue(false);
  jum.assertTrue('asf');
  jum.assertFalse(true);
  jum.assertFalse('asdf');
  jum.assertEquals('asdf', 'fdsa');
  jum.assertNotEquals('asdf', 'asdf');
  jum.assertNull(true);
  jum.assertNotNull(null);
  jum.assertUndefined('asdf');
  jum.assertNotUndefined({}.asdf);
  jum.assertNaN(4);
  jum.assertNotNaN('f');
  jum.fail();
}