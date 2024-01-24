/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ContextualIdentityListener } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/listeners/ContextualIdentityListener.sys.mjs"
);

add_task(async function test_createdOnNewContextualIdentity() {
  const listener = new ContextualIdentityListener();
  const created = listener.once("created");

  listener.startListening();

  ContextualIdentityService.create("test_name");

  const { identity } = await created;
  is(identity.name, "test_name", "Received expected identity");

  listener.stopListening();

  ContextualIdentityService.remove(identity.userContextId);
});

add_task(async function test_deletedOnRemovedContextualIdentity() {
  const listener = new ContextualIdentityListener();
  const deleted = listener.once("deleted");

  listener.startListening();

  const testIdentity = ContextualIdentityService.create("test_name");
  ContextualIdentityService.remove(testIdentity.userContextId);

  const { identity } = await deleted;
  is(identity.name, "test_name", "Received expected identity");

  listener.stopListening();
});
