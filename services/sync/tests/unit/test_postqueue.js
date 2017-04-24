/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let { PostQueue } = Cu.import("resource://services-sync/record.js", {});

initTestLogging("Trace");

function makeRecord(nbytes) {
  // make a string 2-bytes less - the added quotes will make it correct.
  return {
    toJSON: () => "x".repeat(nbytes - 2),
  }
}

function makePostQueue(config, lastModTime, responseGenerator) {
  let stats = {
    posts: [],
  }
  let poster = (data, headers, batch, commit) => {
    let thisPost = { nbytes: data.length, batch, commit };
    if (headers.length) {
      thisPost.headers = headers;
    }
    stats.posts.push(thisPost);
    return Promise.resolve(responseGenerator.next().value);
  }

  let done = () => {}
  let pq = new PostQueue(poster, lastModTime, config, getTestLogger(), done);
  return { pq, stats };
}

add_test(function test_simple() {
  let config = {
    max_post_bytes: 1000,
    max_post_records: 100,
    max_batch_bytes: Infinity,
    max_batch_records: Infinity,
  }

  const time = 11111111;

  function* responseGenerator() {
    yield { success: true, status: 200, headers: { "x-weave-timestamp": time + 100, "x-last-modified": time + 100 } };
  }

  let { pq, stats } = makePostQueue(config, time, responseGenerator());
  pq.enqueue(makeRecord(10));
  pq.flush(true);

  deepEqual(stats.posts, [{
    nbytes: 12, // expect our 10 byte record plus "[]" to wrap it.
    commit: true, // we don't know if we have batch semantics, so committed.
    headers: [["x-if-unmodified-since", time]],
    batch: "true"}]);

  run_next_test();
});

// Test we do the right thing when we need to make multiple posts when there
// are no batch semantics
add_test(function test_max_post_bytes_no_batch() {
  let config = {
    max_post_bytes: 50,
    max_post_records: 4,
    max_batch_bytes: Infinity,
    max_batch_records: Infinity,
  }

  const time = 11111111;
  function* responseGenerator() {
    yield { success: true, status: 200, headers: { "x-weave-timestamp": time + 100, "x-last-modified": time + 100 } };
    yield { success: true, status: 200, headers: { "x-weave-timestamp": time + 200, "x-last-modified": time + 200 } };
  }

  let { pq, stats } = makePostQueue(config, time, responseGenerator());
  pq.enqueue(makeRecord(20)); // total size now 22 bytes - "[" + record + "]"
  pq.enqueue(makeRecord(20)); // total size now 43 bytes - "[" + record + "," + record + "]"
  pq.enqueue(makeRecord(20)); // this will exceed our byte limit, so will be in the 2nd POST.
  pq.flush(true);

  deepEqual(stats.posts, [
    {
      nbytes: 43, // 43 for the first post
      commit: false,
      headers: [["x-if-unmodified-since", time]],
      batch: "true",
    }, {
      nbytes: 22,
      commit: false, // we know we aren't in a batch, so never commit.
      headers: [["x-if-unmodified-since", time + 100]],
      batch: null,
    }
  ]);
  equal(pq.lastModified, time + 200);

  run_next_test();
});

// Similar to the above, but we've hit max_records instead of max_bytes.
add_test(function test_max_post_records_no_batch() {
  let config = {
    max_post_bytes: 100,
    max_post_records: 2,
    max_batch_bytes: Infinity,
    max_batch_records: Infinity,
  }

  const time = 11111111;

  function* responseGenerator() {
    yield { success: true, status: 200, headers: { "x-weave-timestamp": time + 100, "x-last-modified": time + 100 } };
    yield { success: true, status: 200, headers: { "x-weave-timestamp": time + 200, "x-last-modified": time + 200 } };
  }

  let { pq, stats } = makePostQueue(config, time, responseGenerator());
  pq.enqueue(makeRecord(20)); // total size now 22 bytes - "[" + record + "]"
  pq.enqueue(makeRecord(20)); // total size now 43 bytes - "[" + record + "," + record + "]"
  pq.enqueue(makeRecord(20)); // this will exceed our records limit, so will be in the 2nd POST.
  pq.flush(true);

  deepEqual(stats.posts, [
    {
      nbytes: 43, // 43 for the first post
      commit: false,
      batch: "true",
      headers: [["x-if-unmodified-since", time]],
    }, {
      nbytes: 22,
      commit: false, // we know we aren't in a batch, so never commit.
      batch: null,
      headers: [["x-if-unmodified-since", time + 100]],
    }
  ]);
  equal(pq.lastModified, time + 200);

  run_next_test();
});

