"use strict";

var cpuThreadCount;

add_task(async function setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  Services.fog.initializeFOG();

  cpuThreadCount = (await Services.sysinfo.processInfo).count;
});

async function getCpuTimeFromProcInfo() {
  const NS_PER_MS = 1000000;
  let cpuTimeForProcess = p => p.cpuTime / NS_PER_MS;
  let procInfo = await ChromeUtils.requestProcInfo();
  return (
    cpuTimeForProcess(procInfo) +
    procInfo.children.map(cpuTimeForProcess).reduce((a, b) => a + b, 0)
  );
}

add_task(async function test_totalCpuTime_in_parent() {
  let startTime = Date.now();

  let initialProcInfoCpuTime = Math.floor(await getCpuTimeFromProcInfo());
  await Services.fog.testFlushAllChildren();

  let initialCpuTime = Glean.power.totalCpuTimeMs.testGetValue();
  Assert.greater(
    initialCpuTime,
    0,
    "The CPU time used by starting the test harness should be more than 0"
  );
  let initialProcInfoCpuTime2 = Math.ceil(await getCpuTimeFromProcInfo());

  Assert.greaterOrEqual(
    initialCpuTime,
    initialProcInfoCpuTime,
    "The CPU time reported through Glean should be at least as much as the CPU time reported by ProcInfo before asking Glean for the data"
  );
  Assert.lessOrEqual(
    initialCpuTime,
    initialProcInfoCpuTime2,
    "The CPU time reported through Glean should be no more than the CPU time reported by ProcInfo after asking Glean for the data"
  );

  // 50 is an arbitrary value, but the resolution is 16ms on Windows,
  // so this needs to be significantly more than 16.
  const kBusyWaitForMs = 50;
  while (Date.now() - startTime < kBusyWaitForMs) {
    // Burn CPU time...
  }

  let additionalProcInfoCpuTime =
    Math.floor(await getCpuTimeFromProcInfo()) - initialProcInfoCpuTime2;
  await Services.fog.testFlushAllChildren();

  let additionalCpuTime =
    Glean.power.totalCpuTimeMs.testGetValue() - initialCpuTime;
  info(
    `additional CPU time according to ProcInfo: ${additionalProcInfoCpuTime}ms and Glean ${additionalCpuTime}ms`
  );

  // On a machine where the CPU is very busy, our busy wait loop might burn less
  // CPU than expected if other processes are being scheduled instead of us.
  let expectedAdditionalCpuTime = Math.min(
    additionalProcInfoCpuTime,
    kBusyWaitForMs
  );
  Assert.greaterOrEqual(
    additionalCpuTime,
    expectedAdditionalCpuTime,
    `The total CPU time should have increased by at least ${expectedAdditionalCpuTime}ms`
  );
  let wallClockTime = Date.now() - startTime;
  Assert.lessOrEqual(
    additionalCpuTime,
    wallClockTime * cpuThreadCount,
    `The total CPU time should have increased by at most the wall clock time (${wallClockTime}ms) * ${cpuThreadCount} CPU threads`
  );
});

add_task(async function test_totalCpuTime_in_child() {
  const MESSAGE_CHILD_TEST_DONE = "ChildTest:Done";

  let startTime = Date.now();
  await Services.fog.testFlushAllChildren();
  let initialCpuTime = Glean.power.totalCpuTimeMs.testGetValue();

  let initialProcInfoCpuTime = await getCpuTimeFromProcInfo();
  run_test_in_child("test_total_cpu_time_child.js");
  let burntCpuTime = await do_await_remote_message(MESSAGE_CHILD_TEST_DONE);
  let additionalProcInfoCpuTime =
    (await getCpuTimeFromProcInfo()) - initialProcInfoCpuTime;

  await Services.fog.testFlushAllChildren();
  let additionalCpuTime =
    Glean.power.totalCpuTimeMs.testGetValue() - initialCpuTime;
  info(
    `additional CPU time according to ProcInfo: ${additionalProcInfoCpuTime}ms and Glean ${additionalCpuTime}ms`
  );

  // On a machine where the CPU is very busy, our busy wait loop might burn less
  // CPU than expected if other processes are being scheduled instead of us.
  let expectedAdditionalCpuTime = Math.min(
    Math.floor(additionalProcInfoCpuTime),
    burntCpuTime
  );
  Assert.greaterOrEqual(
    additionalCpuTime,
    expectedAdditionalCpuTime,
    `The total CPU time should have increased by at least ${expectedAdditionalCpuTime}ms`
  );
  let wallClockTime = Date.now() - startTime;
  Assert.lessOrEqual(
    additionalCpuTime,
    wallClockTime * cpuThreadCount,
    `The total CPU time should have increased by at most the wall clock time (${wallClockTime}ms) * ${cpuThreadCount} CPU threads`
  );
});
