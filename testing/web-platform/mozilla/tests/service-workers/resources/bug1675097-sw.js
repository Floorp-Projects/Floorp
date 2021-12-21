// We use promises because the message and fetch events do not have a guaranteed
// order, since they come from different task sources.
var resolvePortPromise;
var portPromise = new Promise(resolve => resolvePortPromise = resolve);
var resolveResolveResponsePromise;
var resolveResponsePromise = new Promise(resolve => resolveResolveResponsePromise = resolve);

self.addEventListener('fetch', event => {
    if (event.request.url.indexOf('inner') !== -1) {
        event.respondWith(new Promise(resolve => {
            resolveResolveResponsePromise(resolve);
        }));
        portPromise.then(port => port.postMessage('intercepted'));
    }
});

self.addEventListener('message', event => {
    if (event.data.type === 'register') {
        resolvePortPromise(event.data.port);
    }
    else if (event.data.type === 'ack') {
        self.clients.matchAll()
            .then(() => resolveResponsePromise)
            .then(resolveResponse => resolveResponse(new Response('inner iframe')));
    }
});
