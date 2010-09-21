Cu.import("resource://services-sync/base_records/keys.js");
Cu.import("resource://services-sync/main.js");

function run_test() {
  initTestLogging("Debug");

  // Test fixtures:

  // A private key with a unicode passphrase generated with Sync 1.4
  // and the corresponding public key.
  let pubkeyUri = "http://localhost:8080/1.0/foo/storage/keys/pubkey";
  let privkeyUri = "http://localhost:8080/1.0/foo/storage/keys/privkey";

  // a bunch of currency symbols
  let passphrase = "moneyislike$\u20ac\u00a5\u5143";
  let lowbyteonly = "moneyislike$\u00ac\u00a5\u0043";

  let sync14privkey = {
    salt: "Huk0JwJoJBXeKNioTZBQZA==",
    iv: "bHPlRmSFfkUe5IcfjnoNTA==",
    keyData: "MxAragjVoBL7erecOAaEgq/qSyLgVBFTHdEXpnP1tlEseobFsM+4z5Xhnxbows5+aL1nu50BNwr4Aao9A6Gky/Eeoo4vmWPGlQTaJgX4DDI3IrW6aXkoNzzJqvdYF2EbttnMz/x6JcXhX3KOmBgXl4bz2AgOwwyH1KCYTYhQ8PLB/BKh8vSR7gycwAqOC7OgaH1iAs4TMqy53DIlYGOdQKMxgsbQoRNGEIycBboMc35/e8PROVrp1LIUHniGtPj13J2BEi7sPGFswZbxOSnS+Zu1B8X1HVf3xlpKLGPsrD5ZWrtX/6hZS0tKR2M+ec1vIRYTGevAmICp+f6vdlp27bT9nAlSFwwHKodysNj3SyNQ2Dj6JuBkqSrjk9nV/73/8af8TF6NXZkCH9dRuI+gKrbzK1u6ikrg3ayQRzChLhqt4UvvkufMs2Pu8dIaZBgo1XSQiE+spMdsj/KEinyFeD3AnWi2WwesMSFrs0zWLHxHMgVJw0L6XXpTWPohf/uZ+O5fjULFZoyJ/Jr2AB67shm1wxkIaNIPACg9tjYstF2oyA6LsLUt1ctEXhoj/GjvnZqPXl9GGR1ElKbncKBOl9hxV4l1SEu70w/IVcluDhBIT3Fu1jX96TL+4UeRf3Qev84aY+7xSX2+OnWAFTMb6BsOcJmQXNh6PQ59eiv+AfwlxPedrWnP69+m/JfT5YvvzcEF6eiQKswOLid+O0aiHvqlhH/yxqHh5PjeGnNIWwtHO56d4tdfQsPGGF+dWGPIIw5hrG+GbdgA6nleIrZIEmxkyhVFikqSJLeFxB11+pF7Vk1wANgYPoxAh5T3CiCkX+k0KiJodqt+DThmmcgevdW3yOilmOPRkeYSlkfH9wFt+LnrJ5/LAglyOmudwc+amT6LYMUEp++NzPwk1Lx18hwXe7CQDvFaqvGB/XLQIPXqbCSdNVdRc3yADk5cA4caiOG37i2Qb/q6laJ3kjawSvSI43nkk4akGd1Gt533L1Ip1r70Tm1iV4nyeAO84CPgRPnDLR1KHxRNJZejVE4ouSIR/94veHRUDEFPrsae4JJlXVG1urmyPHXDdrWmquMkyjzbs+YB8JT1eFtT2Wqu9W8NZyMP0R57btNuXrw1qZm5QK5MmrCfqjGzx2dlv0Zito6CtQePlVjMq+gdprqB71Pl98WzvRNXZtK0uxVZjMJSLPqI1Zc9ju/lHgZgarsIFlKTGPcoqGnlUex2n8JBn9/sdiit1YLIFUMsrd7myyC6KaBFCoaFqOyFQjqD1phPBIM8fn5qp8vdsOwBHxIwfGet5UUa9jtrhu0f8ZT1FgYF2Add9bHxh8mlzsrr0dBMYC6/87hfHkcj08hRZgfG4glDlgzjVHGa4Z3KoyhdBmbFH8GRHp6BxjwdJNn9eUkT3bbelphU/wsIplwMDWbAT2fdphiwCYxH87sAiBR44PsQqOnHuzC36IE7Hes+t4f6iUzHF0yDX7txB0b069dHXMW9crXIAGzKMPcBaMn9ojeX6Y5rRfMLCN8nMQa3Bd1xU5/bgpxbsSnOgybIuHPWxMHksbQlj5x+PsLFQLYwAF5/PLjHqrDPcnybh6cJYjMI46vYJXY+A20L14YjRbqsDkO5nlJxBmV2n4s8RPpmLGo="
  };
  let sync14pubkey = "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAyuTplSewMZI+iEuW2tQsEI06ySCht2c7oS5GHQkmVjFW3EWZwBtV+XFhq4T/XM/bhVKHYEQ8h74V14i3zFG/1+g8oQprhls59LMX0W6Q4YZqJQhw4egoS74ZkEQWSCZpQQlpCPfO339OdSUAEHl6Gn63CO06aBrsSWZm4Ue6PtoBdUMjl9Wf6ig0DKAlBxTB3XOXeGzsgnWOMcjyPloSY+LH7gf7LT+79FDCvAX0K85Qyii2kN2ScEzAgGMrGFlK8f7CBBNnjR9RKiOZT0+EzX5DgH8FfBBbbZDgGYcU0bKS262XrmnSbMzhgEl2/bH2w4xRnNdRaE0Dk+Ggi37/SwIDAQAB";

  // A symmetric key that was wrapped with the above public key
  let symkeyData = "Vyrj8pzynsZ6+9LzLJx+myDogLMOboaBMrOi1I3Xa9tO/x5mkoGs7owQbsK1eTbfW3WUAsTpGaozzSmOR27QbVFxksSS38OZsVkT5SAmkwQ/MDPB4A6kJ7fedyTru8o/2O/XGMEQM3Qp5ApE/DJkITWPAx1U3K1f21vV7Mo4dkKkgHCNjsTMplbcA77+EyRLBaQtMaBuJhUi91RLSwKzqpVU0MdmWxgjhv+7+hd76idlaxNDMsAk1n13rYI6Sv6aQ1jqTBBsD7YEa1sShJadB9NxOnl2sugPuEqqnQN6Emb8R2Ok59NzoY+Zhk4n3QkoDeWgJSa96sQbzVa3M88ZVg==";
  let symkey = {keyring:{}};
  symkey.keyring[pubkeyUri] = {
    wrapped: symkeyData,
    hmac: "f0f5f4e4ee08dfed196455496e67af97fcf80cbc425ce8e753591023d3194635"
  };

  // Hook these keys into the Weave API.
  let privkey = new PrivKey(privkeyUri);
  privkey.salt = sync14privkey.salt;
  privkey.iv = sync14privkey.iv;
  privkey.keyData = sync14privkey.keyData;
  privkey.publicKeyUri = pubkeyUri;
  PrivKeys.set(privkeyUri, privkey);

  let pubkey = new PubKey(pubkeyUri);
  pubkey.keyData = sync14pubkey;
  pubkey.privateKeyUri = privkeyUri;
  PubKeys.defaultKeyUri = pubkeyUri;
  PubKeys.set(pubkeyUri, pubkey);

  Weave.Service.username = "foo";
  Weave.Service.clusterURL = "http://localhost:8080/";

  // Set up an engine whose bulk key we need to reupload.
  function SteamEngine() {
    Weave.SyncEngine.call(this, "Steam");
  }
  SteamEngine.prototype = Weave.SyncEngine.prototype;
  Weave.Engines.register(SteamEngine);

  // Set up an engine whose bulk key won't have the right HMAC, so we
  // need to wipe it.
  function PetrolEngine() {
    Weave.SyncEngine.call(this, "Petrol");
  }
  PetrolEngine.prototype = Weave.SyncEngine.prototype;
  Weave.Engines.register(PetrolEngine);
  let petrol_symkey = Weave.Utils.deepCopy(symkey);
  petrol_symkey.keyring[pubkeyUri].hmac = "definitely-not-the-right-HMAC";

  // Set up the server
  let server_privkey = new ServerWBO('privkey');
  let server_steam_key = new ServerWBO('steam', symkey);
  let server_petrol_key = new ServerWBO('petrol', petrol_symkey);
  let server_petrol_coll = new ServerCollection({
    'obj': new ServerWBO('obj', {somedata: "that's going", toget: "wiped"})
  });

  do_test_pending();
  let server = httpd_setup({
    // Need these to make Weave.Service.wipeRemote() etc. happy
    "/1.0/foo/storage/meta/global": new ServerWBO('global', {}).handler(),
    "/1.0/foo/storage/crypto/clients": new ServerWBO('clients', {}).handler(),
    "/1.0/foo/storage/clients": new ServerCollection().handler(),

    // Records to reupload
    "/1.0/foo/storage/keys/privkey": server_privkey.handler(),
    "/1.0/foo/storage/crypto/steam": server_steam_key.handler(),

    // Records that are going to be wiped
    "/1.0/foo/storage/crypto/petrol": server_petrol_key.handler(),
    "/1.0/foo/storage/petrol": server_petrol_coll.handler()
  });

  try {
    _("The original key can be decoded with both the low byte only and the full unicode passphrase.");
    do_check_true(Weave.Svc.Crypto.verifyPassphrase(sync14privkey.keyData,
                                                    passphrase,
                                                    sync14privkey.salt,
                                                    sync14privkey.iv));
    do_check_true(Weave.Svc.Crypto.verifyPassphrase(sync14privkey.keyData,
                                                    lowbyteonly,
                                                    sync14privkey.salt,
                                                    sync14privkey.iv));

    _("An obviously different passphrase will not work.");
    Weave.Service.passphrase = "something completely different";
    do_check_false(Weave.Service._verifyPassphrase());
    do_check_eq(server_privkey.payload, undefined);

    _("The right unicode passphrase will work, even though the key was generated with a low-byte only passphrase.");
    Weave.Service.passphrase = passphrase;
    do_check_true(Weave.Service._verifyPassphrase());

    _("The _needUpdatedKeys flag is set.");
    do_check_true(Weave.Service._needUpdatedKeys);

    _("We can now call updateKeysToUTF8Passphrase to trigger an upload of a new privkey.");
    Weave.Service._updateKeysToUTF8Passphrase();
    do_check_true(!!server_privkey.payload);
    do_check_eq(server_privkey.data.keyData, privkey.keyData);

    _("The new key can't be decoded with the raw passphrase but the UTF8 encoded one.");
    do_check_false(Weave.Svc.Crypto.verifyPassphrase(
      server_privkey.data.keyData, passphrase,
      server_privkey.data.salt, server_privkey.data.iv)
    );

    do_check_true(Weave.Svc.Crypto.verifyPassphrase(
      server_privkey.data.keyData, Weave.Utils.encodeUTF8(passphrase),
      server_privkey.data.salt, server_privkey.data.iv)
    );

    _("The 'steam' bulk key has been reuploaded (though only HMAC changes).");
    let server_wrapped_key = server_steam_key.data.keyring[pubkeyUri];
    do_check_eq(server_wrapped_key.wrapped, symkeyData);
    let hmacKey = Weave.Svc.KeyFactory.keyFromString(
        Ci.nsIKeyObject.HMAC, Weave.Utils.encodeUTF8(passphrase));
    let hmac = Weave.Utils.sha256HMAC(symkeyData, hmacKey);
    do_check_eq(server_wrapped_key.hmac, hmac);

    _("The 'petrol' bulk key had an incorrect HMAC to begin with, so it and all the data from that engine has been wiped.");
    do_check_eq(server_petrol_key.payload, undefined);
    do_check_eq(server_petrol_coll.wbos.obj.payload, undefined);

    _("The _needUpdatedKeys flag is no longer set.");
    do_check_false(Weave.Service._needUpdatedKeys);

  } finally {
    if (server)
      server.stop(do_test_finished);
    Weave.Svc.Prefs.resetBranch("");
  }
}
