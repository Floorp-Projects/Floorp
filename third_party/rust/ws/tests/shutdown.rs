extern crate ws;

use std::sync::mpsc::channel;
use std::thread;
use std::time::Duration;

#[test]
fn shutdown_before_connections() {
    let (tx, rx) = channel();
    let mut connections = 0;

    let socket = ws::Builder::new()
        .build(move |_| {
            tx.send(1).unwrap();
            |_| Ok(())
        })
        .unwrap();

    let handle = socket.broadcaster();

    let t = thread::spawn(move || {
        socket.listen("127.0.0.1:3012").unwrap();
    });

    loop {
        thread::sleep(Duration::from_millis(500));
        let res = rx.try_recv();
        assert!(res.is_err());
        match res {
            Ok(n) => connections += n,
            Err(_) => {
                if connections < 1 {
                    handle.shutdown().unwrap();
                    break;
                }
            }
        }
    }

    assert!(t.join().is_ok());
}
