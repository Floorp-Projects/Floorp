/* vim: set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "plbase64.h"
#include "nsStringAPI.h"
#include "prmem.h"

/*
 * ReadNTLM : reads NTLM messages.
 *
 * based on http://davenport.sourceforge.net/ntlm.html
 */

#define kNegotiateUnicode               0x00000001
#define kNegotiateOEM                   0x00000002
#define kRequestTarget                  0x00000004
#define kUnknown1                       0x00000008
#define kNegotiateSign                  0x00000010
#define kNegotiateSeal                  0x00000020
#define kNegotiateDatagramStyle         0x00000040
#define kNegotiateLanManagerKey         0x00000080
#define kNegotiateNetware               0x00000100
#define kNegotiateNTLMKey               0x00000200
#define kUnknown2                       0x00000400
#define kUnknown3                       0x00000800
#define kNegotiateDomainSupplied        0x00001000
#define kNegotiateWorkstationSupplied   0x00002000
#define kNegotiateLocalCall             0x00004000
#define kNegotiateAlwaysSign            0x00008000
#define kTargetTypeDomain               0x00010000
#define kTargetTypeServer               0x00020000
#define kTargetTypeShare                0x00040000
#define kNegotiateNTLM2Key              0x00080000
#define kRequestInitResponse            0x00100000
#define kRequestAcceptResponse          0x00200000
#define kRequestNonNTSessionKey         0x00400000
#define kNegotiateTargetInfo            0x00800000
#define kUnknown4                       0x01000000
#define kUnknown5                       0x02000000
#define kUnknown6                       0x04000000
#define kUnknown7                       0x08000000
#define kUnknown8                       0x10000000
#define kNegotiate128                   0x20000000
#define kNegotiateKeyExchange           0x40000000
#define kNegotiate56                    0x80000000

static const char NTLM_SIGNATURE[] = "NTLMSSP";
static const char NTLM_TYPE1_MARKER[] = { 0x01, 0x00, 0x00, 0x00 };
static const char NTLM_TYPE2_MARKER[] = { 0x02, 0x00, 0x00, 0x00 };
static const char NTLM_TYPE3_MARKER[] = { 0x03, 0x00, 0x00, 0x00 };

#define NTLM_MARKER_LEN 4
#define NTLM_TYPE1_HEADER_LEN 32
#define NTLM_TYPE2_HEADER_LEN 32
#define NTLM_TYPE3_HEADER_LEN 64

#define LM_HASH_LEN 16
#define LM_RESP_LEN 24

#define NTLM_HASH_LEN 16
#define NTLM_RESP_LEN 24

