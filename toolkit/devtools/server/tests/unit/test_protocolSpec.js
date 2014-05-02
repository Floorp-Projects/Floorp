const run_test = Test(function*() {
  initTestDebuggerServer();
  const connection = DebuggerServer.connectPipe();
  const client = Async(new DebuggerClient(connection));

  yield client.connect();

  const response = yield client.request({
    to: "root",
    type: "protocolDescription"
  });

  assert(response.from == "root");
  assert(typeof(response.types) === "object");

  yield client.close();
});
