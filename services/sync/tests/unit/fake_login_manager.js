Cu.import("resource://weave/util.js");

// ----------------------------------------
// Fake Sample Data
// ----------------------------------------

let fakeSampleLogins = [
  // Fake nsILoginInfo object.
  {hostname: "www.boogle.com",
   formSubmitURL: "http://www.boogle.com/search",
   httpRealm: "",
   username: "",
   password: "",
   usernameField: "test_person",
   passwordField: "test_password"}
];

// ----------------------------------------
// Fake Login Manager
// ----------------------------------------

function FakeLoginManager(fakeLogins) {
  this.fakeLogins = fakeLogins;

  let self = this;

  Utils.getLoginManager = function fake_getLoginManager() {
    // Return a fake nsILoginManager object.
    return {
      getAllLogins: function() { return self.fakeLogins; },
      addLogin: function(login) {
        getTestLogger().info("nsILoginManager.addLogin() called " +
                             "with hostname '" + login.hostname + "'.");
        self.fakeLogins.push(login);
      }
    };
  };
}
