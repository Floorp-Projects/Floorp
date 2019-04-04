'use strict'

const tape = require('tape')
const packet = require('./')
const rcodes = require('./rcodes')
const opcodes = require('./opcodes')
const optioncodes = require('./optioncodes')

tape('unknown', function (t) {
  testEncoder(t, packet.unknown, Buffer.from('hello world'))
  t.end()
})

tape('txt', function (t) {
  testEncoder(t, packet.txt, [])
  testEncoder(t, packet.txt, ['hello world'])
  testEncoder(t, packet.txt, ['hello', 'world'])
  testEncoder(t, packet.txt, [Buffer.from([0, 1, 2, 3, 4, 5])])
  testEncoder(t, packet.txt, ['a', 'b', Buffer.from([0, 1, 2, 3, 4, 5])])
  testEncoder(t, packet.txt, ['', Buffer.allocUnsafe(0)])
  t.end()
})

tape('txt-scalar-string', function (t) {
  const buf = packet.txt.encode('hi')
  const val = packet.txt.decode(buf)
  t.ok(val.length === 1, 'array length')
  t.ok(val[0].toString() === 'hi', 'data')
  t.end()
})

tape('txt-scalar-buffer', function (t) {
  const data = Buffer.from([0, 1, 2, 3, 4, 5])
  const buf = packet.txt.encode(data)
  const val = packet.txt.decode(buf)
  t.ok(val.length === 1, 'array length')
  t.ok(val[0].equals(data), 'data')
  t.end()
})

tape('txt-invalid-data', function (t) {
  t.throws(function () { packet.txt.encode(null) }, 'null')
  t.throws(function () { packet.txt.encode(undefined) }, 'undefined')
  t.throws(function () { packet.txt.encode(10) }, 'number')
  t.end()
})

tape('null', function (t) {
  testEncoder(t, packet.null, Buffer.from([0, 1, 2, 3, 4, 5]))
  t.end()
})

tape('hinfo', function (t) {
  testEncoder(t, packet.hinfo, { cpu: 'intel', os: 'best one' })
  t.end()
})

tape('ptr', function (t) {
  testEncoder(t, packet.ptr, 'hello.world.com')
  t.end()
})

tape('cname', function (t) {
  testEncoder(t, packet.cname, 'hello.cname.world.com')
  t.end()
})

tape('dname', function (t) {
  testEncoder(t, packet.dname, 'hello.dname.world.com')
  t.end()
})

tape('srv', function (t) {
  testEncoder(t, packet.srv, { port: 9999, target: 'hello.world.com' })
  testEncoder(t, packet.srv, { port: 9999, target: 'hello.world.com', priority: 42, weight: 10 })
  t.end()
})

tape('caa', function (t) {
  testEncoder(t, packet.caa, { flags: 128, tag: 'issue', value: 'letsencrypt.org', issuerCritical: true })
  testEncoder(t, packet.caa, { tag: 'issue', value: 'letsencrypt.org', issuerCritical: true })
  testEncoder(t, packet.caa, { tag: 'issue', value: 'letsencrypt.org' })
  t.end()
})

tape('mx', function (t) {
  testEncoder(t, packet.mx, { preference: 10, exchange: 'mx.hello.world.com' })
  testEncoder(t, packet.mx, { exchange: 'mx.hello.world.com' })
  t.end()
})

tape('ns', function (t) {
  testEncoder(t, packet.ns, 'ns.world.com')
  t.end()
})

tape('soa', function (t) {
  testEncoder(t, packet.soa, {
    mname: 'hello.world.com',
    rname: 'root.hello.world.com',
    serial: 2018010400,
    refresh: 14400,
    retry: 3600,
    expire: 604800,
    minimum: 3600
  })
  t.end()
})

tape('a', function (t) {
  testEncoder(t, packet.a, '127.0.0.1')
  t.end()
})

tape('aaaa', function (t) {
  testEncoder(t, packet.aaaa, 'fe80::1')
  t.end()
})

tape('query', function (t) {
  testEncoder(t, packet, {
    type: 'query',
    questions: [{
      type: 'A',
      name: 'hello.a.com'
    }, {
      type: 'SRV',
      name: 'hello.srv.com'
    }]
  })

  testEncoder(t, packet, {
    type: 'query',
    id: 42,
    questions: [{
      type: 'A',
      class: 'IN',
      name: 'hello.a.com'
    }, {
      type: 'SRV',
      name: 'hello.srv.com'
    }]
  })

  testEncoder(t, packet, {
    type: 'query',
    id: 42,
    questions: [{
      type: 'A',
      class: 'CH',
      name: 'hello.a.com'
    }, {
      type: 'SRV',
      name: 'hello.srv.com'
    }]
  })

  t.end()
})

