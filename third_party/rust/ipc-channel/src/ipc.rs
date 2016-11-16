// Copyright 2015 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use platform::{self, OsIpcChannel, OsIpcReceiver, OsIpcReceiverSet, OsIpcSender};
use platform::{OsIpcOneShotServer, OsIpcSelectionResult, OsIpcSharedMemory, OsOpaqueIpcChannel};

use bincode::{self, SizeLimit};
use bincode::serde::DeserializeError;
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use refcell::{RefCell, BorrowState};
use std::cmp::min;
use std::fmt::{self, Debug, Formatter};
use std::io::{Error, ErrorKind};
use std::marker::PhantomData;
use std::mem;
use std::ops::Deref;

thread_local! {
    static OS_IPC_CHANNELS_FOR_DESERIALIZATION: RefCell<Vec<OsOpaqueIpcChannel>> =
        RefCell::new(Vec::new())
}
thread_local! {
    static OS_IPC_SHARED_MEMORY_REGIONS_FOR_DESERIALIZATION:
        RefCell<Vec<Option<OsIpcSharedMemory>>> = RefCell::new(Vec::new())
}
thread_local! {
    static OS_IPC_CHANNELS_FOR_SERIALIZATION: RefCell<Vec<OsIpcChannel>> = RefCell::new(Vec::new())
}
thread_local! {
    static OS_IPC_SHARED_MEMORY_REGIONS_FOR_SERIALIZATION: RefCell<Vec<OsIpcSharedMemory>> =
        RefCell::new(Vec::new())
}

pub fn channel<T>() -> Result<(IpcSender<T>, IpcReceiver<T>),Error>
                  where T: Deserialize + Serialize {
    let (os_sender, os_receiver) = try!(platform::channel());
    let ipc_receiver = IpcReceiver {
        os_receiver: os_receiver,
        phantom: PhantomData,
    };
    let ipc_sender = IpcSender {
        os_sender: os_sender,
        phantom: PhantomData,
    };
    Ok((ipc_sender, ipc_receiver))
}

pub fn bytes_channel() -> Result<(IpcBytesSender, IpcBytesReceiver),Error> {
    let (os_sender, os_receiver) = try!(platform::channel());
    let ipc_bytes_receiver = IpcBytesReceiver {
        os_receiver: os_receiver,
    };
    let ipc_bytes_sender = IpcBytesSender {
        os_sender: os_sender,
    };
    Ok((ipc_bytes_sender, ipc_bytes_receiver))
}

#[derive(Debug)]
pub struct IpcReceiver<T> where T: Deserialize + Serialize {
    os_receiver: OsIpcReceiver,
    phantom: PhantomData<T>,
}

impl<T> IpcReceiver<T> where T: Deserialize + Serialize {
    pub fn recv(&self) -> Result<T,DeserializeError> {
        let (data, os_ipc_channels, os_ipc_shared_memory_regions) = try!(self.os_receiver.recv());
        OpaqueIpcMessage::new(data, os_ipc_channels, os_ipc_shared_memory_regions).to()
    }

    pub fn try_recv(&self) -> Result<T,DeserializeError> {
        let (data, os_ipc_channels, os_ipc_shared_memory_regions) =
            try!(self.os_receiver.try_recv());
        OpaqueIpcMessage::new(data, os_ipc_channels, os_ipc_shared_memory_regions).to()
    }

    pub fn to_opaque(self) -> OpaqueIpcReceiver {
        OpaqueIpcReceiver {
            os_receiver: self.os_receiver,
        }
    }
}

impl<T> Deserialize for IpcReceiver<T> where T: Deserialize + Serialize {
    fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error> where D: Deserializer {
        let index: usize = try!(Deserialize::deserialize(deserializer));
        let os_receiver =
            OS_IPC_CHANNELS_FOR_DESERIALIZATION.with(|os_ipc_channels_for_deserialization| {
                // FIXME(pcwalton): This could panic if the data was corrupt and the index was out
                // of bounds. We should return an `Err` result instead.
                os_ipc_channels_for_deserialization.borrow_mut()[index].to_receiver()
            });
        Ok(IpcReceiver {
            os_receiver: os_receiver,
            phantom: PhantomData,
        })
    }
}

