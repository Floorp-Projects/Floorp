/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

_("Making sure after processing incoming bookmarks, they show up in the right order");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

var check = async function(expected, message) {
  let root = await PlacesUtils.promiseBookmarksTree();

  let bookmarks = (function mapTree(children) {
    return children.map(child => {
      let result = {
        guid: child.guid,
        index: child.index,
      };
      if (child.children) {
        result.children = mapTree(child.children);
      }
      if (child.annos) {
        let orphanAnno = child.annos.find(
          anno => anno.name == "sync/parent");
        if (orphanAnno) {
          result.requestedParent = orphanAnno.value;
        }
      }
      return result;
    });
  }(root.children));

  _("Checking if the bookmark structure is", JSON.stringify(expected));
  _("Got bookmarks:", JSON.stringify(bookmarks));
  deepEqual(bookmarks, expected);
};

add_task(async function test_bookmark_order() {
  let store = new BookmarksEngine(Service)._store;
  initTestLogging("Trace");

  _("Starting with a clean slate of no bookmarks");
  store.wipe();
  await check([{
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
    let bookmark = new Bookmark("http://weave.server/my-bookmark");
    bookmark.id = name;
    bookmark.title = name;
    bookmark.bmkUri = "http://uri/";
    bookmark.parentid = parent || "unfiled";
    bookmark.tags = [];
    return bookmark;
  }

  function folder(name, parent, children) {
    let folder = new BookmarkFolder("http://weave.server/my-bookmark-folder");
    folder.id = name;
    folder.title = name;
    folder.parentid = parent || "unfiled";
    folder.children = children;
    return folder;
  }

  function apply(record) {
    store._childrenToOrder = {};
    store.applyIncoming(record);
    store._orderChildren();
    delete store._childrenToOrder;
  }
  let id10 = "10_aaaaaaaaa";
  _("basic add first bookmark");
  apply(bookmark(id10, ""));
  await check([{
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
  apply(bookmark(id20, ""));
  await check([{
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
  apply(bookmark(id31, id30));
  let f30 = folder(id30, "", [id31]);
  apply(f30);
  await check([{
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
  apply(bookmark(id41, id40));
  await check([{
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
  apply(bookmark(id42, id40));
  await check([{
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
  apply(f40);
  await check([{
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
  apply(f40);
  await check([{
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
  apply(f40);
  await check([{
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
  apply(bookmark(id20, id40));
  await check([{
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
  apply(bookmark(id10, id30));
  f30.children = [id10, id31];
  apply(f30);
  await check([{
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
  apply(bookmark(id20, id30));
  f30.children = [id10, id20, id31];
  apply(f30);
  await check([{
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
  apply(bookmark(id20, ""));
  f30.children = [id10, id31];
  apply(f30);
  await check([{
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

});
