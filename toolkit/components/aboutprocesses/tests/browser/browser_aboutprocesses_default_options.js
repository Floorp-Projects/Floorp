// Test about:processes with default options.
add_task(async function testDefaultOptions() {
  return testAboutProcessesWithConfig({
    showAllFrames: false,
    showThreads: false,
  });
});
