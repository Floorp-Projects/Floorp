import { DOMParser } from "linkedom";

console.log(new DOMParser().parseFromString(`<html id="main-window" data-l10n-args="{&quot;content-title&quot;:&quot;CONTENTTITLE&quot;}"></html>`, "text/xml").toString());
console.log(new DOMParser().parseFromString(`<html id="main-window" data-l10n-args="{&quot;content-title&quot;:&quot;CONTENTTITLE&quot;}"></html>`, "text/html").toString());
