var fs = require('fs'),
    path = require('path');

var keysDir = require('path').resolve(__dirname, '../../keys/'),
    options = {
      key: fs.readFileSync(keysDir + '/spdy-key.pem'),
      cert: fs.readFileSync(keysDir + '/spdy-cert.pem'),
      ca: fs.readFileSync(keysDir + '/spdy-csr.pem'),
      rejectUnauthorized: false
    };

module.exports = options;
