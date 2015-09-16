"use strict";

var SOURCE_URL = getFileUrl("setBreakpoint-on-column-with-no-offsets-in-gcd-script.js");

function run_test() {
  return Task.spawn(function* () {
    do_test_pending();

    let global = testGlobal("test");
    loadSubScript(SOURCE_URL, global);
    Cu.forceGC();

    DebuggerServer.registerModule("xpcshell-test/testactors");
    DebuggerServer.init(() => true);
    DebuggerServer.addTestGlobal(global);
    let client = new DebuggerClient(DebuggerServer.connectPipe());
    yield connect(client);

    let tab = yield findTab(client, "test");
    let [, tabClient] = yield attachTab(client, tab);
    let [, threadClient] = yield attachThread(tabClient);
    yield resume(threadClient);

    let source = yield findSource(threadClient, SOURCE_URL);
    let sourceClient = threadClient.source(source);

    let location = { line: 6, column: 17 };
    let [packet, breakpointClient] = yield setBreakpoint(sourceClient, location);
    do_check_true(packet.isPending);
    do_check_false("actualLocation" in packet);

    executeSoon(function () {
      reload(tabClient).then(function () {
        loadSubScript(SOURCE_URL, global);
      });
    });
    packet = yield waitForPaused(threadClient);
    do_check_eq(packet.type, "paused");
    let why = packet.why;
    do_check_eq(why.type, "breakpoint");
    do_check_eq(why.actors.length, 1);
    do_check_eq(why.actors[0], breakpointClient.actor);
    let frame = packet.frame;
    let where = frame.where;
    do_check_eq(where.source.actor, source.actor);
    do_check_eq(where.line, location.line);
    do_check_eq(where.column, location.column + 7);
    let variables = frame.environment.bindings.variables;
    do_check_eq(variables.a.value, 1);
    do_check_eq(variables.b.value.type, "undefined");
    do_check_eq(variables.c.value.type, "undefined");
    yield resume(threadClient);

    yield close(client);
    do_test_finished();
  });
}
