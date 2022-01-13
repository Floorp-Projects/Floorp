/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate url;
use super::*;
use std::net::{IpAddr, Ipv4Addr, Ipv6Addr};

macro_rules! make_check_parse {
    ($attr_type:ty, $attr_kind:path) => {
        |attr_str: &str| -> $attr_type {
            match parse_attribute(attr_str) {
                Ok(SdpType::Attribute($attr_kind(attr))) => attr,
                Err(e) => panic!("{}", e),
                _ => unreachable!(),
            }
        }
    };

    ($attr_kind:path) => {
        |attr_str: &str| -> SdpAttribute {
            match parse_attribute(attr_str) {
                Ok(SdpType::Attribute($attr_kind)) => $attr_kind,
                Err(e) => panic!("{}", e),
                _ => unreachable!(),
            }
        }
    };
}

macro_rules! make_check_parse_and_serialize {
    ($check_parse_func:ident, $attr_kind:path) => {
        |attr_str: &str| {
            let parsed = $attr_kind($check_parse_func(attr_str));
            assert_eq!(parsed.to_string(), attr_str.to_string());
        }
    };

    ($check_parse_func:ident) => {
        |attr_str: &str| {
            let parsed = $check_parse_func(attr_str);
            assert_eq!(parsed.to_string(), attr_str.to_string());
        }
    };
}

#[test]
fn test_parse_attribute_candidate_and_serialize() {
    let check_parse = make_check_parse!(SdpAttributeCandidate, SdpAttribute::Candidate);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::Candidate);

    check_parse_and_serialize("candidate:0 1 UDP 2122252543 172.16.156.106 49760 typ host");
    check_parse_and_serialize("candidate:foo 1 UDP 2122252543 172.16.156.106 49760 typ host");
    check_parse_and_serialize("candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host");
    check_parse_and_serialize("candidate:0 1 TCP 2122252543 ::1 49760 typ host");
    check_parse_and_serialize("candidate:0 1 TCP 2122252543 2001:db8:4860::4444 49760 typ host");
    check_parse_and_serialize("candidate:0 1 UDP 2122252543 172.16.156.106 49760 typ srflx");
    check_parse_and_serialize("candidate:0 1 UDP 2122252543 172.16.156.106 49760 typ prflx");
    check_parse_and_serialize("candidate:0 1 UDP 2122252543 172.16.156.106 49760 typ relay");
    check_parse_and_serialize(
        "candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host tcptype active",
    );
    check_parse_and_serialize(
        "candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host tcptype passive",
    );
    check_parse_and_serialize(
        "candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host tcptype so",
    );
    check_parse_and_serialize(
        "candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host ufrag foobar",
    );
    check_parse_and_serialize(
        "candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host network-cost 50",
    );
    check_parse_and_serialize("candidate:1 1 UDP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1.4 rport 61665 generation 0");
    check_parse_and_serialize(
        "candidate:1 1 UDP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1.4 rport 61665",
    );
    check_parse_and_serialize("candidate:1 1 TCP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1.4 rport 61665 tcptype passive");
    check_parse_and_serialize("candidate:1 1 TCP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1.4 rport 61665 tcptype passive generation 1");
    check_parse_and_serialize("candidate:1 1 TCP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1.4 rport 61665 tcptype passive generation 1 ufrag +DGd");
    check_parse_and_serialize("candidate:1 1 TCP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1.4 rport 61665 tcptype passive generation 1 ufrag +DGd network-cost 1");
    check_parse_and_serialize(
        "candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host unsupported foo",
    );
    check_parse_and_serialize("candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host unsupported foo more_unsupported bar");

    let candidate = check_parse("candidate:1 1 TCP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1.4 rport 61665 tcptype passive generation 1 ufrag +DGd network-cost 1 unsupported foo");
    assert_eq!(candidate.foundation, "1".to_string());
    assert_eq!(candidate.component, 1);
    assert_eq!(candidate.transport, SdpAttributeCandidateTransport::Tcp);
    assert_eq!(candidate.priority, 1_685_987_071);
    assert_eq!(
        candidate.address,
        Address::from_str("24.23.204.141").unwrap()
    );
    assert_eq!(candidate.port, 54609);
    assert_eq!(candidate.c_type, SdpAttributeCandidateType::Srflx);
    assert_eq!(
        candidate.raddr,
        Some(Address::from_str("192.168.1.4").unwrap())
    );
    assert_eq!(candidate.rport, Some(61665));
    assert_eq!(
        candidate.tcp_type,
        Some(SdpAttributeCandidateTcpType::Passive)
    );
    assert_eq!(candidate.generation, Some(1));
    assert_eq!(candidate.ufrag, Some("+DGd".to_string()));
    assert_eq!(candidate.networkcost, Some(1));
    assert_eq!(
        candidate.unknown_extensions,
        vec![("unsupported".to_string(), "foo".to_string())]
    )
}

#[test]
fn test_anonymize_attribute_candidate() -> Result<(), SdpParserInternalError> {
    let mut anon = StatefulSdpAnonymizer::new();
    let candidate_1 = parse_attribute("candidate:0 1 TCP 2122252543 ::8 49760 typ host")?;
    let candidate_2 =
        parse_attribute("candidate:0 1 UDP 2122252543 172.16.156.106 19361 typ srflx")?;
    let candidate_3 = parse_attribute("candidate:1 1 TCP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1.4 rport 61665 tcptype passive generation 1 ufrag +DGd")?;
    if let SdpType::Attribute(SdpAttribute::Candidate(candidate)) = candidate_1 {
        let masked = candidate.masked_clone(&mut anon);
        assert!(masked.address == Address::Ip(IpAddr::V6(Ipv6Addr::from(1))));
        assert!(masked.port == 1);
    } else {
        unreachable!();
    }

    if let SdpType::Attribute(SdpAttribute::Candidate(candidate)) = candidate_2 {
        let masked = candidate.masked_clone(&mut anon);
        assert!(masked.address == Address::Ip(IpAddr::V4(Ipv4Addr::from(1))));
        assert!(masked.port == 2);
    } else {
        unreachable!();
    }

    if let SdpType::Attribute(SdpAttribute::Candidate(candidate)) = candidate_3 {
        let masked = candidate.masked_clone(&mut anon);
        assert!(masked.address == Address::Ip(IpAddr::V4(Ipv4Addr::from(2))));
        assert!(masked.port == 3);
        assert!(masked.raddr.unwrap() == Address::Ip(IpAddr::V4(Ipv4Addr::from(3))));
        assert!(masked.rport.unwrap() == 4);
    } else {
        unreachable!();
    }
    Ok(())
}

