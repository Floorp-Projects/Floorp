// Copyright 2015 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use bincode::serde::DeserializeError;
use std::sync::mpsc;
use std::sync::{Arc, Mutex};
use std::collections::hash_map::HashMap;
use std::cell::{RefCell};
use std::io::{Error, ErrorKind};
use std::slice;
use std::fmt::{self, Debug, Formatter};
use std::cmp::{PartialEq};
use std::ops::Deref;
use std::mem;
use std::usize;

use uuid::Uuid;

#[derive(Clone)]
struct ServerRecord {
    sender: OsIpcSender,
    conn_sender: mpsc::Sender<bool>,
    conn_receiver: Arc<Mutex<mpsc::Receiver<bool>>>,
}

impl ServerRecord {
    fn new(sender: OsIpcSender) -> ServerRecord {
        let (tx, rx) = mpsc::channel::<bool>();
        ServerRecord {
            sender: sender,
            conn_sender: tx,
            conn_receiver: Arc::new(Mutex::new(rx)),
        }
    }

    fn accept(&self) {
        self.conn_receiver.lock().unwrap().recv().unwrap();
    }

    fn connect(&self) {
        self.conn_sender.send(true).unwrap();
    }
}

lazy_static! {
    static ref ONE_SHOT_SERVERS: Mutex<HashMap<String,ServerRecord>> = Mutex::new(HashMap::new());
}

struct MpscChannelMessage(Vec<u8>, Vec<OsIpcChannel>, Vec<OsIpcSharedMemory>);

pub fn channel() -> Result<(OsIpcSender, OsIpcReceiver),MpscError> {
    let (base_sender, base_receiver) = mpsc::channel::<MpscChannelMessage>();
    Ok((OsIpcSender::new(base_sender), OsIpcReceiver::new(base_receiver)))
}

#[derive(Debug)]
pub struct OsIpcReceiver {
    receiver: RefCell<Option<mpsc::Receiver<MpscChannelMessage>>>,
}

impl PartialEq for OsIpcReceiver {
    fn eq(&self, other: &OsIpcReceiver) -> bool {
        self.receiver.borrow().as_ref().map(|rx| rx as *const _) ==
            other.receiver.borrow().as_ref().map(|rx| rx as *const _)
    }
}

impl OsIpcReceiver {
    fn new(receiver: mpsc::Receiver<MpscChannelMessage>) -> OsIpcReceiver {
        OsIpcReceiver {
            receiver: RefCell::new(Some(receiver)),
        }
    }

    pub fn consume(&self) -> OsIpcReceiver {
        let receiver = self.receiver.borrow_mut().take();
        OsIpcReceiver::new(receiver.unwrap())
    }

    pub fn recv(&self) -> Result<(Vec<u8>, Vec<OsOpaqueIpcChannel>, Vec<OsIpcSharedMemory>),MpscError> {
        let r = self.receiver.borrow();
        match r.as_ref().unwrap().recv() {
            Ok(MpscChannelMessage(d,c,s)) => Ok((d,
                                                 c.into_iter().map(OsOpaqueIpcChannel::new).collect(),
                                                 s)),
            Err(_) => Err(MpscError::ChannelClosedError),
        }
    }

    pub fn try_recv(&self) -> Result<(Vec<u8>, Vec<OsOpaqueIpcChannel>, Vec<OsIpcSharedMemory>),MpscError> {
        let r = self.receiver.borrow();
        match r.as_ref().unwrap().try_recv() {
            Ok(MpscChannelMessage(d,c,s)) => Ok((d,
                                                 c.into_iter().map(OsOpaqueIpcChannel::new).collect(),
                                                 s)),
            Err(_) => Err(MpscError::ChannelClosedError),
        }
    }
}

#[derive(Clone, Debug)]
pub struct OsIpcSender {
    sender: RefCell<mpsc::Sender<MpscChannelMessage>>,
}

impl PartialEq for OsIpcSender {
    fn eq(&self, other: &OsIpcSender) -> bool {
        &*self.sender.borrow() as *const _ ==
            &*other.sender.borrow() as *const _
    }
}

impl OsIpcSender {
    fn new(sender: mpsc::Sender<MpscChannelMessage>) -> OsIpcSender {
        OsIpcSender {
            sender: RefCell::new(sender),
        }
    }

    pub fn connect(name: String) -> Result<OsIpcSender,MpscError> {
        let record = ONE_SHOT_SERVERS.lock().unwrap().get(&name).unwrap().clone();
        record.connect();
        Ok(record.sender)
    }

