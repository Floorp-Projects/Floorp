/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

EnableEngines(["passwords"]);

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in JSON format as it will get parsed by the Python
 * testrunner. It is parsed by the YAML package, so it relatively flexible.
 */
var phases = {
  phase1: "profile1",
  phase2: "profile2",
  phase3: "profile1",
  phase4: "profile2",
};

/*
 * Password lists
 */

var passwords_initial = [
  {
    hostname: "http://www.example.com",
    submitURL: "http://login.example.com",
    username: "joe",
    password: "secret",
    usernameField: "uname",
    passwordField: "pword",
    changes: {
      password: "SeCrEt$$$",
    },
  },
  {
    hostname: "http://www.example.com",
    realm: "login",
    username: "jack",
    password: "secretlogin",
  },
];

var passwords_after_first_update = [
  {
    hostname: "http://www.example.com",
    submitURL: "http://login.example.com",
    username: "joe",
    password: "SeCrEt$$$",
    usernameField: "uname",
    passwordField: "pword",
  },
  {
    hostname: "http://www.example.com",
    realm: "login",
    username: "jack",
    password: "secretlogin",
  },
];

/*
 * Test phases
 */

Phase("phase1", [[Passwords.add, passwords_initial], [Sync]]);

Phase("phase2", [[Passwords.add, passwords_initial], [Sync]]);

Phase("phase3", [
  [Sync],
  [Passwords.verify, passwords_initial],
  [Passwords.modify, passwords_initial],
  [Passwords.verify, passwords_after_first_update],
  [Sync],
]);

Phase("phase4", [[Sync], [Passwords.verify, passwords_after_first_update]]);
