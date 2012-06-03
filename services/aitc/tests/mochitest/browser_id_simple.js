/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const AUD = "http://foo.net";

function test() {
  waitForExplicitFinish();
  setEndpoint("browser_id_mock");

  // Get an assertion for default email.
  BrowserID.getAssertion(gotDefaultAssertion, {audience: AUD});
}

function gotDefaultAssertion(err, ast) {
  is(err, null, "gotDefaultAssertion failed with " + err);
  is(ast, "default@example.org_assertion_" + AUD,
     "gotDefaultAssertion returned wrong assertion");

  // Get an assertion for a specific email.
  BrowserID.getAssertion(gotSpecificAssertion, {
    requiredEmail: "specific@example.org",
    audience: AUD
  });
}

function gotSpecificAssertion(err, ast) {
  is(err, null, "gotSpecificAssertion failed with " + err);
  is(ast, "specific@example.org_assertion_" + AUD,
     "gotSpecificAssertion returned wrong assertion");

  // Get an assertion using sameEmailAs for another domain.
  BrowserID.getAssertion(gotSameEmailAssertion, {
    sameEmailAs: "http://zombo.com",
    audience: AUD
  });
}

function gotSameEmailAssertion(err, ast) {
  is(err, null, "gotSameEmailAssertion failed with " + err);
  is(ast, "assertion_for_sameEmailAs",
     "gotSameEmailAssertion returned wrong assertion");

  finish();
}
