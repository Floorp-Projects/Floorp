function waitForLoad() {
  return new Promise(resolve => addEventListener('load', resolve))
    .then(() => delay(10));
}

function delay(timeout) {
  return new Promise(resolve => step_timeout(() => resolve(), 10));
}

function busy(work) {
  return delay(10).then(() => new Promise(resolve => {
    step_timeout(() => {
      let end = performance.now() + work;
      while (performance.now() < end) {

      }

      resolve();
    }, 1);
  }));
}

function getThrottlingRate(delay) {
  return new Promise(resolve => {
    let start = performance.now();
    setTimeout(() => {
      let rate = Math.floor((performance.now() - start) / delay);
      resolve(rate);
    }, delay);
  });
}

function addElement(t, element, src) {
  return new Promise((resolve, reject) => {
    let e = document.createElement(element);
    e.addEventListener('load', () => resolve(e));
    if (src) {
      e.src = src;
    }
    document.body.appendChild(e);
    t.add_cleanup(() => e.remove());
  });
}

function inFrame(t) {
  return addElement(t, "iframe", "resources/test.html")
    .then(frame => delay(10).then(() => Promise.resolve(frame.contentWindow)));
}

function addWebSocket(t, url) {
  return new Promise((resolve, reject) => {
    let socket = new WebSocket(url);
    socket.onopen = () => {
      t.add_cleanup(() => socket.close());
      resolve();
    };
    socket.onerror = reject;
  });
}

function addRTCPeerConnection(t) {
  return new Promise((resolve, reject) => {
    let connection = new RTCPeerConnection();
    t.add_cleanup(() => {
      connection.close()
    });

    resolve();
  });
}

function addIndexedDB(t) {
  return new Promise((resolve, reject) => {
    let iDBState = {
      running: false,
      db: null
    };

    let req = indexedDB.open("testDB", 1);

    req.onupgradeneeded = e => {
      let db = e.target.result;
      let store = db.createObjectStore("testOS", {keyPath: "id"});
      let index = store.createIndex("index", ["col"]);
    };

    req.onsuccess = e => {
      let db = iDBState.db = e.target.result;
      let store = db.transaction("testOS", "readwrite").objectStore("testOS");
      let ctr = 0;

      iDBState.running = true;

      function putLoop() {
        if (!iDBState.running) {
          return;
        }

        let req = store.put({id: ctr++, col: "foo"});
        req.onsuccess = putLoop;

        if (!iDBState.request) {
          iDBState.request = req;
        }
      }

      putLoop();
      resolve();
    };

    t.add_cleanup(() => {
      iDBState.running = false;
      iDBState.db && iDBState.db.close();
      iDBState.db = null;
    });
  });
}

function addWebAudio(t) {
  return new Promise(resolve => {
    let context = new (window.AudioContext || window.webkitAudioContext)();
    context.onstatechange = () => (context.state === "running") && resolve();

    let gain = context.createGain();
    gain.gain.value = 0.1;
    gain.connect(context.destination);

    let webaudionode = context.createOscillator();
    webaudionode.type = 'square';
    webaudionode.frequency.value = 440; // value in hertz
    webaudionode.connect(gain);
    webaudionode.start();

    t.add_cleanup(() => webaudionode.stop());
  });
}
