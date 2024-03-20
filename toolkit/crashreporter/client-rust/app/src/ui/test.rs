/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! A renderer for use in tests, which doesn't actually render a GUI but allows programmatic
//! interaction.

use super::model::{self, Application, Element};
use std::cell::RefCell;
use std::collections::HashMap;
use std::sync::{
    atomic::{AtomicBool, AtomicU8, Ordering::Relaxed},
    mpsc, Arc, Condvar, Mutex,
};

thread_local! {
    static INTERACT: RefCell<Option<Arc<State>>> = Default::default();
}

/// A test UI which allows access to the UI elements.
#[derive(Default)]
pub struct UI {
    interface: Mutex<Option<UIInterface>>,
}

impl UI {
    pub fn run_loop(&self, app: Application) {
        let (tx, rx) = mpsc::channel();
        let interface = UIInterface { work: tx };

        let elements = id_elements(&app);
        INTERACT.with(cc! { (interface) move |r| {
            if let Some(state) = &*r.borrow() {
                state.set_interface(interface);
            }
        }});
        *self.interface.lock().unwrap() = Some(interface.clone());

        // Close the UI when the root windows are closed.
        // Use a bitfield rather than a count in case the `close` event is fired multiple times.
        assert!(app.windows.len() <= 8);
        let mut windows = 0u8;
        for i in 0..app.windows.len() {
            windows |= 1 << i;
        }
        let windows = Arc::new(AtomicU8::new(windows));
        for (index, window) in app.windows.iter().enumerate() {
            if let Some(c) = &window.element_type.close {
                c.subscribe(cc! { (interface, windows) move |&()| {
                    let old = windows
                        .fetch_update(Relaxed, Relaxed, |x| Some(x & !(1u8 << index)))
                        .unwrap();
                    if old == 1u8 << index {
                        interface.work.send(Command::Finish).unwrap();
                    }
                }});
            } else {
                // No close event, so we must assume a closed state (and assume that _some_ window
                // will have a close event registered so we don't drop the interface now).
                windows
                    .fetch_update(Relaxed, Relaxed, |x| Some(x & !(1u8 << index)))
                    .unwrap();
            }
        }

        while let Ok(f) = rx.recv() {
            match f {
                Command::Invoke(f) => f(),
                Command::Interact(f) => f(&elements),
                Command::Finish => break,
            }
        }

        *self.interface.lock().unwrap() = None;
        INTERACT.with(|r| {
            if let Some(state) = &*r.borrow() {
                state.clear_interface();
            }
        });
    }

    pub fn invoke(&self, f: model::InvokeFn) {
        let guard = self.interface.lock().unwrap();
        if let Some(interface) = &*guard {
            let _ = interface.work.send(Command::Invoke(f));
        }
    }
}

/// Test interaction hook.
#[derive(Clone)]
pub struct Interact {
    state: Arc<State>,
}

impl Interact {
    /// Create an interaction hook for the test UI.
    ///
    /// This should be done before running the UI, and must be done on the same thread that
    /// later runs it.
    pub fn hook() -> Self {
        let v = Interact {
            state: Default::default(),
        };
        {
            let state = v.state.clone();
            INTERACT.with(move |r| *r.borrow_mut() = Some(state));
        }
        v
    }

    /// Wait for the render thread to be ready for interaction.
    pub fn wait_for_ready(&self) {
        let mut guard = self.state.interface.lock().unwrap();
        while guard.is_none() && !self.state.cancel.load(Relaxed) {
            guard = self.state.waiting_for_interface.wait(guard).unwrap();
        }
    }

    /// Cancel an Interact (which causes `wait_for_ready` to always return).
    pub fn cancel(&self) {
        self.state.cancel.store(true, Relaxed);
        self.state.waiting_for_interface.notify_all();
    }

