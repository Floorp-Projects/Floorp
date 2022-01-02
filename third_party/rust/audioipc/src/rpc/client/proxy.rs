// This is a derived version of client_proxy.rs from
// tokio_proto crate used under MIT license.
//
// Original version of client_proxy.rs:
// https://github.com/tokio-rs/tokio-proto/commit/8fb8e482dcd55cf02ceee165f8e08eee799c96d3
//
// The following modifications were made:
// * Remove `Service` trait since audioipc doesn't use `tokio_service`
//   crate.
// * Remove `RefCell` from `ClientProxy` since cubeb is called from
//   multiple threads. `mpsc::UnboundedSender` is thread safe.
// * Simplify the interface to just request (`R`) and response (`Q`)
//   removing error (`E`).
// * Remove the `Envelope` type.
// * Renamed `pair` to `channel` to represent that an `rpc::channel`
//   is being created.
//
// Original License:
//
// Copyright (c) 2016 Tokio contributors
//
// Permission is hereby granted, free of charge, to any
// person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the
// Software without restriction, including without
// limitation the rights to use, copy, modify, merge,
// publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice
// shall be included in all copies or substantial portions
// of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
// ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
// SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
// IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

use futures::sync::{mpsc, oneshot};
use futures::{Async, Future, Poll};
use std::fmt;
use std::io;

/// Message used to dispatch requests to the task managing the
/// client connection.
pub type Request<R, Q> = (R, oneshot::Sender<Q>);

/// Receive requests submitted to the client
pub type Receiver<R, Q> = mpsc::UnboundedReceiver<Request<R, Q>>;

/// Response future returned from a client
pub struct Response<Q> {
    inner: oneshot::Receiver<Q>,
}

pub struct ClientProxy<R, Q> {
    tx: mpsc::UnboundedSender<Request<R, Q>>,
}

impl<R, Q> Clone for ClientProxy<R, Q> {
    fn clone(&self) -> Self {
        ClientProxy {
            tx: self.tx.clone(),
        }
    }
}

pub fn channel<R, Q>() -> (ClientProxy<R, Q>, Receiver<R, Q>) {
    // Create a channel to send the requests to client-side of rpc.
    let (tx, rx) = mpsc::unbounded();

    // Wrap the `tx` part in ClientProxy so the rpc call interface
    // can be implemented.
    let client = ClientProxy { tx };

    (client, rx)
}

impl<R, Q> ClientProxy<R, Q> {
    pub fn call(&self, request: R) -> Response<Q> {
        // The response to returned from the rpc client task over a
        // oneshot channel.
        let (tx, rx) = oneshot::channel();

        // If send returns an Err, its because the other side has been dropped.
        // By ignoring it, we are just dropping the `tx`, which will mean the
        // rx will return Canceled when polled. In turn, that is translated
        // into a BrokenPipe, which conveys the proper error.
        let _ = self.tx.unbounded_send((request, tx));

        Response { inner: rx }
    }
}

impl<R, Q> fmt::Debug for ClientProxy<R, Q>
where
    R: fmt::Debug,
    Q: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "ClientProxy {{ ... }}")
    }
}

impl<Q> Future for Response<Q> {
    type Item = Q;
    type Error = io::Error;

    fn poll(&mut self) -> Poll<Q, io::Error> {
        match self.inner.poll() {
            Ok(Async::Ready(res)) => Ok(Async::Ready(res)),
            Ok(Async::NotReady) => Ok(Async::NotReady),
            // Convert oneshot::Canceled into io::Error
            Err(_) => {
                let e = io::Error::new(io::ErrorKind::BrokenPipe, "broken pipe");
                Err(e)
            }
        }
    }
}

impl<Q> fmt::Debug for Response<Q>
where
    Q: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Response {{ ... }}")
    }
}
