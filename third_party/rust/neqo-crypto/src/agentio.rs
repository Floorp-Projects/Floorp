// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::constants::*;
use crate::err::{nspr, Error, PR_SetError, Res};
use crate::prio;
use crate::ssl;

use neqo_common::{hex, qtrace};
use std::cmp::min;
use std::convert::{TryFrom, TryInto};
use std::fmt;
use std::mem;
use std::ops::Deref;
use std::os::raw::{c_uint, c_void};
use std::pin::Pin;
use std::ptr::{null, null_mut};
use std::vec::Vec;

// Alias common types.
type PrFd = *mut prio::PRFileDesc;
type PrStatus = prio::PRStatus::Type;
const PR_SUCCESS: PrStatus = prio::PRStatus::PR_SUCCESS;
const PR_FAILURE: PrStatus = prio::PRStatus::PR_FAILURE;

/// Convert a pinned, boxed object into a void pointer.
pub fn as_c_void<T: Unpin>(pin: &mut Pin<Box<T>>) -> *mut c_void {
    Pin::into_inner(pin.as_mut()) as *mut T as *mut c_void
}

// This holds the length of the slice, not the slice itself.
#[derive(Default, Debug)]
struct RecordLength {
    epoch: Epoch,
    ct: ssl::SSLContentType::Type,
    len: usize,
}

/// A slice of the output.
#[derive(Default)]
pub struct Record {
    pub epoch: Epoch,
    pub ct: ssl::SSLContentType::Type,
    pub data: Vec<u8>,
}

impl Record {
    #[must_use]
    pub fn new(epoch: Epoch, ct: ssl::SSLContentType::Type, data: &[u8]) -> Self {
        Self {
            epoch,
            ct,
            data: data.to_vec(),
        }
    }

    // Shoves this record into the socket, returns true if blocked.
    pub(crate) fn write(self, fd: *mut ssl::PRFileDesc) -> Res<()> {
        qtrace!("write {:?}", self);
        unsafe {
            ssl::SSL_RecordLayerData(
                fd,
                self.epoch,
                self.ct,
                self.data.as_ptr(),
                c_uint::try_from(self.data.len())?,
            )
        }
    }
}

impl fmt::Debug for Record {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "Record {:?}:{:?} {}",
            self.epoch,
            self.ct,
            hex(&self.data[..])
        )
    }
}

#[derive(Debug, Default)]
pub struct RecordList {
    records: Vec<Record>,
}

impl RecordList {
    fn append(&mut self, epoch: Epoch, ct: ssl::SSLContentType::Type, data: &[u8]) {
        self.records.push(Record::new(epoch, ct, data));
    }

    /// Filter out `EndOfEarlyData` messages.
    pub fn remove_eoed(&mut self) {
        self.records.retain(|rec| rec.epoch != 1);
    }

    #[allow(clippy::unused_self)]
    unsafe extern "C" fn ingest(
        _fd: *mut ssl::PRFileDesc,
        epoch: ssl::PRUint16,
        ct: ssl::SSLContentType::Type,
        data: *const ssl::PRUint8,
        len: c_uint,
        arg: *mut c_void,
    ) -> ssl::SECStatus {
        let a = arg as *mut Self;
        let records = a.as_mut().unwrap();

        let slice = std::slice::from_raw_parts(data, len as usize);
        records.append(epoch, ct, slice);
        ssl::SECSuccess
    }

    /// Create a new record list.
    pub(crate) fn setup(fd: *mut ssl::PRFileDesc) -> Res<Pin<Box<Self>>> {
        let mut records = Box::pin(Self::default());
        unsafe {
            ssl::SSL_RecordLayerWriteCallback(fd, Some(Self::ingest), as_c_void(&mut records))
        }?;
        Ok(records)
    }
}

impl Deref for RecordList {
    type Target = Vec<Record>;
    #[must_use]
    fn deref(&self) -> &Vec<Record> {
        &self.records
    }
}

pub struct RecordListIter(std::vec::IntoIter<Record>);

impl Iterator for RecordListIter {
    type Item = Record;
    fn next(&mut self) -> Option<Self::Item> {
        self.0.next()
    }
}

