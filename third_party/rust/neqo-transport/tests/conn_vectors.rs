// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Tests with the test vectors from the spec.
#![deny(clippy::pedantic)]

use neqo_common::{Datagram, Encoder};
use neqo_transport::{Connection, FixedConnectionIdManager, QuicVersion, State};
use test_fixture::{self, loopback, now};

use std::cell::RefCell;
use std::rc::Rc;

const INITIAL_PACKET_27: &str = "c0ff00001b088394c8f03e5157080000\
                              449e3b343aa8535064a4268a0d9d7b1c\
                              9d250ae355162276e9b1e3011ef6bbc0\
                              ab48ad5bcc2681e953857ca62becd752\
                              4daac473e68d7405fbba4e9ee616c870\
                              38bdbe908c06d9605d9ac49030359eec\
                              b1d05a14e117db8cede2bb09d0dbbfee\
                              271cb374d8f10abec82d0f59a1dee29f\
                              e95638ed8dd41da07487468791b719c5\
                              5c46968eb3b54680037102a28e53dc1d\
                              12903db0af5821794b41c4a93357fa59\
                              ce69cfe7f6bdfa629eef78616447e1d6\
                              11c4baf71bf33febcb03137c2c75d253\
                              17d3e13b684370f668411c0f00304b50\
                              1c8fd422bd9b9ad81d643b20da89ca05\
                              25d24d2b142041cae0af205092e43008\
                              0cd8559ea4c5c6e4fa3f66082b7d303e\
                              52ce0162baa958532b0bbc2bc785681f\
                              cf37485dff6595e01e739c8ac9efba31\
                              b985d5f656cc092432d781db95221724\
                              87641c4d3ab8ece01e39bc85b1543661\
                              4775a98ba8fa12d46f9b35e2a55eb72d\
                              7f85181a366663387ddc20551807e007\
                              673bd7e26bf9b29b5ab10a1ca87cbb7a\
                              d97e99eb66959c2a9bc3cbde4707ff77\
                              20b110fa95354674e395812e47a0ae53\
                              b464dcb2d1f345df360dc227270c7506\
                              76f6724eb479f0d2fbb6124429990457\
                              ac6c9167f40aab739998f38b9eccb24f\
                              d47c8410131bf65a52af841275d5b3d1\
                              880b197df2b5dea3e6de56ebce3ffb6e\
                              9277a82082f8d9677a6767089b671ebd\
                              244c214f0bde95c2beb02cd1172d58bd\
                              f39dce56ff68eb35ab39b49b4eac7c81\
                              5ea60451d6e6ab82119118df02a58684\
                              4a9ffe162ba006d0669ef57668cab38b\
                              62f71a2523a084852cd1d079b3658dc2\
                              f3e87949b550bab3e177cfc49ed190df\
                              f0630e43077c30de8f6ae081537f1e83\
                              da537da980afa668e7b7fb25301cf741\
                              524be3c49884b42821f17552fbd1931a\
                              813017b6b6590a41ea18b6ba49cd48a4\
                              40bd9a3346a7623fb4ba34a3ee571e3c\
                              731f35a7a3cf25b551a680fa68763507\
                              b7fde3aaf023c50b9d22da6876ba337e\
                              b5e9dd9ec3daf970242b6c5aab3aa4b2\
                              96ad8b9f6832f686ef70fa938b31b4e5\
                              ddd7364442d3ea72e73d668fb0937796\
                              f462923a81a47e1cee7426ff6d922126\
                              9b5a62ec03d6ec94d12606cb485560ba\
                              b574816009e96504249385bb61a819be\
                              04f62c2066214d8360a2022beb316240\
                              b6c7d78bbe56c13082e0ca272661210a\
                              bf020bf3b5783f1426436cf9ff418405\
                              93a5d0638d32fc51c5c65ff291a3a7a5\
                              2fd6775e623a4439cc08dd25582febc9\
                              44ef92d8dbd329c91de3e9c9582e41f1\
                              7f3d186f104ad3f90995116c682a2a14\
                              a3b4b1f547c335f0be710fc9fc03e0e5\
                              87b8cda31ce65b969878a4ad4283e6d5\
                              b0373f43da86e9e0ffe1ae0fddd35162\
                              55bd74566f36a38703d5f34249ded1f6\
                              6b3d9b45b9af2ccfefe984e13376b1b2\
                              c6404aa48c8026132343da3f3a33659e\
                              c1b3e95080540b28b7f3fcd35fa5d843\
                              b579a84c089121a60d8c1754915c344e\
                              eaf45a9bf27dc0c1e784161691220913\
                              13eb0e87555abd706626e557fc36a04f\
                              cd191a58829104d6075c5594f627ca50\
                              6bf181daec940f4a4f3af0074eee89da\
                              acde6758312622d4fa675b39f728e062\
                              d2bee680d8f41a597c262648bb18bcfc\
                              13c8b3d97b1a77b2ac3af745d61a34cc\
                              4709865bac824a94bb19058015e4e42d\
                              38d3b779d72edc00c5cd088eff802b05";

