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
  console.info("Web worker received message", event.data);

  if (event.data === "setIndexedDB") {
    setIndexedDB().then(() => {
      self.postMessage("indexedDBSet");
    });
  } else if (event.data === "setIndexedDBNested") {
    console.info("set state nested");
    // Rather than setting indexedDB in this worker spawn a nested worker to set
    // indexedDB.
    let nestedWorker = new Worker("file_web_worker.js");
    nestedWorker.postMessage("setIndexedDB");
    nestedWorker.onmessage = () => {
      console.info("IndexedDB set in nested worker");
      self.postMessage("indexedDBSet");
    };
  }
};
