# frozen_string_literal: true

require 'test/unit'
require 'todolist'

include Test::Unit::Assertions
include Todolist

todo = TodoList.new
entry = TodoEntry.new 'Write bindings for strings in records'

todo.add_item('Write ruby bindings')

assert_equal todo.get_last, 'Write ruby bindings'

todo.add_item('Write tests for bindings')

assert_equal todo.get_last, 'Write tests for bindings'

todo.add_entry(entry)

assert_equal todo.get_last, 'Write bindings for strings in records'
assert_equal todo.get_last_entry.text, 'Write bindings for strings in records'

todo.add_item("Test Ünicode hàndling without an entry can't believe I didn't test this at first 🤣")
assert_equal todo.get_last, "Test Ünicode hàndling without an entry can't believe I didn't test this at first 🤣"

entry2 = TodoEntry.new("Test Ünicode hàndling in an entry can't believe I didn't test this at first 🤣")
todo.add_entry(entry2)
assert_equal todo.get_last_entry.text, "Test Ünicode hàndling in an entry can't believe I didn't test this at first 🤣"

todo2 = TodoList.new
assert todo2.get_items != todo.get_items

assert Todolist.get_default_list == nil

Todolist.set_default_list todo
assert todo.get_items == Todolist.get_default_list.get_items

todo2.make_default
assert todo2.get_items == Todolist.get_default_list.get_items

todo.add_item "Test liveness after being demoted from default"
assert todo.get_last == "Test liveness after being demoted from default"

todo2.add_item "Test shared state through local vs default reference"
assert Todolist.get_default_list.get_last == "Test shared state through local vs default reference"