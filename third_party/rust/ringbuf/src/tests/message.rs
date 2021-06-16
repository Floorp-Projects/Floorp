use std::{
    io::{self, Read, Write},
    thread,
    time::Duration,
};

use crate::RingBuffer;

const THE_BOOK_FOREWORD: &'static str = "
It wasn’t always so clear, but the Rust programming language is fundamentally about empowerment: no matter what kind of code you are writing now, Rust empowers you to reach farther, to program with confidence in a wider variety of domains than you did before.
Take, for example, “systems-level” work that deals with low-level details of memory management, data representation, and concurrency. Traditionally, this realm of programming is seen as arcane, accessible only to a select few who have devoted the necessary years learning to avoid its infamous pitfalls. And even those who practice it do so with caution, lest their code be open to exploits, crashes, or corruption.
Rust breaks down these barriers by eliminating the old pitfalls and providing a friendly, polished set of tools to help you along the way. Programmers who need to “dip down” into lower-level control can do so with Rust, without taking on the customary risk of crashes or security holes, and without having to learn the fine points of a fickle toolchain. Better yet, the language is designed to guide you naturally towards reliable code that is efficient in terms of speed and memory usage.
Programmers who are already working with low-level code can use Rust to raise their ambitions. For example, introducing parallelism in Rust is a relatively low-risk operation: the compiler will catch the classical mistakes for you. And you can tackle more aggressive optimizations in your code with the confidence that you won’t accidentally introduce crashes or vulnerabilities.
But Rust isn’t limited to low-level systems programming. It’s expressive and ergonomic enough to make CLI apps, web servers, and many other kinds of code quite pleasant to write — you’ll find simple examples of both later in the book. Working with Rust allows you to build skills that transfer from one domain to another; you can learn Rust by writing a web app, then apply those same skills to target your Raspberry Pi.
This book fully embraces the potential of Rust to empower its users. It’s a friendly and approachable text intended to help you level up not just your knowledge of Rust, but also your reach and confidence as a programmer in general. So dive in, get ready to learn—and welcome to the Rust community!

— Nicholas Matsakis and Aaron Turon
";

#[test]
fn push_pop_slice() {
    let buf = RingBuffer::<u8>::new(7);
    let (mut prod, mut cons) = buf.split();

    let smsg = THE_BOOK_FOREWORD;

    let pjh = thread::spawn(move || {
        let mut bytes = smsg.as_bytes();
        while bytes.len() > 0 {
            let n = prod.push_slice(bytes);
            if n > 0 {
                bytes = &bytes[n..bytes.len()]
            } else {
                thread::sleep(Duration::from_millis(1))
            }
        }
        loop {
            match prod.push(0) {
                Ok(()) => break,
                Err(_) => thread::sleep(Duration::from_millis(1)),
            }
        }
    });

    let cjh = thread::spawn(move || {
        let mut bytes = Vec::<u8>::new();
        let mut buffer = [0; 5];
        loop {
            let n = cons.pop_slice(&mut buffer);
            if n > 0 {
                bytes.extend_from_slice(&buffer[0..n])
            } else {
                if bytes.ends_with(&[0]) {
                    break;
                } else {
                    thread::sleep(Duration::from_millis(1));
                }
            }
        }

        assert_eq!(bytes.pop().unwrap(), 0);
        String::from_utf8(bytes).unwrap()
    });

    pjh.join().unwrap();
    let rmsg = cjh.join().unwrap();

    assert_eq!(smsg, rmsg);
}

#[test]
fn read_from_write_into() {
    let buf = RingBuffer::<u8>::new(7);
    let (mut prod, mut cons) = buf.split();

    let smsg = THE_BOOK_FOREWORD;

    let pjh = thread::spawn(move || {
        let zero = [0 as u8];
        let mut bytes = smsg.as_bytes().chain(&zero[..]);
        loop {
            if prod.is_full() {
                thread::sleep(Duration::from_millis(1));
            } else {
                if prod.read_from(&mut bytes, None).unwrap() == 0 {
                    break;
                }
            }
        }
    });

    let cjh = thread::spawn(move || {
        let mut bytes = Vec::<u8>::new();
        loop {
            if cons.is_empty() {
                if bytes.ends_with(&[0]) {
                    break;
                } else {
                    thread::sleep(Duration::from_millis(1));
                }
            } else {
                cons.write_into(&mut bytes, None).unwrap();
            }
        }

        assert_eq!(bytes.pop().unwrap(), 0);
        String::from_utf8(bytes).unwrap()
    });

    pjh.join().unwrap();
    let rmsg = cjh.join().unwrap();

    assert_eq!(smsg, rmsg);
}

#[test]
fn read_write() {
    let buf = RingBuffer::<u8>::new(7);
    let (mut prod, mut cons) = buf.split();

    let smsg = THE_BOOK_FOREWORD;

    let pjh = thread::spawn(move || {
        let mut bytes = smsg.as_bytes();
        while bytes.len() > 0 {
            match prod.write(bytes) {
                Ok(n) => bytes = &bytes[n..bytes.len()],
                Err(err) => {
                    assert_eq!(err.kind(), io::ErrorKind::WouldBlock);
                    thread::sleep(Duration::from_millis(1));
                }
            }
        }
        loop {
            match prod.push(0) {
                Ok(()) => break,
                Err(_) => thread::sleep(Duration::from_millis(1)),
            }
        }
    });

    let cjh = thread::spawn(move || {
        let mut bytes = Vec::<u8>::new();
        let mut buffer = [0; 5];
        loop {
            match cons.read(&mut buffer) {
                Ok(n) => bytes.extend_from_slice(&buffer[0..n]),
                Err(err) => {
                    assert_eq!(err.kind(), io::ErrorKind::WouldBlock);
                    if bytes.ends_with(&[0]) {
                        break;
                    } else {
                        thread::sleep(Duration::from_millis(1));
                    }
                }
            }
        }

        assert_eq!(bytes.pop().unwrap(), 0);
        String::from_utf8(bytes).unwrap()
    });

    pjh.join().unwrap();
    let rmsg = cjh.join().unwrap();

    assert_eq!(smsg, rmsg);
}