// Batch tests.

// Test making a single post when batch semantics are in place.
add_test(function test_single_batch() {
  let config = {
    max_post_bytes: 1000,
    max_post_records: 100,
    max_batch_bytes: 2000,
    max_batch_records: 200,
  }
  const time = 11111111;
  function* responseGenerator() {
    yield { success: true, status: 202, obj: { batch: 1234 },
            headers: { "x-last-modified": time, "x-weave-timestamp": time + 100 },
    };
  }

  let { pq, stats } = makePostQueue(config, time, responseGenerator());
  ok(pq.enqueue(makeRecord(10)).enqueued);
  pq.flush(true);

  deepEqual(stats.posts, [
    {
      nbytes: 12, // expect our 10 byte record plus "[]" to wrap it.
      commit: true, // we don't know if we have batch semantics, so committed.
      batch: "true",
      headers: [["x-if-unmodified-since", time]],
    }
  ]);

  run_next_test();
});

// Test we do the right thing when we need to make multiple posts when there
// are batch semantics in place.
add_test(function test_max_post_bytes_batch() {
  let config = {
    max_post_bytes: 50,
    max_post_records: 4,
    max_batch_bytes: 5000,
    max_batch_records: 100,
  }

  const time = 11111111;
  function* responseGenerator() {
    yield { success: true, status: 202, obj: { batch: 1234 },
            headers: { "x-last-modified": time, "x-weave-timestamp": time + 100 },
    };
    yield { success: true, status: 202, obj: { batch: 1234 },
            headers: { "x-last-modified": time + 200, "x-weave-timestamp": time + 200 },
   };
  }

  let { pq, stats } = makePostQueue(config, time, responseGenerator());
  ok(pq.enqueue(makeRecord(20)).enqueued); // total size now 22 bytes - "[" + record + "]"
  ok(pq.enqueue(makeRecord(20)).enqueued); // total size now 43 bytes - "[" + record + "," + record + "]"
  ok(pq.enqueue(makeRecord(20)).enqueued); // this will exceed our byte limit, so will be in the 2nd POST.
  pq.flush(true);

  deepEqual(stats.posts, [
    {
      nbytes: 43, // 43 for the first post
      commit: false,
      batch: "true",
      headers: [["x-if-unmodified-since", time]],
    }, {
      nbytes: 22,
      commit: true,
      batch: 1234,
      headers: [["x-if-unmodified-since", time]],
    }
  ]);

  equal(pq.lastModified, time + 200);

  run_next_test();
});

