'use strict';

self.addEventListener('fetch', event => {
  if (event.request.url.indexOf('fetch_video.py') !== -1) {
    // A no-cors media range request /should not/ be intercepted.
    // Respond with some text to cause an error.
    event.respondWith(new Response('intercepted'));
  }
  else if (event.request.url.indexOf('blank.html') !== -1) {
    // A no-cors media non-range request /should/ be intercepted.
    // Respond with an image to avoid an error.
    event.respondWith(fetch('green.png'));
  }
});