#[test]
fn test_parse_attribute_candidate_errors() {
    assert!(parse_attribute("candidate:0 1 UDP 2122252543 172.16.156.106 49760 typ").is_err());
    assert!(
        parse_attribute("candidate:0 foo UDP 2122252543 172.16.156.106 49760 typ host").is_err()
    );
    assert!(parse_attribute("candidate:0 1 FOO 2122252543 172.16.156.106 49760 typ host").is_err());
    assert!(parse_attribute("candidate:0 1 UDP foo 172.16.156.106 49760 typ host").is_err());
    assert!(parse_attribute("candidate:0 1 UDP 2122252543 372.16.356 49760 typ host").is_err());
    assert!(parse_attribute("candidate:0 1 UDP 2122252543 172.16.156.106 70000 typ host").is_err());
    assert!(
        parse_attribute("candidate:0 1 UDP 2122252543 172.16.156.106 49760 type host").is_err()
    );
    assert!(parse_attribute("candidate:0 1 UDP 2122252543 172.16.156.106 49760 typ fost").is_err());
    assert!(parse_attribute(
        "candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host unsupported"
    )
    .is_err());
    assert!(parse_attribute(
        "candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host network-cost"
    )
    .is_err());
    assert!(parse_attribute("candidate:1 1 UDP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1.4 rport 61665 generation B").is_err());
    assert!(parse_attribute(
        "candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host network-cost C"
    )
    .is_err());
    assert!(parse_attribute(
        "candidate:1 1 UDP 1685987071 24.23.204.141 54609 typ srflx raddr 1%92.168.1 rport 61665"
    )
    .is_err());
    assert!(parse_attribute(
        "candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host tcptype foobar"
    )
    .is_err());
    assert!(parse_attribute(
        "candidate:1 1 UDP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1.4 rport 70000"
    )
    .is_err());
}

#[test]
fn test_parse_dtls_message() {
    let check_parse = make_check_parse!(SdpAttributeDtlsMessage, SdpAttribute::DtlsMessage);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::DtlsMessage);

    check_parse_and_serialize("dtls-message:client SGVsbG8gV29ybGQ=");
    check_parse_and_serialize("dtls-message:server SGVsbG8gV29ybGQ=");
    check_parse_and_serialize("dtls-message:client IGlzdCBl/W4gUeiBtaXQg+JSB1bmQCAkJJkSNEQ=");
    check_parse_and_serialize("dtls-message:server IGlzdCBl/W4gUeiBtaXQg+JSB1bmQCAkJJkSNEQ=");

    match check_parse("dtls-message:client SGVsbG8gV29ybGQ=") {
        SdpAttributeDtlsMessage::Client(x) => {
            assert_eq!(x, "SGVsbG8gV29ybGQ=");
        }
        _ => {
            unreachable!();
        }
    }

    match check_parse("dtls-message:server SGVsbG8gV29ybGQ=") {
        SdpAttributeDtlsMessage::Server(x) => {
            assert_eq!(x, "SGVsbG8gV29ybGQ=");
        }
        _ => {
            unreachable!();
        }
    }

    assert!(parse_attribute("dtls-message:client").is_err());
    assert!(parse_attribute("dtls-message:server").is_err());
    assert!(parse_attribute("dtls-message:unsupported SGVsbG8gV29ybGQ=").is_err());
}

#[test]
fn test_parse_attribute_end_of_candidates() {
    let check_parse = make_check_parse!(SdpAttribute::EndOfCandidates);
    let check_parse_and_serialize = make_check_parse_and_serialize!(check_parse);

    check_parse_and_serialize("end-of-candidates");
    assert!(parse_attribute("end-of-candidates foobar").is_err());
}

#[test]
fn test_parse_attribute_extmap() {
    let check_parse = make_check_parse!(SdpAttributeExtmap, SdpAttribute::Extmap);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::Extmap);

    check_parse_and_serialize("extmap:1/sendonly urn:ietf:params:rtp-hdrext:ssrc-audio-level");
    check_parse_and_serialize("extmap:2/sendrecv urn:ietf:params:rtp-hdrext:ssrc-audio-level");
    check_parse_and_serialize(
        "extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time",
    );
    check_parse_and_serialize(
        "extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time ext_attributes",
    );

    assert!(parse_attribute("extmap:1/sendrecv").is_err());
    assert!(
        parse_attribute("extmap:a/sendrecv urn:ietf:params:rtp-hdrext:ssrc-audio-level").is_err()
    );
    assert!(
        parse_attribute("extmap:4/unsupported urn:ietf:params:rtp-hdrext:ssrc-audio-level")
            .is_err()
    );

    let mut bad_char =
        String::from("extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time ");
    bad_char.push(0x00 as char);
    assert!(parse_attribute(&bad_char).is_err());
}

