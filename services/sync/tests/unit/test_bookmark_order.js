/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

_("Making sure after processing incoming bookmarks, they show up in the right order");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/main.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

Svc.Prefs.set("log.logger.engine.bookmarks", "Trace");
initTestLogging("Trace");
Log.repository.getLogger("Sqlite").level = Log.Level.Info;

function serverForFoo(engine) {
  generateNewKeys(Service.collectionKeys);

  let clientsEngine = Service.clientsEngine;
  return serverForUsers({"foo": "password"}, {
    meta: {
      global: {
        syncID: Service.syncID,
        storageVersion: STORAGE_VERSION,
        engines: {
          clients: {
            version: clientsEngine.version,
            syncID: clientsEngine.syncID,
          },
          [engine.name]: {
            version: engine.version,
            syncID: engine.syncID,
          },
        },
      },
    },
    crypto: {
      keys: encryptPayload({
        id: "keys",
        // Generate a fake default key bundle to avoid resetting the client
        // before the first sync.
        default: [
          Weave.Crypto.generateRandomKey(),
          Weave.Crypto.generateRandomKey(),
        ],
      }),
    },
    [engine.name]: {},
  });
}

async function resolveConflict(engine, collection, timestamp, buildTree,
                               message) {
  let guids = {
    // These items don't exist on the server.
    fx: Utils.makeGUID(),
    nightly: Utils.makeGUID(),
    support: Utils.makeGUID(),
    customize: Utils.makeGUID(),

    // These exist on the server, but in a different order, and `res`
    // has completely different children.
    res: Utils.makeGUID(),
    tb: Utils.makeGUID(),

    // These don't exist locally.
    bz: Utils.makeGUID(),
    irc: Utils.makeGUID(),
    mdn: Utils.makeGUID(),
  };

  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      guid: guids.fx,
      title: "Get Firefox!",
      url: "http://getfirefox.com/",
    }, {
      guid: guids.res,
      title: "Resources",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      children: [{
        guid: guids.nightly,
        title: "Nightly",
        url: "https://nightly.mozilla.org/",
      }, {
        guid: guids.support,
        title: "Support",
        url: "https://support.mozilla.org/",
      }, {
        guid: guids.customize,
        title: "Customize",
        url: "https://mozilla.org/firefox/customize/",
      }],
    }, {
      title: "Get Thunderbird!",
      guid: guids.tb,
      url: "http://getthunderbird.com/",
    }],
  });

  let serverRecords = [{
    id: "menu",
    type: "folder",
    title: "Bookmarks Menu",
    parentid: "places",
    children: [guids.tb, guids.res],
  }, {
    id: guids.tb,
    type: "bookmark",
    parentid: "menu",
    bmkUri: "http://getthunderbird.com/",
    title: "Get Thunderbird!",
  }, {
    id: guids.res,
    type: "folder",
    parentid: "menu",
    title: "Resources",
    children: [guids.irc, guids.bz, guids.mdn],
  }, {
    id: guids.bz,
    type: "bookmark",
    parentid: guids.res,
    bmkUri: "https://bugzilla.mozilla.org/",
    title: "Bugzilla",
  }, {
    id: guids.mdn,
    type: "bookmark",
    parentid: guids.res,
    bmkUri: "https://developer.mozilla.org/",
    title: "MDN",
  }, {
    id: guids.irc,
    type: "bookmark",
    parentid: guids.res,
    bmkUri: "ircs://irc.mozilla.org/nightly",
    title: "IRC",
  }];
  for (let record of serverRecords) {
    collection.insert(record.id, encryptPayload(record), timestamp);
  }

  await sync_engine_and_validate_telem(engine, false);

  let expectedTree = buildTree(guids);
  await assertBookmarksTreeMatches(PlacesUtils.bookmarks.menuGuid,
    expectedTree, message);
}

add_task(async function test_local_order_newer() {
  let engine = Service.engineManager.get("bookmarks");

  let server = serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  try {
    let collection = server.user("foo").collection("bookmarks");
    let serverModified = Date.now() / 1000 - 120;
    await resolveConflict(engine, collection, serverModified, guids => [{
      guid: guids.fx,
      index: 0,
    }, {
      guid: guids.res,
      index: 1,
      children: [{
        guid: guids.nightly,
        index: 0,
      }, {
        guid: guids.support,
        index: 1,
      }, {
        guid: guids.customize,
        index: 2,
      }, {
        guid: guids.irc,
        index: 3,
      }, {
        guid: guids.bz,
        index: 4,
      }, {
        guid: guids.mdn,
        index: 5,
      }],
    }, {
      guid: guids.tb,
      index: 2,
    }], "Should use local order as base if remote is older");
  } finally {
    await engine.wipeClient();
    await Service.startOver();
    await promiseStopServer(server);
  }
});

