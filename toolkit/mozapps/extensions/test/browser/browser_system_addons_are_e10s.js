"use strict";

add_task(async function test_enabled() {
  let addons = await new Promise(resolved => AddonManager.getAllAddons(resolved));
  for (let addon of addons) {
    if (addon.isSystem) {
      ok(addon.multiprocessCompatible,
         `System addon ${addon.id} is not marked as multiprocess compatible`);
      ok(!addon.unpack,
         `System add-on ${addon.id} isn't a packed add-on.`);
    }
  }
});
