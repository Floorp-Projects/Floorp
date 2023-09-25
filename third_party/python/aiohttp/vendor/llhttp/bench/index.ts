// NOTE: run `npm test` to build `./test/tmp/http-loose-request`

import * as assert from 'assert';
import { spawnSync } from 'child_process';
import { existsSync } from 'fs';

const isURL = !process.argv[2] || process.argv[2] === 'url';
const isHTTP = !process.argv[2] || process.argv[2] === 'http';

const requests: Map<string, string> = new Map();

if (!existsSync('./test/tmp/http-loose-request.c')) {
  console.error('Run npm test to build ./test/tmp/http-loose-request');
  process.exit(1);
}

requests.set('seanmonstar/httparse',
  'GET /wp-content/uploads/2010/03/hello-kitty-darth-vader-pink.jpg HTTP/1.1\r\n' +
  'Host: www.kittyhell.com\r\n' +
  'User-Agent: Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10.6; ja-JP-mac; rv:1.9.2.3) Gecko/20100401 Firefox/3.6.3 Pathtraq/0.9\r\n' +
  'Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n' +
  'Accept-Language: ja,en-us;q=0.7,en;q=0.3\r\n' +
  'Accept-Encoding: gzip,deflate\r\n' +
  'Accept-Charset: Shift_JIS,utf-8;q=0.7,*;q=0.7\r\n' +
  'Keep-Alive: 115\r\n' +
  'Connection: keep-alive\r\n' +
  'Cookie: wp_ozh_wsa_visits=2; wp_ozh_wsa_visit_lasttime=xxxxxxxxxx; __utma=xxxxxxxxx.xxxxxxxxxx.xxxxxxxxxx.xxxxxxxxxx.xxxxxxxxxx.x; __utmz=xxxxxxxxx.xxxxxxxxxx.x.x.utmccn=(referral)|utmcsr=reader.livedoor.com|utmcct=/reader/|utmcmd=referral\r\n\r\n');

requests.set('nodejs/http-parser',
  'POST /joyent/http-parser HTTP/1.1\r\n' +
  'Host: github.com\r\n' +
  'DNT: 1\r\n' +
  'Accept-Encoding: gzip, deflate, sdch\r\n' +
  'Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.6,en;q=0.4\r\n' +
  'User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_10_1) ' +
      'AppleWebKit/537.36 (KHTML, like Gecko) ' +
      'Chrome/39.0.2171.65 Safari/537.36\r\n' +
  'Accept: text/html,application/xhtml+xml,application/xml;q=0.9,' +
      'image/webp,*/*;q=0.8\r\n' +
  'Referer: https://github.com/joyent/http-parser\r\n' +
  'Connection: keep-alive\r\n' +
  'Transfer-Encoding: chunked\r\n' +
  'Cache-Control: max-age=0\r\n\r\nb\r\nhello world\r\n0\r\n\r\n');

if (process.argv[2] === 'loop') {
  const reqName = process.argv[3];
  assert(requests.has(reqName), `Unknown request name: "${reqName}"`);

  const request = requests.get(reqName)!;
  spawnSync('./test/tmp/http-loose-request', [
    'loop',
    request
  ], { stdio: 'inherit' });
  process.exit(0);
}

if (isURL) {
  console.log('url loose (C)');

  spawnSync('./test/tmp/url-loose-url-c', [
    'bench',
    'http://example.com/path/to/file?query=value#fragment'
  ], { stdio: 'inherit' });

  console.log('url strict (C)');

  spawnSync('./test/tmp/url-strict-url-c', [
    'bench',
    'http://example.com/path/to/file?query=value#fragment'
  ], { stdio: 'inherit' });
}

if (isHTTP) {
  for(const [name, request] of requests) {
    console.log('http loose: "%s" (C)', name);

    spawnSync('./test/tmp/http-loose-request-c', [
      'bench',
      request
    ], { stdio: 'inherit' });

    console.log('http strict: "%s" (C)', name);

    spawnSync('./test/tmp/http-strict-request-c', [
      'bench',
      request
    ], { stdio: 'inherit' });
  }
}
