self.onfetch = function(event) {
  if (event.request.url.indexOf('sync-xhr-doesnt-deadlock.data') == -1)
    return;
  event.respondWith(fetch('404resource?bustcache=' + Date.now()));
};
