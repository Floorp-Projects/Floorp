extern crate env_logger;
extern crate time;
extern crate url;
/// A simple, but immature, benchmark client for destroying other WebSocket frameworks and proving
/// WS-RS's performance excellence. ;)
/// Make sure you allow for enough connections in your OS (e.g. ulimit -Sn 10000).
extern crate ws;

// Try this against node for some fun

// TODO: Separate this example into a separate lib
// TODO: num threads, num connections per thread, num concurrent connections per thread, num
// messages per connection, length of message, text or binary

use ws::{Builder, CloseCode, Handler, Handshake, Message, Result, Sender, Settings};

const CONNECTIONS: usize = 10_000; // simultaneous
const MESSAGES: u32 = 10;
static MESSAGE: &'static str = "TEST TEST TEST TEST TEST TEST TEST TEST";

fn main() {
    env_logger::init();

    let url = url::Url::parse("ws://127.0.0.1:3012").unwrap();

    struct Connection {
        out: Sender,
        count: u32,
        time: u64,
        total: u64,
    }

    impl Handler for Connection {
        fn on_open(&mut self, _: Handshake) -> Result<()> {
            self.out.send(MESSAGE)?;
            self.count += 1;
            self.time = time::precise_time_ns();
            Ok(())
        }

        fn on_message(&mut self, msg: Message) -> Result<()> {
            assert_eq!(msg.as_text().unwrap(), MESSAGE);
            if self.count > MESSAGES {
                self.out.close(CloseCode::Normal)?;
            } else {
                self.out.send(MESSAGE)?;
                let time = time::precise_time_ns();
                // println!("time {}", time -self.time);
                self.total += time - self.time;
                self.count += 1;
                self.time = time;
            }
            Ok(())
        }
    }

    let mut ws = Builder::new()
        .with_settings(Settings {
            max_connections: CONNECTIONS,
            ..Settings::default()
        })
        .build(|out| Connection {
            out,
            count: 0,
            time: 0,
            total: 0,
        })
        .unwrap();

    for _ in 0..CONNECTIONS {
        ws.connect(url.clone()).unwrap();
    }
    let start = time::precise_time_ns();
    ws.run().unwrap();
    println!(
        "Total time. {}",
        (time::precise_time_ns() - start) / 1_000_000
    )
}
