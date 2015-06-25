function handleString(event) {
  event.respondWith(new Response('Test string'));
}

function handleBlob(event) {
  event.respondWith(new Response(new Blob(['Test blob'])));
}

function handleReferrer(event) {
  event.respondWith(new Response(new Blob(
    ['Referrer: ' + event.request.referrer])));
}

function handleNullBody(event) {
  event.respondWith(new Response());
}

function handleFetch(event) {
  event.respondWith(fetch('other.html'));
}

function handleFormPost(event) {
  event.respondWith(new Promise(function(resolve) {
      event.request.text()
        .then(function(result) {
            resolve(new Response(event.request.method + ':' + result));
          });
    }));
}

var logForMultipleRespondWith = '';

function handleMultipleRespondWith(event) {
  for (var i = 0; i < 3; ++i) {
    logForMultipleRespondWith += '(' + i + ')';
    try {
      event.respondWith(new Response(logForMultipleRespondWith));
    } catch (e) {
      logForMultipleRespondWith += '[' + e.name + ']';
    }
  }
}

var lastResponseForUsedCheck = undefined;

function handleUsedCheck(event) {
  if (!lastResponseForUsedCheck) {
    event.respondWith(fetch('other.html').then(function(response) {
        lastResponseForUsedCheck = response;
        return response;
      }));
  } else {
    event.respondWith(new Response(
        'bodyUsed: ' + lastResponseForUsedCheck.bodyUsed));
  }
}

self.addEventListener('fetch', function(event) {
    var url = event.request.url;
    var handlers = [
      { pattern: '?string', fn: handleString },
      { pattern: '?blob', fn: handleBlob },
      { pattern: '?referrer', fn: handleReferrer },
      { pattern: '?ignore', fn: function() {} },
      { pattern: '?null', fn: handleNullBody },
      { pattern: '?fetch', fn: handleFetch },
      { pattern: '?form-post', fn: handleFormPost },
      { pattern: '?multiple-respond-with', fn: handleMultipleRespondWith },
      { pattern: '?used-check', fn: handleUsedCheck }
    ];

    var handler = null;
    for (var i = 0; i < handlers.length; ++i) {
      if (url.indexOf(handlers[i].pattern) != -1) {
        handler = handlers[i];
        break;
      }
    }

    if (handler) {
      handler.fn(event);
    } else {
      event.respondWith(new Response(new Blob(
        ['Service Worker got an unexpected request: ' + url])));
    }
  });