impl<T> Serialize for IpcReceiver<T> where T: Deserialize + Serialize {
    fn serialize<S>(&self, serializer: &mut S) -> Result<(),S::Error> where S: Serializer {
        let index = OS_IPC_CHANNELS_FOR_SERIALIZATION.with(|os_ipc_channels_for_serialization| {
            let mut os_ipc_channels_for_serialization =
                os_ipc_channels_for_serialization.borrow_mut();
            let index = os_ipc_channels_for_serialization.len();
            os_ipc_channels_for_serialization.push(OsIpcChannel::Receiver(self.os_receiver
                                                                              .consume()));
            index
        });
        index.serialize(serializer)
    }
}

#[derive(Debug)]
pub struct IpcSender<T> where T: Serialize {
    os_sender: OsIpcSender,
    phantom: PhantomData<T>,
}

impl<T> Clone for IpcSender<T> where T: Serialize {
    fn clone(&self) -> IpcSender<T> {
        IpcSender {
            os_sender: self.os_sender.clone(),
            phantom: PhantomData,
        }
    }
}

impl<T> IpcSender<T> where T: Serialize {
    pub fn connect(name: String) -> Result<IpcSender<T>,Error> {
        Ok(IpcSender {
            os_sender: try!(OsIpcSender::connect(name)),
            phantom: PhantomData,
        })
    }

    pub fn send(&self, data: T) -> Result<(),Error> {
        let mut bytes = Vec::with_capacity(4096);
        OS_IPC_CHANNELS_FOR_SERIALIZATION.with(|os_ipc_channels_for_serialization| {
            OS_IPC_SHARED_MEMORY_REGIONS_FOR_SERIALIZATION.with(
                    |os_ipc_shared_memory_regions_for_serialization| {
                if os_ipc_channels_for_serialization.borrow_state() != BorrowState::Unused {
                    return Err(recursive_io_error());
                }

                if os_ipc_shared_memory_regions_for_serialization.borrow_state() != BorrowState::Unused {
                    return Err(recursive_io_error());
                }

                let old_os_ipc_channels =
                    mem::replace(&mut *os_ipc_channels_for_serialization.borrow_mut(), Vec::new());
                let old_os_ipc_shared_memory_regions =
                    mem::replace(&mut *os_ipc_shared_memory_regions_for_serialization.borrow_mut(),
                                 Vec::new());
                let os_ipc_shared_memory_regions;
                let os_ipc_channels;
                {
                    let mut serializer = bincode::serde::Serializer::new(&mut bytes);
                    data.serialize(&mut serializer).unwrap();
                    os_ipc_channels =
                        mem::replace(&mut *os_ipc_channels_for_serialization.borrow_mut(),
                                     old_os_ipc_channels);
                    os_ipc_shared_memory_regions = mem::replace(
                        &mut *os_ipc_shared_memory_regions_for_serialization.borrow_mut(),
                        old_os_ipc_shared_memory_regions);
                };
                self.os_sender.send(&bytes[..],
                                    os_ipc_channels,
                                    os_ipc_shared_memory_regions).map_err(|e| Error::from(e))
            })
        })
    }

    pub fn to_opaque(self) -> OpaqueIpcSender {
        OpaqueIpcSender {
            os_sender: self.os_sender,
        }
    }
}

impl<T> Deserialize for IpcSender<T> where T: Serialize {
    fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error> where D: Deserializer {
        let os_sender = try!(deserialize_os_ipc_sender(deserializer));
        Ok(IpcSender {
            os_sender: os_sender,
            phantom: PhantomData,
        })
    }
}

impl<T> Serialize for IpcSender<T> where T: Serialize {
    fn serialize<S>(&self, serializer: &mut S) -> Result<(),S::Error> where S: Serializer {
        serialize_os_ipc_sender(&self.os_sender, serializer)
    }
}

pub struct IpcReceiverSet {
    os_receiver_set: OsIpcReceiverSet,
}

impl IpcReceiverSet {
    pub fn new() -> Result<IpcReceiverSet,Error> {
        Ok(IpcReceiverSet {
            os_receiver_set: try!(OsIpcReceiverSet::new()),
        })
    }

    pub fn add<T>(&mut self, receiver: IpcReceiver<T>) -> Result<i64,Error>
                  where T: Deserialize + Serialize {
        Ok(try!(self.os_receiver_set.add(receiver.os_receiver)))
    }

