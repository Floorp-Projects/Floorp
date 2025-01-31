const tests = import.meta.glob("./**/*.test.ts");

for (const [path, module] of Object.entries(tests)) {
  try {
    await module();
  } catch (e) {
    console.error(`[nora@test] Failed to run test ${path}:`, e);
  }
}
