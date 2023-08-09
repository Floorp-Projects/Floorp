/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import common head.
{
  /* import-globals-from ../head_common.js */
  let commonFile = do_get_file("../head_common.js", false);
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.

const CURRENT_SCHEMA_VERSION = Ci.nsINavHistoryService.DATABASE_SCHEMA_VERSION;
const FIRST_UPGRADABLE_SCHEMA_VERSION = 52;

async function assertAnnotationsRemoved(db, expectedAnnos) {
  for (let anno of expectedAnnos) {
    let rows = await db.execute(
      `
      SELECT id FROM moz_anno_attributes
      WHERE name = :anno
    `,
      { anno }
    );

    Assert.equal(rows.length, 0, `${anno} should not exist in the database`);
  }
}

async function assertNoOrphanAnnotations(db) {
  let rows = await db.execute(`
    SELECT item_id FROM moz_items_annos
    WHERE item_id NOT IN (SELECT id from moz_bookmarks)
  `);

  Assert.equal(rows.length, 0, `Should have no orphan annotations.`);

  rows = await db.execute(`
    SELECT id FROM moz_anno_attributes
    WHERE id NOT IN (SELECT id from moz_items_annos)
  `);

  Assert.equal(rows.length, 0, `Should have no orphan annotation attributes.`);
}
