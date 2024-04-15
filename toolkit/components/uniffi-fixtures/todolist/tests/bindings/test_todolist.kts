import uniffi.todolist.*

val todo = TodoList()

// This throws an exception:
try {
    todo.getLast()
    throw RuntimeException("Should have thrown a TodoError!")
} catch (e: TodoException.EmptyTodoList) {
    // It's okay, we don't have any items yet!
}

try {
    createEntryWith("")
    throw RuntimeException("Should have thrown a TodoError!")
} catch (e: TodoException) {
    // It's okay, the string was empty!
    assert(e is TodoException.EmptyString)
    assert(e !is TodoException.EmptyTodoList)
}

todo.addItem("Write strings support")

assert(todo.getLast() == "Write strings support")

todo.addItem("Write tests for strings support")

assert(todo.getLast() == "Write tests for strings support")

val entry = createEntryWith("Write bindings for strings as record members")

todo.addEntry(entry)
assert(todo.getLast() == "Write bindings for strings as record members")
assert(todo.getLastEntry().text == "Write bindings for strings as record members")

todo.addItem("Test Ünicode hàndling without an entry can't believe I didn't test this at first 🤣")
assert(todo.getLast() == "Test Ünicode hàndling without an entry can't believe I didn't test this at first 🤣")

val entry2 = TodoEntry("Test Ünicode hàndling in an entry can't believe I didn't test this at first 🤣")
todo.addEntry(entry2)
assert(todo.getLastEntry().text == "Test Ünicode hàndling in an entry can't believe I didn't test this at first 🤣")

assert(todo.getEntries().size == 5)

todo.addEntries(listOf(TodoEntry("foo"), TodoEntry("bar")))
assert(todo.getEntries().size == 7)
assert(todo.getLastEntry().text == "bar")

todo.addItems(listOf("bobo", "fofo"))
assert(todo.getItems().size == 9)
assert(todo.getItems()[7] == "bobo")

assert(getDefaultList() == null)

// Note that each individual object instance needs to be explicitly destroyed,
// either by using the `.use` helper or explicitly calling its `.destroy` method.
// Failure to do so will leak the underlying Rust object.
TodoList().use { todo2 ->
    setDefaultList(todo)
    getDefaultList()!!.use { default ->
        assert(todo.getEntries() == default.getEntries())
        assert(todo2.getEntries() != default.getEntries())
    }

    todo2.makeDefault()
    getDefaultList()!!.use { default ->
        assert(todo.getEntries() != default.getEntries())
        assert(todo2.getEntries() == default.getEntries())
    }

    todo.addItem("Test liveness after being demoted from default")
    assert(todo.getLast() == "Test liveness after being demoted from default")

    todo2.addItem("Test shared state through local vs default reference")
    getDefaultList()!!.use { default ->
        assert(default.getLast() == "Test shared state through local vs default reference")
    }
}

// Ensure the kotlin version of deinit doesn't crash, and is idempotent.
todo.destroy()
todo.destroy()

