async function registerServiceWorkerAndWait(serviceWorkerFile) {
  if (!serviceWorkerFile) {
    throw new Error(
      "No service worker filename has been specified. Please specify a valid filename."
    );
  }

  console.log(`Registering the serviceworker "${serviceWorkerFile}".`);
  await navigator.serviceWorker.register(`./${serviceWorkerFile}`, {
    scope: "./",
  });

  await navigator.serviceWorker.ready;
  console.log("The service worker is ready.");
}
