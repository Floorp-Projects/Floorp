/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const CustomTypes = ChromeUtils.importESModule(
  "resource://gre/modules/RustCustomTypes.sys.mjs"
);

add_task(async function () {
  // JS right now doesn't treat custom types as anything but it's native counterparts
  let demo = await CustomTypes.getCustomTypesDemo();
  Assert.equal(demo.url, "http://example.com/");
  Assert.equal(demo.handle, 123);
});
