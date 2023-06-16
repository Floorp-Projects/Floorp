/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const ExternalTypes = ChromeUtils.importESModule(
  "resource://gre/modules/RustExternalTypes.sys.mjs"
);

add_task(async function () {
  const line = new ExternalTypes.Line(
    new ExternalTypes.Point(0, 0, "p1"),
    new ExternalTypes.Point(2, 1, "p2")
  );
  Assert.equal(await ExternalTypes.gradient(line), 0.5);

  Assert.equal(await ExternalTypes.gradient(null), 0.0);
});
