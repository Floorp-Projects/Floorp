var EXPORTED_SYMBOLS = ["backchannels", "fireEvent", "addBackChannel"];

var backchannels = [];

var fireEvent = function (name, obj) {
  for each(backchannel in backchannels) {
    backchannel.session.encodeOut({'eventType':name, 'result':obj});
  }
}

var addBackChannel = function (backchannel) {
    backchannels.push(backchannel);
}
