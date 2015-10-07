var Cc = Components.classes;
var Ci = Components.interfaces;

// Ensure PSM is initialized.
Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);

function do_check_throws(func) {
  let have_error = false;
  try {
    func();
  } catch(ex) {
    dump("Was expecting an exception. Caught: " + ex + "\n");
    have_error = true;
  }
  do_check_true(have_error);
}

function test_success() {
  let a = Cc["@mozilla.org/services-crypto/sync-jpake;1"]
            .createInstance(Ci.nsISyncJPAKE);
  let b = Cc["@mozilla.org/services-crypto/sync-jpake;1"]
            .createInstance(Ci.nsISyncJPAKE);

  let a_gx1 = {};
  let a_gv1 = {};
  let a_r1 = {};
  let a_gx2 = {};
  let a_gv2 = {};
  let a_r2 = {};

  let b_gx1 = {};
  let b_gv1 = {};
  let b_r1 = {};
  let b_gx2 = {};
  let b_gv2 = {};
  let b_r2 = {};

  a.round1("alice", a_gx1, a_gv1, a_r1, a_gx2, a_gv2, a_r2);
  b.round1("bob", b_gx1, b_gv1, b_r1, b_gx2, b_gv2, b_r2);

  let a_A = {};
  let a_gva = {};
  let a_ra = {};

  let b_A = {};
  let b_gva = {};
  let b_ra = {};

  a.round2("bob", "sekrit", b_gx1.value, b_gv1.value, b_r1.value,
           b_gx2.value, b_gv2.value, b_r2.value, a_A, a_gva, a_ra);
  b.round2("alice", "sekrit", a_gx1.value, a_gv1.value, a_r1.value,
           a_gx2.value, a_gv2.value, a_r2.value, b_A, b_gva, b_ra);

  let a_aes = {};
  let a_hmac = {};
  let b_aes = {};
  let b_hmac = {};

  a.final(b_A.value, b_gva.value, b_ra.value, "ohai", a_aes, a_hmac);
  b.final(a_A.value, a_gva.value, a_ra.value, "ohai", b_aes, b_hmac);

  do_check_eq(a_aes.value, b_aes.value);
  do_check_eq(a_hmac.value, b_hmac.value);
}

function test_failure(modlen) {
  let a = Cc["@mozilla.org/services-crypto/sync-jpake;1"]
            .createInstance(Ci.nsISyncJPAKE);
  let b = Cc["@mozilla.org/services-crypto/sync-jpake;1"]
            .createInstance(Ci.nsISyncJPAKE);

  let a_gx1 = {};
  let a_gv1 = {};
  let a_r1 = {};
  let a_gx2 = {};
  let a_gv2 = {};
  let a_r2 = {};

  let b_gx1 = {};
  let b_gv1 = {};
  let b_r1 = {};
  let b_gx2 = {};
  let b_gv2 = {};
  let b_r2 = {};

  a.round1("alice", a_gx1, a_gv1, a_r1, a_gx2, a_gv2, a_r2);
  b.round1("bob", b_gx1, b_gv1, b_r1, b_gx2, b_gv2, b_r2);

  let a_A = {};
  let a_gva = {};
  let a_ra = {};

  let b_A = {};
  let b_gva = {};
  let b_ra = {};

  // Note how the PINs are different (secret vs. sekrit)
  a.round2("bob", "secret", b_gx1.value, b_gv1.value, b_r1.value,
           b_gx2.value, b_gv2.value, b_r2.value, a_A, a_gva, a_ra);
  b.round2("alice", "sekrit", a_gx1.value, a_gv1.value, a_r1.value,
           a_gx2.value, a_gv2.value, a_r2.value, b_A, b_gva, b_ra);

  let a_aes = {};
  let a_hmac = {};
  let b_aes = {};
  let b_hmac = {};

  a.final(b_A.value, b_gva.value, b_ra.value, "ohai", a_aes, a_hmac);
  b.final(a_A.value, a_gva.value, a_ra.value, "ohai", b_aes, b_hmac);

  do_check_neq(a_aes.value, b_aes.value);
  do_check_neq(a_hmac.value, b_hmac.value);
}

function test_same_signerids() {
  let a = Cc["@mozilla.org/services-crypto/sync-jpake;1"]
            .createInstance(Ci.nsISyncJPAKE);
  let b = Cc["@mozilla.org/services-crypto/sync-jpake;1"]
            .createInstance(Ci.nsISyncJPAKE);

  let gx1 = {};
  let gv1 = {};
  let r1 = {};
  let gx2 = {};
  let gv2 = {};
  let r2 = {};

  a.round1("alice", {}, {}, {}, {}, {}, {});
  b.round1("alice", gx1, gv1, r1, gx2, gv2, r2);
  do_check_throws(function() {
    a.round2("alice", "sekrit", gx1.value, gv1.value, r1.value,
             gx2.value, gv2.value, r2.value, {}, {}, {});
  });
}

