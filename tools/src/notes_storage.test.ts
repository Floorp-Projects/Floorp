// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects } from "@std/assert";
import type { Note } from "../../browser-features/pages-notes/src/lib/merge.ts";
import {
  encodeNotes,
  notesBase,
} from "../../browser-features/modules/common/notes-record.ts";
import type { NotesStorageService } from "../../browser-features/modules/common/notes-storage-types.ts";

const NOTES = "floorp.browser.note.memos";
const BASE = "floorp.browser.note.syncState";
const note = (content = "base", updatedAt = 1): Note => ({
  id: "note-a",
  title: "A",
  content,
  createdAt: 1,
  updatedAt,
});
interface Observer {
  observe(subject: unknown, topic: string, data: string): void;
}
let profileId = 0;

/** Load the actual profile service with synchronous native-style pref observers.
 * FxA identity and the native theme await are controlled; no cloud is contacted.
 */
async function profile(
  local = [note()],
  prior = [note()],
  uid: string | null = "account-a",
) {
  const prefs = new Map<string, string>([
    [NOTES, encodeNotes(local)],
    [
      BASE,
      JSON.stringify({
        accountUid: "account-a",
        lastSyncedSnapshots: notesBase(prior),
        lastSyncTime: 1,
      }),
    ],
  ]);
  const prefObservers = new Set<Observer>();
  const topicObservers = new Map<string, Set<Observer>>();
  const state = {
    uid,
    unavailable: false,
    failWrite: false,
    failBase: false,
    accountPause: null as Promise<void> | null,
  };
  const descriptors = ["Services", "ChromeUtils"].map((key) =>
    [key, Object.getOwnPropertyDescriptor(globalThis, key)] as const
  );
  const services = {
    uuid: { generateUUID: () => crypto.randomUUID() },
    prefs: {
      PREF_STRING: 32,
      getPrefType: (key: string) => prefs.has(key) ? 32 : 0,
      getStringPref(key: string, fallback?: string) {
        const value = prefs.get(key) ?? fallback;
        if (value === undefined) throw new Error("Missing pref");
        return value;
      },
      setStringPref(key: string, value: string) {
        if (key === NOTES && state.failWrite) {
          throw new Error("Notes pref is locked");
        }
        if (key === BASE && state.failBase) {
          // A locked native pref can return success without changing the
          // effective value read by the next merge.
          return;
        }
        const previous = prefs.get(key);
        prefs.set(key, value);
        if (key === NOTES && previous !== value) {
          for (const observer of prefObservers) {
            observer.observe(null, "nsPref:changed", key);
          }
        }
      },
      prefHasUserValue: (key: string) => prefs.has(key),
      clearUserPref: (key: string) => prefs.delete(key),
      addObserver: (_key: string, observer: Observer) =>
        prefObservers.add(observer),
      removeObserver: (_key: string, observer: Observer) =>
        prefObservers.delete(observer),
    },
    obs: {
      addObserver(observer: Observer, topic: string) {
        if (!topicObservers.has(topic)) topicObservers.set(topic, new Set());
        topicObservers.get(topic)!.add(observer);
      },
      removeObserver: (observer: Observer, topic: string) =>
        topicObservers.get(topic)?.delete(observer),
      notifyObservers(subject: unknown, topic: string, data = "") {
        for (const observer of [...(topicObservers.get(topic) ?? [])]) {
          observer.observe(subject, topic, data);
        }
      },
    },
  };
  Object.defineProperty(globalThis, "Services", {
    configurable: true,
    value: services,
  });
  Object.defineProperty(globalThis, "ChromeUtils", {
    configurable: true,
    value: {
      importESModule(uri: string) {
        if (
          uri !== "resource://gre/modules/FxAccounts.sys.mjs"
        ) throw new Error("Unexpected module");
        return {
          getFxAccountsSingleton: () => ({
            async getSignedInUser() {
              if (state.unavailable) throw new Error("FxA unavailable");
              const uid = state.uid;
              if (state.accountPause) await state.accountPause;
              return uid ? { uid } : null;
            },
          }),
        };
      },
    },
  });
  const loaded = await import(
    new URL(
      `../../browser-features/modules/modules/notes/FloorpNotesStorage.sys.mts?profile=${++profileId}`,
      import.meta.url,
    ).href
  ) as { FloorpNotesStorage: NotesStorageService };
  const store = loaded.FloorpNotesStorage;
  return {
    prefs,
    state,
    store,
    services,
    prefObservers,
    topicObservers,
    contents: () =>
      (JSON.parse(prefs.get(NOTES)!) as { contents: string[] }).contents,
    base: () =>
      JSON.parse(prefs.get(BASE)!) as {
        accountUid: string;
        lastSyncedSnapshots: Record<string, Note>;
      },
    apply: (remote: Note[], after?: () => Promise<void>) =>
      store.applyIncoming(encodeNotes(remote), async (merged) => {
        services.prefs.setStringPref(NOTES, merged);
        await after?.();
        return "native-result";
      }),
    close() {
      services.obs.notifyObservers(null, "profile-before-change");
      for (const [key, descriptor] of descriptors) {
        if (descriptor) Object.defineProperty(globalThis, key, descriptor);
        else Reflect.deleteProperty(globalThis, key);
      }
    },
  };
}

