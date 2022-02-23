/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
            0x59u8, 0x4A, 0x8B, 0x73, 0xA7, 0x73, 0x53, 0x71, 0x88, 0xD7, 0x4D, 0x58, 0x28, 0x0C,
            0x79, 0x72, 0x31, 0x29, 0x9B, 0x05, 0x37, 0xDD, 0x58, 0x43, 0xC2, 0xD4, 0x85, 0xA2,
            0xB3, 0x66, 0x38, 0x7A,
        ],
        vec![
            0x30u8, 0xFF, 0x8E, 0x2B, 0xAC, 0x9D, 0xED, 0x70, 0x18, 0x10, 0x67, 0xC8, 0xAE, 0x9E,
            0x68, 0xF3, 0x86, 0x53, 0x51, 0xB0, 0xAC, 0x31, 0xB7, 0xBE, 0x6D, 0xCF, 0xA4, 0x2E,
            0xD3, 0x6E, 0xB4, 0x28,
        ],
        vec![
            0xDFu8, 0x2E, 0xAC, 0x8A, 0xFD, 0x0A, 0x8E, 0x99, 0xBF, 0x5D, 0xE8, 0x3C, 0xE7, 0xFA,
            0xFB, 0x08, 0x3B, 0x3C, 0x54, 0x1D, 0xD7, 0xD4, 0x05, 0x77, 0xA0, 0x72, 0x9B, 0x14,
            0x08, 0x6D, 0x0F, 0x4C,
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
