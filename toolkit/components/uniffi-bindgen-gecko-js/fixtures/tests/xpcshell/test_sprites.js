/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const Sprites = ChromeUtils.import(
  "resource://gre/modules/components-utils/RustSprites.jsm"
);

add_task(async function() {
  Assert.ok(Sprites.Sprite);

  const sempty = await Sprites.Sprite.init(null);
  Assert.deepEqual(await sempty.getPosition(), new Sprites.Point(0, 0));

  const s = await Sprites.Sprite.init(new Sprites.Point(0, 1));
  Assert.deepEqual(await s.getPosition(), new Sprites.Point(0, 1));

  s.moveTo(new Sprites.Point(1, 2));
  Assert.deepEqual(await s.getPosition(), new Sprites.Point(1, 2));

  s.moveBy(new Sprites.Vector(-4, 2));
  Assert.deepEqual(await s.getPosition(), new Sprites.Point(-3, 4));

  const srel = await Sprites.Sprite.newRelativeTo(
    new Sprites.Point(0, 1),
    new Sprites.Vector(1, 1.5)
  );
  Assert.deepEqual(await srel.getPosition(), new Sprites.Point(1, 2.5));
});