impl IntoIterator for RecordList {
    type Item = Record;
    type IntoIter = RecordListIter;
    #[must_use]
    fn into_iter(self) -> Self::IntoIter {
        RecordListIter(self.records.into_iter())
    }
}

pub struct AgentIoInputContext<'a> {
    input: &'a mut AgentIoInput,
}

impl<'a> Drop for AgentIoInputContext<'a> {
    fn drop(&mut self) {
        self.input.reset();
    }
}

#[derive(Debug)]
struct AgentIoInput {
    // input is data that is read by TLS.
    input: *const u8,
    // input_available is how much data is left for reading.
    available: usize,
}

impl AgentIoInput {
    fn wrap<'a: 'c, 'b: 'c, 'c>(&'a mut self, input: &'b [u8]) -> AgentIoInputContext<'c> {
        assert!(self.input.is_null());
        self.input = input.as_ptr();
        self.available = input.len();
        qtrace!("AgentIoInput wrap {:p}", self.input);
        AgentIoInputContext { input: self }
    }

    // Take the data provided as input and provide it to the TLS stack.
    fn read_input(&mut self, buf: *mut u8, count: usize) -> Res<usize> {
        let amount = min(self.available, count);
        if amount == 0 {
            unsafe { PR_SetError(nspr::PR_WOULD_BLOCK_ERROR, 0) };
            return Err(Error::NoDataAvailable);
        }

        let src = unsafe { std::slice::from_raw_parts(self.input, amount) };
        qtrace!([self], "read {}", hex(src));
        let dst = unsafe { std::slice::from_raw_parts_mut(buf, amount) };
        dst.copy_from_slice(&src);
        self.input = self.input.wrapping_add(amount);
        self.available -= amount;
        Ok(amount)
    }

    fn reset(&mut self) {
        qtrace!([self], "reset");
        self.input = null();
        self.available = 0;
    }
}

impl ::std::fmt::Display for AgentIoInput {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "AgentIoInput {:p}", self.input)
    }
}

#[derive(Debug)]
pub struct AgentIo {
    // input collects the input we might provide to TLS.
    input: AgentIoInput,

    // output contains data that is written by TLS.
    output: Vec<u8>,
}

impl AgentIo {
    pub fn new() -> Self {
        Self {
            input: AgentIoInput {
                input: null(),
                available: 0,
            },
            output: Vec::new(),
        }
    }

    unsafe fn borrow(fd: &mut PrFd) -> &mut Self {
        #[allow(clippy::cast_ptr_alignment)]
        let io = (**fd).secret as *mut Self;
        io.as_mut().unwrap()
    }

    pub fn wrap<'a: 'c, 'b: 'c, 'c>(&'a mut self, input: &'b [u8]) -> AgentIoInputContext<'c> {
        assert_eq!(self.output.len(), 0);
        self.input.wrap(input)
    }

    // Stage output from TLS into the output buffer.
    fn save_output(&mut self, buf: *const u8, count: usize) {
        let slice = unsafe { std::slice::from_raw_parts(buf, count) };
        qtrace!([self], "save output {}", hex(slice));
        self.output.extend_from_slice(slice);
    }

    pub fn take_output(&mut self) -> Vec<u8> {
        qtrace!([self], "take output");
        mem::replace(&mut self.output, Vec::new())
    }
}

impl ::std::fmt::Display for AgentIo {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "AgentIo")
    }
}

unsafe extern "C" fn agent_close(fd: PrFd) -> PrStatus {
    (*fd).secret = null_mut();
    if let Some(dtor) = (*fd).dtor {
        dtor(fd);
    }
    PR_SUCCESS
}

unsafe extern "C" fn agent_read(mut fd: PrFd, buf: *mut c_void, amount: prio::PRInt32) -> PrStatus {
    let io = AgentIo::borrow(&mut fd);
    if let Ok(a) = usize::try_from(amount) {
        match io.input.read_input(buf as *mut u8, a) {
            Ok(_) => PR_SUCCESS,
            Err(_) => PR_FAILURE,
        }
    } else {
        PR_FAILURE
    }
}

