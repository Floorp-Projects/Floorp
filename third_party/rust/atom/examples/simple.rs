extern crate atom;

use atom::*;
use std::sync::Arc;
use std::thread;

fn main() {
    // Create an empty atom
    let shared_atom = Arc::new(Atom::empty());

    // set the value 75
    shared_atom.swap(Box::new(75));

    // Spawn a bunch of thread that will try and take the value
    let threads: Vec<thread::JoinHandle<()>> = (0..8)
        .map(|_| {
            let shared_atom = shared_atom.clone();
            thread::spawn(move || {
                // Take the contents of the atom, only one will win the race
                if let Some(v) = shared_atom.take() {
                    println!("I got it: {:?} :D", v);
                } else {
                    println!("I did not get it :(");
                }
            })
        })
        .collect();

    // join the threads
    for t in threads {
        t.join().unwrap();
    }
}
