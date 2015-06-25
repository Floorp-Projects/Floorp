importScripts('worker-testharness.js');

// Helper method that waits for a reply on a port, and resolves a promise with
// the reply.
function wait_for_message_to_port(test, port) {
  return new Promise(function(resolve) {
    var resolved = false;
    port.onmessage = test.step_func(function(event) {
      assert_false(resolved);
      resolved = true;
      resolve(event.data);
    });
  });
}

// Similar helper method, but waits for a reply to a stashed port with a
// particular name.
function wait_for_message_to_stashed_port(test, name) {
  return new Promise(function(resolve) {
    var event_handler = function(e) {
      if (e.source.name === name) {
        self.ports.removeEventListener('message', event_handler);
        resolve(e);
      }
    };
    self.ports.addEventListener('message', event_handler);
  });
}

test(function(test) {
    var channel = new MessageChannel();
    var name = 'somename';
    var stashed_port = self.ports.add(name, channel.port1);
    assert_equals(stashed_port.name, name);
  }, 'Name set when adding port is set correctly.');

promise_test(function(test) {
    var channel = new MessageChannel();
    var stashed_port = self.ports.add('', channel.port1);
    stashed_port.postMessage('pingy ping');
    return wait_for_message_to_port(test, channel.port2)
      .then(test.step_func(function(reply) {
          assert_equals(reply, 'pingy ping');
      }));
  }, 'Messages posted into stashed port arrive on other side.');

promise_test(function(test) {
    var channel = new MessageChannel();
    var name = 'test_events';
    var stashed_port = self.ports.add(name, channel.port1);
    channel.port2.postMessage('ping blah');
    return wait_for_message_to_stashed_port(test, name)
      .then(test.step_func(function(reply) {
          assert_equals(reply.data, 'ping blah');
          assert_equals(reply.source, stashed_port);
      }));
  }, 'Messages sent to stashed port arrive as global events.');
