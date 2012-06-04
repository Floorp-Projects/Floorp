/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-aitc/storage.js");
Cu.import("resource://services-common/async.js");

let queue = null;

function run_test() {
  queue = new AitcQueue("test", run_next_test);
}

add_test(function test_queue_create() {
  do_check_eq(queue._queue.length, 0);
  do_check_eq(queue._writeLock, false);
  run_next_test();
});

add_test(function test_queue_enqueue() {
  // Add to queue.
  let testObj = {foo: "bar"};
  queue.enqueue(testObj, function(err, done) {
    do_check_eq(err, null);
    do_check_true(done);

    // Check if peek value is correct.
    do_check_eq(queue.peek(), testObj);
    // Peek should be idempotent.
    do_check_eq(queue.peek(), testObj);

    run_next_test();
  });
});

add_test(function test_queue_dequeue() {
  // Remove an item and see if queue is empty.
  queue.dequeue(function(err, done) {
    do_check_eq(err, null);
    do_check_true(done);
    do_check_eq(queue.length, 0);
    try {
      queue.peek();
    } catch (e) {
      do_check_eq(e.toString(), "Error: Queue is empty");
      run_next_test();
    }
  });
});

add_test(function test_queue_multiaddremove() {
  // Queues should handle objects, strings and numbers.
  let items = [{test:"object"}, "teststring", 42];

  // Two random numbers: how many items to queue and how many to remove.
  let num = Math.floor(Math.random() * 100 + 1);
  let rem = Math.floor(Math.random() * num + 1);

  // First insert all the items we will remove later.
  for (let i = 0; i < rem; i++) {
    let ins = items[Math.round(Math.random() * 2)];
    let cb = Async.makeSpinningCallback();
    queue.enqueue(ins, cb);
    do_check_true(cb.wait());
  }

  do_check_eq(queue.length, rem);

  // Now insert the items we won't remove.
  let check = [];
  for (let i = 0; i < (num - rem); i++) {
    check.push(items[Math.round(Math.random() * 2)]);
    let cb = Async.makeSpinningCallback();
    queue.enqueue(check[check.length - 1], cb);
    do_check_true(cb.wait());
  }

  do_check_eq(queue.length, num);

  // Now dequeue rem items.
  for (let i = 0; i < rem; i++) {
    let cb = Async.makeSpinningCallback();
    queue.dequeue(cb);
    do_check_true(cb.wait());
  }

  do_check_eq(queue.length, num - rem);

  // Check that the items left are the right ones.
  do_check_eq(JSON.stringify(queue._queue), JSON.stringify(check));

  // Another instance of the same queue should return correct data.
  let queue2 = new AitcQueue("test", function(done) {
    do_check_true(done);
    do_check_eq(queue2.length, queue.length);
    do_check_eq(JSON.stringify(queue._queue), JSON.stringify(queue2._queue));
    run_next_test();
  });
});

/* TODO Bug 760905 - Temporarily disabled for orange.
add_test(function test_queue_writelock() {
  // Queue should not enqueue or dequeue if lock is enabled.
  queue._writeLock = true;
  let len = queue.length;

  queue.enqueue("writeLock test", function(err, done) {
    do_check_eq(err.toString(), "Error: _putFile already in progress");
    do_check_eq(queue.length, len);

    queue.dequeue(function(err, done) {
      do_check_eq(err.toString(), "Error: _putFile already in progress");
      do_check_eq(queue.length, len);
      run_next_test();
    });
  });
});
*/
