self.addEventListener("message", async e => {
  let res = {};

  switch (e.data.type) {
    case "GetHWConcurrency":
      res.result = "OK";
      res.value = navigator.hardwareConcurrency;
      break;

    default:
      res.result = "ERROR";
  }

  e.source.postMessage(res);
});