// Test we do the right thing when the batch bytes limit is exceeded.
add_test(function test_max_post_bytes_batch() {
  let config = {
    max_post_bytes: 50,
    max_post_records: 20,
    max_batch_bytes: 70,
    max_batch_records: 100,
  }

  const time0 = 11111111;
  const time1 = 22222222;
  function* responseGenerator() {
    yield { success: true, status: 202, obj: { batch: 1234 },
            headers: { "x-last-modified": time0, "x-weave-timestamp": time0 + 100 },
    };
    yield { success: true, status: 202, obj: { batch: 1234 },
            headers: { "x-last-modified": time1, "x-weave-timestamp": time1 },
    };
    yield { success: true, status: 202, obj: { batch: 5678 },
            headers: { "x-last-modified": time1, "x-weave-timestamp": time1 + 100 },
    };
    yield { success: true, status: 202, obj: { batch: 5678 },
            headers: { "x-last-modified": time1 + 200, "x-weave-timestamp": time1 + 200 },
    };
  }

  let { pq, stats } = makePostQueue(config, time0, responseGenerator());
  ok(pq.enqueue(makeRecord(20)).enqueued); // total size now 22 bytes - "[" + record + "]"
  ok(pq.enqueue(makeRecord(20)).enqueued); // total size now 43 bytes - "[" + record + "," + record + "]"
  // this will exceed our POST byte limit, so will be in the 2nd POST - but still in the first batch.
  ok(pq.enqueue(makeRecord(20)).enqueued); // 22 bytes for 2nd post, 55 bytes in the batch.
  // this will exceed our batch byte limit, so will be in a new batch.
  ok(pq.enqueue(makeRecord(20)).enqueued); // 22 bytes in 3rd post/2nd batch
  ok(pq.enqueue(makeRecord(20)).enqueued); // 43 bytes in 3rd post/2nd batch
  // This will exceed POST byte limit, so will be in the 4th post, part of the 2nd batch.
  ok(pq.enqueue(makeRecord(20)).enqueued); // 22 bytes for 4th post/2nd batch
  pq.flush(true);

  deepEqual(stats.posts, [
    {
      nbytes: 43, // 43 for the first post
      commit: false,
      batch: "true",
      headers: [["x-if-unmodified-since", time0]],
    }, {
      // second post of 22 bytes in the first batch, committing it.
      nbytes: 22,
      commit: true,
      batch: 1234,
      headers: [["x-if-unmodified-since", time0]],
    }, {
      // 3rd post of 43 bytes in a new batch, not yet committing it.
      nbytes: 43,
      commit: false,
      batch: "true",
      headers: [["x-if-unmodified-since", time1]],
    }, {
      // 4th post of 22 bytes in second batch, committing it.
      nbytes: 22,
      commit: true,
      batch: 5678,
      headers: [["x-if-unmodified-since", time1]],
    },
  ]);

  equal(pq.lastModified, time1 + 200);

  run_next_test();
});

// Test we split up the posts when we exceed the record limit when batch semantics
// are in place.
add_test(function test_max_post_bytes_batch() {
  let config = {
    max_post_bytes: 1000,
    max_post_records: 2,
    max_batch_bytes: 5000,
    max_batch_records: 100,
  }

  const time = 11111111;
  function* responseGenerator() {
    yield { success: true, status: 202, obj: { batch: 1234 },
            headers: { "x-last-modified": time, "x-weave-timestamp": time + 100 },
    };
    yield { success: true, status: 202, obj: { batch: 1234 },
            headers: { "x-last-modified": time + 200, "x-weave-timestamp": time + 200 },
   };
  }

  let { pq, stats } = makePostQueue(config, time, responseGenerator());
  ok(pq.enqueue(makeRecord(20)).enqueued); // total size now 22 bytes - "[" + record + "]"
  ok(pq.enqueue(makeRecord(20)).enqueued); // total size now 43 bytes - "[" + record + "," + record + "]"
  ok(pq.enqueue(makeRecord(20)).enqueued); // will exceed record limit, so will be in 2nd post.
  pq.flush(true);

  deepEqual(stats.posts, [
    {
      nbytes: 43, // 43 for the first post
      commit: false,
      batch: "true",
      headers: [["x-if-unmodified-since", time]],
    }, {
      nbytes: 22,
      commit: true,
      batch: 1234,
      headers: [["x-if-unmodified-since", time]],
    }
  ]);

  equal(pq.lastModified, time + 200);

  run_next_test();
});