const INITIAL_PACKET_29: &str = "cdff00001d088394c8f03e5157080000\
                              449e9cdb990bfb66bc6a93032b50dd89\
                              73972d149421874d3849e3708d71354e\
                              a33bcdc356f3ea6e2a1a1bd7c3d14003\
                              8d3e784d04c30a2cdb40c32523aba2da\
                              fe1c1bf3d27a6be38fe38ae033fbb071\
                              3c1c73661bb6639795b42b97f77068ea\
                              d51f11fbf9489af2501d09481e6c64d4\
                              b8551cd3cea70d830ce2aeeec789ef55\
                              1a7fbe36b3f7e1549a9f8d8e153b3fac\
                              3fb7b7812c9ed7c20b4be190ebd89956\
                              26e7f0fc887925ec6f0606c5d36aa81b\
                              ebb7aacdc4a31bb5f23d55faef5c5190\
                              5783384f375a43235b5c742c78ab1bae\
                              0a188b75efbde6b3774ed61282f9670a\
                              9dea19e1566103ce675ab4e21081fb58\
                              60340a1e88e4f10e39eae25cd685b109\
                              29636d4f02e7fad2a5a458249f5c0298\
                              a6d53acbe41a7fc83fa7cc01973f7a74\
                              d1237a51974e097636b6203997f921d0\
                              7bc1940a6f2d0de9f5a11432946159ed\
                              6cc21df65c4ddd1115f86427259a196c\
                              7148b25b6478b0dc7766e1c4d1b1f515\
                              9f90eabc61636226244642ee148b464c\
                              9e619ee50a5e3ddc836227cad938987c\
                              4ea3c1fa7c75bbf88d89e9ada642b2b8\
                              8fe8107b7ea375b1b64889a4e9e5c38a\
                              1c896ce275a5658d250e2d76e1ed3a34\
                              ce7e3a3f383d0c996d0bed106c2899ca\
                              6fc263ef0455e74bb6ac1640ea7bfedc\
                              59f03fee0e1725ea150ff4d69a7660c5\
                              542119c71de270ae7c3ecfd1af2c4ce5\
                              51986949cc34a66b3e216bfe18b347e6\
                              c05fd050f85912db303a8f054ec23e38\
                              f44d1c725ab641ae929fecc8e3cefa56\
                              19df4231f5b4c009fa0c0bbc60bc75f7\
                              6d06ef154fc8577077d9d6a1d2bd9bf0\
                              81dc783ece60111bea7da9e5a9748069\
                              d078b2bef48de04cabe3755b197d52b3\
                              2046949ecaa310274b4aac0d008b1948\
                              c1082cdfe2083e386d4fd84c0ed0666d\
                              3ee26c4515c4fee73433ac703b690a9f\
                              7bf278a77486ace44c489a0c7ac8dfe4\
                              d1a58fb3a730b993ff0f0d61b4d89557\
                              831eb4c752ffd39c10f6b9f46d8db278\
                              da624fd800e4af85548a294c1518893a\
                              8778c4f6d6d73c93df200960104e062b\
                              388ea97dcf4016bced7f62b4f062cb6c\
                              04c20693d9a0e3b74ba8fe74cc012378\
                              84f40d765ae56a51688d985cf0ceaef4\
                              3045ed8c3f0c33bced08537f6882613a\
                              cd3b08d665fce9dd8aa73171e2d3771a\
                              61dba2790e491d413d93d987e2745af2\
                              9418e428be34941485c93447520ffe23\
                              1da2304d6a0fd5d07d08372202369661\
                              59bef3cf904d722324dd852513df39ae\
                              030d8173908da6364786d3c1bfcb19ea\
                              77a63b25f1e7fc661def480c5d00d444\
                              56269ebd84efd8e3a8b2c257eec76060\
                              682848cbf5194bc99e49ee75e4d0d254\
                              bad4bfd74970c30e44b65511d4ad0e6e\
                              c7398e08e01307eeeea14e46ccd87cf3\
                              6b285221254d8fc6a6765c524ded0085\
                              dca5bd688ddf722e2c0faf9d0fb2ce7a\
                              0c3f2cee19ca0ffba461ca8dc5d2c817\
                              8b0762cf67135558494d2a96f1a139f0\
                              edb42d2af89a9c9122b07acbc29e5e72\
                              2df8615c343702491098478a389c9872\
                              a10b0c9875125e257c7bfdf27eef4060\
                              bd3d00f4c14fd3e3496c38d3c5d1a566\
                              8c39350effbc2d16ca17be4ce29f02ed\
                              969504dda2a8c6b9ff919e693ee79e09\
                              089316e7d1d89ec099db3b2b268725d8\
                              88536a4b8bf9aee8fb43e82a4d919d48\
                              1802771a449b30f3fa2289852607b660";

fn make_server(quic_version: QuicVersion) -> Connection {
    test_fixture::fixture_init();
    Connection::new_server(
        test_fixture::DEFAULT_KEYS,
        test_fixture::DEFAULT_ALPN,
        Rc::new(RefCell::new(FixedConnectionIdManager::new(5))),
        quic_version,
    )
    .expect("create a default server")
}

fn process_client_initial(quic_version: QuicVersion, packet: &str) {
    let mut server = make_server(quic_version);

    let pkt: Vec<u8> = Encoder::from_hex(packet).into();
    let dgram = Datagram::new(loopback(), loopback(), pkt);
    assert_eq!(*server.state(), State::Init);
    let out = server.process(Some(dgram), now());
    assert_eq!(*server.state(), State::Handshaking);
    assert!(out.dgram().is_some());
}

#[test]
fn process_client_initial_27() {
    process_client_initial(QuicVersion::Draft27, &INITIAL_PACKET_27);
}

#[test]
fn process_client_initial_29() {
    process_client_initial(QuicVersion::Draft29, &INITIAL_PACKET_29);
}
