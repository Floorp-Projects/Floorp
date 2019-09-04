window.doSubscribe = async function(applicationServerKey) {
  const registration = await navigator.serviceWorker.register("./sw.js");
  const sub = await registration.pushManager.subscribe({
    applicationServerKey,
  });
  return sub.toJSON();
};

window.doGetSubscription = async function() {
  const registration = await navigator.serviceWorker.register("./sw.js");
  const sub = await registration.pushManager.getSubscription();
  if (sub) {
    return sub.toJSON();
  }

  return null;
};

window.doUnsubscribe = async function() {
  const registration = await navigator.serviceWorker.register("./sw.js");
  const sub = await registration.pushManager.getSubscription();
  sub.unsubscribe();
  return {};
};

window.doWaitForPushEvent = function() {
  return new Promise(resolve => {
    navigator.serviceWorker.addEventListener("message", function(e) {
      if (e.data.type === "push") {
        resolve(e.data.payload);
      }
    });
  });
};

window.doWaitForSubscriptionChange = function() {
  return new Promise(resolve => {
    navigator.serviceWorker.addEventListener("message", function(e) {
      if (e.data.type === "pushsubscriptionchange") {
        resolve(e.data.type);
      }
    });
  });
};
