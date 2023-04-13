export async function worker_function() {

let wt = null;

async function processMessage(e) {
  const target = this;

  function respond(data) {
    target.postMessage(Object.assign(data, {rqid: e.data.rqid}));
  }

  // ORB will block errors (webtransport_url('custom-response.py?:status=404');)
  // so we need to try/catch
  try {
    switch (e.data.op) {
    case 'open': {
      wt = new WebTransport(e.data.url);
      await wt.ready;
      respond({ack: 'open'});
      break;
    }

    case 'openandclose': {
      wt = new WebTransport(e.data.url);
      await wt.ready;
      wt.close();
      await wt.closed;
      respond({ack: 'openandclose'});
      break;
    }

    case 'close': {
      wt.close();
      await wt.closed;
      respond({ack: 'close'});
      break;
    }
    }
  } catch(e) {
    respond({failed: true});
  }
}

self.addEventListener('message', processMessage);

self.addEventListener('connect', ev => {
  // Shared worker case
  ev.ports[0].onmessage = processMessage;
});
}
