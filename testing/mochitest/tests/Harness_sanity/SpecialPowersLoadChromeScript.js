// Just receive 'foo' message and forward it back
// as 'bar' message
addMessageListener("foo", function (message) {
  sendAsyncMessage("bar", message);
});

addMessageListener("valid-assert", function (message) {
  assert.ok(true, "valid assertion");
  assert.equal(1, 1, "another valid assertion");
  sendAsyncMessage("valid-assert-done");
});
