/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testTypingPrintableCharacters({ client }) {
  await setupForInput(toDataURL("<input>"));
  const { Input } = client;

  info("Write 'h'");
  await sendTextKey(Input, "h");
  await checkInputContent("h", 1);

  info("Write 'H'");
  await sendTextKey(Input, "H");
  await checkInputContent("hH", 2);

  info("Send char type event for char [’]");
  await Input.dispatchKeyEvent({
    type: "char",
    modifiers: 0,
    key: "’",
  });
  await checkInputContent("hH’", 3);
});

add_task(async function testArrowKeys({ client }) {
  await setupForInput(toDataURL("<input>"));
  const { Input } = client;

  await sendText(Input, "hH’");
  info("Send Left");
  await sendRawKey(Input, "ArrowLeft");
  await checkInputContent("hH’", 2);

  info("Write 'a'");
  await sendTextKey(Input, "a");
  await checkInputContent("hHa’", 3);

  info("Send Left");
  await sendRawKey(Input, "ArrowLeft");
  await checkInputContent("hHa’", 2);

  info("Send Left");
  await sendRawKey(Input, "ArrowLeft");
  await checkInputContent("hHa’", 1);

  info("Write 'a'");
  await sendTextKey(Input, "a");
  await checkInputContent("haHa’", 2);

  info("Send ALT/CONTROL + Right");
  const modCode = AppInfo.isMac ? alt : ctrl;
  const modKey = AppInfo.isMac ? "Alt" : "Control";
  await dispatchKeyEvent(Input, modKey, "rawKeyDown", modCode);
  await dispatchKeyEvent(Input, "ArrowRight", "rawKeyDown", modCode);
  await dispatchKeyEvent(Input, "ArrowRight", "keyUp");
  await dispatchKeyEvent(Input, modKey, "keyUp");
  await checkInputContent("haHa’", 5);
});

add_task(async function testBackspace({ client }) {
  await setupForInput(toDataURL("<input>"));
  const { Input } = client;

  await sendText(Input, "haHa’");

  info("Delete every character in the input");
  await checkBackspace(Input, "haHa");
  await checkBackspace(Input, "haH");
  await checkBackspace(Input, "ha");
  await checkBackspace(Input, "h");
  await checkBackspace(Input, "");
});

add_task(async function testShiftSelect({ client }) {
  await setupForInput(toDataURL("<input>"));
  const { Input } = client;
  await resetInput("word 2 word3");

  info("Send Shift + Left (select one char to the left)");
  await dispatchKeyEvent(Input, "Shift", "rawKeyDown", shift);
  await sendRawKey(Input, "ArrowLeft", shift);
  await sendRawKey(Input, "ArrowLeft", shift);
  await sendRawKey(Input, "ArrowLeft", shift);
  info("(deleteContentBackward)");
  await checkBackspace(Input, "word 2 wo");
  await dispatchKeyEvent(Input, "Shift", "keyUp");

  await resetInput("word 2 wo");
  info("Send Shift + Left (select one char to the left)");
  await dispatchKeyEvent(Input, "Shift", "rawKeyDown", shift);
  await sendRawKey(Input, "ArrowLeft", shift);
  await sendRawKey(Input, "ArrowLeft", shift);
  await sendTextKey(Input, "H");
  await checkInputContent("word 2 H", 8);
  await dispatchKeyEvent(Input, "Shift", "keyUp");
});

add_task(async function testSelectWord({ client }) {
  await setupForInput(toDataURL("<input>"));
  const { Input } = client;
  await resetInput("word 2 word3");

  info("Send Shift + Ctrl/Alt + Left (select one word to the left)");
  const { primary, primaryKey } = keyForPlatform();
  const combined = shift | primary;
  await dispatchKeyEvent(Input, "Shift", "rawKeyDown", shift);
  await dispatchKeyEvent(Input, primaryKey, "rawKeyDown", combined);
  await sendRawKey(Input, "ArrowLeft", combined);
  await sendRawKey(Input, "ArrowLeft", combined);
  await dispatchKeyEvent(Input, "Shift", "keyUp", primary);
  await dispatchKeyEvent(Input, primaryKey, "keyUp");
  info("(deleteContentBackward)");
  await checkBackspace(Input, "word ");
});

add_task(async function testSelectDelete({ client }) {
  await setupForInput(toDataURL("<input>"));
  const { Input } = client;
  await resetInput("word 2 word3");

  info("Send Ctrl/Alt + Backspace (deleteWordBackward)");
  const { primary, primaryKey } = keyForPlatform();
  await dispatchKeyEvent(Input, primaryKey, "rawKeyDown", primary);
  await checkBackspace(Input, "word 2 ", primary);
  await dispatchKeyEvent(Input, primaryKey, "keyUp");

  await resetInput("word 2 ");
  await sendText(Input, "word4");
  await sendRawKey(Input, "ArrowLeft");
  await sendRawKey(Input, "ArrowLeft");
  await checkInputContent("word 2 word4", 10);

  if (AppInfo.isMac) {
    info("Send Meta + Backspace (deleteSoftLineBackward)");
    await dispatchKeyEvent(Input, "Meta", "rawKeyDown", meta);
    await sendRawKey(Input, "Backspace", meta);
    await dispatchKeyEvent(Input, "Meta", "keyUp");
    await checkInputContent("d4", 0);
  }
});

add_task(async function testCtrlShiftArrows({ client }) {
  await loadURL(
    toDataURL('<select multiple size="3"><option>a<option>b<option>c</select>')
  );
  const { Input } = client;

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    const select = content.document.querySelector("select");
    select.selectedIndex = 0;
    select.focus();
  });

  const combined = shift | ctrl;
  await dispatchKeyEvent(Input, "Control", "rawKeyDown", shift);
  await dispatchKeyEvent(Input, "Shift", "rawKeyDown", combined);
  await sendRawKey(Input, "ArrowDown", combined);
  await dispatchKeyEvent(Input, "Control", "keyUp", shift);
  await dispatchKeyEvent(Input, "Shift", "keyUp");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    const select = content.document.querySelector("select");
    ok(select[0].selected, "First option should be selected");
    ok(select[1].selected, "Second option should be selected");
    ok(!select[2].selected, "Third option should not be selected");
  });
});
