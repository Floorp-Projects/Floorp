add_task(async function testShowFramesAndThreads() {
  await testAboutProcessesWithConfig({
    showAllFrames: true,
    showThreads: true,
  });
});