Deno.test("Notes closed-page merge retains offline edits, conflicts and deletions", async () => {
  for (const local of [[note("offline", 3)], []]) {
    const p = await profile(local);
    try {
      const result = await p.apply([note()]);
      assertEquals(p.contents(), local.map((n) => n.content));
      assertEquals(result, { result: "native-result", needsUpload: true });
      assertEquals(p.base().lastSyncedSnapshots["note-a"].content, "base");
    } finally {
      p.close();
    }
  }
  const p = await profile([note("offline", 3)]);
  try {
    await p.apply([note("remote", 4)]);
    assertEquals(new Set(p.contents()), new Set(["offline", "remote"]));
    const firstMerge = p.prefs.get(NOTES);
    await p.apply([note("remote", 4)]);
    assertEquals(
      p.prefs.get(NOTES),
      firstMerge,
      "redelivery must not multiply conflict copies",
    );
  } finally {
    p.close();
  }
});

Deno.test("Notes remote-only changes converge and valid empty records delete", async () => {
  const p = await profile();
  try {
    assertEquals((await p.apply([note("remote", 4)])).needsUpload, false);
    assertEquals(p.contents(), ["remote"]);
    assertEquals((await p.apply([])).needsUpload, false);
    assertEquals(p.contents(), []);
  } finally {
    p.close();
  }
});

Deno.test("Notes actual identical delivery acknowledges base without a pref-change event", async () => {
  const p = await profile([note("already-same", 2)]);
  try {
    assertEquals((await p.apply([note("already-same", 2)])).needsUpload, false);
    assertEquals(
      p.base().lastSyncedSnapshots["note-a"].content,
      "already-same",
    );
    assertEquals(p.store.getSnapshot().origin, "sync");
    p.store.writeLocal(encodeNotes([]));
    assertEquals((await p.apply([note("already-same", 2)])).needsUpload, true);
    assertEquals(
      p.contents(),
      [],
      "an acknowledged old body must not resurrect a local deletion",
    );
  } finally {
    p.close();
  }
});

Deno.test("Notes local compare-and-merge preserves another window and never acknowledges cloud delivery", async () => {
  const p = await profile();
  try {
    const initial = p.store.getSnapshot().value;
    p.store.writeLocal(
      encodeNotes([note("other-window", 3)]),
      undefined,
      initial,
    );
    p.store.writeLocal(
      encodeNotes([note("this-window", 4)]),
      undefined,
      initial,
    );
    assertEquals(
      new Set(p.contents()),
      new Set(["other-window", "this-window"]),
    );
    assertEquals(p.base().lastSyncedSnapshots["note-a"].content, "base");
    const unchanged = p.store.getSnapshot().value!;
    p.store.writeLocal(unchanged);
    p.services.prefs.setStringPref(NOTES, encodeNotes([note("external", 5)]));
    assertEquals(
      p.store.getSnapshot().origin,
      "external",
      "unchanged writes leave no pending origin mark",
    );
  } finally {
    p.close();
  }
});

Deno.test("Notes repeated local flushes preserve unseen CAS additions and deliberate visible deletions", async () => {
  const p = await profile();
  try {
    const submitted = encodeNotes([note()]);
    const unseen = { ...note("other window", 2), id: "note-b" };
    p.store.writeLocal(encodeNotes([note(), unseen]));
    const first = p.store.writeLocal(submitted, undefined, submitted);
    assertEquals(JSON.parse(first.value!).ids, ["note-a", "note-b"]);
    // The caller still displays only its submitted note, so its next ancestor
    // is the submitted UI value, not the larger returned storage snapshot.
    p.store.writeLocal(submitted, undefined, submitted);
    assertEquals(p.contents(), ["base", "other window"]);
    p.store.writeLocal(encodeNotes([]), undefined, submitted);
    assertEquals(p.contents(), ["other window"]);
  } finally {
    p.close();
  }
});

Deno.test("Notes failed native setters and account changes cannot acknowledge incoming state", async () => {
  const p = await profile();
  try {
    p.state.failWrite = true;
    await assertRejects(
      () =>
        p.store.applyIncoming(encodeNotes([note("remote", 4)]), (merged) => {
          try {
            p.services.prefs.setStringPref(NOTES, merged);
          } catch { /* Native PrefStore logs setter errors. */ }
          return Promise.resolve();
        }),
      Error,
      "did not persist",
    );
    assertEquals(p.contents(), ["base"]);
    assertEquals(p.base().lastSyncedSnapshots["note-a"].content, "base");
    p.state.failWrite = false;
    await assertRejects(
      () =>
        p.apply([note("remote", 4)], () => {
          p.services.obs.notifyObservers(null, "fxaccounts:onlogout");
          return Promise.resolve();
        }),
      Error,
      "context changed",
    );
    assertEquals(p.contents(), ["remote"]);
    assertEquals(p.prefs.has(BASE), false);
  } finally {
    p.close();
  }
});

