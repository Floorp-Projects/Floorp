extern crate ringbuf;

use std::io::Read;
use std::thread;
use std::time::Duration;

use ringbuf::RingBuffer;

fn main() {
    let buf = RingBuffer::<u8>::new(10);
    let (mut prod, mut cons) = buf.split();

    let smsg = "The quick brown fox jumps over the lazy dog";

    let pjh = thread::spawn(move || {
        println!("-> sending message: '{}'", smsg);

        let zero = [0 as u8];
        let mut bytes = smsg.as_bytes().chain(&zero[..]);
        loop {
            if prod.is_full() {
                println!("-> buffer is full, waiting");
                thread::sleep(Duration::from_millis(1));
            } else {
                let n = prod.read_from(&mut bytes, None).unwrap();
                if n == 0 {
                    break;
                }
                println!("-> {} bytes sent", n);
            }
        }

        println!("-> message sent");
    });

    let cjh = thread::spawn(move || {
        println!("<- receiving message");

        let mut bytes = Vec::<u8>::new();
        loop {
            if cons.is_empty() {
                if bytes.ends_with(&[0]) {
                    break;
                } else {
                    println!("<- buffer is empty, waiting");
                    thread::sleep(Duration::from_millis(1));
                }
            } else {
                let n = cons.write_into(&mut bytes, None).unwrap();
                println!("<- {} bytes received", n);
            }
        }

        assert_eq!(bytes.pop().unwrap(), 0);
        let msg = String::from_utf8(bytes).unwrap();
        println!("<- message received: '{}'", msg);

        msg
    });

    pjh.join().unwrap();
    let rmsg = cjh.join().unwrap();

    assert_eq!(smsg, rmsg);
}
