// Holds the promise that the "resolve.fakehtml" call attempts to resolve.
// This is "the SW's promise" that other parts of the test refer to.
var promise;
// Stores the resolve funcution for the current promise.
var pending_resolve_func = null;
// Unique ID to determine which service worker is being used.
const ID = Math.random();

function callAndResetResolve() {
  var local_resolve = pending_resolve_func;
  pending_resolve_func = null;
  local_resolve();
}

self.addEventListener('fetch', function(event) {
  fetchEventHandler(event);
})

async function fetchEventHandler(event){
  var request_url = new URL(event.request.url);
  var url_search = request_url.search.substr(1);
  request_url.search = "";
  if ( request_url.href.endsWith('waitUntilResolved.fakehtml') ) {

      if (pending_resolve_func != null) {
        // Respond with an error if there is already a pending promise
        event.respondWith(Response.error());
        return;
      }

      // Create the new promise.
      promise = new Promise(function(resolve) {
        pending_resolve_func = resolve;
      });
      event.waitUntil(promise);

      event.respondWith(new Response(`
        <html>
        Promise created by ${url_search}
        <script>self.parent.postMessage({ ID:${ID}, source: "${url_search}"
          });</script>
        </html>
        `, {headers: {'Content-Type': 'text/html'}}
      ));

  }
  else if ( request_url.href.endsWith('resolve.fakehtml') ) {
    var has_pending = !!pending_resolve_func;

    event.respondWith(new Response(`
      <html>
      Promise settled for ${url_search}
      <script>self.parent.postMessage({ ID:${ID}, has_pending: ${has_pending},
        source: "${url_search}"  });</script>
      </html>
    `, {headers: {'Content-Type': 'text/html'}}));

    if (has_pending) {
      callAndResetResolve();
    }
  }
}