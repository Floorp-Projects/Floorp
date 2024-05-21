// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! An [HTTP 0.9](https://www.w3.org/Protocols/HTTP/AsImplemented.html) client implementation.

use std::{
    cell::RefCell,
    collections::{HashMap, VecDeque},
    fs::File,
    io::{BufWriter, Write},
    net::SocketAddr,
    path::PathBuf,
    rc::Rc,
    time::Instant,
};

use neqo_common::{event::Provider, qdebug, qinfo, qwarn, Datagram};
use neqo_crypto::{AuthenticationStatus, ResumptionToken};
use neqo_transport::{
    CloseReason, Connection, ConnectionEvent, EmptyConnectionIdGenerator, Error, Output, State,
    StreamId, StreamType,
};
use url::Url;

use super::{get_output_file, qlog_new, Args, CloseState, Res};

pub struct Handler<'a> {
    streams: HashMap<StreamId, Option<BufWriter<File>>>,
    url_queue: VecDeque<Url>,
    all_paths: Vec<PathBuf>,
    args: &'a Args,
    token: Option<ResumptionToken>,
    needs_key_update: bool,
}

impl<'a> super::Handler for Handler<'a> {
    type Client = Connection;

    fn handle(&mut self, client: &mut Self::Client) -> Res<bool> {
        while let Some(event) = client.next_event() {
            if self.needs_key_update {
                match client.initiate_key_update() {
                    Ok(()) => {
                        qdebug!("Keys updated");
                        self.needs_key_update = false;
                        self.download_urls(client);
                    }
                    Err(neqo_transport::Error::KeyUpdateBlocked) => (),
                    Err(e) => return Err(e.into()),
                }
            }

            match event {
                ConnectionEvent::AuthenticationNeeded => {
                    client.authenticated(AuthenticationStatus::Ok, Instant::now());
                }
                ConnectionEvent::RecvStreamReadable { stream_id } => {
                    self.read(client, stream_id)?;
                }
                ConnectionEvent::SendStreamWritable { stream_id } => {
                    qdebug!("stream {stream_id} writable");
                }
                ConnectionEvent::SendStreamComplete { stream_id } => {
                    qdebug!("stream {stream_id} complete");
                }
                ConnectionEvent::SendStreamCreatable { stream_type } => {
                    qdebug!("stream {stream_type:?} creatable");
                    if stream_type == StreamType::BiDi {
                        self.download_urls(client);
                    }
                }
                ConnectionEvent::StateChange(
                    State::WaitInitial | State::Handshaking | State::Connected,
                ) => {
                    qdebug!("{event:?}");
                    self.download_urls(client);
                }
                ConnectionEvent::ResumptionToken(token) => {
                    self.token = Some(token);
                }
                _ => {
                    qwarn!("Unhandled event {event:?}");
                }
            }
        }

        if !self.streams.is_empty() || !self.url_queue.is_empty() {
            return Ok(false);
        }

        if self.args.resume && self.token.is_none() {
            let Some(token) = client.take_resumption_token(Instant::now()) else {
                return Ok(false);
            };
            self.token = Some(token);
        }

        Ok(true)
    }

    fn take_token(&mut self) -> Option<ResumptionToken> {
        self.token.take()
    }
}

pub(crate) fn create_client(
    args: &Args,
    local_addr: SocketAddr,
    remote_addr: SocketAddr,
    hostname: &str,
    resumption_token: Option<ResumptionToken>,
) -> Res<Connection> {
    let alpn = match args.shared.alpn.as_str() {
        "hq-29" | "hq-30" | "hq-31" | "hq-32" => args.shared.alpn.as_str(),
        _ => "hq-interop",
    };

    let mut client = Connection::new_client(
        hostname,
        &[alpn],
        Rc::new(RefCell::new(EmptyConnectionIdGenerator::default())),
        local_addr,
        remote_addr,
        args.shared.quic_parameters.get(alpn),
        Instant::now(),
    )?;

    if let Some(tok) = resumption_token {
        client.enable_resumption(Instant::now(), tok)?;
    }

    let ciphers = args.get_ciphers();
    if !ciphers.is_empty() {
        client.set_ciphers(&ciphers)?;
    }

    client.set_qlog(qlog_new(args, hostname, client.odcid().unwrap())?);

    Ok(client)
}

impl TryFrom<&State> for CloseState {
    type Error = CloseReason;