    /// Run the given function on the element with the given type and identity.
    ///
    /// Panics if either the id is missing or the type is incorrect.
    pub fn element<'a, 'b, T: 'b, F, R>(&self, id: &'a str, f: F) -> R
    where
        &'b T: TryFrom<&'b model::ElementType>,
        F: FnOnce(&model::ElementStyle, &T) -> R + Send + 'a,
        R: Send + 'a,
    {
        self.interact(id, move |element: &IdElement| match element {
            IdElement::Generic(e) => Some(f(&e.style, (&e.element_type).try_into().ok()?)),
            IdElement::Window(_) => None,
        })
        .expect("incorrect element type")
    }

    /// Run the given function on the window with the given identity.
    ///
    /// Panics if the id is missing or the type is incorrect.
    pub fn window<'a, F, R>(&self, id: &'a str, f: F) -> R
    where
        F: FnOnce(&model::ElementStyle, &model::Window) -> R + Send + 'a,
        R: Send + 'a,
    {
        self.interact(id, move |element| match element {
            IdElement::Window(e) => Some(f(&e.style, &e.element_type)),
            IdElement::Generic(_) => None,
        })
        .expect("incorrect element type")
    }

    fn interact<'a, 'b, F, R>(&self, id: &'a str, f: F) -> R
    where
        F: FnOnce(&IdElement<'b>) -> R + Send + 'a,
        R: Send + 'a,
    {
        let (send, recv) = std::sync::mpsc::sync_channel(0);
        {
            let f: Box<dyn FnOnce(&IdElements<'b>) + Send + 'a> = Box::new(move |elements| {
                let _ = send.send(elements.get(id).map(f));
            });

            // # Safety
            // The function is run while `'a` is still valid (we wait here for it to complete).
            let f: Box<dyn FnOnce(&IdElements) + Send + 'static> =
                unsafe { std::mem::transmute(f) };

            let guard = self.state.interface.lock().unwrap();
            let interface = guard.as_ref().expect("renderer is not running");
            let _ = interface.work.send(Command::Interact(f));
        }
        recv.recv().unwrap().expect("failed to get element")
    }
}

#[derive(Clone)]
struct UIInterface {
    work: mpsc::Sender<Command>,
}

enum Command {
    Invoke(Box<dyn FnOnce() + Send + 'static>),
    Interact(Box<dyn FnOnce(&IdElements) + Send + 'static>),
    Finish,
}

enum IdElement<'a> {
    Generic(&'a Element),
    Window(&'a model::TypedElement<model::Window>),
}

type IdElements<'a> = HashMap<String, IdElement<'a>>;

#[derive(Default)]
struct State {
    interface: Mutex<Option<UIInterface>>,
    waiting_for_interface: Condvar,
    cancel: AtomicBool,
}

impl State {
    /// Set the interface for the interaction client to use.
    pub fn set_interface(&self, interface: UIInterface) {
        *self.interface.lock().unwrap() = Some(interface);
        self.waiting_for_interface.notify_all();
    }

    /// Clear the UI interface.
    pub fn clear_interface(&self) {
        *self.interface.lock().unwrap() = None;
    }
}

fn id_elements<'a>(app: &'a Application) -> IdElements<'a> {
    let mut elements: IdElements<'a> = Default::default();

    let mut windows_to_visit: Vec<_> = app.windows.iter().collect();

    let mut to_visit: Vec<&'a Element> = Vec::new();
    while let Some(window) = windows_to_visit.pop() {
        if let Some(id) = &window.style.id {
            elements.insert(id.to_owned(), IdElement::Window(window));
        }
        windows_to_visit.extend(&window.element_type.children);

        if let Some(content) = &window.element_type.content {
            to_visit.push(content);
        }
    }

    while let Some(el) = to_visit.pop() {
        if let Some(id) = &el.style.id {
            elements.insert(id.to_owned(), IdElement::Generic(el));
        }

        use model::ElementType::*;
        match &el.element_type {
            Button(model::Button {
                content: Some(content),
                ..
            })
            | Scroll(model::Scroll {
                content: Some(content),
            }) => {
                to_visit.push(content);
            }
            VBox(model::VBox { items, .. }) | HBox(model::HBox { items, .. }) => {
                for item in items {
                    to_visit.push(item)
                }
            }
            _ => (),
        }
    }

    elements
}
