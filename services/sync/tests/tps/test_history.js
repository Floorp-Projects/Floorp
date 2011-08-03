/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in strict JSON format, as it will get parsed by the Python
 * testrunner (no single quotes, extra comma's, etc).
 */

var phases = { "phase1": "profile1",
               "phase2": "profile2" };

/*
 * History asset lists: these define history entries that are used during
 * the test
 */

// the initial list of history items to add to the browser
var history1 = [
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
        date: 0
      },
      { type: 1,
        date: -1
      },
      { type: 1,
        date: -20
      },
      { type: 2,
        date: -36
      }
    ]
  }
];

// a list of items to delete from the history
var history_to_delete = [
  { uri: "http://www.cnn.com/" },
  { begin: -24,
    end: -1
  },
  { host: "www.google.com" }
];

// a list which reflects items that should be in the history after
// the above items are deleted
var history2 = [
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

// a list which includes history entries that should not be present
// after deletion of the history_to_delete entries
var history_not = [
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
 * Test phases
 * Note:  there is no test phase in which deleted history entries are
 * synced to other clients.  This functionality is not supported by
 * Sync, see bug 446517.
 */

Phase('phase1', [
  [History.add, history1],
  [Sync, SYNC_WIPE_SERVER],
]);

Phase('phase2', [
  [Sync],
  [History.verify, history1],
  [History.delete, history_to_delete],
  [History.verify, history2],
  [History.verifyNot, history_not],
  [Sync]
]);

