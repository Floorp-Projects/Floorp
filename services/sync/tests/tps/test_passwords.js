/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in strict JSON format, as it will get parsed by the Python
 * testrunner (no single quotes, extra comma's, etc).
 */
EnableEngines(["passwords"]);

var phases = { "phase1": "profile1",
               "phase2": "profile2",
               "phase3": "profile1",
               "phase4": "profile2" };

/*
 * Password asset lists: these define password entries that are used during
 * the test
 */

// initial password list to be loaded into the browser
var passwords_initial = [
  { hostname: "http://www.example.com",
    submitURL: "http://login.example.com",
    username: "joe",
    password: "SeCrEt123",
    usernameField: "uname",
    passwordField: "pword",
    changes: {
      password: "zippity-do-dah"
    }
  },
  { hostname: "http://www.example.com",
    realm: "login",
    username: "joe",
    password: "secretlogin"
  }
];

// expected state of passwords after the changes in the above list are applied
var passwords_after_first_update = [
  { hostname: "http://www.example.com",
    submitURL: "http://login.example.com",
    username: "joe",
    password: "zippity-do-dah",
    usernameField: "uname",
    passwordField: "pword"
  },
  { hostname: "http://www.example.com",
    realm: "login",
    username: "joe",
    password: "secretlogin"
  }
];

var passwords_to_delete = [
  { hostname: "http://www.example.com",
    realm: "login",
    username: "joe",
    password: "secretlogin"
  }
];

var passwords_absent = [
  { hostname: "http://www.example.com",
    realm: "login",
    username: "joe",
    password: "secretlogin"
  }
];

// expected state of passwords after the delete operation
var passwords_after_second_update = [
  { hostname: "http://www.example.com",
    submitURL: "http://login.example.com",
    username: "joe",
    password: "zippity-do-dah",
    usernameField: "uname",
    passwordField: "pword"
  }
];

/*
 * Test phases
 */

Phase('phase1', [
  [Passwords.add, passwords_initial],
  [Sync]
]);

Phase('phase2', [
  [Sync],
  [Passwords.verify, passwords_initial],
  [Passwords.modify, passwords_initial],
  [Passwords.verify, passwords_after_first_update],
  [Sync]
]);

Phase('phase3', [
  [Sync],
  [Passwords.verify, passwords_after_first_update],
  [Passwords.delete, passwords_to_delete],
  [Passwords.verify, passwords_after_second_update],
  [Passwords.verifyNot, passwords_absent],
  [Sync]
]);

Phase('phase4', [
  [Sync],
  [Passwords.verify, passwords_after_second_update],
  [Passwords.verifyNot, passwords_absent]
]);
