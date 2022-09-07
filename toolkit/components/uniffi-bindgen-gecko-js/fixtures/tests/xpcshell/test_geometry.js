/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const Geometry = ChromeUtils.import(
  "resource://gre/modules/components-utils/RustGeometry.jsm"
);

add_task(async function() {
  const ln1 = new Geometry.Line(
    new Geometry.Point(0, 0, "p1"),
    new Geometry.Point(1, 2, "p2")
  );
  const ln2 = new Geometry.Line(
    new Geometry.Point(1, 1, "p3"),
    new Geometry.Point(2, 2, "p4")
  );
  const origin = new Geometry.Point(0, 0);
  Assert.ok((await Geometry.intersection(ln1, ln2)).equals(origin));
  Assert.deepEqual(await Geometry.intersection(ln1, ln2), origin);
  Assert.strictEqual(await Geometry.intersection(ln1, ln1), null);
});
