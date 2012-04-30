var path = require('path'),
    spawn = require('child_process').spawn;

function start() {
  var child = spawn(process.execPath, [path.resolve(__dirname, 'app.js')]);
  process.stderr.write('started child with pid: ' + child.pid + '\n');

  child.on('exit', function(code) {
    process.stderr.write('child exit: ' + code + '!\n');
    setTimeout(start, 100);
  });
  child.stdout.pipe(process.stdout);
  child.stderr.pipe(process.stderr);
}
start();
