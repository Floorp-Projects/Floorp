window.addEventListener("load", function () {
  let str = "さ".repeat(97);
  for (let i = 0; i < 2000; i++) {
    let p = document.createElement("p");
    p.textContent = str;
    document.body.appendChild(p);
  }
  let floating = document.createElement("div");
  floating.style.position = "absolute";
  floating.style.top = "5px";
  floating.style.left = "0px";
  floating.style.backgroundColor = "white";
  floating.innerHTML =
    "あああああああああああ<br>あああああああああああ<br>あああああああああああ<br>あああああああああああ<br>あああああああああああ<br>あああああああああああ";
  document.body.appendChild(floating);

  flush_layout(floating);
  perf_start();
  for (let i = 0; i < 1000; i++) {
    floating.style.left = i + "px";
    flush_layout(floating);
  }
  perf_finish();
});
