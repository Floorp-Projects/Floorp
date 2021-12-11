/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  let data = ["Zm9vYmE=", "Zm9vYmE==", "Zm9vYmE==="];
  for (let d in data) {
    Assert.equal(CommonUtils.safeAtoB(data[d]), "fooba");
  }
}
