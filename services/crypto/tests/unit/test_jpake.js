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
  test_success();
  test_failure();
  test_same_signerids();
  test_bad_zkp();
  test_invalid_input_round2();
  test_invalid_input_final();
}
