/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_remote_settings_versioning() {
  const tests = [
    {
      majorVersion: 1,
      existingVersion: "1.0",
      nextVersion: "1.1",
      expectation: true,
    },
    {
      majorVersion: 1,
      existingVersion: null,
      nextVersion: "1.1",
      expectation: true,
    },
    {
      majorVersion: 1,
      existingVersion: null,
      nextVersion: "1.0beta",
      expectation: true,
    },
    {
      majorVersion: 1,
      existingVersion: null,
      nextVersion: "1.0a",
      expectation: true,
    },
    {
      majorVersion: 2,
      existingVersion: null,
      nextVersion: "1.0a",
      expectation: false,
    },
    {
      majorVersion: 2,
      existingVersion: "2.0",
      nextVersion: "1.0a",
      expectation: false,
    },
    {
      majorVersion: 2,
      existingVersion: "2.1",
      nextVersion: "3.2",
      expectation: false,
    },
    {
      majorVersion: 2,
      existingVersion: null,
      nextVersion: "3.2",
      expectation: false,
    },
    {
      majorVersion: 1,
      nextVersion: "1.0",
      existingVersion: undefined,
      expectation: true,
    },
  ];
  for (const {
    majorVersion,
    existingVersion,
    nextVersion,
    expectation,
  } of tests) {
    is(
      TranslationsParent.isBetterRecordVersion(
        majorVersion,
        nextVersion,
        existingVersion
      ),
      expectation,
      `Given a major version of ${majorVersion}, an existing version ${existingVersion} ` +
        `and a next version of ${nextVersion}, is the next version is ` +
        `${expectation ? "" : "not "}best.`
    );
  }
});