#[test]
fn test_parse_attribute_fingerprint() {
    let check_parse = make_check_parse!(SdpAttributeFingerprint, SdpAttribute::Fingerprint);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::Fingerprint);

    check_parse_and_serialize(
        "fingerprint:sha-1 CD:34:D1:62:16:95:7B:B7:EB:74:E2:39:27:97:EB:0B:23:73:AC:BC",
    );
    check_parse_and_serialize(
        "fingerprint:sha-224 CD:34:D1:62:16:95:7B:B7:EB:74:E2:39:27:97:EB:0B:23:73:AC:BC:\
             27:97:EB:0B:23:73:AC:BC",
    );
    check_parse_and_serialize(
        "fingerprint:sha-256 CD:34:D1:62:16:95:7B:B7:EB:74:E2:39:27:97:EB:0B:23:73:AC:BC:\
             27:97:EB:0B:23:73:AC:BC:CD:34:D1:62",
    );
    check_parse_and_serialize(
        "fingerprint:sha-384 CD:34:D1:62:16:95:7B:B7:EB:74:E2:39:27:97:EB:0B:23:73:AC:BC:\
             27:97:EB:0B:23:73:AC:BC:CD:34:D1:62:16:95:7B:B7:EB:74:E2:39:\
             27:97:EB:0B:23:73:AC:BC",
    );
    check_parse_and_serialize(
        "fingerprint:sha-512 CD:34:D1:62:16:95:7B:B7:EB:74:E2:39:27:97:EB:0B:23:73:AC:BC:\
             97:EB:0B:23:73:AC:BC:CD:34:D1:62:16:95:7B:B7:EB:74:E2:39:27:\
             EB:0B:23:73:AC:BC:27:97:EB:0B:23:73:AC:BC:27:97:EB:0B:23:73:\
             BC:EB:0B:23",
    );

    assert!(parse_attribute("fingerprint:sha-1").is_err());
    assert!(parse_attribute(
        "fingerprint:unsupported CD:34:D1:62:16:95:7B:B7:EB:74:E1:39:27:97:EB:0B:23:73:AC:BC"
    )
    .is_err());
    assert!(parse_attribute(
        "fingerprint:sha-1 CDA:34:D1:62:16:95:7B:B7:EB:74:E1:39:27:97:EB:0B:23:73:AC:BC"
    )
    .is_err());
    assert!(parse_attribute(
        "fingerprint:sha-1 CD:34:D1:62:16:95:7B:B7:EB:74:E1:39:27:97:EB:0B:23:73:AC:"
    )
    .is_err());
    assert!(parse_attribute(
        "fingerprint:sha-1 CD:34:D1:62:16:95:7B:B7:EB:74:E1:39:27:97:EB:0B:23:73:AC"
    )
    .is_err());
    assert!(parse_attribute(
        "fingerprint:sha-1 CX:34:D1:62:16:95:7B:B7:EB:74:E1:39:27:97:EB:0B:23:73:AC:BC"
    )
    .is_err());

    assert!(parse_attribute(
        "fingerprint:sha-1 0xCD:34:D1:62:16:95:7B:B7:EB:74:E2:39:27:97:EB:0B:23:73:AC:BC"
    )
    .is_err());
    assert!(parse_attribute(
        "fingerprint:sha-1 CD:0x34:D1:62:16:95:7B:B7:EB:74:E2:39:27:97:EB:0B:23:73:AC:BC"
    )
    .is_err());
    assert!(parse_attribute(
        "fingerprint:sha-1 CD::D1:62:16:95:7B:B7:EB:74:E2:39:27:97:EB:0B:23:73:AC:BC"
    )
    .is_err());
    assert!(parse_attribute(
        "fingerprint:sha-1 CD:0000A:D1:62:16:95:7B:B7:EB:74:E2:39:27:97:EB:0B:23:73:AC:BC"
    )
    .is_err());
    assert!(parse_attribute(
        "fingerprint:sha-1 CD:B:D1:62:16:95:7B:B7:EB:74:E2:39:27:97:EB:0B:23:73:AC:BC"
    )
    .is_err());
}

#[test]
fn test_parse_attribute_fmtp() {
    let check_parse = make_check_parse!(SdpAttributeFmtp, SdpAttribute::Fmtp);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::Fmtp);

    check_parse_and_serialize("fmtp:109 maxplaybackrate=46000;stereo=1;useinbandfec=1");
    check_parse_and_serialize(
        "fmtp:126 profile-level-id=42e01f;level-asymmetry-allowed=1;packetization-mode=1",
    );
    check_parse_and_serialize("fmtp:66 0-15");
    check_parse_and_serialize("fmtp:109 0-15,66");
    check_parse_and_serialize("fmtp:66 111/115");
    assert!(parse_attribute("fmtp:109 maxplaybackrate=48000;stereo=1;useinbandfec=1").is_ok());
    assert!(parse_attribute("fmtp:109 maxplaybackrate=48000; stereo=1; useinbandfec=1").is_ok());
    assert!(parse_attribute("fmtp:109 maxplaybackrate=48000; stereo=1;useinbandfec=1").is_ok());
    check_parse_and_serialize("fmtp:8 maxplaybackrate=46000");
    check_parse_and_serialize("fmtp:8 maxaveragebitrate=46000");
    check_parse_and_serialize("fmtp:8 maxaveragebitrate=46000;ptime=60;minptime=20;maxptime=120");
    check_parse_and_serialize(
        "fmtp:8 max-cpb=1234;max-dpb=32000;max-br=3;max-mbps=46000;usedtx=1;cbr=1",
    );
    assert!(parse_attribute("fmtp:77 ").is_err());
    assert!(parse_attribute("fmtp:109 stereo=2;").is_err());
    assert!(parse_attribute("fmtp:109 111/129;").is_err());
    assert!(parse_attribute("fmtp:109 packetization-mode=3;").is_err());
    assert!(parse_attribute("fmtp:109 maxplaybackrate=48000stereo=1;").is_err());
    assert!(parse_attribute("fmtp:8 ;maxplaybackrate=48000").is_ok());
    assert!(parse_attribute("fmtp:8 packetization-mode=2;;maxplaybackrate=48000").is_ok());
    assert!(parse_attribute("fmtp:8 packetization-mode=2; maxplaybackrate=48000").is_ok());
    assert!(parse_attribute("fmtp:8 maxplaybackrate=48000;").is_ok());
    assert!(parse_attribute("fmtp:8 x-google-start-bitrate=800; maxplaybackrate=48000;").is_ok());
    check_parse_and_serialize("fmtp:97 apt=96");
    check_parse_and_serialize("fmtp:97 apt=96;rtx-time=3000");
    check_parse_and_serialize(
        "fmtp:102 packetization-mode=1;sprop-parameter-sets=Z0LAFYyNQKD5APCIRqA=,aM48gA==",
    );
}

