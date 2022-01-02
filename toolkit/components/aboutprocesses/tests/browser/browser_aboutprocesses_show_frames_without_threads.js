add_task(async function testShowFramesWithoutThreads() {
  await testAboutProcessesWithConfig({
    showAllFrames: true,
    showThreads: false,
  });
});
