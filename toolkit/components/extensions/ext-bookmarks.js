const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/PlacesUtils.jsm");
var Bookmarks = PlacesUtils.bookmarks;

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
  ignoreEvent,
  runSafe,
} = ExtensionUtils;

XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

function getTree(rootGuid, onlyChildren) {
  function convert(node, parent) {
    let treenode = {
      id: node.guid,
      title: node.title || "",
      index: node.index,
      dateAdded: node.dateAdded / 1000,
    };

    if (parent && node.guid != Bookmarks.rootGuid) {
      treenode.parentId = parent.guid;
    }

    if (node.type == PlacesUtils.TYPE_X_MOZ_PLACE) {
      // This isn't quite correct. Recently Bookmarked ends up here ...
      treenode.url = node.uri;
    } else {
      treenode.dateGroupModified = node.lastModified / 1000;

      if (node.children && !onlyChildren) {
        treenode.children = node.children.map(child => convert(child, node));
      }
    }

    return treenode;
  }

  return PlacesUtils.promiseBookmarksTree(rootGuid, {
    excludeItemsCallback: aItem => {
      if (aItem.type == PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR)
        return true;
      return aItem.annos &&
             aItem.annos.find(a => a.name == PlacesUtils.EXCLUDE_FROM_BACKUP_ANNO);
    },
  }).then(root => {
    if (onlyChildren) {
      let children = root.children || [];
      return children.map(child => convert(child, root));
    } else {
      // It seems like the array always just contains the root node.
      return [convert(root, null)];
    }
  });
}

function convert(result) {
  let node = {
    id: result.guid,
    title: result.title || "",
    index: result.index,
    dateAdded: result.dateAdded.getTime(),
  };

  if (result.guid != Bookmarks.rootGuid) {
    node.parentId = result.parentGuid;
  }

  if (result.type == Bookmarks.TYPE_BOOKMARK) {
    node.url = result.url.href; // Output is always URL object.
  } else {
    node.dateGroupModified = result.lastModified.getTime();
  }

  return node;
}

extensions.registerPrivilegedAPI("bookmarks", (extension, context) => {
  return {
    bookmarks: {
      get: function(idOrIdList, callback) {
        let list = Array.isArray(idOrIdList) ? idOrIdList : [idOrIdList];

        Task.spawn(function* () {
          let bookmarks = [];
          for (let id of list) {
            let bookmark;
            try {
              bookmark = yield Bookmarks.fetch({guid: id});
              if (!bookmark) {
                // TODO: set lastError, not found
                return [];
              }
            } catch (e) {
              // TODO: set lastError, probably an invalid guid
              return [];
            }
            bookmarks.push(convert(bookmark));
          }
          return bookmarks;
        }).then(results => runSafe(context, callback, results));
      },

      getChildren: function(id, callback) {
        // TODO: We should optimize this.
        getTree(id, true).then(result => {
          runSafe(context, callback, result);
        }, reason => {
          // TODO: Set lastError
          runSafe(context, callback, []);
        });
      },

      getTree: function(callback) {
        getTree(Bookmarks.rootGuid, false).then(result => {
          runSafe(context, callback, result);
        }, reason => {
          runSafe(context, callback, []);
        });
      },

      getSubTree: function(id, callback) {
        getTree(id, false).then(result => {
          runSafe(context, callback, result);
        }, reason => {
          runSafe(context, callback, []);
        });
      },

      // search

      create: function(bookmark, callback) {
        let info = {
          title: bookmark.title || "",
        };

        // If url is NULL or missing, it will be a folder.
        if ("url" in bookmark && bookmark.url !== null) {
          info.type = Bookmarks.TYPE_BOOKMARK;
          info.url = bookmark.url || "";
        } else {
          info.type = Bookmarks.TYPE_FOLDER;
        }

        if ("index" in bookmark) {
          info.index = bookmark.index;
        }

        if ("parentId" in bookmark) {
          info.parentGuid = bookmark.parentId;
        } else {
          info.parentGuid = Bookmarks.unfiledGuid;
        }

        let failure = reason => {
          // TODO: set lastError.
          if (callback) {
            runSafe(context, callback, null);
          }
        };

        try {
          Bookmarks.insert(info).then(result => {
            if (callback) {
              runSafe(context, callback, convert(result));
            }
          }, failure);
        } catch(e) {
          failure(e);
        }
      },

      move: function(id, destination, callback) {
        let info = {
          guid: id
        };

        if ("parentId" in destination) {
          info.parentGuid = destination.parentId;
        }
        if ("index" in destination) {
          info.index = destination.index;
        }

        let failure = reason => {
          if (callback) {
            runSafe(context, callback, null);
          }
        };

        try {
          Bookmarks.update(info).then(result => {
            if (callback) {
              runSafe(context, callback, convert(result));
            }
          }, failure);
        } catch (e) {
          failure(e);
        }
      },

      update: function(id, changes, callback) {
        let info = {
          guid: id
        };

        if ("title" in changes) {
          info.title = changes.title;
        }
        if ("url" in changes) {
          info.url = changes.url;
        }

        let failure = reason => {
          if (callback) {
            runSafe(context, callback, null);
          }
        };

        try {
          Bookmarks.update(info).then(result => {
            if (callback) {
              runSafe(context, callback, convert(result));
            }
          }, failure);
        } catch (e) {
          failure(e);
        }
      },

      remove: function(id, callback) {
        let info = {
          guid: id
        };

        let failure = reason => {
          if (callback) {
            runSafe(context, callback, null);
          }
        };

        try {
          Bookmarks.remove(info).then(result => {
            if (callback) {
              // The API doesn't give you the old bookmark at the moment
              runSafe(context, callback);
            }
          }, failure);
        } catch (e) {
          failure(e);
        }
      }
    }
  };
});