add_task(async function test_remote_order_newer() {
  let engine = Service.engineManager.get("bookmarks");

  let server = serverForFoo(engine);
  await SyncTestingInfrastructure(server);

  try {
    let collection = server.user("foo").collection("bookmarks");
    let serverModified = Date.now() / 1000 + 120;
    await resolveConflict(engine, collection, serverModified, guids => [{
      guid: guids.tb,
      index: 0,
    }, {
      guid: guids.res,
      index: 1,
      children: [{
        guid: guids.irc,
        index: 0,
      }, {
        guid: guids.bz,
        index: 1,
      }, {
        guid: guids.mdn,
        index: 2,
      }, {
        guid: guids.nightly,
        index: 3,
      }, {
        guid: guids.support,
        index: 4,
      }, {
        guid: guids.customize,
        index: 5,
      }],
    }, {
      guid: guids.fx,
      index: 2,
    }], "Should use remote order as base if local is older");
  } finally {
    await engine.wipeClient();
    await Service.startOver();
    await promiseStopServer(server);
  }
});

add_task(async function test_bookmark_order() {
  let engine = new BookmarksEngine(Service);
  let store = engine._store;

  _("Starting with a clean slate of no bookmarks");
  await store.wipe();
  await assertBookmarksTreeMatches("", [{
    guid: PlacesUtils.bookmarks.menuGuid,
    index: 0,
  }, {
    guid: PlacesUtils.bookmarks.toolbarGuid,
    index: 1,
  }, {
    // Index 2 is the tags root. (Root indices depend on the order of the
    // `CreateRoot` calls in `Database::CreateBookmarkRoots`).
    guid: PlacesUtils.bookmarks.unfiledGuid,
    index: 3,
  }, {
    guid: PlacesUtils.bookmarks.mobileGuid,
    index: 4,
  }], "clean slate");

  function bookmark(name, parent) {
    let bm = new Bookmark("http://weave.server/my-bookmark");
    bm.id = name;
    bm.title = name;
    bm.bmkUri = "http://uri/";
    bm.parentid = parent || "unfiled";
    bm.tags = [];
    return bm;
  }

  function folder(name, parent, children) {
    let bmFolder = new BookmarkFolder("http://weave.server/my-bookmark-folder");
    bmFolder.id = name;
    bmFolder.title = name;
    bmFolder.parentid = parent || "unfiled";
    bmFolder.children = children;
    return bmFolder;
  }

  async function apply(record) {
    store._childrenToOrder = {};
    await store.applyIncoming(record);
    await store._orderChildren();
    delete store._childrenToOrder;
  }
  let id10 = "10_aaaaaaaaa";
  _("basic add first bookmark");
  await apply(bookmark(id10, ""));
  await assertBookmarksTreeMatches("", [{
    guid: PlacesUtils.bookmarks.menuGuid,
    index: 0,
  }, {
    guid: PlacesUtils.bookmarks.toolbarGuid,
    index: 1,
  }, {
    guid: PlacesUtils.bookmarks.unfiledGuid,
    index: 3,
    children: [{
      guid: id10,
      index: 0,
    }],
  }, {
    guid: PlacesUtils.bookmarks.mobileGuid,
    index: 4,
  }], "basic add first bookmark");
  let id20 = "20_aaaaaaaaa";
  _("basic append behind 10");
  await apply(bookmark(id20, ""));
  await assertBookmarksTreeMatches("", [{
    guid: PlacesUtils.bookmarks.menuGuid,
    index: 0,
  }, {
    guid: PlacesUtils.bookmarks.toolbarGuid,
    index: 1,
  }, {
    guid: PlacesUtils.bookmarks.unfiledGuid,
    index: 3,
    children: [{
      guid: id10,
      index: 0,
    }, {
      guid: id20,
      index: 1,
    }],
  }, {
    guid: PlacesUtils.bookmarks.mobileGuid,
    index: 4,
  }], "basic append behind 10");

  let id31 = "31_aaaaaaaaa";
  let id30 = "f30_aaaaaaaa";
  _("basic create in folder");
  await apply(bookmark(id31, id30));
  let f30 = folder(id30, "", [id31]);
  await apply(f30);
  await assertBookmarksTreeMatches("", [{
    guid: PlacesUtils.bookmarks.menuGuid,
    index: 0,
  }, {
    guid: PlacesUtils.bookmarks.toolbarGuid,
    index: 1,
  }, {
    guid: PlacesUtils.bookmarks.unfiledGuid,
    index: 3,
    children: [{
      guid: id10,
      index: 0,
    }, {
      guid: id20,
      index: 1,
    }, {
      guid: id30,
      index: 2,
      children: [{
        guid: id31,
        index: 0,
      }],
    }],
  }, {
    guid: PlacesUtils.bookmarks.mobileGuid,
    index: 4,
  }], "basic create in folder");

  let id41 = "41_aaaaaaaaa";
  let id40 = "f40_aaaaaaaa";
  _("insert missing parent -> append to unfiled");
  await apply(bookmark(id41, id40));
  await assertBookmarksTreeMatches("", [{
    guid: PlacesUtils.bookmarks.menuGuid,
    index: 0,
  }, {
    guid: PlacesUtils.bookmarks.toolbarGuid,
    index: 1,
  }, {
    guid: PlacesUtils.bookmarks.unfiledGuid,
    index: 3,
    children: [{
      guid: id10,
      index: 0,
    }, {
      guid: id20,
      index: 1,
    }, {
      guid: id30,
      index: 2,
      children: [{
        guid: id31,
        index: 0,
      }],
    }, {
      guid: id41,
      index: 3,
      requestedParent: id40,
    }],
  }, {
    guid: PlacesUtils.bookmarks.mobileGuid,
    index: 4,
  }], "insert missing parent -> append to unfiled");

  let id42 = "42_aaaaaaaaa";

  _("insert another missing parent -> append");
  await apply(bookmark(id42, id40));
  await assertBookmarksTreeMatches("", [{
    guid: PlacesUtils.bookmarks.menuGuid,
    index: 0,
  }, {
    guid: PlacesUtils.bookmarks.toolbarGuid,
    index: 1,
  }, {
    guid: PlacesUtils.bookmarks.unfiledGuid,
    index: 3,
    children: [{
      guid: id10,
      index: 0,
    }, {
      guid: id20,
      index: 1,
    }, {
      guid: id30,
      index: 2,
      children: [{
        guid: id31,
        index: 0,
      }],
    }, {
      guid: id41,
      index: 3,
      requestedParent: id40,
    }, {
      guid: id42,
      index: 4,
      requestedParent: id40,
    }],
  }, {
    guid: PlacesUtils.bookmarks.mobileGuid,
    index: 4,
  }], "insert another missing parent -> append");

  _("insert folder -> move children and followers");
  let f40 = folder(id40, "", [id41, id42]);
  await apply(f40);
  await assertBookmarksTreeMatches("", [{
    guid: PlacesUtils.bookmarks.menuGuid,
    index: 0,
  }, {
    guid: PlacesUtils.bookmarks.toolbarGuid,
    index: 1,
  }, {
    guid: PlacesUtils.bookmarks.unfiledGuid,
    index: 3,
    children: [{
      guid: id10,
      index: 0,
    }, {
      guid: id20,
      index: 1,
    }, {
      guid: id30,
      index: 2,
      children: [{
        guid: id31,
        index: 0,
      }],
    }, {
      guid: id40,
      index: 3,
      children: [{
        guid: id41,
        index: 0,
      }, {
        guid: id42,
        index: 1,
      }]
    }],
  }, {
    guid: PlacesUtils.bookmarks.mobileGuid,
    index: 4,
  }], "insert folder -> move children and followers");

  _("Moving 41 behind 42 -> update f40");
  f40.children = [id42, id41];
  await apply(f40);
  await assertBookmarksTreeMatches("", [{
    guid: PlacesUtils.bookmarks.menuGuid,
    index: 0,
  }, {
    guid: PlacesUtils.bookmarks.toolbarGuid,
    index: 1,
  }, {
    guid: PlacesUtils.bookmarks.unfiledGuid,
    index: 3,
    children: [{
      guid: id10,
      index: 0,
    }, {
      guid: id20,
      index: 1,
    }, {
      guid: id30,
      index: 2,
      children: [{
        guid: id31,
        index: 0,
      }],
    }, {
      guid: id40,
      index: 3,
      children: [{
        guid: id42,
        index: 0,
      }, {
        guid: id41,
        index: 1,
      }]
    }],
  }, {
    guid: PlacesUtils.bookmarks.mobileGuid,
    index: 4,
  }], "Moving 41 behind 42 -> update f40");

  _("Moving 10 back to front -> update 10, 20");
  f40.children = [id41, id42];
  await apply(f40);
  await assertBookmarksTreeMatches("", [{
    guid: PlacesUtils.bookmarks.menuGuid,
    index: 0,
  }, {
    guid: PlacesUtils.bookmarks.toolbarGuid,
    index: 1,
  }, {
    guid: PlacesUtils.bookmarks.unfiledGuid,
    index: 3,
    children: [{
      guid: id10,
      index: 0,
    }, {
      guid: id20,
      index: 1,
    }, {
      guid: id30,
      index: 2,
      children: [{
        guid: id31,
        index: 0,
      }],
    }, {
      guid: id40,
      index: 3,
      children: [{
        guid: id41,
        index: 0,
      }, {
        guid: id42,
        index: 1,
      }]
    }],
  }, {
    guid: PlacesUtils.bookmarks.mobileGuid,
    index: 4,
  }], "Moving 10 back to front -> update 10, 20");

  _("Moving 20 behind 42 in f40 -> update 50");
  await apply(bookmark(id20, id40));
  await assertBookmarksTreeMatches("", [{
    guid: PlacesUtils.bookmarks.menuGuid,
    index: 0,
  }, {
    guid: PlacesUtils.bookmarks.toolbarGuid,
    index: 1,
  }, {
    guid: PlacesUtils.bookmarks.unfiledGuid,
    index: 3,
    children: [{
      guid: id10,
      index: 0,
    }, {
      guid: id30,
      index: 1,
      children: [{
        guid: id31,
        index: 0,
      }],
    }, {
      guid: id40,
      index: 2,
      children: [{
        guid: id41,
        index: 0,
      }, {
        guid: id42,
        index: 1,
      }, {
        guid: id20,
        index: 2,
      }]
    }],
  }, {
    guid: PlacesUtils.bookmarks.mobileGuid,
    index: 4,
  }], "Moving 20 behind 42 in f40 -> update 50");

  _("Moving 10 in front of 31 in f30 -> update 10, f30");
  await apply(bookmark(id10, id30));
  f30.children = [id10, id31];
  await apply(f30);
  await assertBookmarksTreeMatches("", [{
    guid: PlacesUtils.bookmarks.menuGuid,
    index: 0,
  }, {
    guid: PlacesUtils.bookmarks.toolbarGuid,
    index: 1,
  }, {
    guid: PlacesUtils.bookmarks.unfiledGuid,
    index: 3,
    children: [{
      guid: id30,
      index: 0,
      children: [{
        guid: id10,
        index: 0,
      }, {
        guid: id31,
        index: 1,
      }],
    }, {
      guid: id40,
      index: 1,
      children: [{
        guid: id41,
        index: 0,
      }, {
        guid: id42,
        index: 1,
      }, {
        guid: id20,
        index: 2,
      }]
    }],
  }, {
    guid: PlacesUtils.bookmarks.mobileGuid,
    index: 4,
  }], "Moving 10 in front of 31 in f30 -> update 10, f30");

  _("Moving 20 from f40 to f30 -> update 20, f30");
  await apply(bookmark(id20, id30));
  f30.children = [id10, id20, id31];
  await apply(f30);
  await assertBookmarksTreeMatches("", [{
    guid: PlacesUtils.bookmarks.menuGuid,
    index: 0,
  }, {
    guid: PlacesUtils.bookmarks.toolbarGuid,
    index: 1,
  }, {
    guid: PlacesUtils.bookmarks.unfiledGuid,
    index: 3,
    children: [{
      guid: id30,
      index: 0,
      children: [{
        guid: id10,
        index: 0,
      }, {
        guid: id20,
        index: 1,
      }, {
        guid: id31,
        index: 2,
      }],
    }, {
      guid: id40,
      index: 1,
      children: [{
        guid: id41,
        index: 0,
      }, {
        guid: id42,
        index: 1,
      }]
    }],
  }, {
    guid: PlacesUtils.bookmarks.mobileGuid,
    index: 4,
  }], "Moving 20 from f40 to f30 -> update 20, f30");

  _("Move 20 back to front -> update 20, f30");
  await apply(bookmark(id20, ""));
  f30.children = [id10, id31];
  await apply(f30);
  await assertBookmarksTreeMatches("", [{
    guid: PlacesUtils.bookmarks.menuGuid,
    index: 0,
  }, {
    guid: PlacesUtils.bookmarks.toolbarGuid,
    index: 1,
  }, {
    guid: PlacesUtils.bookmarks.unfiledGuid,
    index: 3,
    children: [{
      guid: id30,
      index: 0,
      children: [{
        guid: id10,
        index: 0,
      }, {
        guid: id31,
        index: 1,
      }],
    }, {
      guid: id40,
      index: 1,
      children: [{
        guid: id41,
        index: 0,
      }, {
        guid: id42,
        index: 1,
      }],
    }, {
      guid: id20,
      index: 2,
    }],
  }, {
    guid: PlacesUtils.bookmarks.mobileGuid,
    index: 4,
  }], "Move 20 back to front -> update 20, f30");

  engine.resetClient();
  await engine.finalize();
});
