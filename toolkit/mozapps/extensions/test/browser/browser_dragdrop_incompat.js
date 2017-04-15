// Test that drag-and-drop of an incompatible addon generates
// an error.
add_task(async function() {
  let window = await open_manager("addons://list/extension");

  let errorPromise = new Promise(resolve => {
    let observer = {
      observe(subject, topc, data) {
        Services.obs.removeObserver(observer, "addon-install-failed");
        resolve();
      }
    };
    Services.obs.addObserver(observer, "addon-install-failed");
  });

  let url = `${TESTROOT}/addons/browser_dragdrop_incompat.xpi`;
  let viewContainer = window.document.getElementById("view-port");
  EventUtils.synthesizeDrop(viewContainer, viewContainer,
                            [[{type: "text/x-moz-url", data: url}]],
                            "copy", window);

  await errorPromise;
  ok(true, "Got addon-install-failed event");

  await close_manager(window);
});
