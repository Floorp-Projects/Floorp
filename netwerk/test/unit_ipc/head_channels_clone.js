//
// Load standard base class for network tests into child process
//

var {NetUtil} = ChromeUtils.import('resource://gre/modules/NetUtil.jsm');
var {XPCOMUtils} = ChromeUtils.import('resource://gre/modules/XPCOMUtils.jsm');

load("../unit/head_channels.js");

