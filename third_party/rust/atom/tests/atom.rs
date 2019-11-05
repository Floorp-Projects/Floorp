//   Copyright 2015 Colin Sherratt
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

extern crate atom;

use atom::*;
use std::sync::atomic::AtomicUsize;
use std::sync::atomic::Ordering;
use std::sync::*;
use std::thread;

#[test]
fn swap() {
    let a = Atom::empty();
    assert_eq!(a.swap(Box::new(1u8)), None);
    assert_eq!(a.swap(Box::new(2u8)), Some(Box::new(1u8)));
    assert_eq!(a.swap(Box::new(3u8)), Some(Box::new(2u8)));
}

#[test]
fn take() {
    let a = Atom::new(Box::new(7u8));
    assert_eq!(a.take(), Some(Box::new(7)));
    assert_eq!(a.take(), None);
}

#[test]
fn set_if_none() {
    let a = Atom::empty();
    assert_eq!(a.set_if_none(Box::new(7u8)), None);
    assert_eq!(a.set_if_none(Box::new(8u8)), Some(Box::new(8u8)));
}

#[derive(Clone)]
struct Canary(Arc<AtomicUsize>);

impl Drop for Canary {
    fn drop(&mut self) {
        self.0.fetch_add(1, Ordering::SeqCst);
    }
}

#[test]
fn ensure_drop() {
    let v = Arc::new(AtomicUsize::new(0));
    let a = Box::new(Canary(v.clone()));
    let a = Atom::new(a);
    assert_eq!(v.load(Ordering::SeqCst), 0);
    drop(a);
    assert_eq!(v.load(Ordering::SeqCst), 1);
}

#[test]
fn ensure_drop_arc() {
    let v = Arc::new(AtomicUsize::new(0));
    let a = Arc::new(Canary(v.clone()));
    let a = Atom::new(a);
    assert_eq!(v.load(Ordering::SeqCst), 0);
    drop(a);
    assert_eq!(v.load(Ordering::SeqCst), 1);
}

#[test]
fn ensure_send() {
    let atom = Arc::new(Atom::empty());
    let wait = Arc::new(Barrier::new(2));

    let w = wait.clone();
    let a = atom.clone();
    thread::spawn(move || {
        a.swap(Box::new(7u8));
        w.wait();
    });

    wait.wait();
    assert_eq!(atom.take(), Some(Box::new(7u8)));
}

#[test]
fn get() {
    let atom = Arc::new(AtomSetOnce::empty());
    assert_eq!(atom.get(), None);
    assert_eq!(atom.set_if_none(Box::new(8u8)), None);
    assert_eq!(atom.get(), Some(&8u8));
}

#[test]
fn get_arc() {
    let atom = Arc::new(AtomSetOnce::empty());
    assert_eq!(atom.get(), None);
    assert_eq!(atom.set_if_none(Arc::new(8u8)), None);
    assert_eq!(atom.get(), Some(&8u8));

    let v = Arc::new(AtomicUsize::new(0));
    let atom = Arc::new(AtomSetOnce::empty());
    atom.get();
    atom.set_if_none(Arc::new(Canary(v.clone())));
    atom.get();
    drop(atom);

    assert_eq!(v.load(Ordering::SeqCst), 1);
}

#[derive(Debug)]
struct Link {
    next: Option<Box<Link>>,
    value: u32,
}

impl Link {
    fn new(v: u32) -> Box<Link> {
        Box::new(Link {
            next: None,
            value: v,
        })
    }
}

impl GetNextMut for Box<Link> {
    type NextPtr = Option<Box<Link>>;
    fn get_next(&mut self) -> &mut Option<Box<Link>> {
        &mut self.next
    }
}

#[test]
fn lifo() {
    let atom = Atom::empty();
    for i in 0..100 {
        let x = atom.replace_and_set_next(Link::new(99 - i));
        assert_eq!(x, i == 0);
    }

    let expected: Vec<u32> = (0..100).collect();
    let mut found = Vec::new();
    let mut chain = atom.take();
    while let Some(v) = chain {
        found.push(v.value);
        chain = v.next;
    }
    assert_eq!(expected, found);
}

#[allow(dead_code)]
struct LinkCanary {
    next: Option<Box<LinkCanary>>,
    value: Canary,
}

impl LinkCanary {
    fn new(v: Canary) -> Box<LinkCanary> {
        Box::new(LinkCanary {
            next: None,
            value: v,
        })
    }
}

impl GetNextMut for Box<LinkCanary> {
    type NextPtr = Option<Box<LinkCanary>>;
    fn get_next(&mut self) -> &mut Option<Box<LinkCanary>> {
        &mut self.next
    }
}

#[test]
fn lifo_drop() {
    let v = Arc::new(AtomicUsize::new(0));
    let canary = Canary(v.clone());
    let mut link = LinkCanary::new(canary.clone());
    link.next = Some(LinkCanary::new(canary.clone()));

    let atom = Atom::empty();
    atom.replace_and_set_next(link);
    assert_eq!(1, v.load(Ordering::SeqCst));
    drop(atom);
    assert_eq!(2, v.load(Ordering::SeqCst));
}
