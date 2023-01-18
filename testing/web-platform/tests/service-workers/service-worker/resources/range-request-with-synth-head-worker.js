// This worker is meant to test range requests where the initial response
// synthesizes a 206 Partial Content response with leading bytes.
// Then the worker lets subsequent range requests fall back to network.

let initial = true;
function is_initial_request() {
  const old = initial;
  initial = false;
  return old;
}

self.addEventListener('fetch', e => {
    const url = new URL(e.request.url);
    if (url.search.indexOf('VIDEO') == -1) {
      // Fall back for non-video.
      return;
    }

    // Make the first request go cross-origin.
    if (is_initial_request()) {
      // Consistent with fetch-access-control.py?VIDEO
      const init = {
        status: 206,
        headers: {
          "Accept-Ranges": "bytes",
          "Content-Type": "video/ogg",
          "Content-Range": "bytes 0-1/18645",
          "Content-Length": "2",
        },
      };
      e.respondWith(new Response("Og", init));
      return;
    }

    // Fall back for subsequent range requests.
  });
