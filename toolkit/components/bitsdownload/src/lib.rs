/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This crate is meant to be used in Windows only. It provides the
//! bits_interface module, which implements the nsIBits an nsIBitsRequest
//! XPCOM interfaces. These interfaces allow usage of the Windows component:
//! BITS (Background Intelligent Transfer Service). Further documentation can
//! be found in the XPCOM interface definition, located in nsIBits.idl

#![cfg(target_os = "windows")]

extern crate bits_client;
extern crate comedy;
extern crate crossbeam_utils;
extern crate failure;
extern crate libc;
extern crate log;
extern crate moz_task;
extern crate nserror;
extern crate nsstring;
extern crate xpcom;

pub mod bits_interface;
