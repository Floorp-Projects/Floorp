"use strict";

window.success = window.success ? window.success + 1 : 1;

{
  let scripts = document.getElementsByTagName("script");
  let url = new URL(scripts[scripts.length - 1].src);
  let flag = url.searchParams.get("q");
  if (flag) {
    window.postMessage(flag, "*");
  }
}