tape('response', function (t) {
  testEncoder(t, packet, {
    type: 'response',
    answers: [{
      type: 'A',
      class: 'IN',
      flush: true,
      name: 'hello.a.com',
      data: '127.0.0.1'
    }]
  })

  testEncoder(t, packet, {
    type: 'response',
    flags: packet.TRUNCATED_RESPONSE,
    answers: [{
      type: 'A',
      class: 'IN',
      name: 'hello.a.com',
      data: '127.0.0.1'
    }, {
      type: 'SRV',
      class: 'IN',
      name: 'hello.srv.com',
      data: {
        port: 9090,
        target: 'hello.target.com'
      }
    }, {
      type: 'CNAME',
      class: 'IN',
      name: 'hello.cname.com',
      data: 'hello.other.domain.com'
    }]
  })

  testEncoder(t, packet, {
    type: 'response',
    id: 100,
    flags: 0,
    additionals: [{
      type: 'AAAA',
      name: 'hello.a.com',
      data: 'fe80::1'
    }, {
      type: 'PTR',
      name: 'hello.ptr.com',
      data: 'hello.other.ptr.com'
    }, {
      type: 'SRV',
      name: 'hello.srv.com',
      ttl: 42,
      data: {
        port: 9090,
        target: 'hello.target.com'
      }
    }],
    answers: [{
      type: 'NULL',
      name: 'hello.null.com',
      data: Buffer.from([1, 2, 3, 4, 5])
    }]
  })

  testEncoder(t, packet, {
    type: 'response',
    answers: [{
      type: 'TXT',
      name: 'emptytxt.com',
      data: ''
    }]
  })

  t.end()
})

tape('rcode', function (t) {
  const errors = ['NOERROR', 'FORMERR', 'SERVFAIL', 'NXDOMAIN', 'NOTIMP', 'REFUSED', 'YXDOMAIN', 'YXRRSET', 'NXRRSET', 'NOTAUTH', 'NOTZONE', 'RCODE_11', 'RCODE_12', 'RCODE_13', 'RCODE_14', 'RCODE_15']
  for (const i in errors) {
    const code = rcodes.toRcode(errors[i])
    t.ok(errors[i] === rcodes.toString(code), 'rcode conversion from/to string matches: ' + rcodes.toString(code))
  }

  const ops = ['QUERY', 'IQUERY', 'STATUS', 'OPCODE_3', 'NOTIFY', 'UPDATE', 'OPCODE_6', 'OPCODE_7', 'OPCODE_8', 'OPCODE_9', 'OPCODE_10', 'OPCODE_11', 'OPCODE_12', 'OPCODE_13', 'OPCODE_14', 'OPCODE_15']
  for (const j in ops) {
    const ocode = opcodes.toOpcode(ops[j])
    t.ok(ops[j] === opcodes.toString(ocode), 'opcode conversion from/to string matches: ' + opcodes.toString(ocode))
  }

  const buf = packet.encode({
    type: 'response',
    id: 45632,
    flags: 0x8480,
    answers: [{
      type: 'A',
      name: 'hello.example.net',
      data: '127.0.0.1'
    }]
  })
  const val = packet.decode(buf)
  t.ok(val.type === 'response', 'decode type')
  t.ok(val.opcode === 'QUERY', 'decode opcode')
  t.ok(val.flag_qr === true, 'decode flag_qr')
  t.ok(val.flag_aa === true, 'decode flag_aa')
  t.ok(val.flag_tc === false, 'decode flag_tc')
  t.ok(val.flag_rd === false, 'decode flag_rd')
  t.ok(val.flag_ra === true, 'decode flag_ra')
  t.ok(val.flag_z === false, 'decode flag_z')
  t.ok(val.flag_ad === false, 'decode flag_ad')
  t.ok(val.flag_cd === false, 'decode flag_cd')
  t.ok(val.rcode === 'NOERROR', 'decode rcode')
  t.end()
})

