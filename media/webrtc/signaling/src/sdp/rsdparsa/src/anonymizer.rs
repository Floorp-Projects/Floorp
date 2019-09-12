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
mod tests {
    use super::*;

    #[test]
    fn test_mask_ip() {
        let mut anon = StatefulSdpAnonymizer::default();
        let v4 = [
            Ipv4Addr::new(127, 0, 0, 1),
            Ipv4Addr::new(10, 0, 0, 1),
            Ipv4Addr::new(1, 1, 1, 1),
        ];
        let v4_masked = [
            Ipv4Addr::new(0, 0, 0, 1),
            Ipv4Addr::new(0, 0, 0, 2),
            Ipv4Addr::new(0, 0, 0, 3),
        ];
        let v6 = [
            Ipv6Addr::from(0),
            Ipv6Addr::from(528_189_235),
            Ipv6Addr::from(1_623_734_988_148_990),
        ];
        let v6_masked = [Ipv6Addr::from(1), Ipv6Addr::from(2), Ipv6Addr::from(3)];
        for _ in 0..2 {
            assert_eq!(anon.mask_ip(&IpAddr::V4(v4[0])), v4_masked[0]);
            assert_eq!(anon.mask_ip(&IpAddr::V6(v6[0])), v6_masked[0]);

            assert_eq!(anon.mask_ip(&IpAddr::V4(v4[1])), v4_masked[1]);
            assert_eq!(anon.mask_ip(&IpAddr::V6(v6[1])), v6_masked[1]);

            assert_eq!(anon.mask_ip(&IpAddr::V4(v4[2])), v4_masked[2]);
            assert_eq!(anon.mask_ip(&IpAddr::V6(v6[2])), v6_masked[2]);
        }
    }

    #[test]
    fn test_mask_port() {
        let mut anon = StatefulSdpAnonymizer::default();
        let ports = [0, 125, 12346];
        let masked_ports = [1, 2, 3];
        for _ in 0..2 {
            assert_eq!(anon.mask_port(ports[0]), masked_ports[0]);
            assert_eq!(anon.mask_port(ports[1]), masked_ports[1]);
            assert_eq!(anon.mask_port(ports[2]), masked_ports[2]);
        }
    }

    #[test]
    fn test_mask_ice_password() {
        let mut anon = StatefulSdpAnonymizer::default();
        let passwords = ["vasdfioqwenl14082`14", "0", "ncp	HY878hp(poh"];
        let masked_passwords = [
            "ice-password-00000001",
            "ice-password-00000002",
            "ice-password-00000003",
        ];
        for _ in 0..2 {
            assert_eq!(anon.mask_ice_password(passwords[0]), masked_passwords[0]);
            assert_eq!(anon.mask_ice_password(passwords[1]), masked_passwords[1]);
            assert_eq!(anon.mask_ice_password(passwords[2]), masked_passwords[2]);
        }
    }

    #[test]
    fn test_mask_ice_user() {
        let mut anon = StatefulSdpAnonymizer::default();
        let users = ["user1", "user2", "8109q2asdf"];
        let masked_users = [
            "ice-user-00000001",
            "ice-user-00000002",
            "ice-user-00000003",
        ];
        for _ in 0..2 {
            assert_eq!(anon.mask_ice_user(users[0]), masked_users[0]);
            assert_eq!(anon.mask_ice_user(users[1]), masked_users[1]);
            assert_eq!(anon.mask_ice_user(users[2]), masked_users[2]);
        }
    }

    #[test]
    fn test_mask_cert_fingerprint() {
        let mut anon = StatefulSdpAnonymizer::default();
        let prints: [Vec<u8>; 3] = [
            vec![
                0x59u8, 0x4A, 0x8B, 0x73, 0xA7, 0x73, 0x53, 0x71, 0x88, 0xD7, 0x4D, 0x58, 0x28,
                0x0C, 0x79, 0x72, 0x31, 0x29, 0x9B, 0x05, 0x37, 0xDD, 0x58, 0x43, 0xC2, 0xD4, 0x85,
                0xA2, 0xB3, 0x66, 0x38, 0x7A,
            ],
            vec![
                0x30u8, 0xFF, 0x8E, 0x2B, 0xAC, 0x9D, 0xED, 0x70, 0x18, 0x10, 0x67, 0xC8, 0xAE,
                0x9E, 0x68, 0xF3, 0x86, 0x53, 0x51, 0xB0, 0xAC, 0x31, 0xB7, 0xBE, 0x6D, 0xCF, 0xA4,
                0x2E, 0xD3, 0x6E, 0xB4, 0x28,
            ],
            vec![
                0xDFu8, 0x2E, 0xAC, 0x8A, 0xFD, 0x0A, 0x8E, 0x99, 0xBF, 0x5D, 0xE8, 0x3C, 0xE7,
                0xFA, 0xFB, 0x08, 0x3B, 0x3C, 0x54, 0x1D, 0xD7, 0xD4, 0x05, 0x77, 0xA0, 0x72, 0x9B,
                0x14, 0x08, 0x6D, 0x0F, 0x4C,
            ],
        ];

        let masked_prints = [1u64.to_byte_vec(), 2u64.to_byte_vec(), 3u64.to_byte_vec()];
        for _ in 0..2 {
            assert_eq!(anon.mask_cert_finger_print(&prints[0]), masked_prints[0]);
            assert_eq!(anon.mask_cert_finger_print(&prints[1]), masked_prints[1]);
            assert_eq!(anon.mask_cert_finger_print(&prints[2]), masked_prints[2]);
        }
    }

    #[test]
    fn test_mask_cname() {
        let mut anon = StatefulSdpAnonymizer::default();
        let cnames = ["mailto:foo@bar", "JohnDoe", "Jane Doe"];
        let masked_cnames = ["cname-00000001", "cname-00000002", "cname-00000003"];
        for _ in 0..2 {
            assert_eq!(anon.mask_cname(cnames[0]), masked_cnames[0]);
            assert_eq!(anon.mask_cname(cnames[1]), masked_cnames[1]);
            assert_eq!(anon.mask_cname(cnames[2]), masked_cnames[2]);
        }
    }
}
