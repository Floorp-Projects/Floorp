window.addEventListener("message", e => {
  let bc = new BroadcastChannel("a");
  bc.postMessage("ready!");
});
window.open(
  "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/3rdPartyOpen.html"
);
