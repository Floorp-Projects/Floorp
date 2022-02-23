/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate url;
use address::{Address, ExplicitlyTypedAddress};
use std::collections::HashMap;
use std::net::{IpAddr, Ipv4Addr, Ipv6Addr};
use std::num::Wrapping;

pub trait AnonymizingClone {
    fn masked_clone(&self, anon: &mut StatefulSdpAnonymizer) -> Self;
}

pub trait ToBytesVec {
    fn to_byte_vec(&self) -> Vec<u8>;
}

impl ToBytesVec for u64 {
    fn to_byte_vec(&self) -> Vec<u8> {
        let mut bytes = Vec::new();
        let mut val = *self;
        for _ in 0..8 {
            bytes.push(val as u8);
            val <<= 8;
        }
        bytes.reverse();
        bytes
    }
}

/*
* Anonymizes SDP in a stateful fashion, such that a pre-anonymized value will
* always be transformed into the same anonymized value within the context of
* the anonymizer.
* Stores the opaque state necessary for intelligent anonymization of SDP. This
* state can be stored and reused during the offer-answer period, and it
* will maintain a stable set of masked values.
*/
pub struct StatefulSdpAnonymizer {
    ips: HashMap<IpAddr, IpAddr>,
    ip_v4_inc: Wrapping<u32>,
    ip_v6_inc: Wrapping<u128>,
    host_names: AnonymizationStrMap,
    ports: HashMap<u32, u32>,
    port_inc: Wrapping<u32>,
    origin_users: AnonymizationStrMap,
    ice_passwords: AnonymizationStrMap,
    ice_users: AnonymizationStrMap,
    cert_finger_prints: HashMap<Vec<u8>, Vec<u8>>,
    cert_finger_print_inc: Wrapping<u64>,
    cnames: AnonymizationStrMap,
}

impl Default for StatefulSdpAnonymizer {
    fn default() -> Self {
        Self::new()
    }
}

impl StatefulSdpAnonymizer {
    pub fn new() -> Self {
        StatefulSdpAnonymizer {
            ips: HashMap::new(),
            ip_v4_inc: Wrapping(0),
            ip_v6_inc: Wrapping(0),
            host_names: AnonymizationStrMap::new("fqdn-", 8),
            ports: HashMap::new(),
            port_inc: Wrapping(0),
            origin_users: AnonymizationStrMap::new("origin-user-", 8),
            ice_passwords: AnonymizationStrMap::new("ice-password-", 8),
            ice_users: AnonymizationStrMap::new("ice-user-", 8),
            cert_finger_prints: HashMap::new(),
            cert_finger_print_inc: Wrapping(0),
            cnames: AnonymizationStrMap::new("cname-", 8),
        }
    }

    pub fn mask_host(&mut self, host: &str) -> String {
        self.host_names.mask(host)
    }

    pub fn mask_ip(&mut self, addr: &IpAddr) -> IpAddr {
        if let Some(address) = self.ips.get(addr) {
            return *address;
        }
        let mapped = match addr {
            IpAddr::V4(_) => {
                self.ip_v4_inc += Wrapping(1);
                IpAddr::V4(Ipv4Addr::from(self.ip_v4_inc.0))
            }
            IpAddr::V6(_) => {
                self.ip_v6_inc += Wrapping(1);
                IpAddr::V6(Ipv6Addr::from(self.ip_v6_inc.0))
            }
        };
        self.ips.insert(*addr, mapped);
        mapped
    }

    pub fn mask_address(&mut self, address: &Address) -> Address {
        match address {
            Address::Fqdn(host) => Address::Fqdn(self.mask_host(host)),
            Address::Ip(ip) => Address::Ip(self.mask_ip(ip)),
        }
    }

    pub fn mask_typed_address(
        &mut self,
        address: &ExplicitlyTypedAddress,
    ) -> ExplicitlyTypedAddress {
        match address {
            ExplicitlyTypedAddress::Fqdn {
                address_type,
                domain,
            } => ExplicitlyTypedAddress::Fqdn {
                address_type: *address_type,
                domain: self.mask_host(domain),
            },
            ExplicitlyTypedAddress::Ip(ip) => ExplicitlyTypedAddress::Ip(self.mask_ip(ip)),
        }
    }

    pub fn mask_port(&mut self, port: u32) -> u32 {
        if let Some(stored) = self.ports.get(&port) {
            return *stored;
        }
        self.port_inc += Wrapping(1);
        self.ports.insert(port, self.port_inc.0);
        self.port_inc.0
    }

    pub fn mask_origin_user(&mut self, user: &str) -> String {
        self.origin_users.mask(user)
    }

    pub fn mask_ice_password(&mut self, password: &str) -> String {
        self.ice_passwords.mask(password)
    }

    pub fn mask_ice_user(&mut self, user: &str) -> String {
        self.ice_users.mask(user)
    }

    pub fn mask_cert_finger_print(&mut self, finger_print: &[u8]) -> Vec<u8> {
        if let Some(stored) = self.cert_finger_prints.get(finger_print) {
            return stored.clone();
        }
        self.cert_finger_print_inc += Wrapping(1);
        self.cert_finger_prints.insert(
            finger_print.to_vec(),
            self.cert_finger_print_inc.0.to_byte_vec(),
        );
        self.cert_finger_print_inc.0.to_byte_vec()
    }

    pub fn mask_cname(&mut self, cname: &str) -> String {
        self.cnames.mask(cname)
    }
}

struct AnonymizationStrMap {
    map: HashMap<String, String>,
    counter: Wrapping<u64>,
    prefix: &'static str,
    padding: usize,
}

impl AnonymizationStrMap {
    pub fn new(prefix: &'static str, padding: usize) -> Self {
        Self {
            map: HashMap::new(),
            counter: Wrapping(0),
            prefix,
            padding,
        }
    }

    pub fn mask(&mut self, value: &str) -> String {
        let key = value.to_owned();
        if let Some(stored) = self.map.get(&key) {
            return stored.clone();
        }
        self.counter += Wrapping(1);
        let store = format!(
            "{}{:0padding$}",
            self.prefix,
            self.counter.0,
            padding = self.padding
        );
        self.map.insert(key, store.clone());
        store
    }
}

#[cfg(test)]
#[path = "./anonymizer_tests.rs"]
mod tests;
