"use strict";

add_task(async function skipMe1() {
  Assert.ok(false, "Not skipped after all.");
});

add_task(async function skipMe2() {
  Assert.ok(false, "Not skipped after all.");
}).skip();

add_task(async function skipMe3() {
  Assert.ok(false, "Not skipped after all.");
}).only();

add_task(async function skipMeNot() {
  Assert.ok(true, "Well well well.");
}).only();

add_task(async function skipMe4() {
  Assert.ok(false, "Not skipped after all.");
});
