/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { generateUUID } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/UUID.sys.mjs"
);

add_task(function test_UUID_valid() {
  const uuid = generateUUID();
  const regExp = new RegExp(
    /^[a-f|0-9]{8}-[a-f|0-9]{4}-[a-f|0-9]{4}-[a-f|0-9]{4}-[a-f|0-9]{12}$/g
  );
  ok(regExp.test(uuid));
});

add_task(function test_UUID_unique() {
  const uuid1 = generateUUID();
  const uuid2 = generateUUID();
  notEqual(uuid1, uuid2);
});
