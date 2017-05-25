// Copyright 2015, Igor Shaula
// Licensed under the MIT License <LICENSE or
// http://opensource.org/licenses/MIT>. This file
// may not be copied, modified, or distributed
// except according to those terms.
use std::ptr;
use std::io;
use super::winapi;
use super::kernel32;
use super::ktmw32;

#[derive(Debug)]
pub struct Transaction {
    pub handle: winapi::HANDLE,
}

impl Transaction {
    //TODO: add arguments
    pub fn new() -> io::Result<Transaction> {
        unsafe {
            let handle = ktmw32::CreateTransaction(
                ptr::null_mut(),
                ptr::null_mut(),
                0,
                0,
                0,
                0,
                ptr::null_mut(),
            );
            if handle == winapi::INVALID_HANDLE_VALUE {
                return Err(io::Error::last_os_error())
            };
            Ok(Transaction{ handle: handle })
        }
    }

    pub fn commit(&self) -> io::Result<()> {
        unsafe {
            match ktmw32::CommitTransaction(self.handle) {
                0 => Err(io::Error::last_os_error()),
                _ => Ok(())
            }
        }
    }

    pub fn rollback(&self) -> io::Result<()> {
        unsafe {
            match ktmw32::RollbackTransaction(self.handle) {
                0 => Err(io::Error::last_os_error()),
                _ => Ok(())
            }
        }
    }

    fn close_(&mut self) -> io::Result<()> {
        unsafe {
            match kernel32::CloseHandle(self.handle) {
                0 => Err(io::Error::last_os_error()),
                _ => Ok(())
            }
        }
    }
}

impl Drop for Transaction {
    fn drop(&mut self) {
        self.close_().unwrap_or(());
    }
}
