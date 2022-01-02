"use strict";

window.success = window.success ? window.success + 1 : 1;
window.addEventListener(
  "load",
  () => {
    let el = document.createElement("div");
    el.setAttribute("id", "good");
    document.body.appendChild(el);
  },
  { once: true }
);
