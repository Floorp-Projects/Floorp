onmessage = e => {
  let runnableStr = `(() => {return (${e.data.callback});})();`;
  let runnable = eval(runnableStr); // eslint-disable-line no-eval
  runnable.call(this).then(result => {
    postMessage({ result });
  });
};
