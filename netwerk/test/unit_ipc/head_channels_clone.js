/* import-globals-from ../unit/head_channels.js */
// Load standard base class for network tests into child process
//

var { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);
var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

load("../unit/head_channels.js");
