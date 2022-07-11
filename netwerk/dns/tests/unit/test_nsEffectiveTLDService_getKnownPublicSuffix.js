/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests getPublicSuffix with the validate argument.
 */

"use strict";

add_task(() => {
  for (let [suffix, isKnown] of [
    ["", false],
    [null, false],
    ["mozbacon", false],
    ["com", true],
    ["circle", true],
    ["bd", true],
    ["gov.bd", true],
    ["ck", true],
    ["www.ck", true],
    ["bs", true],
    ["com.bs", true],
    ["網絡.cn", true],
    ["valléedaoste.it", true],
    ["aurskog-høland.no", true],
    ["公司.香港", true],
    ["भारतम्", true],
    ["فلسطين", true],
  ]) {
    let origin = "test." + suffix;
    Assert.equal(
      !!Services.eTLD.getKnownPublicSuffixFromHost(origin),
      isKnown,
      `"${suffix}" should ${isKnown ? " " : "not "}be a known public suffix`
    );
    Assert.equal(
      !!Services.eTLD.getKnownPublicSuffix(
        Services.io.newURI("http://" + origin)
      ),
      isKnown,
      `"${suffix}" should ${isKnown ? " " : "not "}be a known public suffix`
    );
  }
});