#[test]
fn test_anonymize_attribute_fingerprint() -> Result<(), SdpParserInternalError> {
    let mut anon = StatefulSdpAnonymizer::new();
    if let SdpType::Attribute(SdpAttribute::Fingerprint(print)) = parse_attribute(
        "fingerprint:sha-1 CD:34:D1:62:16:95:7B:B7:EB:74:E2:39:27:97:EB:0B:23:73:AC:BC",
    )? {
        assert!(print.masked_clone(&mut anon).to_string() == "sha-1 00:00:00:00:00:00:00:01");
    } else {
        unreachable!();
    }
    Ok(())
}

#[test]
fn test_parse_attribute_group() {
    let check_parse = make_check_parse!(SdpAttributeGroup, SdpAttribute::Group);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::Group);

    check_parse_and_serialize("group:LS");
    check_parse_and_serialize("group:LS 1 2");
    check_parse_and_serialize("group:FID 1 2");
    check_parse_and_serialize("group:SRF 1 2");
    check_parse_and_serialize("group:FEC S1 R1");
    check_parse_and_serialize("group:ANAT S1 R1");
    check_parse_and_serialize("group:DDP L1 L2 L3");
    check_parse_and_serialize("group:BUNDLE sdparta_0 sdparta_1 sdparta_2");

    assert!(parse_attribute("group:").is_err());
    assert!(matches!(
        parse_attribute("group:NEVER_SUPPORTED_SEMANTICS"),
        Err(SdpParserInternalError::Unsupported(_))
    ));
}

#[test]
fn test_parse_attribute_bundle_only() {
    let check_parse = make_check_parse!(SdpAttribute::BundleOnly);
    let check_parse_and_serialize = make_check_parse_and_serialize!(check_parse);

    check_parse_and_serialize("bundle-only");

    assert!(parse_attribute("bundle-only foobar").is_err());
}

#[test]
fn test_parse_attribute_ice_lite() {
    let check_parse = make_check_parse!(SdpAttribute::IceLite);
    let check_parse_and_serialize = make_check_parse_and_serialize!(check_parse);

    check_parse_and_serialize("ice-lite");

    assert!(parse_attribute("ice-lite foobar").is_err());
}

#[test]
fn test_parse_attribute_extmap_allow_mixed() {
    let check_parse = make_check_parse!(SdpAttribute::ExtmapAllowMixed);
    let check_parse_and_serialize = make_check_parse_and_serialize!(check_parse);

    check_parse_and_serialize("extmap-allow-mixed");

    assert!(parse_attribute("extmap-allow-mixed 100").is_err());
}

#[test]
fn test_parse_attribute_ice_mismatch() {
    let check_parse = make_check_parse!(SdpAttribute::IceMismatch);
    let check_parse_and_serialize = make_check_parse_and_serialize!(check_parse);

    check_parse_and_serialize("ice-mismatch");

    assert!(parse_attribute("ice-mismatch foobar").is_err());
}

#[test]
fn test_parse_attribute_ice_options() {
    let check_parse = make_check_parse!(Vec<String>, SdpAttribute::IceOptions);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::IceOptions);

    check_parse_and_serialize("ice-options:trickle");

    assert!(parse_attribute("ice-options:").is_err());
}

#[test]
fn test_parse_attribute_ice_pacing() {
    let check_parse = make_check_parse!(u64, SdpAttribute::IcePacing);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::IcePacing);

    check_parse_and_serialize("ice-pacing:50");

    assert!(parse_attribute("ice-pacing:").is_err());
    assert!(parse_attribute("ice-pacing:10000000000").is_err());
    assert!(parse_attribute("ice-pacing:50 100").is_err());
    assert!(parse_attribute("ice-pacing:foobar").is_err());
}

#[test]
fn test_parse_attribute_ice_pwd() {
    let check_parse = make_check_parse!(String, SdpAttribute::IcePwd);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::IcePwd);

    check_parse_and_serialize("ice-pwd:e3baa26dd2fa5030d881d385f1e36cce");

    assert!(parse_attribute("ice-pwd:").is_err());
}

#[test]
fn test_parse_attribute_ice_ufrag() {
    let check_parse = make_check_parse!(String, SdpAttribute::IceUfrag);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::IceUfrag);

    check_parse_and_serialize("ice-ufrag:58b99ead");

    assert!(parse_attribute("ice-ufrag:").is_err());
}

#[test]
fn test_parse_attribute_identity() {
    let check_parse = make_check_parse!(String, SdpAttribute::Identity);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::Identity);

    check_parse_and_serialize("identity:eyJpZHAiOnsiZG9tYWluIjoiZXhhbXBsZS5vcmciLCJwcm90b2NvbCI6ImJvZ3VzIn0sImFzc2VydGlvbiI6IntcImlkZW50aXR5XCI6XCJib2JAZXhhbXBsZS5vcmdcIixcImNvbnRlbnRzXCI6XCJhYmNkZWZnaGlqa2xtbm9wcXJzdHV2d3l6XCIsXCJzaWduYXR1cmVcIjpcIjAxMDIwMzA0MDUwNlwifSJ9");

    assert!(parse_attribute("identity:").is_err());
}

#[test]
fn test_parse_attribute_imageattr() {
    let check_parse = make_check_parse!(SdpAttributeImageAttr, SdpAttribute::ImageAttr);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::ImageAttr);

    check_parse_and_serialize("imageattr:120 send * recv *");
    check_parse_and_serialize("imageattr:99 send [x=320,y=240] recv [x=320,y=240]");
    check_parse_and_serialize(
        "imageattr:97 send [x=800,y=640,sar=1.1,q=0.6] [x=480,y=320] recv [x=330,y=250]",
    );
    check_parse_and_serialize("imageattr:97 send [x=[480:16:800],y=[320:16:640],par=[1.2-1.3],q=0.6] [x=[176:8:208],y=[144:8:176],par=[1.2-1.3]] recv *");
    assert!(parse_attribute("imageattr:97 recv [x=800,y=640,sar=1.1] send [x=330,y=250]").is_ok());

    check_parse_and_serialize("imageattr:99 send [x=320,y=240]");
    assert!(parse_attribute("imageattr:100 recv [x=320,y=240]").is_ok());
    assert!(parse_attribute("imageattr:97 recv [x=800,y=640,sar=1.1,foo=[123,456],q=0.5] send [x=330,y=250,bar=foo,sar=[20-40]]").is_ok());
    assert!(parse_attribute("imageattr:97 recv [x=800,y=640,sar=1.1,foo=abc xyz,q=0.5] send [x=330,y=250,bar=foo,sar=[20-40]]").is_ok());

    assert!(parse_attribute("imageattr:").is_err());
    assert!(parse_attribute("imageattr:100").is_err());
    assert!(parse_attribute("imageattr:120 send * recv * send *").is_err());
    assert!(parse_attribute("imageattr:99 send [x=320]").is_err());
    assert!(parse_attribute("imageattr:99 recv [y=240]").is_err());
    assert!(parse_attribute("imageattr:99 send [x=320,y=240").is_err());
    assert!(parse_attribute("imageattr:99 send x=320,y=240]").is_err());
    assert!(parse_attribute("imageattr:97 send [x=800,y=640,sar=1.1] send [x=330,y=250]").is_err());
}

