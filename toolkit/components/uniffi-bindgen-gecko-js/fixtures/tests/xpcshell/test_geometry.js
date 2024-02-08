/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const Geometry = ChromeUtils.importESModule(
  "resource://gre/modules/RustGeometry.sys.mjs"
);

add_task(async function () {
  const ln1 = new Geometry.Line(
    new Geometry.Point({ coord_x: 0, coord_y: 0 }),
    new Geometry.Point({ coord_x: 1, coord_y: 2 })
  );
  const ln2 = new Geometry.Line(
    new Geometry.Point({ coord_x: 1, coord_y: 1 }),
    new Geometry.Point({ coord_x: 2, coord_y: 2 })
  );
  const origin = new Geometry.Point({ coord_x: 0, coord_y: 0 });
  Assert.ok(
    (await Geometry.intersection({ start: ln1, end: ln2 })).equals(origin)
  );
  Assert.deepEqual(
    await Geometry.intersection({ start: ln1, end: ln2 }),
    origin
  );
  Assert.strictEqual(
    await Geometry.intersection({ start: ln1, end: ln1 }),
    null
  );
});