    fn try_from(value: &State) -> Result<Self, Self::Error> {
        let (state, error) = match value {
            State::Closing { error, .. } | State::Draining { error, .. } => {
                (CloseState::Closing, error)
            }
            State::Closed(error) => (CloseState::Closed, error),
            _ => return Ok(CloseState::NotClosing),
        };

        if error.is_error() {
            Err(error.clone())
        } else {
            Ok(state)
        }
    }
}

impl super::Client for Connection {
    fn process_output(&mut self, now: Instant) -> Output {
        self.process_output(now)
    }

    fn process_multiple_input<'a, I>(&mut self, dgrams: I, now: Instant)
    where
        I: IntoIterator<Item = &'a Datagram>,
    {
        self.process_multiple_input(dgrams, now);
    }

    fn close<S>(&mut self, now: Instant, app_error: neqo_transport::AppError, msg: S)
    where
        S: AsRef<str> + std::fmt::Display,
    {
        if !self.state().closed() {
            self.close(now, app_error, msg);
        }
    }

    fn is_closed(&self) -> Result<CloseState, CloseReason> {
        self.state().try_into()
    }

    fn stats(&self) -> neqo_transport::Stats {
        self.stats()
    }

    fn has_events(&self) -> bool {
        neqo_common::event::Provider::has_events(self)
    }
}

impl<'b> Handler<'b> {
    pub fn new(url_queue: VecDeque<Url>, args: &'b Args) -> Self {
        Self {
            streams: HashMap::new(),
            url_queue,
            all_paths: Vec::new(),
            args,
            token: None,
            needs_key_update: args.key_update,
        }
    }

    fn download_urls(&mut self, client: &mut Connection) {
        loop {
            if self.url_queue.is_empty() {
                break;
            }
            if self.streams.len() >= self.args.concurrency {
                break;
            }
            if !self.download_next(client) {
                break;
            }
        }
    }

    fn download_next(&mut self, client: &mut Connection) -> bool {
        if self.needs_key_update {
            qdebug!("Deferring requests until after first key update");
            return false;
        }
        let url = self
            .url_queue
            .pop_front()
            .expect("download_next called with empty queue");
        match client.stream_create(StreamType::BiDi) {
            Ok(client_stream_id) => {
                qinfo!("Created stream {client_stream_id} for {url}");
                let req = format!("GET {}\r\n", url.path());
                _ = client
                    .stream_send(client_stream_id, req.as_bytes())
                    .unwrap();
                client.stream_close_send(client_stream_id).unwrap();
                let out_file = get_output_file(&url, &self.args.output_dir, &mut self.all_paths);
                self.streams.insert(client_stream_id, out_file);
                true
            }
            Err(e @ (Error::StreamLimitError | Error::ConnectionState)) => {
                qwarn!("Cannot create stream {e:?}");
                self.url_queue.push_front(url);
                false
            }
            Err(e) => {
                panic!("Error creating stream {e:?}");
            }
        }
    }

    /// Read and maybe print received data from a stream.
    // Returns bool: was fin received?
    fn read_from_stream(
        client: &mut Connection,
        stream_id: StreamId,
        output_read_data: bool,
        maybe_out_file: &mut Option<BufWriter<File>>,
    ) -> Res<bool> {
        let mut data = vec![0; 4096];
        loop {
            let (sz, fin) = client.stream_recv(stream_id, &mut data)?;
            if sz == 0 {
                return Ok(fin);
            }

            if let Some(out_file) = maybe_out_file {
                out_file.write_all(&data[..sz])?;
            } else if !output_read_data {
                qdebug!("READ[{stream_id}]: {sz} bytes");
            } else {
                qdebug!(
                    "READ[{}]: {}",
                    stream_id,
                    String::from_utf8(data.clone()).unwrap()
                );
            }
            if fin {
                return Ok(true);
            }
        }
    }

    fn read(&mut self, client: &mut Connection, stream_id: StreamId) -> Res<()> {
        match self.streams.get_mut(&stream_id) {
            None => {
                qwarn!("Data on unexpected stream: {stream_id}");
                return Ok(());
            }
            Some(maybe_out_file) => {
                let fin_recvd = Self::read_from_stream(
                    client,
                    stream_id,
                    self.args.output_read_data,
                    maybe_out_file,
                )?;

                if fin_recvd {
                    if let Some(mut out_file) = maybe_out_file.take() {
                        out_file.flush()?;
                    } else {
                        qinfo!("<FIN[{stream_id}]>");
                    }
                    self.streams.remove(&stream_id);
                    self.download_urls(client);
                }
            }
        }
        Ok(())
    }
}
