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
 * Bookmark asset lists: these define bookmarks that are used during the test
 */

// the initial list of bookmarks to be added to the browser
var bookmarks_initial = {
  "menu": [
    { uri: "http://www.google.com",
      tags: ["google", "computers", "internet", "www"],
      changes: {
        title: "Google",
        tags: ["google", "computers", "misc"],
      }
    },
    { uri: "http://bugzilla.mozilla.org/show_bug.cgi?id=%s",
      title: "Bugzilla",
      keyword: "bz",
      changes: {
        keyword: "bugzilla"
      }
    },
    { folder: "foldera" },
    { uri: "http://www.mozilla.com" },
    { separator: true },
    { folder: "folderb" },
  ],
  "menu/foldera": [
    { uri: "http://www.yahoo.com",
      title: "testing Yahoo",
      changes: {
        location: "menu/folderb"
      }
    },
    { uri: "http://www.cnn.com",
      description: "This is a description of the site a at www.cnn.com",
      changes: {
        uri: "http://money.cnn.com",
        description: "new description",
      }
    },
    { livemark: "Livemark1",
      feedUri: "http://rss.wunderground.com/blog/JeffMasters/rss.xml",
      siteUri: "http://www.wunderground.com/blog/JeffMasters/show.html",
      changes: {
        livemark: "LivemarkOne"
      }
    },
  ],
  "menu/folderb": [
    { uri: "http://www.apple.com",
      tags: ["apple", "mac"],
      changes: {
        uri: "http://www.apple.com/iphone/",
        title: "iPhone",
        location: "menu",
        position: "Google",
        tags: []
      }
    }
  ],
  toolbar: [
    { uri: "place:queryType=0&sort=8&maxResults=10&beginTimeRef=1&beginTime=0",
      title: "Visited Today"
    }
  ]
};

// the state of bookmarks after the first 'modify' action has been performed
// on them
var bookmarks_after_first_modify = {
  "menu": [
    { uri: "http://www.apple.com/iphone/",
      title: "iPhone",
      before: "Google",
      tags: []
    },
    { uri: "http://www.google.com",
      title: "Google",
      tags: [ "google", "computers", "misc"]
    },
    { uri: "http://bugzilla.mozilla.org/show_bug.cgi?id=%s",
      title: "Bugzilla",
      keyword: "bugzilla"
    },
    { folder: "foldera" },
    { uri: "http://www.mozilla.com" },
    { separator: true },
    { folder: "folderb",
      changes: {
        location: "menu/foldera",
        folder: "Folder B",
        description: "folder description"
      }
    }
  ],
  "menu/foldera": [
    { uri: "http://money.cnn.com",
      title: "http://www.cnn.com",
      description: "new description"
    },
    { livemark: "LivemarkOne",
      feedUri: "http://rss.wunderground.com/blog/JeffMasters/rss.xml",
      siteUri: "http://www.wunderground.com/blog/JeffMasters/show.html"
    }
  ],
  "menu/folderb": [
    { uri: "http://www.yahoo.com",
      title: "testing Yahoo"
    }
  ],
  "toolbar": [
    { uri: "place:queryType=0&sort=8&maxResults=10&beginTimeRef=1&beginTime=0",
      title: "Visited Today"
    }
  ]
};

// a list of bookmarks to delete during a 'delete' action
var bookmarks_to_delete = {
  "menu": [
    { uri: "http://www.google.com",
      title: "Google",
      tags: [ "google", "computers", "misc" ]
    }
  ]
};

// the state of bookmarks after the second 'modify' action has been performed
// on them
var bookmarks_after_second_modify = {
  "menu": [
    { uri: "http://www.apple.com/iphone/",
      title: "iPhone"
    },
    { uri: "http://bugzilla.mozilla.org/show_bug.cgi?id=%s",
      title: "Bugzilla",
      keyword: "bugzilla"
    },
    { folder: "foldera" },
    { uri: "http://www.mozilla.com" },
    { separator: true },
  ],
  "menu/foldera": [
    { uri: "http://money.cnn.com",
      title: "http://www.cnn.com",
      description: "new description"
    },
    { livemark: "LivemarkOne",
      feedUri: "http://rss.wunderground.com/blog/JeffMasters/rss.xml",
      siteUri: "http://www.wunderground.com/blog/JeffMasters/show.html"
    },
    { folder: "Folder B",
      description: "folder description"
    }
  ],
  "menu/foldera/Folder B": [
    { uri: "http://www.yahoo.com",
      title: "testing Yahoo"
    }
  ]
};

// a list of bookmarks which should not be present after the last
// 'delete' and 'modify' actions
var bookmarks_absent = {
  "menu": [
    { uri: "http://www.google.com",
      title: "Google"
    },
    { folder: "folderb" },
    { folder: "Folder B" }
  ]
};

