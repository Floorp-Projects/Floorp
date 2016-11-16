// Copyright 2015 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::collections::HashMap;
use std::sync::Mutex;
use std::sync::mpsc::{self, Receiver, Sender};
use std::thread;

use ipc::{self, IpcReceiver, IpcReceiverSet, IpcSelectionResult, IpcSender, OpaqueIpcMessage};
use ipc::{OpaqueIpcReceiver};
use serde::{Deserialize, Serialize};

lazy_static! {
    pub static ref ROUTER: RouterProxy = RouterProxy::new();
}

pub struct RouterProxy {
    comm: Mutex<RouterProxyComm>,
}

impl RouterProxy {
    pub fn new() -> RouterProxy {
        let (msg_sender, msg_receiver) = mpsc::channel();
        let (wakeup_sender, wakeup_receiver) = ipc::channel().unwrap();
        thread::spawn(move || Router::new(msg_receiver, wakeup_receiver).run());
        RouterProxy {
            comm: Mutex::new(RouterProxyComm {
                msg_sender: msg_sender,
                wakeup_sender: wakeup_sender,
            }),
        }
    }

    pub fn add_route(&self, receiver: OpaqueIpcReceiver, callback: RouterHandler) {
        let comm = self.comm.lock().unwrap();
        comm.msg_sender.send(RouterMsg::AddRoute(receiver, callback)).unwrap();
        comm.wakeup_sender.send(()).unwrap();
    }

    /// A convenience function to route an `IpcReceiver<T>` to an existing `Sender<T>`.
    pub fn route_ipc_receiver_to_mpsc_sender<T>(&self,
                                                ipc_receiver: IpcReceiver<T>,
                                                mpsc_sender: Sender<T>)
                                                where T: Deserialize +
                                                         Serialize +
                                                         Send +
                                                         'static {
        self.add_route(ipc_receiver.to_opaque(), Box::new(move |message| {
            drop(mpsc_sender.send(message.to::<T>().unwrap()))
        }))
    }

    /// A convenience function to route an `IpcReceiver<T>` to a `Receiver<T>`: the most common
    /// use of a `Router`.
    pub fn route_ipc_receiver_to_new_mpsc_receiver<T>(&self, ipc_receiver: IpcReceiver<T>)
                                                  -> Receiver<T>
                                                  where T: Deserialize +
                                                           Serialize +
                                                           Send +
                                                           'static {
        let (mpsc_sender, mpsc_receiver) = mpsc::channel();
        self.route_ipc_receiver_to_mpsc_sender(ipc_receiver, mpsc_sender);
        mpsc_receiver
    }
}

struct RouterProxyComm {
    msg_sender: Sender<RouterMsg>,
    wakeup_sender: IpcSender<()>,
}

struct Router {
    msg_receiver: Receiver<RouterMsg>,
    msg_wakeup_id: i64,
    ipc_receiver_set: IpcReceiverSet,
    handlers: HashMap<i64,RouterHandler>,
}

impl Router {
    fn new(msg_receiver: Receiver<RouterMsg>, wakeup_receiver: IpcReceiver<()>) -> Router {
        let mut ipc_receiver_set = IpcReceiverSet::new().unwrap();
        let msg_wakeup_id = ipc_receiver_set.add(wakeup_receiver).unwrap();
        Router {
            msg_receiver: msg_receiver,
            msg_wakeup_id: msg_wakeup_id,
            ipc_receiver_set: ipc_receiver_set,
            handlers: HashMap::new(),
        }
    }

    fn run(&mut self) {
        loop {
            let results = match self.ipc_receiver_set.select() {
                Ok(results) => results,
                Err(_) => break,
            };
            for result in results.into_iter() {
                match result {
                    IpcSelectionResult::MessageReceived(id, _) if id == self.msg_wakeup_id => {
                        match self.msg_receiver.recv().unwrap() {
                            RouterMsg::AddRoute(receiver, handler) => {
                                let new_receiver_id = self.ipc_receiver_set
                                                          .add_opaque(receiver)
                                                          .unwrap();
                                self.handlers.insert(new_receiver_id, handler);
                            }
                        }
                    }
                    IpcSelectionResult::MessageReceived(id, message) => {
                        self.handlers.get_mut(&id).unwrap()(message)
                    }
                    IpcSelectionResult::ChannelClosed(id) => {
                        self.handlers.remove(&id).unwrap();
                    }
                }
            }
        }
    }
}

enum RouterMsg {
    AddRoute(OpaqueIpcReceiver, RouterHandler),
}

pub type RouterHandler = Box<FnMut(OpaqueIpcMessage) + Send>;

