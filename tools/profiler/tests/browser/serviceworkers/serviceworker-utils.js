async function registerServiceWorkerAndWait() {
  console.log("Registering the serviceworker.");
  await navigator.serviceWorker.register("./serviceworker_cache_first.js", {
    scope: "./",
  });

  await navigator.serviceWorker.ready;
  console.log("The service worker is ready.");
}
