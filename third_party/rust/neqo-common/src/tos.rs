// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::fmt::Debug;

use enum_map::Enum;

/// ECN (Explicit Congestion Notification) codepoints mapped to the
/// lower 2 bits of the TOS field.
/// <https://www.iana.org/assignments/dscp-registry/dscp-registry.xhtml>
#[derive(Copy, Clone, PartialEq, Eq, Enum, Default, Debug)]
#[repr(u8)]
pub enum IpTosEcn {
    #[default]
    /// Not-ECT, Not ECN-Capable Transport, RFC3168
    NotEct = 0b00,

    /// ECT(1), ECN-Capable Transport(1), RFC8311 and RFC9331
    Ect1 = 0b01,

    /// ECT(0), ECN-Capable Transport(0), RFC3168
    Ect0 = 0b10,

    /// CE, Congestion Experienced, RFC3168
    Ce = 0b11,
}

impl From<IpTosEcn> for u8 {
    fn from(v: IpTosEcn) -> Self {
        v as u8
    }
}

impl From<u8> for IpTosEcn {
    fn from(v: u8) -> Self {
        match v & 0b11 {
            0b00 => IpTosEcn::NotEct,
            0b01 => IpTosEcn::Ect1,
            0b10 => IpTosEcn::Ect0,
            0b11 => IpTosEcn::Ce,
            _ => unreachable!(),
        }
    }
}

/// Diffserv Codepoints, mapped to the upper six bits of the TOS field.
/// <https://www.iana.org/assignments/dscp-registry/dscp-registry.xhtml>
#[derive(Copy, Clone, PartialEq, Eq, Enum, Default, Debug)]
#[repr(u8)]
pub enum IpTosDscp {
    #[default]
    /// Class Selector 0, RFC2474
    Cs0 = 0b0000_0000,

    /// Class Selector 1, RFC2474
    Cs1 = 0b0010_0000,

    /// Class Selector 2, RFC2474
    Cs2 = 0b0100_0000,

    /// Class Selector 3, RFC2474
    Cs3 = 0b0110_0000,

    /// Class Selector 4, RFC2474
    Cs4 = 0b1000_0000,

    /// Class Selector 5, RFC2474
    Cs5 = 0b1010_0000,

    /// Class Selector 6, RFC2474
    Cs6 = 0b1100_0000,

    /// Class Selector 7, RFC2474
    Cs7 = 0b1110_0000,

    /// Assured Forwarding 11, RFC2597
    Af11 = 0b0010_1000,

    /// Assured Forwarding 12, RFC2597
    Af12 = 0b0011_0000,

    /// Assured Forwarding 13, RFC2597
    Af13 = 0b0011_1000,

    /// Assured Forwarding 21, RFC2597
    Af21 = 0b0100_1000,

    /// Assured Forwarding 22, RFC2597
    Af22 = 0b0101_0000,

    /// Assured Forwarding 23, RFC2597
    Af23 = 0b0101_1000,

    /// Assured Forwarding 31, RFC2597
    Af31 = 0b0110_1000,

    /// Assured Forwarding 32, RFC2597
    Af32 = 0b0111_0000,

    /// Assured Forwarding 33, RFC2597
    Af33 = 0b0111_1000,

    /// Assured Forwarding 41, RFC2597
    Af41 = 0b1000_1000,

    /// Assured Forwarding 42, RFC2597
    Af42 = 0b1001_0000,

    /// Assured Forwarding 43, RFC2597
    Af43 = 0b1001_1000,

    /// Expedited Forwarding, RFC3246
    Ef = 0b1011_1000,

    /// Capacity-Admitted Traffic, RFC5865
    VoiceAdmit = 0b1011_0000,

    /// Lower-Effort, RFC8622
    Le = 0b0000_0100,
}

impl From<IpTosDscp> for u8 {
    fn from(v: IpTosDscp) -> Self {
        v as u8
    }
}