    pub fn get_max_fragment_size() -> usize {
        usize::MAX
    }

    pub fn send(&self,
                data: &[u8],
                ports: Vec<OsIpcChannel>,
                shared_memory_regions: Vec<OsIpcSharedMemory>)
                -> Result<(),MpscError>
    {
        match self.sender.borrow().send(MpscChannelMessage(data.to_vec(), ports, shared_memory_regions)) {
            Err(_) => Err(MpscError::ChannelClosedError),
            Ok(_) => Ok(()),
        }
    }
}

pub struct OsIpcReceiverSet {
    last_index: usize,
    receiver_ids: Vec<usize>,
    receivers: Vec<OsIpcReceiver>,
}

impl OsIpcReceiverSet {
    pub fn new() -> Result<OsIpcReceiverSet,MpscError> {
        Ok(OsIpcReceiverSet {
            last_index: 0,
            receiver_ids: vec![],
            receivers: vec![],
        })
    }

    pub fn add(&mut self, receiver: OsIpcReceiver) -> Result<i64,MpscError> {
        self.last_index += 1;
        self.receiver_ids.push(self.last_index);
        self.receivers.push(receiver.consume());
        Ok(self.last_index as i64)
    }

    pub fn select(&mut self) -> Result<Vec<OsIpcSelectionResult>,MpscError> {
        let mut receivers: Vec<Option<mpsc::Receiver<MpscChannelMessage>>> = Vec::with_capacity(self.receivers.len());
        let mut r_id: i64 = -1;
        let mut r_index: usize = 0;

        {
            let select = mpsc::Select::new();
            // we *must* allocate exact capacity for this, because the Handles *can't move*
            let mut handles: Vec<mpsc::Handle<MpscChannelMessage>> = Vec::with_capacity(self.receivers.len());

            for r in &self.receivers {
                let inner_r = mem::replace(&mut *r.receiver.borrow_mut(), None);
                receivers.push(inner_r);
            }
            
            for r in &receivers {
                unsafe {
                    handles.push(select.handle(r.as_ref().unwrap()));
                    handles.last_mut().unwrap().add();
                }
            }

            let id = select.wait();

            for (index,h) in handles.iter().enumerate() {
                if h.id() == id {
                    r_index = index;
                    r_id = self.receiver_ids[index] as i64;
                    break;
                }
            }
        }

        // put the receivers back
        for (index,r) in self.receivers.iter().enumerate() {
            mem::replace(&mut *r.receiver.borrow_mut(), mem::replace(&mut receivers[index], None));
        }

        if r_id == -1 {
            return Err(MpscError::UnknownError);
        }

        let receivers = &mut self.receivers;
        match receivers[r_index].recv() {
            Ok((data, channels, shmems)) =>
                Ok(vec![OsIpcSelectionResult::DataReceived(r_id, data, channels, shmems)]),
            Err(MpscError::ChannelClosedError) => {
                receivers.remove(r_index);
                self.receiver_ids.remove(r_index);
                Ok(vec![OsIpcSelectionResult::ChannelClosed(r_id)])
            },
            Err(err) => Err(err),
        }
    }
}

pub enum OsIpcSelectionResult {
    DataReceived(i64, Vec<u8>, Vec<OsOpaqueIpcChannel>, Vec<OsIpcSharedMemory>),
    ChannelClosed(i64),
}

impl OsIpcSelectionResult {
    pub fn unwrap(self) -> (i64, Vec<u8>, Vec<OsOpaqueIpcChannel>, Vec<OsIpcSharedMemory>) {
        match self {
            OsIpcSelectionResult::DataReceived(id, data, channels, shared_memory_regions) => {
                (id, data, channels, shared_memory_regions)
            }
            OsIpcSelectionResult::ChannelClosed(id) => {
                panic!("OsIpcSelectionResult::unwrap(): receiver ID {} was closed!", id)
            }
        }
    }
}

pub struct OsIpcOneShotServer {
    receiver: OsIpcReceiver,
    name: String,
}

impl OsIpcOneShotServer {
    pub fn new() -> Result<(OsIpcOneShotServer, String),MpscError> {
        let (sender, receiver) = try!(channel());

        let name = Uuid::new_v4().to_string();
        let record = ServerRecord::new(sender);
        ONE_SHOT_SERVERS.lock().unwrap().insert(name.clone(), record);
        Ok((OsIpcOneShotServer {
            receiver: receiver,
            name: name.clone(),
        },name.clone()))
    }