function test_bad_zkp() {
  let a = Cc["@mozilla.org/services-crypto/sync-jpake;1"]
            .createInstance(Ci.nsISyncJPAKE);
  let b = Cc["@mozilla.org/services-crypto/sync-jpake;1"]
            .createInstance(Ci.nsISyncJPAKE);

  let gx1 = {};
  let gv1 = {};
  let r1 = {};
  let gx2 = {};
  let gv2 = {};
  let r2 = {};

  a.round1("alice", {}, {}, {}, {}, {}, {});
  b.round1("bob", gx1, gv1, r1, gx2, gv2, r2);
  do_check_throws(function() {
    a.round2("invalid", "sekrit", gx1.value, gv1.value, r1.value,
             gx2.value, gv2.value, r2.value, {}, {}, {});
  });
}

function test_x4_zero() {
  // The PKCS#11 API for J-PAKE does not allow us to choose any of the nonces.
  // In order to test the defence against x4 (mod p) == 1, we had to generate
  // our own signed nonces using a the FreeBL JPAKE_Sign function directly.
  // To verify the signatures are accurate, pass the given value of R as the
  // "testRandom" parameter to FreeBL's JPAKE_Sign, along with the given values
  // for X and GX, using signerID "alice". Then verify that each GV returned
  // from JPAKE_Sign matches the value specified here.
  let test = function(badGX, badX_GV, badX_R) {
    let a = Cc["@mozilla.org/services-crypto/sync-jpake;1"]
            .createInstance(Ci.nsISyncJPAKE);
    let b = Cc["@mozilla.org/services-crypto/sync-jpake;1"]
            .createInstance(Ci.nsISyncJPAKE);

    let a_gx1 = {};
    let a_gv1 = {};
    let a_r1 = {};
    let a_gx2 = {};
    let a_gv2 = {};
    let a_r2 = {};

    let b_gx1 = {};
    let b_gv1 = {};
    let b_r1 = {};
    let b_gx2 = {};
    let b_gv2 = {};
    let b_r2 = {};

    a.round1("alice", a_gx1, a_gv1, a_r1, a_gx2, a_gv2, a_r2);
    b.round1("bob", b_gx1, b_gv1, b_r1, b_gx2, b_gv2, b_r2);

    // Replace the g^x2 generated by A with the given illegal value.
    a_gx2.value = badGX;
    a_gv2.value = badX_GV;
    a_r2.value = badX_R;

    let b_A = {};
    let b_gva = {};
    let b_ra = {};

    do_check_throws(function() {
        b.round2("alice", "secret", a_gx1.value, a_gv1.value, a_r1.value,
                    a_gx2.value, a_gv2.value, a_r2.value, b_A, b_gva, b_ra);
    });
  };
  
  // g^x is NIST 3072's p + 1, (p + 1) mod p == 1, x == 0
  test("90066455B5CFC38F9CAA4A48B4281F292C260FEEF01FD61037E56258A7795A1C"
         + "7AD46076982CE6BB956936C6AB4DCFE05E6784586940CA544B9B2140E1EB523F"
         + "009D20A7E7880E4E5BFA690F1B9004A27811CD9904AF70420EEFD6EA11EF7DA1"
         + "29F58835FF56B89FAA637BC9AC2EFAAB903402229F491D8D3485261CD068699B"
         + "6BA58A1DDBBEF6DB51E8FE34E8A78E542D7BA351C21EA8D8F1D29F5D5D159394"
         + "87E27F4416B0CA632C59EFD1B1EB66511A5A0FBF615B766C5862D0BD8A3FE7A0"
         + "E0DA0FB2FE1FCB19E8F9996A8EA0FCCDE538175238FC8B0EE6F29AF7F642773E"
         + "BE8CD5402415A01451A840476B2FCEB0E388D30D4B376C37FE401C2A2C2F941D"
         + "AD179C540C1C8CE030D460C4D983BE9AB0B20F69144C1AE13F9383EA1C08504F"
         + "B0BF321503EFE43488310DD8DC77EC5B8349B8BFE97C2C560EA878DE87C11E3D"
         + "597F1FEA742D73EEC7F37BE43949EF1A0D15C3F3E3FC0A8335617055AC91328E"
         + "C22B50FC15B941D3D1624CD88BC25F3E941FDDC6200689581BFEC416B4B2CB74",
       "5386107A0DD4A96ECF8D9BCF864BDE23AAEF13351F5550D777A32C1FEC165ED67AE51"
         + "66C3876AABC1FED1A0993754F3AEE256530F529548F8FE010BC0D070175569845"
         + "CF009AD24BC897A9CA1F18E1A9CE421DD54FD93AB528BC2594B47791713165276"
         + "7B76903190C3DCD2076FEC1E61FFFC32D1B07273B06EA2889E66FCBFD41FE8984"
         + "5FCE36056B09D1F20E58BB6BAA07A32796F11998BEF0AB3D387E2FB4FE3073FEB"
         + "634BA91709010A70DA29C06F8F92D638C4F158680EAFEB5E0E323BD7DACB671C0"
         + "BA3EDEEAB5CAA243CABAB28E7205AC9A0AAEAFE132635DAC7FE001C19F880A96E"
         + "395C42536D694F81B4F44DC66D7D6FBE933C56ABF585837291D8751C18EB1F3FB"
         + "620582E6A7B795D699E38C270863A289583CB9D07651E6BA3B82BC656B49BD09B"
         + "6B8C27F370120C7CB89D0829BE51D56356EA836012E9204FF4D1CA8B1B7F9C768"
         + "4BB2B0F226FD4042EEBAD931FDBD4F81F8425B305752F5E37FFA2B73BB5A034EC"
         + "7EEF5AAC92EA212897E3A2B8961D2147710ECCE127B942AB2",
       "05CC4DF005FE006C11111624E14806E4A904A4D1D6A53E795AC7867A960CD4FD");

  // x == 0 implies g^x == 1
  test("01",
       "488759644532FA7C53E5239F2A365D4B9189582BDD2967A1852FE56568382B65"
         + "C66BDFCD9B581EAEF4BB497CAF1290ECDFA47A1D1658DC5DC9248D9A4135"
         + "DC70B6A8497CDF117236841FA18500DC696A92EEF5000ABE68E9C75B37BC"
         + "6A722126BE728163AA90A6B03D5585994D3403557EEF08E819C72D143BBC"
         + "CDF74559645066CB3607E1B0430365356389FC8FB3D66FD2B6E2E834EC23"
         + "0B0234956752D07F983C918488C8E5A124B062D50B44C5E6FB36BCB03E39"
         + "0385B17CF8062B6688371E6AF5915C2B1AAA31C9294943CC6DC1B994FC09"
         + "49CA31828B83F3D6DFB081B26045DFD9F10092588B63F1D6E68881A06522"
         + "5A417CA9555B036DE89D349AC794A43EB28FE320F9A321F06A9364C88B54"
         + "99EEF4816375B119824ACC9AA56D1340B6A49D05F855DE699B351012028C"
         + "CA43001F708CC61E71CA3849935BEEBABC0D268CD41B8D2B8DCA705FDFF8"
         + "1DAA772DA96EDEA0B291FD5C0C1B8EFE5318D37EBC1BFF53A9DDEC4171A6"
         + "479E341438970058E25C8F2BCDA6166C8BF1B065C174",
       "8B2BACE575179D762F6F2FFDBFF00B497C07766AB3EED9961447CF6F43D06A97");
}

