Cu.import("resource://services-sync/util.js");

function run_test() {
  let data = ["Zm9vYmE=", "Zm9vYmE==", "Zm9vYmE==="];
  for (let d in data) {
    do_check_eq(Utils.safeAtoB(data[d]), "fooba");
  }
}
