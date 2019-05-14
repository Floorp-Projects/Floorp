extern crate ws;
#[cfg(feature="ssl")]
extern crate openssl;
extern crate env_logger;
extern crate url;

#[cfg(feature="ssl")]
use openssl::ssl::{SslConnectorBuilder, SslMethod, SslStream, SslVerifyMode};
#[cfg(feature="ssl")]
use ws::util::TcpStream;

#[cfg(feature="ssl")]
struct Client {
    out: ws::Sender,
}

#[cfg(feature="ssl")]
impl ws::Handler for Client {

    fn on_message(&mut self, msg: ws::Message) -> ws::Result<()> {
        println!("msg = {}", msg);
        self.out.close(ws::CloseCode::Normal)
    }

    fn upgrade_ssl_client(&mut self, sock: TcpStream, _: &url::Url) -> ws::Result<SslStream<TcpStream>> {
        let mut builder = SslConnectorBuilder::new(SslMethod::tls()).map_err(|e| {
            ws::Error::new(ws::ErrorKind::Internal, format!("Failed to upgrade client to SSL: {}", e))
        })?;
        builder.builder_mut().set_verify(SslVerifyMode::empty());

        let connector = builder.build();
        connector
            .danger_connect_without_providing_domain_for_certificate_verification_and_server_name_indication(sock)
            .map_err(From::from)
    }
}

#[cfg(feature="ssl")]
fn main () {
    // Setup logging
    env_logger::init().unwrap();

    if let Err(error) = ws::connect("wss://localhost:3443/api/websocket", |out| {

        if let Err(_) = out.send("Hello WebSocket") {
            println!("Websocket couldn't queue an initial message.")
        } else {
            println!("Client sent message 'Hello WebSocket'. ")
        }

        Client { out: out }

    }) {
        println!("Failed to create WebSocket due to: {:?}", error);
    }
}

#[cfg(not(feature="ssl"))]
fn main() {
    println!("SSL feature is not enabled.")
}
