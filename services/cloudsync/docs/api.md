### Importing the JS module

````
Cu.import("resource://gre/modules/CloudSync.jsm");

let cloudSync = CloudSync();
console.log(cloudSync); // Module is imported
````

### cloudSync.local

#### id

Local device ID. Is unique.

````
let localId = cloudSync.local.id;
````

#### name

Local device name.

````
let localName = cloudSync.local.name;
````

### CloudSync.tabs

#### addEventListener(type, callback)

Add an event handler for Tabs events. Valid type is `change`. The callback receives no arguments.

````
function handleTabChange() {
  // Tabs have changed.
}

cloudSync.tabs.addEventListener("change", handleTabChange);
````

Change events are emitted when a tab is opened or closed, when a tab is selected, or when the page changes for an open tab.

#### removeEventListener(type, callback)

Remove an event handler. Pass the type and function that were passed to addEventListener.

````
cloudSync.tabs.removeEventListener("change", handleTabChange);
````

#### mergeRemoteTabs(client, tabs)

Merge remote tabs from upstream by updating existing items, adding new tabs, and deleting existing tabs. Accepts a client and a list of tabs. Returns a promise.

````
let remoteClient = {
  id: "fawe78",
  name: "My Firefox client",
};

let remoteTabs = [
    {title: "Google",
     url: "https://www.google.com",
     icon: "https://www.google.com/favicon.ico",
     lastUsed: 1400799296192},
    {title: "Reddit",
     url: "http://www.reddit.com",
     icon: "http://www.reddit.com/favicon.ico",
     lastUsed: 1400799296192
     deleted: true},
];

cloudSync.tabs.mergeRemoteTabs(client, tabs).then(
  function() {
    console.log("merge complete");
  }
);
````

#### getLocalTabs()

Returns a promise. Passes a list of local tabs when complete.

````
cloudSync.tabs.getLocalTabs().then(
  function(tabs) {
    console.log(JSON.stringify(tabs));
  }
);
````

#### clearRemoteTabs(client)

Clears all tabs for a remote client.

````
let remoteClient = {
  id: "fawe78",
  name: "My Firefox client",
};

cloudSync.tabs.clearRemoteTabs(client);
````

### cloudSync.bookmarks

#### getRootFolder(name)

Gets the named root folder, creating it if it doesn't exist. The root folder object has a number of methods (see the next section for details).

````
cloudSync.bookmarks.getRootFolder("My Bookmarks").then(
  function(rootFolder) {
    console.log(rootFolder);
  }
);
````

### cloudSync.bookmarks.RootFolder

This is a root folder object for bookmarks, created by `cloudSync.bookmarks.getRootFolder`.

#### BOOKMARK

Bookmark type. Used in results objects.

````
let bookmarkType = rootFolder.BOOKMARK;
````

#### FOLDER

Folder type. Used in results objects.

````
let folderType = rootFolder.FOLDER;
````

#### SEPARATOR

Separator type. Used in results objects.

````
let separatorType = rootFolder.SEPARATOR;
````

#### addEventListener(type, callback)

Add an event handler for Tabs events. Valid types are `add, remove, change, move`. The callback receives an ID corresponding to the target item.

````
function handleBoookmarkEvent(id) {
  console.log("event for id:", id);
}

rootFolder.addEventListener("add", handleBookmarkEvent);
rootFolder.addEventListener("remove", handleBookmarkEvent);
rootFolder.addEventListener("change", handleBookmarkEvent);
rootFolder.addEventListener("move", handleBookmarkEvent);
````

#### removeEventListener(type, callback)

Remove an event handler. Pass the type and function that were passed to addEventListener.

````
rootFolder.removeEventListener("add", handleBookmarkEvent);
rootFolder.removeEventListener("remove", handleBookmarkEvent);
rootFolder.removeEventListener("change", handleBookmarkEvent);
rootFolder.removeEventListener("move", handleBookmarkEvent);
````

#### getLocalItems()

Callback receives a list of items on the local client. Results have the following form:

````
{
  id: "faw8e7f", // item guid
  parent: "f7sydf87y", // parent folder guid
  dateAdded: 1400799296192, // timestamp
  lastModified: 1400799296192, // timestamp
  uri: "https://www.google.ca", // null for FOLDER and SEPARATOR
  title: "Google"
  type: rootFolder.BOOKMARK, // should be one of rootFolder.{BOOKMARK, FOLDER, SEPARATOR},
  index: 0 // must be unique among folder items
}
````

````
rootFolder.getLocalItems().then(
  function(items) {
    console.log(JSON.stringify(items));
  }
);
````

#### getLocalItemsById([...])

Callback receives a list of items, specified by ID, on the local client. Results have the same form as `getLocalItems()` above.

````
rootFolder.getLocalItemsById(["213r23f", "f22fy3f3"]).then(
  function(items) {
    console.log(JSON.stringify(items));
  }
);
````

#### mergeRemoteItems([...])

Merge remote items from upstream by updating existing items, adding new items, and deleting existing items. Folders are created first so that subsequent operations will succeed. Items have the same form as `getLocalItems()` above. Items that do not have an ID will have an ID generated for them. The results structure will contain this generated ID.

````
rootFolder.mergeRemoteItems([
        {
          id: 'f2398f23',
          type: rootFolder.FOLDER,
          title: 'Folder 1',
          parent: '9f8237f928'
        },
        {
          id: '9f8237f928',
          type: rootFolder.FOLDER,
          title: 'Folder 0',
        }
      ]).then(
  function(items) {
    console.log(items); // any generated IDs are filled in now
    console.log("merge completed");
  }
);
````