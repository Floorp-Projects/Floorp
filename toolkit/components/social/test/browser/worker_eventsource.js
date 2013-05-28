let port, es;

let url = "https://example.com/browser/toolkit/components/social/test/browser/eventsource.resource";

function ok(a, msg) {
  port.postMessage({topic: "eventSourceTest",
                   result: {ok: a, msg: msg}});
}

function is(a, b, msg) {
  port.postMessage({topic: "eventSourceTest",
                   result: {is: a, match: b, msg: msg}});
}

function esListener(e) {
  esListener.msg_ok = true;
}

function esOnmessage(e) {
  ok(true, "onmessage test");
  ok(esListener.msg_ok, "listener test");
  es.close();
  port.postMessage({topic: "pong"});
}

function doTest() {
  try {
    es = new EventSource(url);
    is(es.url, url, "eventsource.resource accessed", "we can create an eventsource instance");
    es.addEventListener('test-message', esListener, true);
    es.onmessage = esOnmessage;
  } catch (e) {}
  ok(!!es, "we can create an eventsource instance");
}

onconnect = function(e) {
  port = e.ports[0];
  port.onmessage = function(e) {
    if (e.data.topic == "ping") {
      doTest();
    }
  }
}
