function testFetchNoCors(file, options, ...pipe) {
  return fetch(`${file}${pipe.length ? `?pipe=${pipe.join("|")}` : ""}`, {
    ...(options || {}),
    mode: "no-cors",
  });
}

function promise_internal_response_is_filtered(fetchPromise, message) {
  return promise_test(async () => {
    const response = await fetchPromise;

    // A parent filtered opaque response is defined here as a response that isn't just an
    // opaque response, but also where the internal response has been made unavailable.
    // `Response.cloneUnfiltered` is used to inspect the state of the internal response,
    // which is exactly what we want to be missing in this case.
    const unfiltered = SpecialPowers.wrap(response).cloneUnfiltered();
    assert_equals(
      await SpecialPowers.unwrap(unfiltered).text(),
      "",
      "The internal response should be empty"
    );
    assert_equals(
      Array.from(await SpecialPowers.unwrap(unfiltered).headers).length,
      0,
      "The internal response should have no headers"
    );
  }, message);
}
