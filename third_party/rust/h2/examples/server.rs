extern crate bytes;
extern crate env_logger;
extern crate futures;
extern crate h2;
extern crate http;
extern crate tokio_core;

use h2::server;

use bytes::*;
use futures::*;
use http::*;

use tokio_core::net::TcpListener;
use tokio_core::reactor;

pub fn main() {
    let _ = env_logger::try_init();

    let mut core = reactor::Core::new().unwrap();
    let handle = core.handle();

    let listener = TcpListener::bind(&"127.0.0.1:5928".parse().unwrap(), &handle).unwrap();

    println!("listening on {:?}", listener.local_addr());

    let server = listener.incoming().for_each(move |(socket, _)| {
        // let socket = io_dump::Dump::to_stdout(socket);

        let connection = server::handshake(socket)
            .and_then(|conn| {
                println!("H2 connection bound");

                conn.for_each(|(request, mut respond)| {
                    println!("GOT request: {:?}", request);

                    let response = Response::builder().status(StatusCode::OK).body(()).unwrap();

                    let mut send = match respond.send_response(response, false) {
                        Ok(send) => send,
                        Err(e) => {
                            println!(" error respond; err={:?}", e);
                            return Ok(());
                        }
                    };

                    println!(">>>> sending data");
                    if let Err(e) = send.send_data(Bytes::from_static(b"hello world"), true) {
                        println!("  -> err={:?}", e);
                    }

                    Ok(())
                })
            })
            .and_then(|_| {
                println!("~~~~~~~~~~~~~~~~~~~~~~~~~~~ H2 connection CLOSE !!!!!! ~~~~~~~~~~~");
                Ok(())
            })
            .then(|res| {
                if let Err(e) = res {
                    println!("  -> err={:?}", e);
                }

                Ok(())
            });

        handle.spawn(connection);
        Ok(())
    });

    core.run(server).unwrap();
}
