/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function pngArrayBuffer(size) {
  const canvas = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
  canvas.height = canvas.width = size;
  const ctx = canvas.getContext("2d");
  ctx.fillStyle = "blue";
  ctx.fillRect(0, 0, size, size);
  return new Promise(resolve => {
    canvas.toBlob((blob) => {
      const fileReader = new FileReader();
      fileReader.onload = () => {
        resolve(fileReader.result);
      };
      fileReader.readAsArrayBuffer(blob);
    });
  });
}

async function checkIconInView(view, name, findIcon) {
  const manager = await open_manager(view);
  const icon = findIcon(manager.document);
  const size = Number(icon.src.match(/icon(\d+)\.png/)[1]);
  is(icon.clientWidth, icon.clientHeight, `The icon should be square in ${name}`);
  is(size, icon.clientWidth * window.devicePixelRatio, `The correct icon size should have been chosen in ${name}`);
  await close_manager(manager);
}

add_task(async function test_addon_icon() {
  // This test loads an extension with a variety of icon sizes, and checks that the
  // fitting one is chosen. If this fails it's because you changed the icon size in
  // about:addons but didn't update some AddonManager.getPreferredIconURL call.
  const id = "@test-addon-icon";
  const icons = {};
  const files = {};
  const file = await pngArrayBuffer(256);
  for (let size = 1; size <= 256; ++size) {
    let fileName = `icon${size}.png`;
    icons[size] = fileName;
    files[fileName] = file;
  }
  const extensionDefinition = {
    useAddonManager: "temporary",
    manifest: {
      "applications": {
        "gecko": {id},
      },
      icons,
    },
    files,
  };

  const extension = ExtensionTestUtils.loadExtension(extensionDefinition);
  await extension.startup();

  info(`Testing XUL about:addons`);
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", false]],
  });

  await checkIconInView("addons://list/extension", "list", doc => {
    const addon = get_addon_element(doc.defaultView, id);
    const content = doc.getAnonymousElementByAttribute(addon, "class", "content-container");
    return content.querySelector(".icon");
  });

  await checkIconInView("addons://detail/" + encodeURIComponent(id), "details", doc => {
    return doc.getElementById("detail-icon");
  });

  await SpecialPowers.popPrefEnv();

  info(`Testing HTML about:addons`);
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", true]],
  });

  await checkIconInView("addons://list/extension", "list", doc => {
    return get_addon_element(doc.defaultView, id).querySelector(".addon-icon");
  });

  await checkIconInView("addons://detail/" + encodeURIComponent(id), "details", doc => {
    return get_addon_element(doc.defaultView, id).querySelector(".addon-icon");
  });

  await SpecialPowers.popPrefEnv();

  await extension.unload();
});
