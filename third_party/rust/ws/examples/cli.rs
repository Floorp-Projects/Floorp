extern crate clap;
extern crate env_logger;
extern crate term;
/// Run this cli like this:
/// cargo run --example server
/// cargo run --example cli -- ws://127.0.0.1:3012
extern crate ws;

use std::io;
use std::io::prelude::*;
use std::sync::mpsc::Sender as TSender;
use std::sync::mpsc::channel;
use std::thread;

use clap::{App, Arg};
use ws::{connect, CloseCode, Error, ErrorKind, Handler, Handshake, Message, Result, Sender};

fn main() {
    // Setup logging
    env_logger::init();

    // setup command line arguments
    let matches = App::new("WS Command Line Client")
        .version("1.1")
        .author("Jason Housley <housleyjk@gmail.com>")
        .about("Connect to a WebSocket and send messages from the command line.")
        .arg(
            Arg::with_name("URL")
                .help("The URL of the WebSocket server.")
                .required(true)
                .index(1),
        )
        .get_matches();

    let url = matches.value_of("URL").unwrap().to_string();

    let (tx, rx) = channel();

    // Run client thread with channel to give it's WebSocket message sender back to us
    let client = thread::spawn(move || {
        println!("Connecting to {}", url);
        connect(url, |sender| Client {
            ws_out: sender,
            thread_out: tx.clone(),
        }).unwrap();
    });

    if let Ok(Event::Connect(sender)) = rx.recv() {
        // If we were able to connect, print the instructions
        instructions();

        // Main loop
        loop {
            // Get user input
            let mut input = String::new();
            io::stdin().read_line(&mut input).unwrap();

            if let Ok(Event::Disconnect) = rx.try_recv() {
                break;
            }

            if input.starts_with("/h") {
                // Show help
                instructions()
            } else if input.starts_with("/c") {
                // If the close arguments are good, close the connection
                let args: Vec<&str> = input.split(' ').collect();
                if args.len() == 1 {
                    // Simple close
                    println!("Closing normally, please wait...");
                    sender.close(CloseCode::Normal).unwrap();
                } else if args.len() == 2 {
                    // Close with a specific code
                    if let Ok(code) = args[1].trim().parse::<u16>() {
                        let code = CloseCode::from(code);
                        println!("Closing with code: {:?}, please wait...", code);
                        sender.close(code).unwrap();
                    } else {
                        display(&format!("Unable to parse {} as close code.", args[1]));
                        // Keep accepting input if the close arguments are invalid
                        continue;
                    }
                } else {
                    // Close with a code and a reason
                    if let Ok(code) = args[1].trim().parse::<u16>() {
                        let code = CloseCode::from(code);
                        let reason = args[2..].join(" ");
                        println!(
                            "Closing with code: {:?} and reason: {}, please wait...",
                            code,
                            reason.trim()
                        );
                        sender
                            .close_with_reason(code, reason.trim().to_string())
                            .unwrap();
                    } else {
                        display(&format!("Unable to parse {} as close code.", args[1]));
                        // Keep accepting input if the close arguments are invalid
                        continue;
                    }
                }
                break;
            } else {
                // Send the message
                display(&format!(">>> {}", input.trim()));
                sender.send(input.trim()).unwrap();
            }
        }
    }

    // Ensure the client has a chance to finish up
    client.join().unwrap();
}

fn display(string: &str) {
    let mut view = term::stdout().unwrap();
    view.carriage_return().unwrap();
    view.delete_line().unwrap();
    println!("{}", string);
    print!("?> ");
    io::stdout().flush().unwrap();
}

fn instructions() {
    println!("Type /close [code] [reason] to close the connection.");
    println!("Type /help to show these instructions.");
    println!("Other input will be sent as messages.\n");
    print!("?> ");
    io::stdout().flush().unwrap();
}

struct Client {
    ws_out: Sender,
    thread_out: TSender<Event>,
}

impl Handler for Client {
    fn on_open(&mut self, _: Handshake) -> Result<()> {
        self.thread_out
            .send(Event::Connect(self.ws_out.clone()))
            .map_err(|err| {
                Error::new(
                    ErrorKind::Internal,
                    format!("Unable to communicate between threads: {:?}.", err),
                )
            })
    }

    fn on_message(&mut self, msg: Message) -> Result<()> {
        display(&format!("<<< {}", msg));
        Ok(())
    }

    fn on_close(&mut self, code: CloseCode, reason: &str) {
        if reason.is_empty() {
            display(&format!(
                "<<< Closing<({:?})>\nHit any key to end session.",
                code
            ));
        } else {
            display(&format!(
                "<<< Closing<({:?}) {}>\nHit any key to end session.",
                code, reason
            ));
        }

        if let Err(err) = self.thread_out.send(Event::Disconnect) {
            display(&format!("{:?}", err))
        }
    }

    fn on_error(&mut self, err: Error) {
        display(&format!("<<< Error<{:?}>", err))
    }
}

enum Event {
    Connect(Sender),
    Disconnect,
}
