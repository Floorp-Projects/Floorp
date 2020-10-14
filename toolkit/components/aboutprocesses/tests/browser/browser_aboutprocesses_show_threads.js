// Test about:processes with showThreads: true, showAllFrames: false.
add_task(async function testShowThreads() {
  return testAboutProcessesWithConfig({
    showAllFrames: false,
    showThreads: true,
  });
});
