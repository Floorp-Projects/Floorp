extern crate env_logger;
extern crate futures;
extern crate h2;
extern crate http;
extern crate tokio_core;

use h2::client;
use h2::RecvStream;

use futures::*;
use http::*;

use tokio_core::net::TcpStream;
use tokio_core::reactor;

struct Process {
    body: RecvStream,
    trailers: bool,
}

impl Future for Process {
    type Item = ();
    type Error = h2::Error;

    fn poll(&mut self) -> Poll<(), h2::Error> {
        loop {
            if self.trailers {
                let trailers = try_ready!(self.body.poll_trailers());

                println!("GOT TRAILERS: {:?}", trailers);

                return Ok(().into());
            } else {
                match try_ready!(self.body.poll()) {
                    Some(chunk) => {
                        println!("GOT CHUNK = {:?}", chunk);
                    },
                    None => {
                        self.trailers = true;
                    },
                }
            }
        }
    }
}

pub fn main() {
    let _ = env_logger::try_init();

    let mut core = reactor::Core::new().unwrap();
    let handle = core.handle();

    let tcp = TcpStream::connect(&"127.0.0.1:5928".parse().unwrap(), &handle);

    let tcp = tcp.then(|res| {
        let tcp = res.unwrap();
        client::handshake(tcp)
    }).then(|res| {
            let (mut client, h2) = res.unwrap();

            println!("sending request");

            let request = Request::builder()
                .uri("https://http2.akamai.com/")
                .body(())
                .unwrap();

            let mut trailers = HeaderMap::new();
            trailers.insert("zomg", "hello".parse().unwrap());

            let (response, mut stream) = client.send_request(request, false).unwrap();

            // send trailers
            stream.send_trailers(trailers).unwrap();

            // Spawn a task to run the conn...
            handle.spawn(h2.map_err(|e| println!("GOT ERR={:?}", e)));

            response
                .and_then(|response| {
                    println!("GOT RESPONSE: {:?}", response);

                    // Get the body
                    let (_, body) = response.into_parts();

                    Process {
                        body,
                        trailers: false,
                    }
                })
                .map_err(|e| {
                    println!("GOT ERR={:?}", e);
                })
        });

    core.run(tcp).unwrap();
}