impl From<u8> for IpTosDscp {
    fn from(v: u8) -> Self {
        match v & 0b1111_1100 {
            0b0000_0000 => IpTosDscp::Cs0,
            0b0010_0000 => IpTosDscp::Cs1,
            0b0100_0000 => IpTosDscp::Cs2,
            0b0110_0000 => IpTosDscp::Cs3,
            0b1000_0000 => IpTosDscp::Cs4,
            0b1010_0000 => IpTosDscp::Cs5,
            0b1100_0000 => IpTosDscp::Cs6,
            0b1110_0000 => IpTosDscp::Cs7,
            0b0010_1000 => IpTosDscp::Af11,
            0b0011_0000 => IpTosDscp::Af12,
            0b0011_1000 => IpTosDscp::Af13,
            0b0100_1000 => IpTosDscp::Af21,
            0b0101_0000 => IpTosDscp::Af22,
            0b0101_1000 => IpTosDscp::Af23,
            0b0110_1000 => IpTosDscp::Af31,
            0b0111_0000 => IpTosDscp::Af32,
            0b0111_1000 => IpTosDscp::Af33,
            0b1000_1000 => IpTosDscp::Af41,
            0b1001_0000 => IpTosDscp::Af42,
            0b1001_1000 => IpTosDscp::Af43,
            0b1011_1000 => IpTosDscp::Ef,
            0b1011_0000 => IpTosDscp::VoiceAdmit,
            0b0000_0100 => IpTosDscp::Le,
            _ => unreachable!(),
        }
    }
}

/// The type-of-service field in an IP packet.
#[allow(clippy::module_name_repetitions)]
#[derive(Copy, Clone, PartialEq, Eq)]
pub struct IpTos(u8);

impl From<IpTosEcn> for IpTos {
    fn from(v: IpTosEcn) -> Self {
        Self(u8::from(v))
    }
}
impl From<IpTosDscp> for IpTos {
    fn from(v: IpTosDscp) -> Self {
        Self(u8::from(v))
    }
}
impl From<(IpTosDscp, IpTosEcn)> for IpTos {
    fn from(v: (IpTosDscp, IpTosEcn)) -> Self {
        Self(u8::from(v.0) | u8::from(v.1))
    }
}
impl From<IpTos> for u8 {
    fn from(v: IpTos) -> Self {
        v.0
    }
}

impl Debug for IpTos {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_tuple("IpTos")
            .field(&IpTosDscp::from(self.0 & 0xfc))
            .field(&IpTosEcn::from(self.0 & 0x3))
            .finish()
    }
}

