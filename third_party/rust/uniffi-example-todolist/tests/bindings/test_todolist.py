from todolist import *

todo = TodoList()

entry = TodoEntry(text="Write bindings for strings in records")

todo.add_item("Write python bindings")

assert(todo.get_last() == "Write python bindings")

todo.add_item("Write tests for bindings")

assert(todo.get_last() == "Write tests for bindings")

todo.add_entry(entry)

assert(todo.get_last() == "Write bindings for strings in records")
assert(todo.get_last_entry().text == "Write bindings for strings in records")

todo.add_item("Test Ünicode hàndling without an entry can't believe I didn't test this at first 🤣")
assert(todo.get_last() == "Test Ünicode hàndling without an entry can't believe I didn't test this at first 🤣")

entry2 = TodoEntry(text="Test Ünicode hàndling in an entry can't believe I didn't test this at first 🤣")
todo.add_entry(entry2)
assert(todo.get_last_entry().text == "Test Ünicode hàndling in an entry can't believe I didn't test this at first 🤣")

todo2 = TodoList()
assert(todo != todo2)
assert(todo is not todo2)

assert(get_default_list() is None)

set_default_list(todo)
assert(todo.get_items() == get_default_list().get_items())

todo2.make_default()
assert(todo2.get_items() == get_default_list().get_items())

todo.add_item("Test liveness after being demoted from default")
assert(todo.get_last() == "Test liveness after being demoted from default")

todo2.add_item("Test shared state through local vs default reference")
assert(get_default_list().get_last() == "Test shared state through local vs default reference")