static void PrintFlags(uint32_t flags)
{
#define TEST(_flag) \
  if (flags & k ## _flag) \
    printf("    0x%08x (" # _flag ")\n", k ## _flag)

  TEST(NegotiateUnicode);
  TEST(NegotiateOEM);
  TEST(RequestTarget);
  TEST(Unknown1);
  TEST(NegotiateSign);
  TEST(NegotiateSeal);
  TEST(NegotiateDatagramStyle);
  TEST(NegotiateLanManagerKey);
  TEST(NegotiateNetware);
  TEST(NegotiateNTLMKey);
  TEST(Unknown2);
  TEST(Unknown3);
  TEST(NegotiateDomainSupplied);
  TEST(NegotiateWorkstationSupplied);
  TEST(NegotiateLocalCall);
  TEST(NegotiateAlwaysSign);
  TEST(TargetTypeDomain);
  TEST(TargetTypeServer);
  TEST(TargetTypeShare);
  TEST(NegotiateNTLM2Key);
  TEST(RequestInitResponse);
  TEST(RequestAcceptResponse);
  TEST(RequestNonNTSessionKey);
  TEST(NegotiateTargetInfo);
  TEST(Unknown4);
  TEST(Unknown5);
  TEST(Unknown6);
  TEST(Unknown7);
  TEST(Unknown8);
  TEST(Negotiate128);
  TEST(NegotiateKeyExchange);
  TEST(Negotiate56);

#undef TEST
}

static void
PrintBuf(const char *tag, const uint8_t *buf, uint32_t bufLen)
{
  int i;

  printf("%s =\n", tag);
  while (bufLen > 0)
  {
    int count = bufLen;
    if (count > 8)
      count = 8;

    printf("    ");
    for (i=0; i<count; ++i)
    {
      printf("0x%02x ", int(buf[i]));
    }
    for (; i<8; ++i)
    {
      printf("     ");
    }

    printf("   ");
    for (i=0; i<count; ++i)
    {
      if (isprint(buf[i]))
        printf("%c", buf[i]);
      else
        printf(".");
    }
    printf("\n");

    bufLen -= count;
    buf += count;
  }
}

static uint16_t
ReadUint16(const uint8_t *&buf)
{
  uint16_t x;
#ifdef IS_BIG_ENDIAN
  x = ((uint16_t) buf[1]) | ((uint16_t) buf[0] << 8);
#else
  x = ((uint16_t) buf[0]) | ((uint16_t) buf[1] << 8);
#endif
  buf += sizeof(x);
  return x;
}

static uint32_t
ReadUint32(const uint8_t *&buf)
{
  uint32_t x;
#ifdef IS_BIG_ENDIAN
  x = ( (uint32_t) buf[3])        |
      (((uint32_t) buf[2]) << 8)  |
      (((uint32_t) buf[1]) << 16) |
      (((uint32_t) buf[0]) << 24);
#else
  x = ( (uint32_t) buf[0])        |
      (((uint32_t) buf[1]) << 8)  |
      (((uint32_t) buf[2]) << 16) |
      (((uint32_t) buf[3]) << 24);
#endif
  buf += sizeof(x);
  return x;
}

typedef struct {
  uint16_t length;
  uint16_t capacity;
  uint32_t offset;
} SecBuf;

static void
ReadSecBuf(SecBuf *s, const uint8_t *&buf)
{
  s->length = ReadUint16(buf);
  s->capacity = ReadUint16(buf);
  s->offset = ReadUint32(buf);
}

static void
ReadType1MsgBody(const uint8_t *inBuf, uint32_t start)
{
  const uint8_t *cursor = inBuf + start;
  uint32_t flags;

  PrintBuf("flags", cursor, 4);
  // read flags
  flags = ReadUint32(cursor);
  PrintFlags(flags);

  // type 1 message may not include trailing security buffers
  if ((flags & kNegotiateDomainSupplied) | 
      (flags & kNegotiateWorkstationSupplied))
  {
    SecBuf secbuf;
    ReadSecBuf(&secbuf, cursor);
    PrintBuf("supplied domain", inBuf + secbuf.offset, secbuf.length);

    ReadSecBuf(&secbuf, cursor);
    PrintBuf("supplied workstation", inBuf + secbuf.offset, secbuf.length);
  }
}

static void
ReadType2MsgBody(const uint8_t *inBuf, uint32_t start)
{
  uint16_t targetLen, offset;
  uint32_t flags;
  const uint8_t *target;
  const uint8_t *cursor = inBuf + start;

  // read target name security buffer
  targetLen = ReadUint16(cursor);
  ReadUint16(cursor); // discard next 16-bit value
  offset = ReadUint32(cursor); // get offset from inBuf
  target = inBuf + offset;

  PrintBuf("target", target, targetLen);

  PrintBuf("flags", cursor, 4);
  // read flags
  flags = ReadUint32(cursor);
  PrintFlags(flags);

  // read challenge
  PrintBuf("challenge", cursor, 8);
  cursor += 8;

  PrintBuf("context", cursor, 8);
  cursor += 8;

  SecBuf secbuf;
  ReadSecBuf(&secbuf, cursor);
  PrintBuf("target information", inBuf + secbuf.offset, secbuf.length);
}

static void
ReadType3MsgBody(const uint8_t *inBuf, uint32_t start)
{
  const uint8_t *cursor = inBuf + start;

  SecBuf secbuf;

  ReadSecBuf(&secbuf, cursor); // LM response
  PrintBuf("LM response", inBuf + secbuf.offset, secbuf.length);

  ReadSecBuf(&secbuf, cursor); // NTLM response
  PrintBuf("NTLM response", inBuf + secbuf.offset, secbuf.length);

  ReadSecBuf(&secbuf, cursor); // domain name
  PrintBuf("domain name", inBuf + secbuf.offset, secbuf.length);

  ReadSecBuf(&secbuf, cursor); // user name
  PrintBuf("user name", inBuf + secbuf.offset, secbuf.length);

  ReadSecBuf(&secbuf, cursor); // workstation name
  PrintBuf("workstation name", inBuf + secbuf.offset, secbuf.length);

  ReadSecBuf(&secbuf, cursor); // session key
  PrintBuf("session key", inBuf + secbuf.offset, secbuf.length);

  uint32_t flags = ReadUint32(cursor);
  PrintBuf("flags", (const uint8_t *) &flags, sizeof(flags));
  PrintFlags(flags);
}

static void
ReadMsg(const char *base64buf, uint32_t bufLen)
{
  uint8_t *inBuf = (uint8_t *) PL_Base64Decode(base64buf, bufLen, nullptr);
  if (!inBuf)
  {
    printf("PL_Base64Decode failed\n");
    return;
  }

  const uint8_t *cursor = inBuf;

  PrintBuf("signature", cursor, 8);

  // verify NTLMSSP signature
  if (memcmp(cursor, NTLM_SIGNATURE, sizeof(NTLM_SIGNATURE)) != 0)
  {
    printf("### invalid or corrupt NTLM signature\n");
  }
  cursor += sizeof(NTLM_SIGNATURE);

  PrintBuf("message type", cursor, 4);

  if (memcmp(cursor, NTLM_TYPE1_MARKER, sizeof(NTLM_MARKER_LEN)) == 0)
    ReadType1MsgBody(inBuf, 12);
  else if (memcmp(cursor, NTLM_TYPE2_MARKER, sizeof(NTLM_MARKER_LEN)) == 0)
    ReadType2MsgBody(inBuf, 12);
  else if (memcmp(cursor, NTLM_TYPE3_MARKER, sizeof(NTLM_MARKER_LEN)) == 0)
    ReadType3MsgBody(inBuf, 12);
  else
    printf("### invalid or unknown message type\n"); 

  PR_Free(inBuf);
}

int main(int argc, char **argv)
{
  if (argc == 1)
  {
    printf("usage: ntlmread <msg>\n");
    return -1;
  }
  ReadMsg(argv[1], (uint32_t) strlen(argv[1]));
  return 0;
}