    pub fn accept(self) -> Result<(OsIpcReceiver,
                                    Vec<u8>,
                                    Vec<OsOpaqueIpcChannel>,
                                    Vec<OsIpcSharedMemory>),MpscError>
    {
        let record = ONE_SHOT_SERVERS.lock().unwrap().get(&self.name).unwrap().clone();
        record.accept();
        ONE_SHOT_SERVERS.lock().unwrap().remove(&self.name).unwrap();
        let (data, channels, shmems) = try!(self.receiver.recv());
        Ok((self.receiver, data, channels, shmems))
    }
}

#[derive(PartialEq, Debug)]
pub enum OsIpcChannel {
    Sender(OsIpcSender),
    Receiver(OsIpcReceiver),
}

#[derive(PartialEq, Debug)]
pub struct OsOpaqueIpcChannel {
    channel: RefCell<Option<OsIpcChannel>>,
}

impl OsOpaqueIpcChannel {
    fn new(channel: OsIpcChannel) -> OsOpaqueIpcChannel {
        OsOpaqueIpcChannel {
            channel: RefCell::new(Some(channel))
        }
    }

    pub fn to_receiver(&self) -> OsIpcReceiver {
        match self.channel.borrow_mut().take().unwrap() {
            OsIpcChannel::Sender(_) => panic!("Opaque channel is not a receiver!"),
            OsIpcChannel::Receiver(r) => r
        }
    }
    
    pub fn to_sender(&mut self) -> OsIpcSender {
        match self.channel.borrow_mut().take().unwrap() {
            OsIpcChannel::Sender(s) => s,
            OsIpcChannel::Receiver(_) => panic!("Opaque channel is not a sender!"),
        }
    }
}

pub struct OsIpcSharedMemory {
    ptr: *mut u8,
    length: usize,
    data: Arc<Vec<u8>>,
}

unsafe impl Send for OsIpcSharedMemory {}
unsafe impl Sync for OsIpcSharedMemory {}

impl Clone for OsIpcSharedMemory {
    fn clone(&self) -> OsIpcSharedMemory {
        OsIpcSharedMemory {
            ptr: self.ptr,
            length: self.length,
            data: self.data.clone(),
        }
    }
}

impl PartialEq for OsIpcSharedMemory {
    fn eq(&self, other: &OsIpcSharedMemory) -> bool {
        **self == **other
    }
}

impl Debug for OsIpcSharedMemory {
    fn fmt(&self, formatter: &mut Formatter) -> Result<(), fmt::Error> {
        (**self).fmt(formatter)
    }
}

impl Deref for OsIpcSharedMemory {
    type Target = [u8];

    #[inline]
    fn deref(&self) -> &[u8] {
        if self.ptr.is_null() {
            panic!("attempted to access a consumed `OsIpcSharedMemory`")
        }
        unsafe {
            slice::from_raw_parts(self.ptr, self.length)
        }
    }
}

impl OsIpcSharedMemory {
    pub fn from_byte(byte: u8, length: usize) -> OsIpcSharedMemory {
        let mut v = Arc::new(vec![byte; length]);
        OsIpcSharedMemory {
            ptr: Arc::get_mut(&mut v).unwrap().as_mut_ptr(),
            length: length,
            data: v
        }
    }

    pub fn from_bytes(bytes: &[u8]) -> OsIpcSharedMemory {
        let mut v = Arc::new(bytes.to_vec());
        OsIpcSharedMemory {
            ptr: Arc::get_mut(&mut v).unwrap().as_mut_ptr(),
            length: v.len(),
            data: v
        }
    }
}

#[derive(Debug, PartialEq)]
pub enum MpscError {
    ChannelClosedError,
    UnknownError,
}

impl MpscError {
    #[allow(dead_code)]
    pub fn channel_is_closed(&self) -> bool {
        *self == MpscError::ChannelClosedError
    }
}

impl From<MpscError> for DeserializeError {
    fn from(mpsc_error: MpscError) -> DeserializeError {
        DeserializeError::IoError(mpsc_error.into())
    }
}

impl From<MpscError> for Error {
    fn from(mpsc_error: MpscError) -> Error {
        match mpsc_error {
            MpscError::ChannelClosedError => {
                Error::new(ErrorKind::BrokenPipe, "MPSC channel closed")
            }
            MpscError::UnknownError => Error::new(ErrorKind::Other, "Other MPSC channel error"),
        }
    }
}

