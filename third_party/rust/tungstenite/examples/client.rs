extern crate tungstenite;
extern crate url;
extern crate env_logger;

use url::Url;
use tungstenite::{Message, connect};

fn main() {
    env_logger::init();

    let (mut socket, response) = connect(Url::parse("ws://localhost:3012/socket").unwrap())
        .expect("Can't connect");

    println!("Connected to the server");
    println!("Response HTTP code: {}", response.code);
    println!("Response contains the following headers:");
    for &(ref header, _ /*value*/) in response.headers.iter() {
        println!("* {}", header);
    }

    socket.write_message(Message::Text("Hello WebSocket".into())).unwrap();
    loop {
        let msg = socket.read_message().expect("Error reading message");
        println!("Received: {}", msg);
    }
    // socket.close(None);

}