#[test]
fn test_parse_attribute_imageattr_recv_and_verify() {
    let check_parse = make_check_parse!(SdpAttributeImageAttr, SdpAttribute::ImageAttr);

    let imageattr = check_parse(
        "imageattr:* recv [x=800,y=[50,80,30],sar=1.1] send [x=330,y=250,sar=[1.1,1.3,1.9],q=0.1]",
    );
    assert_eq!(imageattr.pt, SdpAttributePayloadType::Wildcard);
    match imageattr.recv {
        SdpAttributeImageAttrSetList::Sets(sets) => {
            assert_eq!(sets.len(), 1);

            let set = &sets[0];
            assert_eq!(
                set.x,
                SdpAttributeImageAttrXyRange::DiscreteValues(vec![800])
            );
            assert_eq!(
                set.y,
                SdpAttributeImageAttrXyRange::DiscreteValues(vec![50, 80, 30])
            );
            assert_eq!(set.par, None);
            assert_eq!(
                set.sar,
                Some(SdpAttributeImageAttrSRange::DiscreteValues(vec![1.1]))
            );
            assert_eq!(set.q, None);
        }
        _ => {
            unreachable!();
        }
    }
    match imageattr.send {
        SdpAttributeImageAttrSetList::Sets(sets) => {
            assert_eq!(sets.len(), 1);

            let set = &sets[0];
            assert_eq!(
                set.x,
                SdpAttributeImageAttrXyRange::DiscreteValues(vec![330])
            );
            assert_eq!(
                set.y,
                SdpAttributeImageAttrXyRange::DiscreteValues(vec![250])
            );
            assert_eq!(set.par, None);
            assert_eq!(
                set.sar,
                Some(SdpAttributeImageAttrSRange::DiscreteValues(vec![
                    1.1, 1.3, 1.9,
                ]))
            );
            assert_eq!(set.q, Some(0.1));
        }
        _ => {
            unreachable!();
        }
    }
}

#[test]
fn test_parse_attribute_imageattr_send_and_verify() {
    let check_parse = make_check_parse!(SdpAttributeImageAttr, SdpAttribute::ImageAttr);

    let imageattr = check_parse(
        "imageattr:97 send [x=[480:16:800],y=[100,200,300],par=[1.2-1.3],q=0.6] [x=1080,y=[144:176],sar=[0.5-0.7]] recv *"
        );
    assert_eq!(imageattr.pt, SdpAttributePayloadType::PayloadType(97));
    match imageattr.send {
        SdpAttributeImageAttrSetList::Sets(sets) => {
            assert_eq!(sets.len(), 2);

            let first_set = &sets[0];
            assert_eq!(
                first_set.x,
                SdpAttributeImageAttrXyRange::Range(480, 800, Some(16))
            );
            assert_eq!(
                first_set.y,
                SdpAttributeImageAttrXyRange::DiscreteValues(vec![100, 200, 300])
            );
            assert_eq!(
                first_set.par,
                Some(SdpAttributeImageAttrPRange { min: 1.2, max: 1.3 })
            );
            assert_eq!(first_set.sar, None);
            assert_eq!(first_set.q, Some(0.6));

            let second_set = &sets[1];
            assert_eq!(
                second_set.x,
                SdpAttributeImageAttrXyRange::DiscreteValues(vec![1080])
            );
            assert_eq!(
                second_set.y,
                SdpAttributeImageAttrXyRange::Range(144, 176, None)
            );
            assert_eq!(second_set.par, None);
            assert_eq!(
                second_set.sar,
                Some(SdpAttributeImageAttrSRange::Range(0.5, 0.7))
            );
            assert_eq!(second_set.q, None);
        }
        _ => {
            unreachable!();
        }
    }
    assert_eq!(imageattr.recv, SdpAttributeImageAttrSetList::Wildcard);
}

#[test]
fn test_parse_attribute_inactive() {
    let check_parse = make_check_parse!(SdpAttribute::Inactive);
    let check_parse_and_serialize = make_check_parse_and_serialize!(check_parse);

    check_parse_and_serialize("inactive");
    assert!(parse_attribute("inactive foobar").is_err());
}

#[test]
fn test_parse_attribute_label() {
    let check_parse = make_check_parse!(String, SdpAttribute::Label);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::Label);

    check_parse_and_serialize("label:1");
    check_parse_and_serialize("label:foobar");
    check_parse_and_serialize("label:foobar barfoo");

    assert!(parse_attribute("label:").is_err());
}

#[test]
fn test_parse_attribute_maxptime() {
    let check_parse = make_check_parse!(u64, SdpAttribute::MaxPtime);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::MaxPtime);

    check_parse_and_serialize("maxptime:60");

    assert!(parse_attribute("maxptime:").is_err());
    assert!(parse_attribute("maxptime:60 100").is_err());
    assert!(parse_attribute("maxptime:foobar").is_err());
}

#[test]
fn test_parse_attribute_mid() {
    let check_parse = make_check_parse!(String, SdpAttribute::Mid);
    let check_parse_and_serialize = make_check_parse_and_serialize!(check_parse, SdpAttribute::Mid);

    check_parse_and_serialize("mid:sdparta_0");
    check_parse_and_serialize("mid:sdparta_0 sdparta_1 sdparta_2");

    assert!(parse_attribute("mid:").is_err());
}

