'use strict';

self.addEventListener('fetch', event => {
  if (event.request.url.indexOf('fetch_video.py') !== -1) {
    // respond with a non-video
    event.respondWith(new Response('intercepted'));
  }
});
