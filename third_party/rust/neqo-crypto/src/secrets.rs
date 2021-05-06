// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::agentio::as_c_void;
use crate::constants::Epoch;
use crate::err::Res;
use crate::p11::{PK11SymKey, PK11_ReferenceSymKey, SymKey};
use crate::ssl::{PRFileDesc, SSLSecretCallback, SSLSecretDirection};

use neqo_common::qdebug;
use std::os::raw::c_void;
use std::pin::Pin;
use std::ptr::NonNull;

experimental_api!(SSL_SecretCallback(
    fd: *mut PRFileDesc,
    cb: SSLSecretCallback,
    arg: *mut c_void,
));

#[derive(Clone, Copy, Debug)]
pub enum SecretDirection {
    Read,
    Write,
}

impl From<SSLSecretDirection::Type> for SecretDirection {
    #[must_use]
    fn from(dir: SSLSecretDirection::Type) -> Self {
        match dir {
            SSLSecretDirection::ssl_secret_read => Self::Read,
            SSLSecretDirection::ssl_secret_write => Self::Write,
            _ => unreachable!(),
        }
    }
}

#[derive(Debug, Default)]
#[allow(clippy::module_name_repetitions)]
pub struct DirectionalSecrets {
    // We only need to maintain 3 secrets for the epochs used during the handshake.
    secrets: [Option<SymKey>; 3],
}

impl DirectionalSecrets {
    fn put(&mut self, epoch: Epoch, key: SymKey) {
        assert!(epoch > 0);
        let i = (epoch - 1) as usize;
        assert!(i < self.secrets.len());
        // assert!(self.secrets[i].is_none());
        self.secrets[i] = Some(key);
    }

    pub fn take(&mut self, epoch: Epoch) -> Option<SymKey> {
        assert!(epoch > 0);
        let i = (epoch - 1) as usize;
        assert!(i < self.secrets.len());
        self.secrets[i].take()
    }
}

#[derive(Debug, Default)]
pub struct Secrets {
    r: DirectionalSecrets,
    w: DirectionalSecrets,
}

impl Secrets {
    #[allow(clippy::unused_self)]
    unsafe extern "C" fn secret_available(
        _fd: *mut PRFileDesc,
        epoch: u16,
        dir: SSLSecretDirection::Type,
        secret: *mut PK11SymKey,
        arg: *mut c_void,
    ) {
        let secrets = arg.cast::<Self>().as_mut().unwrap();
        secrets.put_raw(epoch, dir, secret);
    }

    fn put_raw(&mut self, epoch: Epoch, dir: SSLSecretDirection::Type, key_ptr: *mut PK11SymKey) {
        let key_ptr = unsafe { PK11_ReferenceSymKey(key_ptr) };
        let key = match NonNull::new(key_ptr) {
            None => panic!("NSS shouldn't be passing out NULL secrets"),
            Some(p) => SymKey::new(p),
        };
        self.put(SecretDirection::from(dir), epoch, key);
    }

    fn put(&mut self, dir: SecretDirection, epoch: Epoch, key: SymKey) {
        qdebug!("{:?} secret available for {:?}", dir, epoch);
        let keys = match dir {
            SecretDirection::Read => &mut self.r,
            SecretDirection::Write => &mut self.w,
        };
        keys.put(epoch, key);
    }
}

#[derive(Debug)]
pub struct SecretHolder {
    secrets: Pin<Box<Secrets>>,
}

impl SecretHolder {
    /// This registers with NSS.  The lifetime of this object needs to match the lifetime
    /// of the connection, or bad things might happen.
    pub fn register(&mut self, fd: *mut PRFileDesc) -> Res<()> {
        let p = as_c_void(&mut self.secrets);
        unsafe { SSL_SecretCallback(fd, Some(Secrets::secret_available), p) }
    }

    pub fn take_read(&mut self, epoch: Epoch) -> Option<SymKey> {
        self.secrets.r.take(epoch)
    }

    pub fn take_write(&mut self, epoch: Epoch) -> Option<SymKey> {
        self.secrets.w.take(epoch)
    }
}

impl Default for SecretHolder {
    fn default() -> Self {
        Self {
            secrets: Box::pin(Secrets::default()),
        }
    }
}
