// Controlled child used only by host tests, never in native mode.
const [mode, path] = Deno.args;
if (mode === "compile") { await Deno.mkdir(path); console.log("fake compile"); }
else if (mode === "sign") { console.log("fake sign"); }
else if (mode === "env") { console.log(Deno.env.get("PWA_TEST_SENTINEL") ?? "absent"); }
else if (mode === "fail") { console.error("controlled failure"); Deno.exit(42); }
else if (mode === "slow") { await new Promise((r) => setTimeout(r, 10000)); }
else if (mode === "overflow") { console.log("x".repeat(40000)); }
else Deno.exit(43);