/*
 * History asset lists: these define history entries that are used during
 * the test
 */

// the initial list of history items to add to the browser
var history_initial = [
  { uri: "http://www.google.com/",
    title: "Google",
    visits: [
      { type: 1, date: 0 },
      { type: 2, date: -1 }
    ]
  },
  { uri: "http://www.cnn.com/",
    title: "CNN",
    visits: [
      { type: 1, date: -1 },
      { type: 2, date: -36 }
    ]
  },
  { uri: "http://www.google.com/language_tools?hl=en",
    title: "Language Tools",
    visits: [
      { type: 1, date: 0 },
      { type: 2, date: -40 }
    ]
  },
  { uri: "http://www.mozilla.com/",
    title: "Mozilla",
    visits: [
      { type: 1, date: 0 },
      { type: 1, date: -1 },
      { type: 1, date: -20 },
      { type: 2, date: -36 }
    ]
  }
];

// a list of history entries to delete during a 'delete' action
var history_to_delete = [
  { uri: "http://www.cnn.com/" },
  { begin: -24,
    end: -1 },
  { host: "www.google.com" }
];

// the expected history entries after the first 'delete' action
var history_after_delete = [
  { uri: "http://www.mozilla.com/",
    title: "Mozilla",
    visits: [
      { type: 1,
        date: 0
      },
      { type: 2,
        date: -36
      }
    ]
  }
];

// history entries expected to not exist after a 'delete' action
var history_absent = [
  { uri: "http://www.google.com/",
    title: "Google",
    visits: [
      { type: 1,
        date: 0
      },
      { type: 2,
        date: -1
      }
    ]
  },
  { uri: "http://www.cnn.com/",
    title: "CNN",
    visits: [
      { type: 1,
        date: -1
      },
      { type: 2,
        date: -36
      }
    ]
  },
  { uri: "http://www.google.com/language_tools?hl=en",
    title: "Language Tools",
    visits: [
      { type: 1,
        date: 0
      },
      { type: 2,
        date: -40
      }
    ]
  },
  { uri: "http://www.mozilla.com/",
    title: "Mozilla",
    visits: [
      { type: 1,
        date: -1
      },
      { type: 1,
        date: -20
      }
    ]
  }
];

/*
 * Password asset lists: these define password entries that are used during
 * the test
 */

// the initial list of passwords to add to the browser
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

// the expected state of passwords after the first 'modify' action
var passwords_after_first_modify = [
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

// a list of passwords to delete during a 'delete' action
var passwords_to_delete = [
  { hostname: "http://www.example.com",
    realm: "login",
    username: "joe",
    password: "secretlogin"
  }
];

// a list of passwords expected to be absent after 'delete' and 'modify'
// actions
var passwords_absent = [
  { hostname: "http://www.example.com",
    realm: "login",
    username: "joe",
    password: "secretlogin"
  }
];

// the expected state of passwords after the seconds 'modify' action
var passwords_after_second_modify = [
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

Phase("phase1", [
  [Bookmarks.add, bookmarks_initial],
  [Passwords.add, passwords_initial],
  [History.add, history_initial],
  [Sync],
]);

Phase("phase2", [
  [Sync],
  [Bookmarks.verify, bookmarks_initial],
  [Passwords.verify, passwords_initial],
  [History.verify, history_initial],
  [Bookmarks.modify, bookmarks_initial],
  [Passwords.modify, passwords_initial],
  [History.delete, history_to_delete],
  [Bookmarks.verify, bookmarks_after_first_modify],
  [Passwords.verify, passwords_after_first_modify],
  [History.verify, history_after_delete],
  [History.verifyNot, history_absent],
  [Sync],
]);

Phase("phase3", [
  [Sync],
  [Bookmarks.verify, bookmarks_after_first_modify],
  [Passwords.verify, passwords_after_first_modify],
  [History.verify, history_after_delete],
  [Bookmarks.modify, bookmarks_after_first_modify],
  [Passwords.modify, passwords_after_first_modify],
  [Bookmarks.delete, bookmarks_to_delete],
  [Passwords.delete, passwords_to_delete],
  [Bookmarks.verify, bookmarks_after_second_modify],
  [Passwords.verify, passwords_after_second_modify],
  [Bookmarks.verifyNot, bookmarks_absent],
  [Passwords.verifyNot, passwords_absent],
  [Sync],
]);

Phase("phase4", [
  [Sync],
  [Bookmarks.verify, bookmarks_after_second_modify],
  [Passwords.verify, passwords_after_second_modify],
  [Bookmarks.verifyNot, bookmarks_absent],
  [Passwords.verifyNot, passwords_absent],
  [History.verifyNot, history_absent],
]);