tape('name_encoding', function (t) {
  let data = 'foo.example.com'
  const buf = Buffer.allocUnsafe(255)
  let offset = 0
  packet.name.encode(data, buf, offset)
  t.ok(packet.name.encode.bytes === 17, 'name encoding length matches')
  let dd = packet.name.decode(buf, offset)
  t.ok(data === dd, 'encode/decode matches')
  offset += packet.name.encode.bytes

  data = 'com'
  packet.name.encode(data, buf, offset)
  t.ok(packet.name.encode.bytes === 5, 'name encoding length matches')
  dd = packet.name.decode(buf, offset)
  t.ok(data === dd, 'encode/decode matches')
  offset += packet.name.encode.bytes

  data = 'example.com.'
  packet.name.encode(data, buf, offset)
  t.ok(packet.name.encode.bytes === 13, 'name encoding length matches')
  dd = packet.name.decode(buf, offset)
  t.ok(data.slice(0, -1) === dd, 'encode/decode matches')
  offset += packet.name.encode.bytes

  data = '.'
  packet.name.encode(data, buf, offset)
  t.ok(packet.name.encode.bytes === 1, 'name encoding length matches')
  dd = packet.name.decode(buf, offset)
  t.ok(data === dd, 'encode/decode matches')
  t.end()
})

tape('stream', function (t) {
  const val = {
    type: 'query',
    id: 45632,
    flags: 0x8480,
    answers: [{
      type: 'A',
      name: 'test2.example.net',
      data: '198.51.100.1'
    }]
  }
  const buf = packet.streamEncode(val)
  const val2 = packet.streamDecode(buf)

  t.same(buf.length, packet.streamEncode.bytes, 'streamEncode.bytes was set correctly')
  t.ok(compare(t, val2.type, val.type), 'streamDecoded type match')
  t.ok(compare(t, val2.id, val.id), 'streamDecoded id match')
  t.ok(parseInt(val2.flags) === parseInt(val.flags & 0x7FFF), 'streamDecoded flags match')
  const answer = val.answers[0]
  const answer2 = val2.answers[0]
  t.ok(compare(t, answer.type, answer2.type), 'streamDecoded RR type match')
  t.ok(compare(t, answer.name, answer2.name), 'streamDecoded RR name match')
  t.ok(compare(t, answer.data, answer2.data), 'streamDecoded RR rdata match')
  t.end()
})

tape('opt', function (t) {
  const val = {
    type: 'query',
    questions: [{
      type: 'A',
      name: 'hello.a.com'
    }],
    additionals: [{
      type: 'OPT',
      name: '.',
      udpPayloadSize: 1024
    }]
  }
  testEncoder(t, packet, val)
  let buf = packet.encode(val)
  let val2 = packet.decode(buf)
  const additional1 = val.additionals[0]
  let additional2 = val2.additionals[0]
  t.ok(compare(t, additional1.name, additional2.name), 'name matches')
  t.ok(compare(t, additional1.udpPayloadSize, additional2.udpPayloadSize), 'udp payload size matches')
  t.ok(compare(t, 0, additional2.flags), 'flags match')
  additional1.flags = packet.DNSSEC_OK
  additional1.extendedRcode = 0x80
  additional1.options = [ {
    code: 'CLIENT_SUBNET', // edns-client-subnet, see RFC 7871
    ip: 'fe80::',
    sourcePrefixLength: 64
  }, {
    code: 8, // still ECS
    ip: '5.6.0.0',
    sourcePrefixLength: 16,
    scopePrefixLength: 16
  }, {
    code: 'padding',
    length: 31
  }, {
    code: 'TCP_KEEPALIVE'
  }, {
    code: 'tcp_keepalive',
    timeout: 150
  }, {
    code: 'KEY_TAG',
    tags: [1, 82, 987]
  }]
  buf = packet.encode(val)
  val2 = packet.decode(buf)
  additional2 = val2.additionals[0]
  t.ok(compare(t, 1 << 15, additional2.flags), 'DO bit set in flags')
  t.ok(compare(t, true, additional2.flag_do), 'DO bit set')
  t.ok(compare(t, additional1.extendedRcode, additional2.extendedRcode), 'extended rcode matches')
  t.ok(compare(t, 8, additional2.options[0].code))
  t.ok(compare(t, 'fe80::', additional2.options[0].ip))
  t.ok(compare(t, 64, additional2.options[0].sourcePrefixLength))
  t.ok(compare(t, '5.6.0.0', additional2.options[1].ip))
  t.ok(compare(t, 16, additional2.options[1].sourcePrefixLength))
  t.ok(compare(t, 16, additional2.options[1].scopePrefixLength))
  t.ok(compare(t, additional1.options[2].length, additional2.options[2].data.length))
  t.ok(compare(t, additional1.options[3].timeout, undefined))
  t.ok(compare(t, additional1.options[4].timeout, additional2.options[4].timeout))
  t.ok(compare(t, additional1.options[5].tags, additional2.options[5].tags))
  t.end()
})