Deno.test("Notes start-over during account lookup fences old incoming before writes", async () => {
  const p = await profile();
  try {
    const pause = Promise.withResolvers<void>();
    p.state.accountPause = pause.promise;
    const applying = p.apply([note("remote", 4)]);
    p.services.obs.notifyObservers(null, "weave:service:start-over");
    pause.resolve();
    await assertRejects(() => applying, Error, "context changed");
    assertEquals(p.contents(), ["base"]);
    assertEquals(p.prefs.has(BASE), false);
  } finally {
    p.close();
  }
});

Deno.test("Notes cold account mismatch fences old base and unavailable FxA permits local saves", async () => {
  const p = await profile([note()], [note()], "account-b");
  try {
    assertEquals((await p.apply([])).needsUpload, true);
    assertEquals(p.contents(), ["base"]);
    assertEquals(p.base().accountUid, "account-b");
    p.state.unavailable = true;
    p.store.writeLocal(encodeNotes([note("offline", 5)]));
    const snapshot = p.store.getSnapshot();
    const read = await p.store.readSyncState(snapshot);
    assertEquals(read.context.accountUid, null);
    p.store.writeLocal(
      encodeNotes([note("still offline", 6)]),
      read.context,
      snapshot.value,
    );
    assertEquals(p.contents(), ["still offline"]);
  } finally {
    p.close();
  }
});

Deno.test("Notes profile shutdown removes its preference and account observers", async () => {
  const p = await profile();
  try {
    p.services.obs.notifyObservers(null, "profile-before-change");
    assertEquals(p.prefObservers.size, 0);
    assertEquals(
      [...p.topicObservers.values()].reduce((sum, set) => sum + set.size, 0),
      0,
    );
  } finally {
    p.close();
  }
});

Deno.test("Notes successful outgoing payload acknowledgement prevents later delete resurrection", async () => {
  const p = await profile([note("uploaded edit", 3)]);
  try {
    const context = await p.store.prepareUpload();
    const acceptedPayload = p.prefs.get(NOTES)!;
    await p.store.acknowledgeUploaded(acceptedPayload, context);
    assertEquals(
      p.base().lastSyncedSnapshots["note-a"].content,
      "uploaded edit",
    );
    p.store.writeLocal(encodeNotes([]));
    assertEquals((await p.apply([note("uploaded edit", 3)])).needsUpload, true);
    assertEquals(p.contents(), []);
  } finally {
    p.close();
  }
});

Deno.test("Notes outgoing acknowledgement records exact accepted bytes while later edits remain local", async () => {
  const p = await profile([note("uploaded edit", 3)]);
  try {
    const context = await p.store.prepareUpload();
    const acceptedPayload = p.prefs.get(NOTES)!;
    p.store.writeLocal(encodeNotes([note("typed during upload", 4)]));
    await p.store.acknowledgeUploaded(acceptedPayload, context);
    assertEquals(p.contents(), ["typed during upload"]);
    assertEquals(
      p.base().lastSyncedSnapshots["note-a"].content,
      "uploaded edit",
    );
    assertEquals((await p.apply([note("uploaded edit", 3)])).needsUpload, true);
    assertEquals(p.contents(), ["typed during upload"]);
  } finally {
    p.close();
  }
});

Deno.test("Notes outgoing base write failure remains visible to native retry handling", async () => {
  const p = await profile([note("uploaded edit", 3)]);
  try {
    const context = await p.store.prepareUpload();
    p.state.failBase = true;
    await assertRejects(
      () => p.store.acknowledgeUploaded(p.prefs.get(NOTES)!, context),
      Error,
      "did not persist",
    );
    assertEquals(p.base().lastSyncedSnapshots["note-a"].content, "base");
    assertEquals(p.contents(), ["uploaded edit"]);
  } finally {
    p.close();
  }
});

Deno.test("Notes outgoing acknowledgement cannot cross an account epoch during lookup", async () => {
  const p = await profile([note("uploaded edit", 3)]);
  try {
    const context = await p.store.prepareUpload();
    const pause = Promise.withResolvers<void>();
    p.state.accountPause = pause.promise;
    const acknowledging = p.store.acknowledgeUploaded(
      p.prefs.get(NOTES)!,
      context,
    );
    p.services.obs.notifyObservers(null, "fxaccounts:onlogout");
    pause.resolve();
    await assertRejects(() => acknowledging, Error, "context changed");
    assertEquals(p.prefs.has(BASE), false);
    assertEquals(p.contents(), ["uploaded edit"]);
  } finally {
    p.close();
  }
});
