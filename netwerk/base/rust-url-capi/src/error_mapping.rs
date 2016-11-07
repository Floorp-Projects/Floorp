/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use url::ParseError;

pub trait ErrorCode {
  fn error_code(&self) -> i32;
}

impl<T: ErrorCode> ErrorCode for Result<(), T> {
  fn error_code(&self) -> i32 {
    match *self {
      Ok(_) => 0,
      Err(ref error) => error.error_code(),
    }
  }
}

impl ErrorCode for Result<(), ()> {
  fn error_code(&self) -> i32 {
    match *self {
      Ok(_)  =>    0,
      Err(_) => -255,
    }
  }
}

impl ErrorCode for ParseError {
  fn error_code(&self) -> i32 {
    match *self {
      ParseError::EmptyHost                              =>  -1,
      ParseError::InvalidPort                            =>  -2,
      ParseError::InvalidIpv6Address                     =>  -3,
      ParseError::InvalidDomainCharacter                 =>  -4,
      ParseError::IdnaError                              =>  -5,
      ParseError::InvalidIpv4Address                     =>  -6,
      ParseError::RelativeUrlWithoutBase                 =>  -7,
      ParseError::RelativeUrlWithCannotBeABaseBase       =>  -8,
      ParseError::SetHostOnCannotBeABaseUrl              =>  -9,
      ParseError::Overflow                               => -10,
    }
  }
}

pub enum NSError {
  OK,
  InvalidArg,
  Failure,
}

impl ErrorCode for NSError {
  #[allow(overflowing_literals)]
  fn error_code(&self) -> i32 {
    match *self {
      NSError::OK => 0,
      NSError::InvalidArg => 0x80070057,
      NSError::Failure => 0x80004005
    }
  }
}
