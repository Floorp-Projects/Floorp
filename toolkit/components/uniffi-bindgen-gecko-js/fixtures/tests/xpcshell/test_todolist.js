/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  TodoList,
  TodoEntry,
  getDefaultList,
  setDefaultList,
} = ChromeUtils.import(
  "resource://gre/modules/components-utils/RustTodolist.jsm"
);

add_task(async function() {
  const todo = await TodoList.init();
  const entry = new TodoEntry("Write bindings for strings in records");

  await todo.addItem("Write JS bindings");
  Assert.equal(await todo.getLast(), "Write JS bindings");

  await todo.addItem("Write tests for bindings");
  Assert.equal(await todo.getLast(), "Write tests for bindings");

  await todo.addEntry(entry);
  Assert.equal(await todo.getLast(), "Write bindings for strings in records");
  Assert.equal(
    (await todo.getLastEntry()).text,
    "Write bindings for strings in records"
  );
  Assert.ok((await todo.getLastEntry()).equals(entry));

  await todo.addItem(
    "Test Ünicode hàndling without an entry can't believe I didn't test this at first 🤣"
  );
  Assert.equal(
    await todo.getLast(),
    "Test Ünicode hàndling without an entry can't believe I didn't test this at first 🤣"
  );

  const entry2 = new TodoEntry(
    "Test Ünicode hàndling in an entry can't believe I didn't test this at first 🤣"
  );
  await todo.addEntry(entry2);
  Assert.equal(
    (await todo.getLastEntry()).text,
    "Test Ünicode hàndling in an entry can't believe I didn't test this at first 🤣"
  );

  const todo2 = await TodoList.init();
  Assert.notEqual(todo, todo2);
  Assert.notStrictEqual(todo, todo2);

  Assert.strictEqual(await getDefaultList(), null);

  await setDefaultList(todo);
  Assert.deepEqual(
    await todo.getItems(),
    await (await getDefaultList()).getItems()
  );

  todo2.makeDefault();
  Assert.deepEqual(
    await todo2.getItems(),
    await (await getDefaultList()).getItems()
  );

  await todo.addItem("Test liveness after being demoted from default");
  Assert.equal(
    await todo.getLast(),
    "Test liveness after being demoted from default"
  );

  todo2.addItem("Test shared state through local vs default reference");
  Assert.equal(
    await (await getDefaultList()).getLast(),
    "Test shared state through local vs default reference"
  );
});