impl Default for IpTos {
    fn default() -> Self {
        (IpTosDscp::default(), IpTosEcn::default()).into()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn iptosecn_into_u8() {
        assert_eq!(u8::from(IpTosEcn::NotEct), 0b00);
        assert_eq!(u8::from(IpTosEcn::Ect1), 0b01);
        assert_eq!(u8::from(IpTosEcn::Ect0), 0b10);
        assert_eq!(u8::from(IpTosEcn::Ce), 0b11);
    }

    #[test]
    fn u8_into_iptosecn() {
        assert_eq!(IpTosEcn::from(0b00), IpTosEcn::NotEct);
        assert_eq!(IpTosEcn::from(0b01), IpTosEcn::Ect1);
        assert_eq!(IpTosEcn::from(0b10), IpTosEcn::Ect0);
        assert_eq!(IpTosEcn::from(0b11), IpTosEcn::Ce);
    }

    #[test]
    fn iptosdscp_into_u8() {
        assert_eq!(u8::from(IpTosDscp::Cs0), 0b0000_0000);
        assert_eq!(u8::from(IpTosDscp::Cs1), 0b0010_0000);
        assert_eq!(u8::from(IpTosDscp::Cs2), 0b0100_0000);
        assert_eq!(u8::from(IpTosDscp::Cs3), 0b0110_0000);
        assert_eq!(u8::from(IpTosDscp::Cs4), 0b1000_0000);
        assert_eq!(u8::from(IpTosDscp::Cs5), 0b1010_0000);
        assert_eq!(u8::from(IpTosDscp::Cs6), 0b1100_0000);
        assert_eq!(u8::from(IpTosDscp::Cs7), 0b1110_0000);
        assert_eq!(u8::from(IpTosDscp::Af11), 0b0010_1000);
        assert_eq!(u8::from(IpTosDscp::Af12), 0b0011_0000);
        assert_eq!(u8::from(IpTosDscp::Af13), 0b0011_1000);
        assert_eq!(u8::from(IpTosDscp::Af21), 0b0100_1000);
        assert_eq!(u8::from(IpTosDscp::Af22), 0b0101_0000);
        assert_eq!(u8::from(IpTosDscp::Af23), 0b0101_1000);
        assert_eq!(u8::from(IpTosDscp::Af31), 0b0110_1000);
        assert_eq!(u8::from(IpTosDscp::Af32), 0b0111_0000);
        assert_eq!(u8::from(IpTosDscp::Af33), 0b0111_1000);
        assert_eq!(u8::from(IpTosDscp::Af41), 0b1000_1000);
        assert_eq!(u8::from(IpTosDscp::Af42), 0b1001_0000);
        assert_eq!(u8::from(IpTosDscp::Af43), 0b1001_1000);
        assert_eq!(u8::from(IpTosDscp::Ef), 0b1011_1000);
        assert_eq!(u8::from(IpTosDscp::VoiceAdmit), 0b1011_0000);
        assert_eq!(u8::from(IpTosDscp::Le), 0b0000_0100);
    }

    #[test]
    fn u8_into_iptosdscp() {
        assert_eq!(IpTosDscp::from(0b0000_0000), IpTosDscp::Cs0);
        assert_eq!(IpTosDscp::from(0b0010_0000), IpTosDscp::Cs1);
        assert_eq!(IpTosDscp::from(0b0100_0000), IpTosDscp::Cs2);
        assert_eq!(IpTosDscp::from(0b0110_0000), IpTosDscp::Cs3);
        assert_eq!(IpTosDscp::from(0b1000_0000), IpTosDscp::Cs4);
        assert_eq!(IpTosDscp::from(0b1010_0000), IpTosDscp::Cs5);
        assert_eq!(IpTosDscp::from(0b1100_0000), IpTosDscp::Cs6);
        assert_eq!(IpTosDscp::from(0b1110_0000), IpTosDscp::Cs7);
        assert_eq!(IpTosDscp::from(0b0010_1000), IpTosDscp::Af11);
        assert_eq!(IpTosDscp::from(0b0011_0000), IpTosDscp::Af12);
        assert_eq!(IpTosDscp::from(0b0011_1000), IpTosDscp::Af13);
        assert_eq!(IpTosDscp::from(0b0100_1000), IpTosDscp::Af21);
        assert_eq!(IpTosDscp::from(0b0101_0000), IpTosDscp::Af22);
        assert_eq!(IpTosDscp::from(0b0101_1000), IpTosDscp::Af23);
        assert_eq!(IpTosDscp::from(0b0110_1000), IpTosDscp::Af31);
        assert_eq!(IpTosDscp::from(0b0111_0000), IpTosDscp::Af32);
        assert_eq!(IpTosDscp::from(0b0111_1000), IpTosDscp::Af33);
        assert_eq!(IpTosDscp::from(0b1000_1000), IpTosDscp::Af41);
        assert_eq!(IpTosDscp::from(0b1001_0000), IpTosDscp::Af42);
        assert_eq!(IpTosDscp::from(0b1001_1000), IpTosDscp::Af43);
        assert_eq!(IpTosDscp::from(0b1011_1000), IpTosDscp::Ef);
        assert_eq!(IpTosDscp::from(0b1011_0000), IpTosDscp::VoiceAdmit);
        assert_eq!(IpTosDscp::from(0b0000_0100), IpTosDscp::Le);
    }

    #[test]
    fn iptosecn_into_iptos() {
        let ecn = IpTosEcn::default();
        let iptos_ecn: IpTos = ecn.into();
        assert_eq!(u8::from(iptos_ecn), ecn as u8);
    }

    #[test]
    fn iptosdscp_into_iptos() {
        let dscp = IpTosDscp::default();
        let iptos_dscp: IpTos = dscp.into();
        assert_eq!(u8::from(iptos_dscp), dscp as u8);
    }
}