    pub fn add_opaque(&mut self, receiver: OpaqueIpcReceiver) -> Result<i64,Error> {
        Ok(try!(self.os_receiver_set.add(receiver.os_receiver)))
    }

    pub fn select(&mut self) -> Result<Vec<IpcSelectionResult>,Error> {
        let results = try!(self.os_receiver_set.select());
        Ok(results.into_iter().map(|result| {
            match result {
                OsIpcSelectionResult::DataReceived(os_receiver_id,
                                                   data,
                                                   os_ipc_channels,
                                                   os_ipc_shared_memory_regions) => {
                    IpcSelectionResult::MessageReceived(os_receiver_id, OpaqueIpcMessage {
                        data: data,
                        os_ipc_channels: os_ipc_channels,
                        os_ipc_shared_memory_regions:
                            os_ipc_shared_memory_regions.into_iter().map(
                                |os_ipc_shared_memory_region| {
                                    Some(os_ipc_shared_memory_region)
                                }).collect(),
                    })
                }
                OsIpcSelectionResult::ChannelClosed(os_receiver_id) => {
                    IpcSelectionResult::ChannelClosed(os_receiver_id)
                }
            }
        }).collect())
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct IpcSharedMemory {
    os_shared_memory: OsIpcSharedMemory,
}

impl Deref for IpcSharedMemory {
    type Target = [u8];

    #[inline]
    fn deref(&self) -> &[u8] {
        &*self.os_shared_memory
    }
}

impl Deserialize for IpcSharedMemory {
    fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error> where D: Deserializer {
        let index: usize = try!(Deserialize::deserialize(deserializer));
        let os_shared_memory = OS_IPC_SHARED_MEMORY_REGIONS_FOR_DESERIALIZATION.with(
            |os_ipc_shared_memory_regions_for_deserialization| {
                // FIXME(pcwalton): This could panic if the data was corrupt and the index was out
                // of bounds. We should return an `Err` result instead.
                mem::replace(
                    &mut os_ipc_shared_memory_regions_for_deserialization.borrow_mut()[index],
                    None).unwrap()
            });
        Ok(IpcSharedMemory {
            os_shared_memory: os_shared_memory,
        })
    }
}

impl Serialize for IpcSharedMemory {
    fn serialize<S>(&self, serializer: &mut S) -> Result<(),S::Error> where S: Serializer {
        let index = OS_IPC_SHARED_MEMORY_REGIONS_FOR_SERIALIZATION.with(
            |os_ipc_shared_memory_regions_for_serialization| {
                let mut os_ipc_shared_memory_regions_for_serialization =
                    os_ipc_shared_memory_regions_for_serialization.borrow_mut();
                let index = os_ipc_shared_memory_regions_for_serialization.len();
                os_ipc_shared_memory_regions_for_serialization.push(self.os_shared_memory
                                                                        .clone());
                index
            });
        index.serialize(serializer)
    }
}

impl IpcSharedMemory {
    pub fn from_bytes(bytes: &[u8]) -> IpcSharedMemory {
        IpcSharedMemory {
            os_shared_memory: OsIpcSharedMemory::from_bytes(bytes),
        }
    }

    pub fn from_byte(byte: u8, length: usize) -> IpcSharedMemory {
        IpcSharedMemory {
            os_shared_memory: OsIpcSharedMemory::from_byte(byte, length),
        }
    }
}

pub enum IpcSelectionResult {
    MessageReceived(i64, OpaqueIpcMessage),
    ChannelClosed(i64),
}

impl IpcSelectionResult {
    pub fn unwrap(self) -> (i64, OpaqueIpcMessage) {
        match self {
            IpcSelectionResult::MessageReceived(id, message) => (id, message),
            IpcSelectionResult::ChannelClosed(id) => {
                panic!("IpcSelectionResult::unwrap(): channel {} closed", id)
            }
        }
    }
}

pub struct OpaqueIpcMessage {
    data: Vec<u8>,
    os_ipc_channels: Vec<OsOpaqueIpcChannel>,
    os_ipc_shared_memory_regions: Vec<Option<OsIpcSharedMemory>>,
}

impl Debug for OpaqueIpcMessage {
    fn fmt(&self, formatter: &mut Formatter) -> Result<(), fmt::Error> {
        match String::from_utf8(self.data.clone()) {
            Ok(string) => string.chars().take(256).collect::<String>().fmt(formatter),
            Err(..) => self.data[0..min(self.data.len(), 256)].fmt(formatter),
        }
    }
}

impl OpaqueIpcMessage {
    fn new(data: Vec<u8>,
           os_ipc_channels: Vec<OsOpaqueIpcChannel>,
           os_ipc_shared_memory_regions: Vec<OsIpcSharedMemory>)
           -> OpaqueIpcMessage {
        OpaqueIpcMessage {
            data: data,
            os_ipc_channels: os_ipc_channels,
            os_ipc_shared_memory_regions:
                os_ipc_shared_memory_regions.into_iter()
                                            .map(|os_ipc_shared_memory_region| {
                    Some(os_ipc_shared_memory_region)
                }).collect(),
        }
    }

    pub fn to<T>(mut self) -> Result<T,DeserializeError> where T: Deserialize + Serialize {
        OS_IPC_CHANNELS_FOR_DESERIALIZATION.with(|os_ipc_channels_for_deserialization| {
            OS_IPC_SHARED_MEMORY_REGIONS_FOR_DESERIALIZATION.with(
                    |os_ipc_shared_memory_regions_for_deserialization| {
               if os_ipc_channels_for_deserialization.borrow_state() != BorrowState::Unused {
                    return Err(DeserializeError::IoError(recursive_io_error()));
                }

                if os_ipc_shared_memory_regions_for_deserialization.borrow_state() != BorrowState::Unused {
                    return Err(DeserializeError::IoError(recursive_io_error()));
                }

                mem::swap(&mut *os_ipc_channels_for_deserialization.borrow_mut(),
                          &mut self.os_ipc_channels);
                mem::swap(&mut *os_ipc_shared_memory_regions_for_deserialization.borrow_mut(),
                          &mut self.os_ipc_shared_memory_regions);
                let mut data = &*self.data;
                let mut deserializer = bincode::serde::Deserializer::new(&mut data,
                                                                         SizeLimit::Infinite);
                let result = try!(Deserialize::deserialize(&mut deserializer));
                mem::swap(&mut *os_ipc_shared_memory_regions_for_deserialization.borrow_mut(),
                          &mut self.os_ipc_shared_memory_regions);
                mem::swap(&mut *os_ipc_channels_for_deserialization.borrow_mut(),
                          &mut self.os_ipc_channels);
                Ok(result)
            })
        })
    }
}

#[derive(Clone, Debug)]
pub struct OpaqueIpcSender {
    os_sender: OsIpcSender,
}

impl OpaqueIpcSender {
    pub fn to<T>(self) -> IpcSender<T> where T: Deserialize + Serialize {
        IpcSender {
            os_sender: self.os_sender,
            phantom: PhantomData,
        }
    }
}

impl Deserialize for OpaqueIpcSender {
    fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error> where D: Deserializer {
        let os_sender = try!(deserialize_os_ipc_sender(deserializer));
        Ok(OpaqueIpcSender {
            os_sender: os_sender,
        })
    }
}

impl Serialize for OpaqueIpcSender {
    fn serialize<S>(&self, serializer: &mut S) -> Result<(),S::Error> where S: Serializer {
        serialize_os_ipc_sender(&self.os_sender, serializer)
    }
}

#[derive(Debug)]
pub struct OpaqueIpcReceiver {
    os_receiver: OsIpcReceiver,
}

pub struct IpcOneShotServer<T> {
    os_server: OsIpcOneShotServer,
    phantom: PhantomData<T>,
}

impl<T> IpcOneShotServer<T> where T: Deserialize + Serialize {
    pub fn new() -> Result<(IpcOneShotServer<T>, String),Error> {
        let (os_server, name) = try!(OsIpcOneShotServer::new());
        Ok((IpcOneShotServer {
            os_server: os_server,
            phantom: PhantomData,
        }, name))
    }

    pub fn accept(self) -> Result<(IpcReceiver<T>,T),DeserializeError> {
        let (os_receiver, data, os_channels, os_shared_memory_regions) =
            try!(self.os_server.accept());
        let value = try!(OpaqueIpcMessage {
            data: data,
            os_ipc_channels: os_channels,
            os_ipc_shared_memory_regions: os_shared_memory_regions.into_iter()
                                                                  .map(|os_shared_memory_region| {
                Some(os_shared_memory_region)
            }).collect(),
        }.to());
        Ok((IpcReceiver {
            os_receiver: os_receiver,
            phantom: PhantomData,
        }, value))
    }
}

#[derive(Debug)]
pub struct IpcBytesReceiver {
    os_receiver: OsIpcReceiver,
}

impl IpcBytesReceiver {
    #[inline]
    pub fn recv(&self) -> Result<Vec<u8>,DeserializeError> {
        match self.os_receiver.recv() {
            Ok((data, _, _)) => Ok(data),
            Err(err) => Err(err.into()),
        }
    }
}

impl Deserialize for IpcBytesReceiver {
    fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error> where D: Deserializer {
        let index: usize = try!(Deserialize::deserialize(deserializer));
        let os_receiver =
            OS_IPC_CHANNELS_FOR_DESERIALIZATION.with(|os_ipc_channels_for_deserialization| {
                // FIXME(pcwalton): This could panic if the data was corrupt and the index was out
                // of bounds. We should return an `Err` result instead.
                os_ipc_channels_for_deserialization.borrow_mut()[index].to_receiver()
            });
        Ok(IpcBytesReceiver {
            os_receiver: os_receiver,
        })
    }
}

impl Serialize for IpcBytesReceiver {
    fn serialize<S>(&self, serializer: &mut S) -> Result<(),S::Error> where S: Serializer {
        let index = OS_IPC_CHANNELS_FOR_SERIALIZATION.with(|os_ipc_channels_for_serialization| {
            let mut os_ipc_channels_for_serialization =
                os_ipc_channels_for_serialization.borrow_mut();
            let index = os_ipc_channels_for_serialization.len();
            os_ipc_channels_for_serialization.push(OsIpcChannel::Receiver(self.os_receiver
                                                                              .consume()));
            index
        });
        index.serialize(serializer)
    }
}

#[derive(Debug)]
pub struct IpcBytesSender {
    os_sender: OsIpcSender,
}

impl Clone for IpcBytesSender {
    fn clone(&self) -> IpcBytesSender {
        IpcBytesSender {
            os_sender: self.os_sender.clone(),
        }
    }
}

impl Deserialize for IpcBytesSender {
    fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error> where D: Deserializer {
        let os_sender = try!(deserialize_os_ipc_sender(deserializer));
        Ok(IpcBytesSender {
            os_sender: os_sender,
        })
    }
}

impl Serialize for IpcBytesSender {
    fn serialize<S>(&self, serializer: &mut S) -> Result<(),S::Error> where S: Serializer {
        serialize_os_ipc_sender(&self.os_sender, serializer)
    }
}

impl IpcBytesSender {
    #[inline]
    pub fn send(&self, data: &[u8]) -> Result<(),Error> {
        self.os_sender.send(data, vec![], vec![]).map_err(|e| Error::from(e))
    }
}

fn serialize_os_ipc_sender<S>(os_ipc_sender: &OsIpcSender, serializer: &mut S)
                              -> Result<(),S::Error> where S: Serializer {
    let index = OS_IPC_CHANNELS_FOR_SERIALIZATION.with(|os_ipc_channels_for_serialization| {
        let mut os_ipc_channels_for_serialization =
            os_ipc_channels_for_serialization.borrow_mut();
        let index = os_ipc_channels_for_serialization.len();
        os_ipc_channels_for_serialization.push(OsIpcChannel::Sender(os_ipc_sender.clone()));
        index
    });
    index.serialize(serializer)
}

fn deserialize_os_ipc_sender<D>(deserializer: &mut D)
                                -> Result<OsIpcSender, D::Error> where D: Deserializer {
    let index: usize = try!(Deserialize::deserialize(deserializer));
    OS_IPC_CHANNELS_FOR_DESERIALIZATION.with(|os_ipc_channels_for_deserialization| {
        // FIXME(pcwalton): This could panic if the data was corrupt and the index was out of
        // bounds. We should return an `Err` result instead.
        Ok(os_ipc_channels_for_deserialization.borrow_mut()[index].to_sender())
    })
}

fn recursive_io_error() -> Error {
    Error::new(ErrorKind::Other, "recursive IPC channel use during serialization")
}