// Test that a single huge record fails to enqueue
add_test(function test_huge_record() {
  let config = {
    max_post_bytes: 50,
    max_post_records: 100,
    max_batch_bytes: 5000,
    max_batch_records: 100,
  }

  const time = 11111111;
  function* responseGenerator() {
    yield { success: true, status: 202, obj: { batch: 1234 },
            headers: { "x-last-modified": time, "x-weave-timestamp": time + 100 },
    };
    yield { success: true, status: 202, obj: { batch: 1234 },
            headers: { "x-last-modified": time + 200, "x-weave-timestamp": time + 200 },
   };
  }

  let { pq, stats } = makePostQueue(config, time, responseGenerator());
  ok(pq.enqueue(makeRecord(20)).enqueued);

  let { enqueued, error } = pq.enqueue(makeRecord(1000));
  ok(!enqueued);
  notEqual(error, undefined);

  // make sure that we keep working, skipping the bad record entirely
  // (handling the error the queue reported is left up to caller)
  ok(pq.enqueue(makeRecord(20)).enqueued);
  ok(pq.enqueue(makeRecord(20)).enqueued);

  pq.flush(true);

  deepEqual(stats.posts, [
    {
      nbytes: 43, // 43 for the first post
      commit: false,
      batch: "true",
      headers: [["x-if-unmodified-since", time]],
    }, {
      nbytes: 22,
      commit: true,
      batch: 1234,
      headers: [["x-if-unmodified-since", time]],
    }
  ]);

  equal(pq.lastModified, time + 200);

  run_next_test();
});

// Test we do the right thing when the batch record limit is exceeded.
add_test(function test_max_records_batch() {
  let config = {
    max_post_bytes: 1000,
    max_post_records: 3,
    max_batch_bytes: 10000,
    max_batch_records: 5,
  }

  const time0 = 11111111;
  const time1 = 22222222;
  function* responseGenerator() {
    yield { success: true, status: 202, obj: { batch: 1234 },
            headers: { "x-last-modified": time0, "x-weave-timestamp": time0 + 100 },
    };
    yield { success: true, status: 202, obj: { batch: 1234 },
            headers: { "x-last-modified": time1, "x-weave-timestamp": time1 },
    };
    yield { success: true, status: 202, obj: { batch: 5678 },
            headers: { "x-last-modified": time1, "x-weave-timestamp": time1 + 100 },
    };
    yield { success: true, status: 202, obj: { batch: 5678 },
            headers: { "x-last-modified": time1 + 200, "x-weave-timestamp": time1 + 200 },
    };
  }

  let { pq, stats } = makePostQueue(config, time0, responseGenerator());

  ok(pq.enqueue(makeRecord(20)).enqueued);
  ok(pq.enqueue(makeRecord(20)).enqueued);
  ok(pq.enqueue(makeRecord(20)).enqueued);

  ok(pq.enqueue(makeRecord(20)).enqueued);
  ok(pq.enqueue(makeRecord(20)).enqueued);

  ok(pq.enqueue(makeRecord(20)).enqueued);
  ok(pq.enqueue(makeRecord(20)).enqueued);
  ok(pq.enqueue(makeRecord(20)).enqueued);

  ok(pq.enqueue(makeRecord(20)).enqueued);

  pq.flush(true);

  deepEqual(stats.posts, [
    { // 3 records
      nbytes: 64,
      commit: false,
      batch: "true",
      headers: [["x-if-unmodified-since", time0]],
    }, { // 2 records -- end batch1
      nbytes: 43,
      commit: true,
      batch: 1234,
      headers: [["x-if-unmodified-since", time0]],
    }, { // 3 records
      nbytes: 64,
      commit: false,
      batch: "true",
      headers: [["x-if-unmodified-since", time1]],
    }, { // 1 record -- end batch2
      nbytes: 22,
      commit: true,
      batch: 5678,
      headers: [["x-if-unmodified-since", time1]],
    },
  ]);

  equal(pq.lastModified, time1 + 200);

  run_next_test();
});
