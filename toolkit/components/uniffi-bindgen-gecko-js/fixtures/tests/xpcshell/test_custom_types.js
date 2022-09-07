/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const Customtypes = ChromeUtils.import(
  "resource://gre/modules/components-utils/RustCustomtypes.jsm"
);

add_task(async function() {
  // JS right now doesn't treat custom types as anything but it's native counterparts
  let demo = await Customtypes.getCustomTypesDemo();
  Assert.equal(demo.url, "http://example.com/");
  Assert.equal(demo.handle, 123);
});
