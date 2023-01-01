const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const SVG_NS = "http://www.w3.org/2000/svg";
const XHTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const browser = Services.appShell.createWindowlessBrowser(false);
