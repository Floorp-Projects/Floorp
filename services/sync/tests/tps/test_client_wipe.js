/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in strict JSON format, as it will get parsed by the Python
 * testrunner (no single quotes, extra comma's, etc).
 */

var phases = { "phase1": "profile1",
               "phase2": "profile2",
               "phase3": "profile1"};

/*
 * Bookmark lists
 */

// the initial list of bookmarks to add to the browser
var bookmarks_initial = {
  toolbar: [
    { uri: "http://www.google.com",
      title: "Google"
    },
    { uri: "http://www.cnn.com",
      title: "CNN",
      changes: {
        position: "Google"
      }
    },
    { uri: "http://www.mozilla.com",
      title: "Mozilla"
    },
    { uri: "http://www.firefox.com",
      title: "Firefox",
      changes: {
        position: "Mozilla"
      }
    }
  ]
};

var bookmarks_after_move = {
  toolbar: [
    { uri: "http://www.cnn.com",
      title: "CNN"
    },
    { uri: "http://www.google.com",
      title: "Google"
    },
    { uri: "http://www.firefox.com",
      title: "Firefox"
    },
    { uri: "http://www.mozilla.com",
      title: "Mozilla"
    }
  ]
};

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
var passwords_after_change = [
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

/*
 * Prefs to use in the test
 */
var prefs1 = [
  { name: "browser.startup.homepage",
    value: "http://www.getfirefox.com"
  },
  { name: "browser.urlbar.maxRichResults",
    value: 20
  },
  { name: "browser.link.open_newwindow",
    value: true
  }
];

var prefs2 = [
  { name: "browser.startup.homepage",
    value: "http://www.mozilla.com"
  },
  { name: "browser.urlbar.maxRichResults",
    value: 18
  },
  { name: "browser.link.open_newwindow",
    value: false
  }
];

/*
 * Test phases
 */

// Add prefs,passwords and bookmarks to profile1 and sync.
Phase('phase1', [
  [Passwords.add, passwords_initial],
  [Bookmarks.add, bookmarks_initial],
  [Prefs.modify, prefs1],
  [Prefs.verify, prefs1],
  [Sync]
]);

// Sync profile2 and verify same prefs,passwords and bookmarks are present.
Phase('phase2', [
  [Sync],
  [Prefs.verify, prefs1],
  [Passwords.verify, passwords_initial],
  [Bookmarks.verify, bookmarks_initial]
]);

// Using profile1, change some prefs,bookmarks and pwds, then do another sync with wipe-client.
// Verify that the cloud's  settings are restored, and the recent local changes
// discarded.
Phase('phase3', [
  [Prefs.modify, prefs2],
  [Passwords.modify, passwords_initial],
  [Bookmarks.modify, bookmarks_initial],
  [Prefs.verify, prefs2],
  [Passwords.verify, passwords_after_change],
  [Bookmarks.verify, bookmarks_after_move],
  [Sync, SYNC_WIPE_CLIENT],
  [Prefs.verify, prefs1],
  [Passwords.verify, passwords_initial],
  [Bookmarks.verify, bookmarks_initial]
]);

