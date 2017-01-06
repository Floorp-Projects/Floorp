addEventListener('fetch', evt => {
  if (evt.request.url.endsWith('slow')) {
    let start = Date.now();
    while(Date.now() - start < 2000);
  }
});
