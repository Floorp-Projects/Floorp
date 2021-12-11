'use strict'

const dnsPacket = require('..')
const dgram = require('dgram')

const socket = dgram.createSocket('udp4')

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

socket.on('message', function (message, rinfo) {
  console.log(rinfo)
  console.log(dnsPacket.decode(message)) // prints out a response from google dns
  socket.close()
})

socket.send(buf, 0, buf.length, 53, '8.8.8.8')
