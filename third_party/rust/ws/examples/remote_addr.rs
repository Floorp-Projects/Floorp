/// Example showing how to obtain the ip address of the client, where possible.
extern crate ws;

struct Server {
    ws: ws::Sender,
}

impl ws::Handler for Server {
    fn on_open(&mut self, shake: ws::Handshake) -> ws::Result<()> {
        if let Some(ip_addr) = shake.remote_addr()? {
            println!("Connection opened from {}.", ip_addr)
        } else {
            println!("Unable to obtain client's IP address.")
        }
        Ok(())
    }

    fn on_message(&mut self, _: ws::Message) -> ws::Result<()> {
        self.ws.close(ws::CloseCode::Normal)
    }
}

fn main() {
    ws::listen("127.0.0.1:3012", |out| Server { ws: out }).unwrap()
}