function test_invalid_input_round2() {
  let a = Cc["@mozilla.org/services-crypto/sync-jpake;1"]   
            .createInstance(Ci.nsISyncJPAKE);

  a.round1("alice", {}, {}, {}, {}, {}, {});
  do_check_throws(function() {
    a.round2("invalid", "sekrit", "some", "real", "garbage",
             "even", "more", "garbage", {}, {}, {});
  });
}

function test_invalid_input_final() {
  let a = Cc["@mozilla.org/services-crypto/sync-jpake;1"]
            .createInstance(Ci.nsISyncJPAKE);
  let b = Cc["@mozilla.org/services-crypto/sync-jpake;1"]
            .createInstance(Ci.nsISyncJPAKE);

  let gx1 = {};
  let gv1 = {};
  let r1 = {};
  let gx2 = {};
  let gv2 = {};
  let r2 = {};

  a.round1("alice", {}, {}, {}, {}, {}, {});
  b.round1("bob", gx1, gv1, r1, gx2, gv2, r2);
  a.round2("bob", "sekrit", gx1.value, gv1.value, r1.value,
           gx2.value, gv2.value, r2.value, {}, {}, {});
  do_check_throws(function() {
    a.final("some", "garbage", "alright", "foobar-info", {}, {});
  });
}

function run_test() {
  test_x4_zero();
  test_success();
  test_failure();
  test_same_signerids();
  test_bad_zkp();
  test_invalid_input_round2();
  test_invalid_input_final();
}
