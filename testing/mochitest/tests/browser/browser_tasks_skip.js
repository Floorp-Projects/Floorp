"use strict";

add_task(async function skipMeNot1() {
  Assert.ok(true, "Well well well.");
});

add_task(async function skipMe1() {
  Assert.ok(false, "Not skipped after all.");
}).skip();

add_task(async function skipMeNot2() {
  Assert.ok(true, "Well well well.");
});

add_task(async function skipMeNot3() {
  Assert.ok(true, "Well well well.");
});

add_task(async function skipMe2() {
  Assert.ok(false, "Not skipped after all.");
}).skip();