tape('dnskey', function (t) {
  testEncoder(t, packet.dnskey, {
    flags: packet.dnskey.SECURE_ENTRYPOINT | packet.dnskey.ZONE_KEY,
    algorithm: 1,
    key: Buffer.from([0, 1, 2, 3, 4, 5])
  })
  t.end()
})

tape('rrsig', function (t) {
  const testRRSIG = {
    typeCovered: 'A',
    algorithm: 1,
    labels: 2,
    originalTTL: 3600,
    expiration: 1234,
    inception: 1233,
    keyTag: 2345,
    signersName: 'foo.com',
    signature: Buffer.from([0, 1, 2, 3, 4, 5])
  }
  testEncoder(t, packet.rrsig, testRRSIG)

  // Check the signature length is correct with extra junk at the end
  const buf = Buffer.allocUnsafe(packet.rrsig.encodingLength(testRRSIG) + 4)
  packet.rrsig.encode(testRRSIG, buf)
  const val2 = packet.rrsig.decode(buf)
  t.ok(compare(t, testRRSIG, val2))

  t.end()
})

tape('rrp', function (t) {
  testEncoder(t, packet.rp, {
    mbox: 'foo.bar.com',
    txt: 'baz.bar.com'
  })
  testEncoder(t, packet.rp, {
    mbox: 'foo.bar.com'
  })
  testEncoder(t, packet.rp, {
    txt: 'baz.bar.com'
  })
  testEncoder(t, packet.rp, {})
  t.end()
})

tape('nsec', function (t) {
  testEncoder(t, packet.nsec, {
    nextDomain: 'foo.com',
    rrtypes: ['A', 'DNSKEY', 'CAA', 'DLV']
  })
  testEncoder(t, packet.nsec, {
    nextDomain: 'foo.com',
    rrtypes: ['TXT'] // 16
  })
  testEncoder(t, packet.nsec, {
    nextDomain: 'foo.com',
    rrtypes: ['TKEY'] // 249
  })
  testEncoder(t, packet.nsec, {
    nextDomain: 'foo.com',
    rrtypes: ['RRSIG', 'NSEC']
  })
  testEncoder(t, packet.nsec, {
    nextDomain: 'foo.com',
    rrtypes: ['TXT', 'RRSIG']
  })
  testEncoder(t, packet.nsec, {
    nextDomain: 'foo.com',
    rrtypes: ['TXT', 'NSEC']
  })

  // Test with the sample NSEC from https://tools.ietf.org/html/rfc4034#section-4.3
  var sampleNSEC = Buffer.from('003704686f7374076578616d706c6503636f6d00' +
      '0006400100000003041b000000000000000000000000000000000000000000000' +
      '000000020', 'hex')
  var decoded = packet.nsec.decode(sampleNSEC)
  t.ok(compare(t, decoded, {
    nextDomain: 'host.example.com',
    rrtypes: ['A', 'MX', 'RRSIG', 'NSEC', 'UNKNOWN_1234']
  }))
  var reencoded = packet.nsec.encode(decoded)
  t.same(sampleNSEC.length, reencoded.length)
  t.same(sampleNSEC, reencoded)
  t.end()
})

tape('nsec3', function (t) {
  testEncoder(t, packet.nsec3, {
    algorithm: 1,
    flags: 0,
    iterations: 257,
    salt: Buffer.from([42, 42, 42]),
    nextDomain: Buffer.from([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19]),
    rrtypes: ['A', 'DNSKEY', 'CAA', 'DLV']
  })
  t.end()
})

tape('ds', function (t) {
  testEncoder(t, packet.ds, {
    keyTag: 1234,
    algorithm: 1,
    digestType: 1,
    digest: Buffer.from([0, 1, 2, 3, 4, 5])
  })
  t.end()
})

