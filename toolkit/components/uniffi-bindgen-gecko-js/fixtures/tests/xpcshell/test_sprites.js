/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const Sprites = ChromeUtils.importESModule(
  "resource://gre/modules/RustSprites.sys.mjs"
);

add_task(async function () {
  Assert.ok(Sprites.Sprite);

  const sempty = await Sprites.Sprite.init(null);
  Assert.deepEqual(
    await sempty.getPosition(),
    new Sprites.Point({ x: 0, y: 0 })
  );

  const s = await Sprites.Sprite.init(new Sprites.Point({ x: 0, y: 1 }));
  Assert.deepEqual(await s.getPosition(), new Sprites.Point({ x: 0, y: 1 }));

  s.moveTo(new Sprites.Point({ x: 1, y: 2 }));
  Assert.deepEqual(await s.getPosition(), new Sprites.Point({ x: 1, y: 2 }));

  s.moveBy(new Sprites.Vector({ dx: -4, dy: 2 }));
  Assert.deepEqual(await s.getPosition(), new Sprites.Point({ x: -3, y: 4 }));

  const srel = await Sprites.Sprite.newRelativeTo(
    new Sprites.Point({ x: 0, y: 1 }),
    new Sprites.Vector({ dx: 1, dy: 1.5 })
  );
  Assert.deepEqual(
    await srel.getPosition(),
    new Sprites.Point({ x: 1, y: 2.5 })
  );
});
