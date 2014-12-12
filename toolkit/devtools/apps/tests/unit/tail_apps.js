if (gClient) {
  // Close the test remote connection before leaving this test
  gClient.close(function () {
    run_next_test();
  });
}