unsafe extern "C" fn agent_recv(
    mut fd: PrFd,
    buf: *mut c_void,
    amount: prio::PRInt32,
    flags: prio::PRIntn,
    _timeout: prio::PRIntervalTime,
) -> prio::PRInt32 {
    let io = AgentIo::borrow(&mut fd);
    if flags != 0 {
        return PR_FAILURE;
    }
    if let Ok(a) = usize::try_from(amount) {
        match io.input.read_input(buf as *mut u8, a) {
            Ok(v) => prio::PRInt32::try_from(v).unwrap_or(PR_FAILURE),
            Err(_) => PR_FAILURE,
        }
    } else {
        PR_FAILURE
    }
}

unsafe extern "C" fn agent_write(
    mut fd: PrFd,
    buf: *const c_void,
    amount: prio::PRInt32,
) -> PrStatus {
    let io = AgentIo::borrow(&mut fd);
    if let Ok(a) = usize::try_from(amount) {
        io.save_output(buf as *const u8, a);
        amount
    } else {
        PR_FAILURE
    }
}

unsafe extern "C" fn agent_send(
    mut fd: PrFd,
    buf: *const c_void,
    amount: prio::PRInt32,
    flags: prio::PRIntn,
    _timeout: prio::PRIntervalTime,
) -> prio::PRInt32 {
    let io = AgentIo::borrow(&mut fd);

    if flags != 0 {
        return PR_FAILURE;
    }
    if let Ok(a) = usize::try_from(amount) {
        io.save_output(buf as *const u8, a);
        amount
    } else {
        PR_FAILURE
    }
}

unsafe extern "C" fn agent_available(mut fd: PrFd) -> prio::PRInt32 {
    let io = AgentIo::borrow(&mut fd);
    io.input.available.try_into().unwrap_or(PR_FAILURE)
}

unsafe extern "C" fn agent_available64(mut fd: PrFd) -> prio::PRInt64 {
    let io = AgentIo::borrow(&mut fd);
    io.input
        .available
        .try_into()
        .unwrap_or_else(|_| PR_FAILURE.into())
}

#[allow(clippy::cast_possible_truncation)]
unsafe extern "C" fn agent_getname(_fd: PrFd, addr: *mut prio::PRNetAddr) -> PrStatus {
    let a = addr.as_mut().unwrap();
    // Cast is safe because prio::PR_AF_INET is 2
    a.inet.family = prio::PR_AF_INET as prio::PRUint16;
    a.inet.port = 0;
    a.inet.ip = 0;
    PR_SUCCESS
}

unsafe extern "C" fn agent_getsockopt(_fd: PrFd, opt: *mut prio::PRSocketOptionData) -> PrStatus {
    let o = opt.as_mut().unwrap();
    if o.option == prio::PRSockOption::PR_SockOpt_Nonblocking {
        o.value.non_blocking = 1;
        return PR_SUCCESS;
    }
    PR_FAILURE
}

pub const METHODS: &prio::PRIOMethods = &prio::PRIOMethods {
    file_type: prio::PRDescType::PR_DESC_LAYERED,
    close: Some(agent_close),
    read: Some(agent_read),
    write: Some(agent_write),
    available: Some(agent_available),
    available64: Some(agent_available64),
    fsync: None,
    seek: None,
    seek64: None,
    fileInfo: None,
    fileInfo64: None,
    writev: None,
    connect: None,
    accept: None,
    bind: None,
    listen: None,
    shutdown: None,
    recv: Some(agent_recv),
    send: Some(agent_send),
    recvfrom: None,
    sendto: None,
    poll: None,
    acceptread: None,
    transmitfile: None,
    getsockname: Some(agent_getname),
    getpeername: Some(agent_getname),
    reserved_fn_6: None,
    reserved_fn_5: None,
    getsocketoption: Some(agent_getsockopt),
    setsocketoption: None,
    sendfile: None,
    connectcontinue: None,
    reserved_fn_3: None,
    reserved_fn_2: None,
    reserved_fn_1: None,
    reserved_fn_0: None,
};
