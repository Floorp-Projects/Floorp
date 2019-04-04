
'use strict'

/*
 * Sample code to make DNS over HTTPS request using POST
 * AUTHOR: Tom Pusateri <pusateri@bangj.com>
 * DATE: March 17, 2018
 * LICENSE: MIT
 */

const dnsPacket = require('..')
const https = require('https')

function getRandomInt (min, max) {
  return Math.floor(Math.random() * (max - min + 1)) + min
}

const buf = dnsPacket.encode({
  type: 'query',
  id: getRandomInt(1, 65534),
  flags: dnsPacket.RECURSION_DESIRED,
  questions: [{
    type: 'A',
    name: 'google.com'
  }]
})

const options = {
  hostname: 'dns.google.com',
  port: 443,
  path: '/experimental',
  method: 'POST',
  headers: {
    'Content-Type': 'application/dns-udpwireformat',
    'Content-Length': Buffer.byteLength(buf)
  }
}

const request = https.request(options, (response) => {
  console.log('statusCode:', response.statusCode)
  console.log('headers:', response.headers)

  response.on('data', (d) => {
    console.log(dnsPacket.decode(d))
  })
})

request.on('error', (e) => {
  console.error(e)
})
request.write(buf)
request.end()
