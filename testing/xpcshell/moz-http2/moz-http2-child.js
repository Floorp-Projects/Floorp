function sendBackResponse(evalResult, e) {
  const output = { result: evalResult, error: "", errorStack: ""};
  if (e) {
    output.error = e.toString();
    output.errorStack = e.stack;
  }
  process.send(output);
}

process.on('message', (msg) => {
  const code = msg.code;
  let evalResult = null;
  try {
    evalResult = eval(code);
    if (evalResult instanceof Promise) {
      evalResult
        .then(x => sendBackResponse(x))
        .catch(e => sendBackResponse(undefined, e));
      return;
    }
  } catch (e) {
    sendBackResponse(undefined, e);
    return;
  }
  sendBackResponse(evalResult)
});
