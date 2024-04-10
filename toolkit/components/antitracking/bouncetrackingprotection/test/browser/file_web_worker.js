// A web worker which can set indexedDB.

function setIndexedDB() {
  return new Promise((resolve, reject) => {
    let request = self.indexedDB.open("bounce", 1);
    request.onsuccess = () => {
      console.info("Opened indexedDB");
      resolve();
    };
    request.onerror = event => {
      console.error("Error opening indexedDB", event);
      reject();
    };
    request.onupgradeneeded = event => {
      console.info("Initializing indexedDB");
      let db = event.target.result;
      db.createObjectStore("bounce");
    };
  });
}

self.onmessage = function (event) {
  if (event.data === "setIndexedDB") {
    setIndexedDB().then(() => {
      self.postMessage("indexedDBSet");
    });
  }
};
