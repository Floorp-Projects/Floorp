// The module has an import that's immediately called by the start function.
const module = new Uint8Array([
    0,  97, 115, 109,   1,   0,   0,   0,   1,  4,   1,  96,
    0,   0,   2,   7,   1,   1,  97,   1,  98,  0,   0,   3,
    2,   1,   0,   8,   1,   1,  10,   6,   1,  4,   0,  16,
    0,  11,   0,  36,  16, 115, 111, 117, 114, 99, 101,  77,
   97, 112, 112, 105, 110, 103,  85,  82,  76, 18,  46,  47,
  114, 101, 108, 101,  97, 115, 101,  46, 119, 97, 115, 109,
   46, 109,  97, 112
]);

// The WebSocket server does not need to exist, because the bug occurs before
// any connection is attempted.
const imports = {
    a: {
        b: Reflect.construct.bind(null, WebSocket, ["ws://localhost:1234"])
    }
};

promise_test(
    () => WebAssembly.instantiate(module, imports),
    "Ensure WebSockets can be constructed from WebAssembly"
);