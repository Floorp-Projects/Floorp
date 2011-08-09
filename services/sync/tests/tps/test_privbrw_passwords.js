/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in strict JSON format, as it will get parsed by the Python
 * testrunner (no single quotes, extra comma's, etc).
 */

var phases = { "phase1": "profile1",
               "phase2": "profile2",
               "phase3": "profile1",
               "phase4": "profile2" };

/*
 * Password data
 */

// Initial password data
var passwords_initial = [
   { hostname: "http://www.example.com",
     submitURL: "http://login.example.com",
     username: "joe",
     password: "secret",
     usernameField: "uname",
     passwordField: "pword",
     changes: {
       password: "SeCrEt$$$"
     }
   },
   { hostname: "http://www.example.com",
     realm: "login",
     username: "jack",
     password: "secretlogin"
   }
];

// Password after first modify action has been performed
var passwords_after_first_change = [
   { hostname: "http://www.example.com",
     submitURL: "http://login.example.com",
     username: "joe",
     password: "SeCrEt$$$",
     usernameField: "uname",
     passwordField: "pword",
     changes: {
        username: "james"
     }
   },
   { hostname: "http://www.example.com",
     realm: "login",
     username: "jack",
     password: "secretlogin"
   }
];

// Password after second modify action has been performed
var passwords_after_second_change = [
   { hostname: "http://www.example.com",
     submitURL: "http://login.example.com",
     username: "james",
     password: "SeCrEt$$$",
     usernameField: "uname",
     passwordField: "pword"
   },
   { hostname: "http://www.example.com",
     realm: "login",
     username: "jack",
     password: "secretlogin"
   }
];

/*
 * Test phases
 */

Phase('phase1', [
  [Passwords.add, passwords_initial],
  [Sync, SYNC_WIPE_SERVER]
]);

Phase('phase2', [
  [Sync],
  [Passwords.verify, passwords_initial],
  [Passwords.modify, passwords_initial],
  [Passwords.verify, passwords_after_first_change],
  [Sync]
]);

Phase('phase3', [
  [Sync],
  [SetPrivateBrowsing, true],
  [Passwords.verify, passwords_after_first_change],
  [Passwords.modify, passwords_after_first_change],
  [Passwords.verify, passwords_after_second_change],
  [Sync]
]);

Phase('phase4', [
  [Sync],
  [Passwords.verify, passwords_after_second_change]
]);