#[test]
fn test_parse_attribute_msid() {
    let check_parse = make_check_parse!(SdpAttributeMsid, SdpAttribute::Msid);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::Msid);

    check_parse_and_serialize("msid:{5a990edd-0568-ac40-8d97-310fc33f3411}");
    check_parse_and_serialize(
        "msid:{5a990edd-0568-ac40-8d97-310fc33f3411} {218cfa1c-617d-2249-9997-60929ce4c405}",
    );

    assert!(parse_attribute("msid:").is_err());
}

#[test]
fn test_parse_attribute_msid_semantics() {
    let check_parse = make_check_parse!(SdpAttributeMsidSemantic, SdpAttribute::MsidSemantic);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::MsidSemantic);

    check_parse_and_serialize("msid-semantic:WMS *");
    check_parse_and_serialize("msid-semantic:WMS foo");

    assert!(parse_attribute("msid-semantic:").is_err());
}

#[test]
fn test_parse_attribute_ptime() {
    let check_parse = make_check_parse!(u64, SdpAttribute::Ptime);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::Ptime);

    check_parse_and_serialize("ptime:30");
    assert!(parse_attribute("ptime:").is_err());
}

#[test]
fn test_anonymize_remote_candidate() -> Result<(), SdpParserInternalError> {
    let mut anon = StatefulSdpAnonymizer::new();
    if let SdpType::Attribute(SdpAttribute::RemoteCandidate(remote)) =
        parse_attribute("remote-candidates:0 10.0.0.1 5555")?
    {
        let masked = remote.masked_clone(&mut anon);
        assert_eq!(masked.address, Address::Ip(IpAddr::V4(Ipv4Addr::from(1))));
        assert_eq!(masked.port, 1);
    } else {
        unreachable!();
    }
    Ok(())
}

#[test]
fn test_parse_attribute_rid() {
    let check_parse = make_check_parse!(SdpAttributeRid, SdpAttribute::Rid);
    let check_parse_and_serialize = make_check_parse_and_serialize!(check_parse, SdpAttribute::Rid);

    check_parse_and_serialize("rid:foo send pt=10");
    check_parse_and_serialize("rid:110 send pt=9,10");
    check_parse_and_serialize("rid:110 send pt=9,10;max-fs=10");
    check_parse_and_serialize("rid:110 send pt=9,10;max-width=10;depends=1,2,3");

    assert!(
        parse_attribute("rid:110 send pt=9, 10;max-fs=10;UNKNOWN=100; depends=1, 2, 3").is_ok()
    );
    assert!(parse_attribute("rid:110 send max-fs=10").is_ok());
    assert!(parse_attribute("rid:110 recv max-width=1920;max-height=1080").is_ok());

    check_parse_and_serialize("rid:110 recv max-mbps=420;max-cpb=3;max-dpb=3");
    check_parse_and_serialize("rid:110 recv scale-down-by=1.35;depends=1,2,3");
    check_parse_and_serialize("rid:110 recv max-width=10;depends=1,2,3");
    check_parse_and_serialize("rid:110 recv max-fs=10;UNKNOWN=100;depends=1,2,3");

    assert!(parse_attribute("rid:").is_err());
    assert!(parse_attribute("rid:120 send pt=").is_err());
    assert!(parse_attribute("rid:120 send pt=;max-width=10").is_err());
    assert!(parse_attribute("rid:120 send pt=9;max-width=").is_err());
    assert!(parse_attribute("rid:120 send pt=9;max-width=;max-width=10").is_err());
}

#[test]
fn test_parse_attribute_rid_and_verify() {
    let check_parse = make_check_parse!(SdpAttributeRid, SdpAttribute::Rid);
    let check_parse_and_serialize = make_check_parse_and_serialize!(check_parse, SdpAttribute::Rid);

    check_parse_and_serialize("rid:foo send");
    let mut rid = check_parse("rid:foo send");
    assert_eq!(rid.id, "foo");
    assert_eq!(rid.direction, SdpSingleDirection::Send);

    check_parse_and_serialize("rid:110 send pt=9");
    rid = check_parse("rid:110 send pt=9");
    assert_eq!(rid.id, "110");
    assert_eq!(rid.direction, SdpSingleDirection::Send);
    assert_eq!(rid.formats, vec![9]);

    check_parse_and_serialize("rid:110 send pt=9,10;max-fs=10;UNKNOWN=100;depends=1,2,3");
    rid = check_parse("rid:110 send pt=9,10;max-fs=10;UNKNOWN=100;depends=1,2,3");
    assert_eq!(rid.id, "110");
    assert_eq!(rid.direction, SdpSingleDirection::Send);
    assert_eq!(rid.formats, vec![9, 10]);
    assert_eq!(rid.params.max_fs, 10);
    assert_eq!(rid.params.unknown, vec!["UNKNOWN=100"]);
    assert_eq!(rid.depends, vec!["1", "2", "3"]);

    check_parse_and_serialize("rid:110 recv max-fps=42;max-fs=10;max-br=3;max-pps=1000");
    rid = check_parse("rid:110 recv max-fps=42;max-fs=10;max-br=3;max-pps=1000");
    assert_eq!(rid.id, "110");
    assert_eq!(rid.direction, SdpSingleDirection::Recv);
    assert_eq!(rid.params.max_fps, 42);
    assert_eq!(rid.params.max_fs, 10);
    assert_eq!(rid.params.max_br, 3);
    assert_eq!(rid.params.max_pps, 1000);
}

#[test]
fn test_parse_attribute_recvonly() {
    let check_parse = make_check_parse!(SdpAttribute::Recvonly);
    let check_parse_and_serialize = make_check_parse_and_serialize!(check_parse);

    check_parse_and_serialize("recvonly");
    assert!(parse_attribute("recvonly foobar").is_err());
}

#[test]
fn test_parse_attribute_remote_candidate() {
    let check_parse = make_check_parse!(SdpAttributeRemoteCandidate, SdpAttribute::RemoteCandidate);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::RemoteCandidate);

    check_parse_and_serialize("remote-candidates:0 10.0.0.1 5555");
    check_parse_and_serialize("remote-candidates:12345 ::1 5555");

    assert!(parse_attribute("remote-candidates:abc 10.0.0.1 5555").is_err());
    assert!(parse_attribute("remote-candidates:0 10.0.0.1 70000").is_err());
    assert!(parse_attribute("remote-candidates:0 10.0.0.1").is_err());
    assert!(parse_attribute("remote-candidates:0").is_err());
    assert!(parse_attribute("remote-candidates:").is_err());
}

