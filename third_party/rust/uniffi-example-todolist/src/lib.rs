/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::sync::{Arc, RwLock};

use once_cell::sync::Lazy;

#[derive(Debug, Clone)]
pub struct TodoEntry {
    text: String,
}

// There is a single "default" TodoList that can be shared
// by all consumers of this component. Depending on requirements,
// a real app might like to use a `Weak<>` rather than an `Arc<>`
// here to reduce the risk of circular references.
static DEFAULT_LIST: Lazy<RwLock<Option<Arc<TodoList>>>> = Lazy::new(|| RwLock::new(None));

#[derive(Debug, thiserror::Error)]
pub enum TodoError {
    #[error("The todo does not exist!")]
    TodoDoesNotExist,
    #[error("The todolist is empty!")]
    EmptyTodoList,
    #[error("That todo already exists!")]
    DuplicateTodo,
    #[error("Empty String error!: {0}")]
    EmptyString(String),
    #[error("I am a delegated Error: {0}")]
    DeligatedError(#[from] std::io::Error),
}

/// Get a reference to the global default TodoList, if set.
///
fn get_default_list() -> Option<Arc<TodoList>> {
    DEFAULT_LIST.read().unwrap().clone()
}

/// Set the global default TodoList.
///
/// This will silently drop any previously set value.
///
fn set_default_list(list: Arc<TodoList>) {
    *DEFAULT_LIST.write().unwrap() = Some(list);
}

/// Create a new TodoEntry from the given string.
///
fn create_entry_with<S: Into<String>>(item: S) -> Result<TodoEntry> {
    let text = item.into();
    if text.is_empty() {
        return Err(TodoError::EmptyString(
            "Cannot add empty string as entry".to_string(),
        ));
    }
    Ok(TodoEntry { text })
}

type Result<T, E = TodoError> = std::result::Result<T, E>;

// A simple Todolist.
// UniFFI requires that we use interior mutability for managing mutable state, so we wrap our `Vec` in a RwLock.
// (A Mutex would also work, but a RwLock is more appropriate for this use-case, so we use it).
#[derive(Debug)]
pub struct TodoList {
    items: RwLock<Vec<String>>,
}

impl TodoList {
    fn new() -> Self {
        Self {
            items: RwLock::new(Vec::new()),
        }
    }

    fn add_item<S: Into<String>>(&self, item: S) -> Result<()> {
        let item = item.into();
        if item.is_empty() {
            return Err(TodoError::EmptyString(
                "Cannot add empty string as item".to_string(),
            ));
        }
        let mut items = self.items.write().unwrap();
        if items.contains(&item) {
            return Err(TodoError::DuplicateTodo);
        }
        items.push(item);
        Ok(())
    }

    fn get_last(&self) -> Result<String> {
        let items = self.items.read().unwrap();
        items.last().cloned().ok_or(TodoError::EmptyTodoList)
    }

    fn get_first(&self) -> Result<String> {
        let items = self.items.read().unwrap();
        items.first().cloned().ok_or(TodoError::EmptyTodoList)
    }

    fn add_entries(&self, entries: Vec<TodoEntry>) {
        let mut items = self.items.write().unwrap();
        items.extend(entries.into_iter().map(|e| e.text))
    }

    fn add_entry(&self, entry: TodoEntry) -> Result<()> {
        self.add_item(entry.text)
    }

    fn add_items<S: Into<String>>(&self, items: Vec<S>) {
        let mut my_items = self.items.write().unwrap();
        my_items.extend(items.into_iter().map(Into::into))
    }

    fn get_items(&self) -> Vec<String> {
        let items = self.items.read().unwrap();
        items.clone()
    }

    fn get_entries(&self) -> Vec<TodoEntry> {
        let items = self.items.read().unwrap();
        items
            .iter()
            .map(|text| TodoEntry { text: text.clone() })
            .collect()
    }

    fn get_last_entry(&self) -> Result<TodoEntry> {
        let text = self.get_last()?;
        Ok(TodoEntry { text })
    }

    fn clear_item<S: Into<String>>(&self, item: S) -> Result<()> {
        let item = item.into();
        let mut items = self.items.write().unwrap();
        let idx = items
            .iter()
            .position(|s| s == &item)
            .ok_or(TodoError::TodoDoesNotExist)?;
        items.remove(idx);
        Ok(())
    }

    fn make_default(self: Arc<Self>) {
        set_default_list(self);
    }
}

uniffi::include_scaffolding!("todolist");
