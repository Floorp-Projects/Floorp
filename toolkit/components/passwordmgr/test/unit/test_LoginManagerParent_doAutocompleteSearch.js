/**
 * Test LoginManagerParent.doAutocompleteSearch()
 */

"use strict";

const {sinon} = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const {LoginManagerParent: LMP} = ChromeUtils.import("resource://gre/modules/LoginManagerParent.jsm");

add_task(async function test_doAutocompleteSearch_generated_noLogins() {
  Services.prefs.setBoolPref("signon.generation.available", true); // TODO: test both with false
  Services.prefs.setBoolPref("signon.generation.enabled", true);

  ok(LMP.doAutocompleteSearch, "doAutocompleteSearch exists");

  // Default to the happy path
  let arg1 = {
    autocompleteInfo: {
      section: "",
      addressType: "",
      contactType: "",
      fieldName: "new-password",
      canAutomaticallyPersist: false,
    },
    browsingContextId: 123,
    formOrigin: "https://example.com",
    actionOrigin: "https://mozilla.org",
    searchString: "",
    previousResult: null,
    requestId: "foo",
    isSecure: true,
    isPasswordField: true,
  };

  let sendMessageStub = sinon.stub();
  let fakeBrowser = {
    messageManager: {
      sendAsyncMessage: sendMessageStub,
    },
  };

  sinon.stub(LMP._browsingContextGlobal, "get").withArgs(123).callsFake(() => {
    return {
      currentWindowGlobal: {
        documentPrincipal: Services.scriptSecurityManager.createCodebasePrincipalFromOrigin("https://www.example.com^userContextId=1"),
      },
    };
  });

  LMP.doAutocompleteSearch(arg1, fakeBrowser);
  ok(sendMessageStub.calledOnce, "sendAsyncMessage was called");
  let msg1 = sendMessageStub.firstCall.args[1];
  equal(msg1.requestId, arg1.requestId, "requestId matches");
  equal(msg1.logins.length, 0, "no logins");
  ok(msg1.generatedPassword, "has a generated password");
  equal(msg1.generatedPassword.length, 15, "generated password length");
  sendMessageStub.resetHistory();

  info("repeat the search and ensure the same password was used");
  LMP.doAutocompleteSearch(arg1, fakeBrowser);
  ok(sendMessageStub.calledOnce, "sendAsyncMessage was called");
  let msg2 = sendMessageStub.firstCall.args[1];
  equal(msg2.requestId, arg1.requestId, "requestId matches");
  equal(msg2.logins.length, 0, "no logins");
  equal(msg2.generatedPassword, msg1.generatedPassword, "same generated password");
  sendMessageStub.resetHistory();

  info("Check cases where a password shouldn't be generated");

  LMP.doAutocompleteSearch({...arg1, ...{isPasswordField: false}}, fakeBrowser);
  ok(sendMessageStub.calledOnce, "sendAsyncMessage was called");
  let msg = sendMessageStub.firstCall.args[1];
  equal(msg.requestId, arg1.requestId, "requestId matches");
  equal(msg.generatedPassword, null, "no generated password when not a pw. field");
  sendMessageStub.resetHistory();

  let arg1_2 = {...arg1};
  arg1_2.autocompleteInfo.fieldName = "";
  LMP.doAutocompleteSearch(arg1_2, fakeBrowser);
  ok(sendMessageStub.calledOnce, "sendAsyncMessage was called");
  msg = sendMessageStub.firstCall.args[1];
  equal(msg.requestId, arg1.requestId, "requestId matches");
  equal(msg.generatedPassword, null, "no generated password when not autocomplete=new-password");
  sendMessageStub.resetHistory();

  LMP.doAutocompleteSearch({...arg1, ...{browsingContextId: 999}}, fakeBrowser);
  ok(sendMessageStub.calledOnce, "sendAsyncMessage was called");
  msg = sendMessageStub.firstCall.args[1];
  equal(msg.requestId, arg1.requestId, "requestId matches");
  equal(msg.generatedPassword, null, "no generated password with a missing browsingContextId");
  sendMessageStub.resetHistory();
});