#[test]
fn test_parse_attribute_sendonly() {
    let check_parse = make_check_parse!(SdpAttribute::Sendonly);
    let check_parse_and_serialize = make_check_parse_and_serialize!(check_parse);

    check_parse_and_serialize("sendonly");
    assert!(parse_attribute("sendonly foobar").is_err());
}

#[test]
fn test_parse_attribute_sendrecv() {
    let check_parse = make_check_parse!(SdpAttribute::Sendrecv);
    let check_parse_and_serialize = make_check_parse_and_serialize!(check_parse);

    check_parse_and_serialize("sendrecv");
    assert!(parse_attribute("sendrecv foobar").is_err());
}

#[test]
fn test_parse_attribute_setup() {
    let check_parse = make_check_parse!(SdpAttributeSetup, SdpAttribute::Setup);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::Setup);

    check_parse_and_serialize("setup:active");
    check_parse_and_serialize("setup:passive");
    check_parse_and_serialize("setup:actpass");
    check_parse_and_serialize("setup:holdconn");

    assert!(parse_attribute("setup:").is_err());
    assert!(parse_attribute("setup:foobar").is_err());
}

#[test]
fn test_parse_attribute_rtcp() {
    let check_parse = make_check_parse!(SdpAttributeRtcp, SdpAttribute::Rtcp);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::Rtcp);

    check_parse_and_serialize("rtcp:5000");
    check_parse_and_serialize("rtcp:9 IN IP4 0.0.0.0");
    check_parse_and_serialize("rtcp:9 IN IP6 2001:db8::1");

    assert!(parse_attribute("rtcp:").is_err());
    assert!(parse_attribute("rtcp:70000").is_err());
    assert!(parse_attribute("rtcp:9 IN").is_err());
    assert!(parse_attribute("rtcp:9 IN IP4").is_err());
    assert!(parse_attribute("rtcp:9 IN IP4 ::1").is_err());
}

#[test]
fn test_parse_attribute_rtcp_fb() {
    let check_parse = make_check_parse!(SdpAttributeRtcpFb, SdpAttribute::Rtcpfb);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::Rtcpfb);

    check_parse_and_serialize("rtcp-fb:101 ack rpsi");
    check_parse_and_serialize("rtcp-fb:101 ack app");
    check_parse_and_serialize("rtcp-fb:101 ccm");
    check_parse_and_serialize("rtcp-fb:101 ccm fir");
    check_parse_and_serialize("rtcp-fb:101 ccm tmmbr");
    check_parse_and_serialize("rtcp-fb:101 ccm tstr");
    check_parse_and_serialize("rtcp-fb:101 ccm vbcm");
    check_parse_and_serialize("rtcp-fb:101 nack");
    check_parse_and_serialize("rtcp-fb:101 nack sli");
    check_parse_and_serialize("rtcp-fb:101 nack pli");
    check_parse_and_serialize("rtcp-fb:101 nack rpsi");
    check_parse_and_serialize("rtcp-fb:101 nack app");
    check_parse_and_serialize("rtcp-fb:101 trr-int 1");
    check_parse_and_serialize("rtcp-fb:101 goog-remb");
    check_parse_and_serialize("rtcp-fb:101 transport-cc");

    assert!(parse_attribute("rtcp-fb:101 unknown").is_err());
    assert!(parse_attribute("rtcp-fb:101 ack").is_err());
    assert!(parse_attribute("rtcp-fb:101 ccm unknwon").is_err());
    assert!(parse_attribute("rtcp-fb:101 nack unknown").is_err());
    assert!(parse_attribute("rtcp-fb:101 trr-int").is_err());
    assert!(parse_attribute("rtcp-fb:101 trr-int a").is_err());
    assert!(parse_attribute("rtcp-fb:101 goog-remb unknown").is_err());
    assert!(parse_attribute("rtcp-fb:101 transport-cc unknown").is_err());
}

#[test]
fn test_parse_attribute_rtcp_mux() {
    let check_parse = make_check_parse!(SdpAttribute::RtcpMux);
    let check_parse_and_serialize = make_check_parse_and_serialize!(check_parse);

    check_parse_and_serialize("rtcp-mux");
    assert!(parse_attribute("rtcp-mux foobar").is_err());
}

#[test]
fn test_parse_attribute_rtcp_mux_only() {
    let check_parse = make_check_parse!(SdpAttribute::RtcpMuxOnly);
    let check_parse_and_serialize = make_check_parse_and_serialize!(check_parse);

    check_parse_and_serialize("rtcp-mux-only");
    assert!(parse_attribute("rtcp-mux-only bar").is_err());
}

#[test]
fn test_parse_attribute_rtcp_rsize() {
    let check_parse = make_check_parse!(SdpAttribute::RtcpRsize);
    let check_parse_and_serialize = make_check_parse_and_serialize!(check_parse);

    check_parse_and_serialize("rtcp-rsize");
    assert!(parse_attribute("rtcp-rsize foobar").is_err());
}

#[test]
fn test_parse_attribute_rtpmap() {
    let check_parse = make_check_parse!(SdpAttributeRtpmap, SdpAttribute::Rtpmap);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::Rtpmap);

    check_parse_and_serialize("rtpmap:109 opus/48000");
    check_parse_and_serialize("rtpmap:109 opus/48000/2");

    assert!(parse_attribute("rtpmap: ").is_err());
    assert!(parse_attribute("rtpmap:109 ").is_err());
    assert!(parse_attribute("rtpmap:109 opus").is_err());
    assert!(parse_attribute("rtpmap:128 opus/48000").is_err());
}

