// Just receive 'foo' message and forward it back
// as 'bar' message
addMessageListener("foo", function (message) {
  sendAsyncMessage("bar", message);
});

