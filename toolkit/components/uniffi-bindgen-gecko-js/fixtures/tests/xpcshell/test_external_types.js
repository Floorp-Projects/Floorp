/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const ExternalTypes = ChromeUtils.importESModule(
  "resource://gre/modules/RustExternalTypes.sys.mjs"
);

add_task(async function () {
  const line = new ExternalTypes.Line({
    start: await new ExternalTypes.Point({ coordX: 0, coordY: 0 }),
    end: await new ExternalTypes.Point({ coordX: 2, coordY: 1 }),
  });
  Assert.equal(await ExternalTypes.gradient(line), 0.5);

  Assert.equal(await ExternalTypes.gradient(null), 0.0);
});