#[test]
fn test_parse_attribute_sctpmap() {
    let check_parse = make_check_parse!(SdpAttributeSctpmap, SdpAttribute::Sctpmap);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::Sctpmap);

    check_parse_and_serialize("sctpmap:5000 webrtc-datachannel 256");

    assert!(parse_attribute("sctpmap:70000 webrtc-datachannel").is_err());
    assert!(parse_attribute("sctpmap:70000 webrtc-datachannel 256").is_err());
    assert!(parse_attribute("sctpmap:5000 unsupported 256").is_err());
    assert!(parse_attribute("sctpmap:5000 webrtc-datachannel 2a").is_err());
}

#[test]
fn test_parse_attribute_sctp_port() {
    let check_parse = make_check_parse!(u64, SdpAttribute::SctpPort);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::SctpPort);

    check_parse_and_serialize("sctp-port:5000");

    assert!(parse_attribute("sctp-port:").is_err());
    assert!(parse_attribute("sctp-port:70000").is_err());
}

#[test]
fn test_parse_attribute_max_message_size() {
    let check_parse = make_check_parse!(u64, SdpAttribute::MaxMessageSize);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::MaxMessageSize);

    check_parse_and_serialize("max-message-size:1");
    check_parse_and_serialize("max-message-size:100000");
    check_parse_and_serialize("max-message-size:4294967297");
    check_parse_and_serialize("max-message-size:0");

    assert!(parse_attribute("max-message-size:").is_err());
    assert!(parse_attribute("max-message-size:abc").is_err());
}

#[test]
fn test_parse_attribute_simulcast() {
    let check_parse = make_check_parse!(SdpAttributeSimulcast, SdpAttribute::Simulcast);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::Simulcast);

    check_parse_and_serialize("simulcast:send 1");
    check_parse_and_serialize("simulcast:recv test");
    check_parse_and_serialize("simulcast:recv ~test");
    check_parse_and_serialize("simulcast:recv test;foo");
    check_parse_and_serialize("simulcast:recv foo,bar");
    check_parse_and_serialize("simulcast:recv foo,bar;test");
    check_parse_and_serialize("simulcast:send 1;4,5 recv 6;7");
    check_parse_and_serialize("simulcast:send 1,2,3;~4,~5 recv 6;~7,~8");
    // old draft 03 notation used by Firefox 55
    assert!(parse_attribute("simulcast: send rid=foo;bar").is_ok());

    assert!(parse_attribute("simulcast:").is_err());
    assert!(parse_attribute("simulcast:send").is_err());
    assert!(parse_attribute("simulcast:foobar 1").is_err());
    assert!(parse_attribute("simulcast:send 1 foobar 2").is_err());
    // old draft 03 notation used by Firefox 55
    assert!(parse_attribute("simulcast: send foo=8;10").is_err());
}

#[test]
fn test_parse_attribute_ssrc() {
    let check_parse = make_check_parse!(SdpAttributeSsrc, SdpAttribute::Ssrc);
    let check_parse_and_serialize =
        make_check_parse_and_serialize!(check_parse, SdpAttribute::Ssrc);

    check_parse_and_serialize("ssrc:2655508255");
    check_parse_and_serialize("ssrc:2655508255 foo");
    check_parse_and_serialize("ssrc:2655508255 cname:{735484ea-4f6c-f74a-bd66-7425f8476c2e}");
    check_parse_and_serialize("ssrc:2082260239 msid:1d0cdb4e-5934-4f0f-9f88-40392cb60d31 315b086a-5cb6-4221-89de-caf0b038c79d");

    assert!(parse_attribute("ssrc:").is_err());
    assert!(parse_attribute("ssrc:foo").is_err());
}

#[test]
fn test_anonymize_attribute_ssrc() -> Result<(), SdpParserInternalError> {
    let mut anon = StatefulSdpAnonymizer::new();
    let parsed = parse_attribute("ssrc:2655508255 cname:{735484ea-4f6c-f74a-bd66-7425f8476c2e}")?;
    let (ssrc1, masked) = if let SdpType::Attribute(a) = parsed {
        let masked = a.masked_clone(&mut anon);
        match (a, masked) {
            (SdpAttribute::Ssrc(ssrc), SdpAttribute::Ssrc(masked)) => (ssrc, masked),
            (_, _) => unreachable!(),
        }
    } else {
        unreachable!()
    };
    assert_eq!(ssrc1.id, masked.id);
    assert_eq!(ssrc1.attribute, masked.attribute);
    assert_eq!("cname-00000001", masked.value.unwrap());

    let ssrc2 = parse_attribute("ssrc:2082260239 msid:1d0cdb4e-5934-4f0f-9f88-40392cb60d31 315b086a-5cb6-4221-89de-caf0b038c79d")?;
    if let SdpType::Attribute(SdpAttribute::Ssrc(ssrc2)) = ssrc2 {
        let masked = ssrc2.masked_clone(&mut anon);
        assert_eq!(ssrc2.id, masked.id);
        assert_eq!(ssrc2.attribute, masked.attribute);
        assert_eq!(ssrc2.value, masked.value);
    } else {
        unreachable!()
    }
    Ok(())
}

#[test]
fn test_parse_attribute_ssrc_group() {
    let parsed = parse_attribute("ssrc-group:FID 3156517279 2673335628");
    match parsed {
        Ok(SdpType::Attribute(attr)) => {
            assert_eq!(attr.to_string(), "ssrc-group:FID 3156517279 2673335628");
            let (semantic, ssrcs) = match attr {
                SdpAttribute::SsrcGroup(semantic, ssrcs) => {
                    let stringified_ssrcs: Vec<String> =
                        ssrcs.iter().map(|ssrc| ssrc.to_string()).collect();
                    (semantic.to_string(), stringified_ssrcs)
                }
                _ => unreachable!(),
            };
            assert_eq!(semantic, "FID");
            assert_eq!(ssrcs.len(), 2);
            assert_eq!(ssrcs[0], "3156517279");
            assert_eq!(ssrcs[1], "2673335628");
        }
        Err(e) => panic!("{}", e),
        _ => unreachable!(),
    }

    assert!(parse_attribute("ssrc-group:").is_err());
    assert!(parse_attribute("ssrc-group:BLAH").is_err());
    assert!(parse_attribute("ssrc-group:FID").is_err());
}

#[test]
fn test_parse_unknown_attribute() {
    assert!(parse_attribute("unknown").is_err())
}
