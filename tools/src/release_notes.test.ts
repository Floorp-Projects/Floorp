import { strict as assert } from "node:assert";
import {
  effectiveReleaseNotesMode,
  initialReleaseNotesChoice,
  releaseNotesAudience,
  shouldShowReleaseNotesChoice,
} from "../../browser-features/modules/common/release-notes.ts";

Deno.test("release notes: unconfirmed choices never restrict extensions or suppress notes", () => {
  for (const mode of [null, "support", "blocking", "disabled", "invalid"]) {
    assert.equal(effectiveReleaseNotesMode(mode, false), "blocking");
    assert.equal(effectiveReleaseNotesMode(mode, null), "blocking");
  }
});

Deno.test("release notes: existing-user notice appears once, never for new or confirmed users", () => {
  assert.equal(shouldShowReleaseNotesChoice("existing", false, false), true);
  assert.equal(shouldShowReleaseNotesChoice("existing", false, true), false);
  assert.equal(shouldShowReleaseNotesChoice("existing", true, false), false);
  assert.equal(shouldShowReleaseNotesChoice("new", false, false), false);
});

Deno.test("release notes: only new profiles preselect support without applying it", () => {
  assert.equal(initialReleaseNotesChoice("blocking", false, "new"), "support");
  assert.equal(effectiveReleaseNotesMode("support", false), "blocking");
  for (const audience of ["existing", null, "invalid"]) {
    assert.equal(
      initialReleaseNotesChoice("support", false, audience),
      "blocking",
    );
  }
});

Deno.test("release notes: confirmed selections survive reopening setup", () => {
  for (const mode of ["disabled", "blocking", "support"] as const) {
    assert.equal(initialReleaseNotesChoice(mode, true, "new"), mode);
    assert.equal(initialReleaseNotesChoice(mode, true, "existing"), mode);
    assert.equal(effectiveReleaseNotesMode(mode, true), mode);
  }
  assert.equal(effectiveReleaseNotesMode("invalid", true), "blocking");
});

Deno.test("release notes: an absent new preference does not make an existing user new", () => {
  assert.equal(releaseNotesAudience(true, undefined, false), "new");
  assert.equal(releaseNotesAudience(false, "12.0", true), "existing");
  assert.equal(releaseNotesAudience(true, "12.0", false), "existing");
  assert.equal(releaseNotesAudience(true, undefined, true), "existing");
  assert.equal(releaseNotesAudience(false, undefined, false), "existing");
});

Deno.test("release notes: development patch matches packaged Runtime changes", async () => {
  const source = await Deno.readTextFile(
    new URL(
      "../../.github/patches/floorp-runtime/common/release-notes-guards.patch",
      import.meta.url,
    ),
  );
  const development = await Deno.readTextFile(
    new URL("../patches/release-notes-guards.patch", import.meta.url),
  );
  const expected = source.replaceAll("\r\n", "\n").split("diff --git ").slice(
    1,
    3,
  ).map((part) => "diff --git " + part).join("").replaceAll(
    "toolkit/components/extensions/",
    "modules/",
  );
  assert.equal(development.replaceAll("\r\n", "\n"), expected);
});
