function get_query_params(url) {
  var search = (new URL(url)).search;
  if (!search) {
    return {};
  }
  var ret = {};
  var params = search.substring(1).split('&');
  params.forEach(function(param) {
      var element = param.split('=');
      ret[decodeURIComponent(element[0])] = decodeURIComponent(element[1]);
    });
  return ret;
}

function get_request_init(params) {
  var init = {};
  if (params['method']) {
    init['method'] = params['method'];
  }
  if (params['mode']) {
    init['mode'] = params['mode'];
  }
  if (params['credentials']) {
    init['credentials'] = params['credentials'];
  }
  return init;
}

self.addEventListener('fetch', function(event) {
    var params = get_query_params(event.request.url);
    var init = get_request_init(params);
    var url = params['url'];
    if (params['ignore']) {
      return;
    }
    if (params['reject']) {
      event.respondWith(new Promise(function(resolve, reject) {
          reject();
        }));
      return;
    }
    if (params['resolve-null']) {
      event.respondWith(new Promise(function(resolve) {
          resolve(null);
        }));
      return;
    }
    if (params['generate-png']) {
      var binary = atob(
          'iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAAXNSR0IArs4c6QAAAA' +
          'RnQU1BAACxjwv8YQUAAAAJcEhZcwAADsQAAA7EAZUrDhsAAAAhSURBVDhPY3wro/Kf' +
          'gQLABKXJBqMGjBoAAqMGDLwBDAwAEsoCTFWunmQAAAAASUVORK5CYII=');
      var array = new Uint8Array(binary.length);
      for(var i = 0; i < binary.length; i++) {
        array[i] = binary.charCodeAt(i)
      };
      event.respondWith(new Response(new Blob([array], {type: 'image/png'})));
      return;
    }
    event.respondWith(new Promise(function(resolve, reject) {
        var request = event.request;
        if (url) {
          request = new Request(url, init);
        }
        fetch(request).then(resolve, reject);
      }));
  });
