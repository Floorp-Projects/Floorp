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
use std::mem;
use std::sync::{Arc, Barrier};
use std::thread;

const THREADS: usize = 100;

#[derive(Debug)]
struct Link {
    next: AtomSetOnce<Box<Link>>,
}

impl Drop for Link {
    fn drop(&mut self) {
        // This is done to avoid a recusive drop of the List
        while let Some(mut h) = self.next.atom().take() {
            self.next = mem::replace(&mut h.next, AtomSetOnce::empty());
        }
    }
}

fn main() {
    let b = Arc::new(Barrier::new(THREADS + 1));

    let head = Arc::new(Link {
        next: AtomSetOnce::empty(),
    });

    for _ in 0..THREADS {
        let b = b.clone();
        let head = head.clone();
        thread::spawn(move || {
            let mut hptr = &*head;

            for _ in 0..10_000 {
                let mut my_awesome_node = Box::new(Link {
                    next: AtomSetOnce::empty(),
                });

                loop {
                    while let Some(h) = hptr.next.get() {
                        hptr = h;
                    }

                    my_awesome_node = match hptr.next.set_if_none(my_awesome_node) {
                        Some(v) => v,
                        None => break,
                    };
                }
            }
            b.wait();
        });
    }

    b.wait();

    let mut hptr = &*head;
    let mut count = 0;
    while let Some(h) = hptr.next.get() {
        hptr = h;
        count += 1;
    }
    println!(
        "Using {} threads we wrote {} links at the same time!",
        THREADS, count
    );
}
