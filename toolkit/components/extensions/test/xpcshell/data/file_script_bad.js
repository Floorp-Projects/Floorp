"use strict";

window.failure = true;
window.addEventListener(
  "load",
  () => {
    let el = document.createElement("div");
    el.setAttribute("id", "bad");
    document.body.appendChild(el);
  },
  { once: true }
);
