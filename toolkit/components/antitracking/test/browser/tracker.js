window.addEventListener("message", e => {
  let bc = new BroadcastChannel("a");
  bc.postMessage("ready!");
});
window.open("https://tracking.example.com/browser/toolkit/components/antitracking/test/browser/3rdPartyOpen.html");