tape('unpack', function (t) {
  const buf = Buffer.from([
    0x00, 0x79,
    0xde, 0xad, 0x85, 0x00, 0x00, 0x01, 0x00, 0x01,
    0x00, 0x02, 0x00, 0x02, 0x02, 0x6f, 0x6a, 0x05,
    0x62, 0x61, 0x6e, 0x67, 0x6a, 0x03, 0x63, 0x6f,
    0x6d, 0x00, 0x00, 0x01, 0x00, 0x01, 0xc0, 0x0c,
    0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x0e, 0x10,
    0x00, 0x04, 0x81, 0xfa, 0x0b, 0xaa, 0xc0, 0x0f,
    0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x0e, 0x10,
    0x00, 0x05, 0x02, 0x63, 0x6a, 0xc0, 0x0f, 0xc0,
    0x0f, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x0e,
    0x10, 0x00, 0x02, 0xc0, 0x0c, 0xc0, 0x3a, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x00, 0x0e, 0x10, 0x00,
    0x04, 0x45, 0x4d, 0x9b, 0x9c, 0xc0, 0x0c, 0x00,
    0x1c, 0x00, 0x01, 0x00, 0x00, 0x0e, 0x10, 0x00,
    0x10, 0x20, 0x01, 0x04, 0x18, 0x00, 0x00, 0x50,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xf9
  ])
  const val = packet.streamDecode(buf)
  const answer = val.answers[0]
  const authority = val.authorities[1]
  t.ok(val.rcode === 'NOERROR', 'decode rcode')
  t.ok(compare(t, answer.type, 'A'), 'streamDecoded RR type match')
  t.ok(compare(t, answer.name, 'oj.bangj.com'), 'streamDecoded RR name match')
  t.ok(compare(t, answer.data, '129.250.11.170'), 'streamDecoded RR rdata match')
  t.ok(compare(t, authority.type, 'NS'), 'streamDecoded RR type match')
  t.ok(compare(t, authority.name, 'bangj.com'), 'streamDecoded RR name match')
  t.ok(compare(t, authority.data, 'oj.bangj.com'), 'streamDecoded RR rdata match')
  t.end()
})

tape('optioncodes', function (t) {
  const opts = [
    [0, 'OPTION_0'],
    [1, 'LLQ'],
    [2, 'UL'],
    [3, 'NSID'],
    [4, 'OPTION_4'],
    [5, 'DAU'],
    [6, 'DHU'],
    [7, 'N3U'],
    [8, 'CLIENT_SUBNET'],
    [9, 'EXPIRE'],
    [10, 'COOKIE'],
    [11, 'TCP_KEEPALIVE'],
    [12, 'PADDING'],
    [13, 'CHAIN'],
    [14, 'KEY_TAG'],
    [26946, 'DEVICEID'],
    [65535, 'OPTION_65535'],
    [64000, 'OPTION_64000'],
    [65002, 'OPTION_65002'],
    [-1, null]
  ]
  for (const [code, str] of opts) {
    const s = optioncodes.toString(code)
    t.ok(compare(t, s, str), `${code} => ${str}`)
    t.ok(compare(t, optioncodes.toCode(s), code), `${str} => ${code}`)
  }
  t.ok(compare(t, optioncodes.toCode('INVALIDINVALID'), -1))
  t.end()
})

function testEncoder (t, rpacket, val) {
  const buf = rpacket.encode(val)
  const val2 = rpacket.decode(buf)

  t.same(buf.length, rpacket.encode.bytes, 'encode.bytes was set correctly')
  t.same(buf.length, rpacket.encodingLength(val), 'encoding length matches')
  t.ok(compare(t, val, val2), 'decoded object match')

  const buf2 = rpacket.encode(val2)
  const val3 = rpacket.decode(buf2)

  t.same(buf2.length, rpacket.encode.bytes, 'encode.bytes was set correctly on re-encode')
  t.same(buf2.length, rpacket.encodingLength(val), 'encoding length matches on re-encode')

  t.ok(compare(t, val, val3), 'decoded object match on re-encode')
  t.ok(compare(t, val2, val3), 're-encoded decoded object match on re-encode')

  const bigger = Buffer.allocUnsafe(buf2.length + 10)

  const buf3 = rpacket.encode(val, bigger, 10)
  const val4 = rpacket.decode(buf3, 10)

  t.ok(buf3 === bigger, 'echoes buffer on external buffer')
  t.same(rpacket.encode.bytes, buf.length, 'encode.bytes is the same on external buffer')
  t.ok(compare(t, val, val4), 'decoded object match on external buffer')
}

function compare (t, a, b) {
  if (Buffer.isBuffer(a)) return a.toString('hex') === b.toString('hex')
  if (typeof a === 'object' && a && b) {
    const keys = Object.keys(a)
    for (let i = 0; i < keys.length; i++) {
      if (!compare(t, a[keys[i]], b[keys[i]])) {
        return false
      }
    }
  } else if (Array.isArray(b) && !Array.isArray(a)) {
    // TXT always decode as array
    return a.toString() === b[0].toString()
  } else {
    return a === b
  }
  return true
}
