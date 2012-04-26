var fs = require('fs');

/*
 * GET home page.
 */

var image = fs.readFileSync(__dirname + '/../public/images/nodejs.png');

exports.index = function(req, res){
  if (res.isSpdy) {
    var ua = req.headers['user-agent'];

    // Firefox doesn't support push streams now
    // should be fixed once: https://bugzilla.mozilla.org/show_bug.cgi?id=718210
    // will reach all it's builds
    if (!ua || ua.match(/Firefox/i) === null) {
      // Push stream image
      res.push(
        '/images/nodejs.png',
        { 'content-type': 'image/png' },
        function(err, stream) {
          if (err) return;
          stream.on('error', function() {});
          stream.end(image);
        }
      );
    }
  }
  res.render('index', {
    title: 'SPDY - Twitlog',
    notice: req.isSpdy ?
      'Yay! This page was requested via SPDY protocol'
      :
      'Oh, no... your browser requested this page via HTTPS'
  });
};
