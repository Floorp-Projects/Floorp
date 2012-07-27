/* vim:set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prlog.h"

#include <stdlib.h>
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsServiceManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsNSSShutDown.h"
#include "nsNTLMAuthModule.h"
#include "nsNativeCharsetUtils.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "prsystem.h"
#include "nss.h"
#include "pk11func.h"
#include "md4.h"

#ifdef PR_LOGGING
PRLogModuleInfo *gNTLMLog = PR_NewLogModule("NTLM");

#define LOG(x) PR_LOG(gNTLMLog, PR_LOG_DEBUG, x)
#define LOG_ENABLED() PR_LOG_TEST(gNTLMLog, PR_LOG_DEBUG)
#else
#define LOG(x)
#endif

static void des_makekey(const PRUint8 *raw, PRUint8 *key);
static void des_encrypt(const PRUint8 *key, const PRUint8 *src, PRUint8 *hash);
static void md5sum(const PRUint8 *input, PRUint32 inputLen, PRUint8 *result);

//-----------------------------------------------------------------------------
// this file contains a cross-platform NTLM authentication implementation. it
// is based on documentation from: http://davenport.sourceforge.net/ntlm.html
//-----------------------------------------------------------------------------

#define NTLM_NegotiateUnicode               0x00000001
#define NTLM_NegotiateOEM                   0x00000002
#define NTLM_RequestTarget                  0x00000004
#define NTLM_Unknown1                       0x00000008
#define NTLM_NegotiateSign                  0x00000010
#define NTLM_NegotiateSeal                  0x00000020
#define NTLM_NegotiateDatagramStyle         0x00000040
#define NTLM_NegotiateLanManagerKey         0x00000080
#define NTLM_NegotiateNetware               0x00000100
#define NTLM_NegotiateNTLMKey               0x00000200
#define NTLM_Unknown2                       0x00000400
#define NTLM_Unknown3                       0x00000800
#define NTLM_NegotiateDomainSupplied        0x00001000
#define NTLM_NegotiateWorkstationSupplied   0x00002000
#define NTLM_NegotiateLocalCall             0x00004000
#define NTLM_NegotiateAlwaysSign            0x00008000
#define NTLM_TargetTypeDomain               0x00010000
#define NTLM_TargetTypeServer               0x00020000
#define NTLM_TargetTypeShare                0x00040000
#define NTLM_NegotiateNTLM2Key              0x00080000
#define NTLM_RequestInitResponse            0x00100000
#define NTLM_RequestAcceptResponse          0x00200000
#define NTLM_RequestNonNTSessionKey         0x00400000
#define NTLM_NegotiateTargetInfo            0x00800000
#define NTLM_Unknown4                       0x01000000
#define NTLM_Unknown5                       0x02000000
#define NTLM_Unknown6                       0x04000000
#define NTLM_Unknown7                       0x08000000
#define NTLM_Unknown8                       0x10000000
#define NTLM_Negotiate128                   0x20000000
#define NTLM_NegotiateKeyExchange           0x40000000
#define NTLM_Negotiate56                    0x80000000

// we send these flags with our type 1 message
#define NTLM_TYPE1_FLAGS      \
  (NTLM_NegotiateUnicode |    \
   NTLM_NegotiateOEM |        \
   NTLM_RequestTarget |       \
   NTLM_NegotiateNTLMKey |    \
   NTLM_NegotiateAlwaysSign | \
   NTLM_NegotiateNTLM2Key)

static const char NTLM_SIGNATURE[] = "NTLMSSP";
static const char NTLM_TYPE1_MARKER[] = { 0x01, 0x00, 0x00, 0x00 };
static const char NTLM_TYPE2_MARKER[] = { 0x02, 0x00, 0x00, 0x00 };
static const char NTLM_TYPE3_MARKER[] = { 0x03, 0x00, 0x00, 0x00 };

#define NTLM_TYPE1_HEADER_LEN 32
#define NTLM_TYPE2_HEADER_LEN 32
#define NTLM_TYPE3_HEADER_LEN 64

#define LM_HASH_LEN 16
#define LM_RESP_LEN 24

#define NTLM_HASH_LEN 16
#define NTLM_RESP_LEN 24

//-----------------------------------------------------------------------------

static bool SendLM()
{
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (!prefs)
    return false;

  bool val;
  nsresult rv = prefs->GetBoolPref("network.ntlm.send-lm-response", &val);
  return NS_SUCCEEDED(rv) && val;
}

//-----------------------------------------------------------------------------

#ifdef PR_LOGGING

/**
 * Prints a description of flags to the NSPR Log, if enabled.
 */
static void LogFlags(PRUint32 flags)
{
  if (!LOG_ENABLED())
    return;
#define TEST(_flag) \
  if (flags & NTLM_ ## _flag) \
    PR_LogPrint("    0x%08x (" # _flag ")\n", NTLM_ ## _flag)

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

/**
 * Prints a hexdump of buf to the NSPR Log, if enabled.
 * @param tag Description of the data, will be printed in front of the data
 * @param buf the data to print
 * @param bufLen length of the data
 */
static void
LogBuf(const char *tag, const PRUint8 *buf, PRUint32 bufLen)
{
  int i;

  if (!LOG_ENABLED())
    return;

  PR_LogPrint("%s =\n", tag);
  char line[80];
  while (bufLen > 0)
  {
    int count = bufLen;
    if (count > 8)
      count = 8;

    strcpy(line, "    ");
    for (i=0; i<count; ++i)
    {
      int len = strlen(line);
      PR_snprintf(line + len, sizeof(line) - len, "0x%02x ", int(buf[i]));
    }
    for (; i<8; ++i)
    {
      int len = strlen(line);
      PR_snprintf(line + len, sizeof(line) - len, "     ");
    }

    int len = strlen(line);
    PR_snprintf(line + len, sizeof(line) - len, "   ");
    for (i=0; i<count; ++i)
    {
      len = strlen(line);
      if (isprint(buf[i]))
        PR_snprintf(line + len, sizeof(line) - len, "%c", buf[i]);
      else
        PR_snprintf(line + len, sizeof(line) - len, ".");
    }
    PR_LogPrint("%s\n", line);

    bufLen -= count;
    buf += count;
  }
}

#include "plbase64.h"
#include "prmem.h"
/**
 * Print base64-encoded token to the NSPR Log.
 * @param name Description of the token, will be printed in front
 * @param token The token to print
 * @param tokenLen length of the data in token
 */
static void LogToken(const char *name, const void *token, PRUint32 tokenLen)
{
  if (!LOG_ENABLED())
    return;

  char *b64data = PL_Base64Encode((const char *) token, tokenLen, NULL);
  if (b64data)
  {
    PR_LogPrint("%s: %s\n", name, b64data);
    PR_Free(b64data);
  }
}

#else
#define LogFlags(x)
#define LogBuf(a,b,c)
#define LogToken(a,b,c)

#endif // PR_LOGGING

//-----------------------------------------------------------------------------

// byte order swapping
#define SWAP16(x) ((((x) & 0xff) << 8) | (((x) >> 8) & 0xff))
#define SWAP32(x) ((SWAP16((x) & 0xffff) << 16) | (SWAP16((x) >> 16)))

static void *
WriteBytes(void *buf, const void *data, PRUint32 dataLen)
{
  memcpy(buf, data, dataLen);
  return (PRUint8 *) buf + dataLen;
}

static void *
WriteDWORD(void *buf, PRUint32 dword)
{
#ifdef IS_BIG_ENDIAN 
  // NTLM uses little endian on the wire
  dword = SWAP32(dword);
#endif
  return WriteBytes(buf, &dword, sizeof(dword));
}

static void *
WriteSecBuf(void *buf, PRUint16 length, PRUint32 offset)
{
#ifdef IS_BIG_ENDIAN
  length = SWAP16(length);
  offset = SWAP32(offset);
#endif
  buf = WriteBytes(buf, &length, sizeof(length));
  buf = WriteBytes(buf, &length, sizeof(length));
  buf = WriteBytes(buf, &offset, sizeof(offset));
  return buf;
}

#ifdef IS_BIG_ENDIAN
/**
 * WriteUnicodeLE copies a unicode string from one buffer to another.  The
 * resulting unicode string is in little-endian format.  The input string is
 * assumed to be in the native endianness of the local machine.  It is safe
 * to pass the same buffer as both input and output, which is a handy way to
 * convert the unicode buffer to little-endian on big-endian platforms.
 */
static void *
WriteUnicodeLE(void *buf, const PRUnichar *str, PRUint32 strLen)
{
  // convert input string from BE to LE
  PRUint8 *cursor = (PRUint8 *) buf,
          *input  = (PRUint8 *) str;
  for (PRUint32 i=0; i<strLen; ++i, input+=2, cursor+=2)
  {
    // allow for the case where |buf == str|
    PRUint8 temp = input[0];
    cursor[0] = input[1];
    cursor[1] = temp;
  }
  return buf;
}
#endif

static PRUint16
ReadUint16(const PRUint8 *&buf)
{
  PRUint16 x = ((PRUint16) buf[0]) | ((PRUint16) buf[1] << 8);
  buf += sizeof(x);
  return x;
}

static PRUint32
ReadUint32(const PRUint8 *&buf)
{
  PRUint32 x = ( (PRUint32) buf[0])        |
               (((PRUint32) buf[1]) << 8)  |
               (((PRUint32) buf[2]) << 16) |
               (((PRUint32) buf[3]) << 24);
  buf += sizeof(x);
  return x;
}

//-----------------------------------------------------------------------------

static void
ZapBuf(void *buf, size_t bufLen)
{
  memset(buf, 0, bufLen);
}

static void
ZapString(nsCString &s)
{
  ZapBuf(s.BeginWriting(), s.Length());
}

static void
ZapString(nsString &s)
{
  ZapBuf(s.BeginWriting(), s.Length() * 2);
}

static const unsigned char LM_MAGIC[] = "KGS!@#$%";

/**
 * LM_Hash computes the LM hash of the given password.
 *
 * @param password
 *        null-terminated unicode password.
 * @param hash
 *        16-byte result buffer
 */
static void
LM_Hash(const nsString &password, unsigned char *hash)
{
  // convert password to OEM character set.  we'll just use the native
  // filesystem charset.
  nsCAutoString passbuf;
  NS_CopyUnicodeToNative(password, passbuf);
  ToUpperCase(passbuf);
  PRUint32 n = passbuf.Length();
  passbuf.SetLength(14);
  for (PRUint32 i=n; i<14; ++i)
    passbuf.SetCharAt('\0', i);

  unsigned char k1[8], k2[8];
  des_makekey((const unsigned char *) passbuf.get()    , k1);
  des_makekey((const unsigned char *) passbuf.get() + 7, k2);
  ZapString(passbuf);

  // use password keys to hash LM magic string twice.
  des_encrypt(k1, LM_MAGIC, hash);
  des_encrypt(k2, LM_MAGIC, hash + 8);
}

/**
 * NTLM_Hash computes the NTLM hash of the given password.
 *
 * @param password
 *        null-terminated unicode password.
 * @param hash
 *        16-byte result buffer
 */
static void
NTLM_Hash(const nsString &password, unsigned char *hash)
{
  PRUint32 len = password.Length();
  PRUint8 *passbuf;
  
#ifdef IS_BIG_ENDIAN
  passbuf = (PRUint8 *) malloc(len * 2);
  WriteUnicodeLE(passbuf, password.get(), len);
#else
  passbuf = (PRUint8 *) password.get();
#endif

  md4sum(passbuf, len * 2, hash);

#ifdef IS_BIG_ENDIAN
  ZapBuf(passbuf, len * 2);
  free(passbuf);
#endif
}

//-----------------------------------------------------------------------------

/** 
 * LM_Response generates the LM response given a 16-byte password hash and the
 * challenge from the Type-2 message.
 *
 * @param hash
 *        16-byte password hash
 * @param challenge
 *        8-byte challenge from Type-2 message
 * @param response
 *        24-byte buffer to contain the LM response upon return
 */
static void
LM_Response(const PRUint8 *hash, const PRUint8 *challenge, PRUint8 *response)
{
  PRUint8 keybytes[21], k1[8], k2[8], k3[8];

  memcpy(keybytes, hash, 16);
  ZapBuf(keybytes + 16, 5);

  des_makekey(keybytes     , k1);
  des_makekey(keybytes +  7, k2);
  des_makekey(keybytes + 14, k3);

  des_encrypt(k1, challenge, response);
  des_encrypt(k2, challenge, response + 8);
  des_encrypt(k3, challenge, response + 16);
}

//-----------------------------------------------------------------------------

static nsresult
GenerateType1Msg(void **outBuf, PRUint32 *outLen)
{
  //
  // verify that bufLen is sufficient
  //
  *outLen = NTLM_TYPE1_HEADER_LEN;
  *outBuf = nsMemory::Alloc(*outLen);
  if (!*outBuf)
    return NS_ERROR_OUT_OF_MEMORY;

  //
  // write out type 1 msg
  //
  void *cursor = *outBuf;

  // 0 : signature
  cursor = WriteBytes(cursor, NTLM_SIGNATURE, sizeof(NTLM_SIGNATURE));

  // 8 : marker
  cursor = WriteBytes(cursor, NTLM_TYPE1_MARKER, sizeof(NTLM_TYPE1_MARKER));

  // 12 : flags
  cursor = WriteDWORD(cursor, NTLM_TYPE1_FLAGS);

  //
  // NOTE: it is common for the domain and workstation fields to be empty.
  //       this is true of Win2k clients, and my guess is that there is
  //       little utility to sending these strings before the charset has
  //       been negotiated.  we follow suite -- anyways, it doesn't hurt
  //       to save some bytes on the wire ;-)
  //

  // 16 : supplied domain security buffer (empty)
  cursor = WriteSecBuf(cursor, 0, 0);

  // 24 : supplied workstation security buffer (empty)
  cursor = WriteSecBuf(cursor, 0, 0);

  return NS_OK;
}

struct Type2Msg
{
  PRUint32    flags;         // NTLM_Xxx bitwise combination
  PRUint8     challenge[8];  // 8 byte challenge
  const void *target;        // target string (type depends on flags)
  PRUint32    targetLen;     // target length in bytes
};

static nsresult
ParseType2Msg(const void *inBuf, PRUint32 inLen, Type2Msg *msg)
{
  // make sure inBuf is long enough to contain a meaningful type2 msg.
  //
  // 0  NTLMSSP Signature
  // 8  NTLM Message Type
  // 12 Target Name
  // 20 Flags
  // 24 Challenge
  // 32 end of header, start of optional data blocks
  //
  if (inLen < NTLM_TYPE2_HEADER_LEN)
    return NS_ERROR_UNEXPECTED;

  const PRUint8 *cursor = (const PRUint8 *) inBuf;

  // verify NTLMSSP signature
  if (memcmp(cursor, NTLM_SIGNATURE, sizeof(NTLM_SIGNATURE)) != 0)
    return NS_ERROR_UNEXPECTED;

  cursor += sizeof(NTLM_SIGNATURE);

  // verify Type-2 marker
  if (memcmp(cursor, NTLM_TYPE2_MARKER, sizeof(NTLM_TYPE2_MARKER)) != 0)
    return NS_ERROR_UNEXPECTED;

  cursor += sizeof(NTLM_TYPE2_MARKER);

  // Read target name security buffer: ...
  // ... read target length.
  PRUint32 targetLen = ReadUint16(cursor);
  // ... skip next 16-bit "allocated space" value.
  ReadUint16(cursor);
  // ... read offset from inBuf.
  PRUint32 offset = ReadUint32(cursor);
  // Check the offset / length combo is in range of the input buffer, including
  // integer overflow checking.
  if (NS_LIKELY(offset < offset + targetLen && offset + targetLen <= inLen)) {
    msg->targetLen = targetLen;
    msg->target = ((const PRUint8 *) inBuf) + offset;
  }
  else
  {
    // Do not error out, for (conservative) backward compatibility.
    msg->targetLen = 0;
    msg->target = NULL;
  }

  // read flags
  msg->flags = ReadUint32(cursor);

  // read challenge
  memcpy(msg->challenge, cursor, sizeof(msg->challenge));
  cursor += sizeof(msg->challenge);


  LOG(("NTLM type 2 message:\n"));
  LogBuf("target", (const PRUint8 *) msg->target, msg->targetLen);
  LogBuf("flags", (const PRUint8 *) &msg->flags, 4);
  LogFlags(msg->flags);
  LogBuf("challenge", msg->challenge, sizeof(msg->challenge));

  // we currently do not implement LMv2/NTLMv2 or NTLM2 responses,
  // so we can ignore target information.  we may want to enable
  // support for these alternate mechanisms in the future.
  return NS_OK;
}

static nsresult
GenerateType3Msg(const nsString &domain,
                 const nsString &username,
                 const nsString &password,
                 const void     *inBuf,
                 PRUint32        inLen,
                 void          **outBuf,
                 PRUint32       *outLen)
{
  // inBuf contains Type-2 msg (the challenge) from server

  nsresult rv;
  Type2Msg msg;

  rv = ParseType2Msg(inBuf, inLen, &msg);
  if (NS_FAILED(rv))
    return rv;

  bool unicode = (msg.flags & NTLM_NegotiateUnicode);

  // temporary buffers for unicode strings
#ifdef IS_BIG_ENDIAN
  nsAutoString ucsDomainBuf, ucsUserBuf;
#endif
  nsAutoString ucsHostBuf; 
  // temporary buffers for oem strings
  nsCAutoString oemDomainBuf, oemUserBuf, oemHostBuf;
  // pointers and lengths for the string buffers; encoding is unicode if
  // the "negotiate unicode" flag was set in the Type-2 message.
  const void *domainPtr, *userPtr, *hostPtr;
  PRUint32 domainLen, userLen, hostLen;

  //
  // get domain name
  //
  if (unicode)
  {
#ifdef IS_BIG_ENDIAN
    ucsDomainBuf = domain;
    domainPtr = ucsDomainBuf.get();
    domainLen = ucsDomainBuf.Length() * 2;
    WriteUnicodeLE((void *) domainPtr, (const PRUnichar *) domainPtr,
                   ucsDomainBuf.Length());
#else
    domainPtr = domain.get();
    domainLen = domain.Length() * 2;
#endif
  }
  else
  {
    NS_CopyUnicodeToNative(domain, oemDomainBuf);
    domainPtr = oemDomainBuf.get();
    domainLen = oemDomainBuf.Length();
  }

  //
  // get user name
  //
  if (unicode)
  {
#ifdef IS_BIG_ENDIAN
    ucsUserBuf = username;
    userPtr = ucsUserBuf.get();
    userLen = ucsUserBuf.Length() * 2;
    WriteUnicodeLE((void *) userPtr, (const PRUnichar *) userPtr,
                   ucsUserBuf.Length());
#else
    userPtr = username.get();
    userLen = username.Length() * 2;
#endif
  }
  else
  {
    NS_CopyUnicodeToNative(username, oemUserBuf);
    userPtr = oemUserBuf.get();
    userLen = oemUserBuf.Length();
  }

  //
  // get workstation name (use local machine's hostname)
  //
  char hostBuf[SYS_INFO_BUFFER_LENGTH];
  if (PR_GetSystemInfo(PR_SI_HOSTNAME, hostBuf, sizeof(hostBuf)) == PR_FAILURE)
    return NS_ERROR_UNEXPECTED;
  hostLen = strlen(hostBuf);
  if (unicode)
  {
    // hostname is ASCII, so we can do a simple zero-pad expansion:
    CopyASCIItoUTF16(nsDependentCString(hostBuf, hostLen), ucsHostBuf);
    hostPtr = ucsHostBuf.get();
    hostLen = ucsHostBuf.Length() * 2;
#ifdef IS_BIG_ENDIAN
    WriteUnicodeLE((void *) hostPtr, (const PRUnichar *) hostPtr,
                   ucsHostBuf.Length());
#endif
  }
  else
    hostPtr = hostBuf;

  //
  // now that we have generated all of the strings, we can allocate outBuf.
  //
  *outLen = NTLM_TYPE3_HEADER_LEN + hostLen + domainLen + userLen +
            LM_RESP_LEN + NTLM_RESP_LEN;
  *outBuf = nsMemory::Alloc(*outLen);
  if (!*outBuf)
    return NS_ERROR_OUT_OF_MEMORY;

  //
  // next, we compute the LM and NTLM responses.
  //
  PRUint8 lmResp[LM_RESP_LEN], ntlmResp[NTLM_RESP_LEN], ntlmHash[NTLM_HASH_LEN];
  if (msg.flags & NTLM_NegotiateNTLM2Key)
  {
    // compute NTLM2 session response
    PRUint8 sessionHash[16], temp[16];

    PK11_GenerateRandom(lmResp, 8);
    memset(lmResp + 8, 0, LM_RESP_LEN - 8);

    memcpy(temp, msg.challenge, 8);
    memcpy(temp + 8, lmResp, 8);
    md5sum(temp, 16, sessionHash);

    NTLM_Hash(password, ntlmHash);
    LM_Response(ntlmHash, sessionHash, ntlmResp);
  }
  else
  {
    NTLM_Hash(password, ntlmHash);
    LM_Response(ntlmHash, msg.challenge, ntlmResp);

    if (SendLM())
    {
      PRUint8 lmHash[LM_HASH_LEN];
      LM_Hash(password, lmHash);
      LM_Response(lmHash, msg.challenge, lmResp);
    }
    else
    {
      // According to http://davenport.sourceforge.net/ntlm.html#ntlmVersion2,
      // the correct way to not send the LM hash is to send the NTLM hash twice
      // in both the LM and NTLM response fields.
      LM_Response(ntlmHash, msg.challenge, lmResp);
    }
  }

  //
  // finally, we assemble the Type-3 msg :-)
  //
  void *cursor = *outBuf;
  PRUint32 offset;

  // 0 : signature
  cursor = WriteBytes(cursor, NTLM_SIGNATURE, sizeof(NTLM_SIGNATURE));

  // 8 : marker
  cursor = WriteBytes(cursor, NTLM_TYPE3_MARKER, sizeof(NTLM_TYPE3_MARKER));

  // 12 : LM response sec buf
  offset = NTLM_TYPE3_HEADER_LEN + domainLen + userLen + hostLen;
  cursor = WriteSecBuf(cursor, LM_RESP_LEN, offset);
  memcpy((PRUint8 *) *outBuf + offset, lmResp, LM_RESP_LEN);

  // 20 : NTLM response sec buf
  offset += LM_RESP_LEN;
  cursor = WriteSecBuf(cursor, NTLM_RESP_LEN, offset);
  memcpy((PRUint8 *) *outBuf + offset, ntlmResp, NTLM_RESP_LEN);

  // 28 : domain name sec buf
  offset = NTLM_TYPE3_HEADER_LEN;
  cursor = WriteSecBuf(cursor, domainLen, offset);
  memcpy((PRUint8 *) *outBuf + offset, domainPtr, domainLen);

  // 36 : user name sec buf
  offset += domainLen;
  cursor = WriteSecBuf(cursor, userLen, offset);
  memcpy((PRUint8 *) *outBuf + offset, userPtr, userLen);

  // 44 : workstation (host) name sec buf
  offset += userLen;
  cursor = WriteSecBuf(cursor, hostLen, offset);
  memcpy((PRUint8 *) *outBuf + offset, hostPtr, hostLen);

  // 52 : session key sec buf (not used)
  cursor = WriteSecBuf(cursor, 0, 0);

  // 60 : negotiated flags
  cursor = WriteDWORD(cursor, msg.flags & NTLM_TYPE1_FLAGS);

  return NS_OK;
}

//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS1(nsNTLMAuthModule, nsIAuthModule)

nsNTLMAuthModule::~nsNTLMAuthModule()
{
  ZapString(mPassword);
}

nsresult
nsNTLMAuthModule::InitTest()
{
  nsNSSShutDownPreventionLock locker;
  //
  // disable NTLM authentication when FIPS mode is enabled.
  //
  return PK11_IsFIPS() ? NS_ERROR_NOT_AVAILABLE : NS_OK;
}

NS_IMETHODIMP
nsNTLMAuthModule::Init(const char      *serviceName,
                       PRUint32         serviceFlags,
                       const PRUnichar *domain,
                       const PRUnichar *username,
                       const PRUnichar *password)
{
  NS_ASSERTION(serviceFlags == nsIAuthModule::REQ_DEFAULT, "unexpected service flags");

  mDomain = domain;
  mUsername = username;
  mPassword = password;
  return NS_OK;
}

NS_IMETHODIMP
nsNTLMAuthModule::GetNextToken(const void *inToken,
                               PRUint32    inTokenLen,
                               void      **outToken,
                               PRUint32   *outTokenLen)
{
  nsresult rv;
  nsNSSShutDownPreventionLock locker;
  //
  // disable NTLM authentication when FIPS mode is enabled.
  //
  if (PK11_IsFIPS())
    return NS_ERROR_NOT_AVAILABLE;

  // if inToken is non-null, then assume it contains a type 2 message...
  if (inToken)
  {
    LogToken("in-token", inToken, inTokenLen);
    rv = GenerateType3Msg(mDomain, mUsername, mPassword, inToken,
                          inTokenLen, outToken, outTokenLen);
  }
  else
  {
    rv = GenerateType1Msg(outToken, outTokenLen);
  }

#ifdef PR_LOGGING
  if (NS_SUCCEEDED(rv))
    LogToken("out-token", *outToken, *outTokenLen);
#endif

  return rv;
}

NS_IMETHODIMP
nsNTLMAuthModule::Unwrap(const void *inToken,
                        PRUint32    inTokenLen,
                        void      **outToken,
                        PRUint32   *outTokenLen)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNTLMAuthModule::Wrap(const void *inToken,
                       PRUint32    inTokenLen,
                       bool        confidential,
                       void      **outToken,
                       PRUint32   *outTokenLen)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNTLMAuthModule::GetModuleProperties(PRUint32 *flags)
{
    *flags = 0;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// DES support code

// set odd parity bit (in least significant bit position)
static PRUint8
des_setkeyparity(PRUint8 x)
{
  if ((((x >> 7) ^ (x >> 6) ^ (x >> 5) ^
        (x >> 4) ^ (x >> 3) ^ (x >> 2) ^
        (x >> 1)) & 0x01) == 0)
    x |= 0x01;
  else
    x &= 0xfe;
  return x;
}

// build 64-bit des key from 56-bit raw key
static void
des_makekey(const PRUint8 *raw, PRUint8 *key)
{
  key[0] = des_setkeyparity(raw[0]);
  key[1] = des_setkeyparity((raw[0] << 7) | (raw[1] >> 1));
  key[2] = des_setkeyparity((raw[1] << 6) | (raw[2] >> 2));
  key[3] = des_setkeyparity((raw[2] << 5) | (raw[3] >> 3));
  key[4] = des_setkeyparity((raw[3] << 4) | (raw[4] >> 4));
  key[5] = des_setkeyparity((raw[4] << 3) | (raw[5] >> 5));
  key[6] = des_setkeyparity((raw[5] << 2) | (raw[6] >> 6));
  key[7] = des_setkeyparity((raw[6] << 1));
}

// run des encryption algorithm (using NSS)
static void
des_encrypt(const PRUint8 *key, const PRUint8 *src, PRUint8 *hash)
{
  CK_MECHANISM_TYPE cipherMech = CKM_DES_ECB;
  PK11SlotInfo *slot = nsnull;
  PK11SymKey *symkey = nsnull;
  PK11Context *ctxt = nsnull;
  SECItem keyItem, *param = nsnull;
  SECStatus rv;
  unsigned int n;
  
  slot = PK11_GetBestSlot(cipherMech, nsnull);
  if (!slot)
  {
    NS_ERROR("no slot");
    goto done;
  }

  keyItem.data = (PRUint8 *) key;
  keyItem.len = 8;
  symkey = PK11_ImportSymKey(slot, cipherMech,
                             PK11_OriginUnwrap, CKA_ENCRYPT,
                             &keyItem, nsnull);
  if (!symkey)
  {
    NS_ERROR("no symkey");
    goto done;
  }

  // no initialization vector required
  param = PK11_ParamFromIV(cipherMech, nsnull);
  if (!param)
  {
    NS_ERROR("no param");
    goto done;
  }

  ctxt = PK11_CreateContextBySymKey(cipherMech, CKA_ENCRYPT,
                                    symkey, param);
  if (!ctxt)
  {
    NS_ERROR("no context");
    goto done;
  }

  rv = PK11_CipherOp(ctxt, hash, (int *) &n, 8, (PRUint8 *) src, 8);
  if (rv != SECSuccess)
  {
    NS_ERROR("des failure");
    goto done;
  }

  rv = PK11_DigestFinal(ctxt, hash+8, &n, 0);
  if (rv != SECSuccess)
  {
    NS_ERROR("des failure");
    goto done;
  }

done:
  if (ctxt)
    PK11_DestroyContext(ctxt, true);
  if (symkey)
    PK11_FreeSymKey(symkey);
  if (param)
    SECITEM_FreeItem(param, true);
  if (slot)
    PK11_FreeSlot(slot);
}

//-----------------------------------------------------------------------------
// MD5 support code

static void md5sum(const PRUint8 *input, PRUint32 inputLen, PRUint8 *result)
{
  PK11Context *ctxt = PK11_CreateDigestContext(SEC_OID_MD5);
  if (ctxt)
  {
    if (PK11_DigestBegin(ctxt) == SECSuccess)
    {
      if (PK11_DigestOp(ctxt, input, inputLen) == SECSuccess)
      {
        PRUint32 resultLen = 16;
        PK11_DigestFinal(ctxt, result, &resultLen, resultLen);
      }
    }
    PK11_DestroyContext(ctxt, true);
  }
}
