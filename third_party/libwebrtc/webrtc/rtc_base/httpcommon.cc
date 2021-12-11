/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <time.h>

#if defined(WEBRTC_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define SECURITY_WIN32
#include <security.h>
#endif

#include <algorithm>

#include "rtc_base/arraysize.h"
#include "rtc_base/base64.h"
#include "rtc_base/checks.h"
#include "rtc_base/cryptstring.h"
#include "rtc_base/httpcommon-inl.h"
#include "rtc_base/httpcommon.h"
#include "rtc_base/messagedigest.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/stringencode.h"
#include "rtc_base/stringutils.h"

namespace rtc {

#if defined(WEBRTC_WIN)
extern const ConstantLabel SECURITY_ERRORS[];
#endif

//////////////////////////////////////////////////////////////////////
// Enum - TODO: expose globally later?
//////////////////////////////////////////////////////////////////////

bool find_string(size_t& index, const std::string& needle,
                 const char* const haystack[], size_t max_index) {
  for (index=0; index<max_index; ++index) {
    if (_stricmp(needle.c_str(), haystack[index]) == 0) {
      return true;
    }
  }
  return false;
}

template<class E>
struct Enum {
  static const char** Names;
  static size_t Size;

  static inline const char* Name(E val) { return Names[val]; }
  static inline bool Parse(E& val, const std::string& name) {
    size_t index;
    if (!find_string(index, name, Names, Size))
      return false;
    val = static_cast<E>(index);
    return true;
  }

  E val;

  inline operator E&() { return val; }
  inline Enum& operator=(E rhs) { val = rhs; return *this; }

  inline const char* name() const { return Name(val); }
  inline bool assign(const std::string& name) { return Parse(val, name); }
  inline Enum& operator=(const std::string& rhs) { assign(rhs); return *this; }
};

#define ENUM(e,n) \
  template<> const char** Enum<e>::Names = n; \
  template<> size_t Enum<e>::Size = sizeof(n)/sizeof(n[0])

//////////////////////////////////////////////////////////////////////
// HttpCommon
//////////////////////////////////////////////////////////////////////

static const char* kHttpVersions[HVER_LAST+1] = {
  "1.0", "1.1", "Unknown"
};
ENUM(HttpVersion, kHttpVersions);

static const char* kHttpVerbs[HV_LAST+1] = {
  "GET", "POST", "PUT", "DELETE", "CONNECT", "HEAD"
};
ENUM(HttpVerb, kHttpVerbs);

static const char* kHttpHeaders[HH_LAST+1] = {
  "Age",
  "Cache-Control",
  "Connection",
  "Content-Disposition",
  "Content-Length",
  "Content-Range",
  "Content-Type",
  "Cookie",
  "Date",
  "ETag",
  "Expires",
  "Host",
  "If-Modified-Since",
  "If-None-Match",
  "Keep-Alive",
  "Last-Modified",
  "Location",
  "Proxy-Authenticate",
  "Proxy-Authorization",
  "Proxy-Connection",
  "Range",
  "Set-Cookie",
  "TE",
  "Trailers",
  "Transfer-Encoding",
  "Upgrade",
  "User-Agent",
  "WWW-Authenticate",
};
ENUM(HttpHeader, kHttpHeaders);

const char* ToString(HttpVersion version) {
  return Enum<HttpVersion>::Name(version);
}

bool FromString(HttpVersion& version, const std::string& str) {
  return Enum<HttpVersion>::Parse(version, str);
}

const char* ToString(HttpVerb verb) {
  return Enum<HttpVerb>::Name(verb);
}

bool FromString(HttpVerb& verb, const std::string& str) {
  return Enum<HttpVerb>::Parse(verb, str);
}

const char* ToString(HttpHeader header) {
  return Enum<HttpHeader>::Name(header);
}

bool FromString(HttpHeader& header, const std::string& str) {
  return Enum<HttpHeader>::Parse(header, str);
}

bool HttpCodeHasBody(uint32_t code) {
  return !HttpCodeIsInformational(code)
         && (code != HC_NO_CONTENT) && (code != HC_NOT_MODIFIED);
}

bool HttpCodeIsCacheable(uint32_t code) {
  switch (code) {
  case HC_OK:
  case HC_NON_AUTHORITATIVE:
  case HC_PARTIAL_CONTENT:
  case HC_MULTIPLE_CHOICES:
  case HC_MOVED_PERMANENTLY:
  case HC_GONE:
    return true;
  default:
    return false;
  }
}

bool HttpHeaderIsEndToEnd(HttpHeader header) {
  switch (header) {
  case HH_CONNECTION:
  case HH_KEEP_ALIVE:
  case HH_PROXY_AUTHENTICATE:
  case HH_PROXY_AUTHORIZATION:
  case HH_PROXY_CONNECTION:  // Note part of RFC... this is non-standard header
  case HH_TE:
  case HH_TRAILERS:
  case HH_TRANSFER_ENCODING:
  case HH_UPGRADE:
    return false;
  default:
    return true;
  }
}

bool HttpHeaderIsCollapsible(HttpHeader header) {
  switch (header) {
  case HH_SET_COOKIE:
  case HH_PROXY_AUTHENTICATE:
  case HH_WWW_AUTHENTICATE:
    return false;
  default:
    return true;
  }
}

bool HttpShouldKeepAlive(const HttpData& data) {
  std::string connection;
  if ((data.hasHeader(HH_PROXY_CONNECTION, &connection)
      || data.hasHeader(HH_CONNECTION, &connection))) {
    return (_stricmp(connection.c_str(), "Keep-Alive") == 0);
  }
  return (data.version >= HVER_1_1);
}

namespace {

inline bool IsEndOfAttributeName(size_t pos, size_t len, const char * data) {
  if (pos >= len)
    return true;
  if (isspace(static_cast<unsigned char>(data[pos])))
    return true;
  // The reason for this complexity is that some attributes may contain trailing
  // equal signs (like base64 tokens in Negotiate auth headers)
  if ((pos+1 < len) && (data[pos] == '=') &&
      !isspace(static_cast<unsigned char>(data[pos+1])) &&
      (data[pos+1] != '=')) {
    return true;
  }
  return false;
}

// TODO: unittest for EscapeAttribute and HttpComposeAttributes.

std::string EscapeAttribute(const std::string& attribute) {
  const size_t kMaxLength = attribute.length() * 2 + 1;
  char* buffer = STACK_ARRAY(char, kMaxLength);
  size_t len = escape(buffer, kMaxLength, attribute.data(), attribute.length(),
                      "\"", '\\');
  return std::string(buffer, len);
}

}  // anonymous namespace

void HttpComposeAttributes(const HttpAttributeList& attributes, char separator,
                           std::string* composed) {
  std::stringstream ss;
  for (size_t i=0; i<attributes.size(); ++i) {
    if (i > 0) {
      ss << separator << " ";
    }
    ss << attributes[i].first;
    if (!attributes[i].second.empty()) {
      ss << "=\"" << EscapeAttribute(attributes[i].second) << "\"";
    }
  }
  *composed = ss.str();
}

void HttpParseAttributes(const char * data, size_t len,
                         HttpAttributeList& attributes) {
  size_t pos = 0;
  while (true) {
    // Skip leading whitespace
    while ((pos < len) && isspace(static_cast<unsigned char>(data[pos]))) {
      ++pos;
    }

    // End of attributes?
    if (pos >= len)
      return;

    // Find end of attribute name
    size_t start = pos;
    while (!IsEndOfAttributeName(pos, len, data)) {
      ++pos;
    }

    HttpAttribute attribute;
    attribute.first.assign(data + start, data + pos);

    // Attribute has value?
    if ((pos < len) && (data[pos] == '=')) {
      ++pos; // Skip '='
      // Check if quoted value
      if ((pos < len) && (data[pos] == '"')) {
        while (++pos < len) {
          if (data[pos] == '"') {
            ++pos;
            break;
          }
          if ((data[pos] == '\\') && (pos + 1 < len))
            ++pos;
          attribute.second.append(1, data[pos]);
        }
      } else {
        while ((pos < len) &&
            !isspace(static_cast<unsigned char>(data[pos])) &&
            (data[pos] != ',')) {
          attribute.second.append(1, data[pos++]);
        }
      }
    }

    attributes.push_back(attribute);
    if ((pos < len) && (data[pos] == ',')) ++pos; // Skip ','
  }
}

bool HttpHasAttribute(const HttpAttributeList& attributes,
                      const std::string& name,
                      std::string* value) {
  for (HttpAttributeList::const_iterator it = attributes.begin();
       it != attributes.end(); ++it) {
    if (it->first == name) {
      if (value) {
        *value = it->second;
      }
      return true;
    }
  }
  return false;
}

bool HttpHasNthAttribute(HttpAttributeList& attributes,
                         size_t index,
                         std::string* name,
                         std::string* value) {
  if (index >= attributes.size())
    return false;

  if (name)
    *name = attributes[index].first;
  if (value)
    *value = attributes[index].second;
  return true;
}

bool HttpDateToSeconds(const std::string& date, time_t* seconds) {
  const char* const kTimeZones[] = {
    "UT", "GMT", "EST", "EDT", "CST", "CDT", "MST", "MDT", "PST", "PDT",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "K", "L", "M",
    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y"
  };
  const int kTimeZoneOffsets[] = {
     0,  0, -5, -4, -6, -5, -7, -6, -8, -7,
    -1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12,
     1,  2,  3,  4,  5,  6,  7,  8,  9,  10,  11,  12
  };

  RTC_DCHECK(nullptr != seconds);
  struct tm tval;
  memset(&tval, 0, sizeof(tval));
  char month[4], zone[6];
  memset(month, 0, sizeof(month));
  memset(zone, 0, sizeof(zone));

  if (7 != sscanf(date.c_str(), "%*3s, %d %3s %d %d:%d:%d %5c",
                  &tval.tm_mday, month, &tval.tm_year,
                  &tval.tm_hour, &tval.tm_min, &tval.tm_sec, zone)) {
    return false;
  }
  switch (toupper(month[2])) {
  case 'N': tval.tm_mon = (month[1] == 'A') ? 0 : 5; break;
  case 'B': tval.tm_mon = 1; break;
  case 'R': tval.tm_mon = (month[0] == 'M') ? 2 : 3; break;
  case 'Y': tval.tm_mon = 4; break;
  case 'L': tval.tm_mon = 6; break;
  case 'G': tval.tm_mon = 7; break;
  case 'P': tval.tm_mon = 8; break;
  case 'T': tval.tm_mon = 9; break;
  case 'V': tval.tm_mon = 10; break;
  case 'C': tval.tm_mon = 11; break;
  }
  tval.tm_year -= 1900;
  time_t gmt, non_gmt = mktime(&tval);
  if ((zone[0] == '+') || (zone[0] == '-')) {
    if (!isdigit(zone[1]) || !isdigit(zone[2])
        || !isdigit(zone[3]) || !isdigit(zone[4])) {
      return false;
    }
    int hours = (zone[1] - '0') * 10 + (zone[2] - '0');
    int minutes = (zone[3] - '0') * 10 + (zone[4] - '0');
    int offset = (hours * 60 + minutes) * 60;
    gmt = non_gmt + ((zone[0] == '+') ? offset : -offset);
  } else {
    size_t zindex;
    if (!find_string(zindex, zone, kTimeZones, arraysize(kTimeZones))) {
      return false;
    }
    gmt = non_gmt + kTimeZoneOffsets[zindex] * 60 * 60;
  }
  // TODO: Android should support timezone, see b/2441195
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS) || defined(WEBRTC_ANDROID) || defined(WEBRTC_BSD)
  tm *tm_for_timezone = localtime(&gmt);
  *seconds = gmt + tm_for_timezone->tm_gmtoff;
#else
#if defined(_MSC_VER) && _MSC_VER >= 1900
  long timezone = 0;
  _get_timezone(&timezone);
#endif
  *seconds = gmt - timezone;
#endif
  return true;
}

std::string HttpAddress(const SocketAddress& address, bool secure) {
  return (address.port() == HttpDefaultPort(secure))
          ? address.hostname() : address.ToString();
}

//////////////////////////////////////////////////////////////////////
// HttpData
//////////////////////////////////////////////////////////////////////

HttpData::HttpData() : version(HVER_1_1) {
}

HttpData::~HttpData() = default;

void
HttpData::clear(bool release_document) {
  // Clear headers first, since releasing a document may have far-reaching
  // effects.
  headers_.clear();
  if (release_document) {
    document.reset();
  }
}

void
HttpData::copy(const HttpData& src) {
  headers_ = src.headers_;
}

void
HttpData::changeHeader(const std::string& name, const std::string& value,
                       HeaderCombine combine) {
  if (combine == HC_AUTO) {
    HttpHeader header;
    // Unrecognized headers are collapsible
    combine = !FromString(header, name) || HttpHeaderIsCollapsible(header)
              ? HC_YES : HC_NO;
  } else if (combine == HC_REPLACE) {
    headers_.erase(name);
    combine = HC_NO;
  }
  // At this point, combine is one of (YES, NO, NEW)
  if (combine != HC_NO) {
    HeaderMap::iterator it = headers_.find(name);
    if (it != headers_.end()) {
      if (combine == HC_YES) {
        it->second.append(",");
        it->second.append(value);
      }
      return;
    }
  }
  headers_.insert(HeaderMap::value_type(name, value));
}

size_t HttpData::clearHeader(const std::string& name) {
  return headers_.erase(name);
}

HttpData::iterator HttpData::clearHeader(iterator header) {
  iterator deprecated = header++;
  headers_.erase(deprecated);
  return header;
}

bool
HttpData::hasHeader(const std::string& name, std::string* value) const {
  HeaderMap::const_iterator it = headers_.find(name);
  if (it == headers_.end()) {
    return false;
  } else if (value) {
    *value = it->second;
  }
  return true;
}

void HttpData::setContent(const std::string& content_type,
                          StreamInterface* document) {
  setHeader(HH_CONTENT_TYPE, content_type);
  setDocumentAndLength(document);
}

void HttpData::setDocumentAndLength(StreamInterface* document) {
  // TODO: Consider calling Rewind() here?
  RTC_DCHECK(!hasHeader(HH_CONTENT_LENGTH, nullptr));
  RTC_DCHECK(!hasHeader(HH_TRANSFER_ENCODING, nullptr));
  RTC_DCHECK(document != nullptr);
  this->document.reset(document);
  size_t content_length = 0;
  if (this->document->GetAvailable(&content_length)) {
    char buffer[32];
    sprintfn(buffer, sizeof(buffer), "%d", content_length);
    setHeader(HH_CONTENT_LENGTH, buffer);
  } else {
    setHeader(HH_TRANSFER_ENCODING, "chunked");
  }
}

//
// HttpRequestData
//

void
HttpRequestData::clear(bool release_document) {
  verb = HV_GET;
  path.clear();
  HttpData::clear(release_document);
}

void
HttpRequestData::copy(const HttpRequestData& src) {
  verb = src.verb;
  path = src.path;
  HttpData::copy(src);
}

size_t
HttpRequestData::formatLeader(char* buffer, size_t size) const {
  RTC_DCHECK(path.find(' ') == std::string::npos);
  return sprintfn(buffer, size, "%s %.*s HTTP/%s", ToString(verb), path.size(),
                  path.data(), ToString(version));
}

HttpError
HttpRequestData::parseLeader(const char* line, size_t len) {
  unsigned int vmajor, vminor;
  int vend, dstart, dend;
  // sscanf isn't safe with strings that aren't null-terminated, and there is
  // no guarantee that |line| is. Create a local copy that is null-terminated.
  std::string line_str(line, len);
  line = line_str.c_str();
  if ((sscanf(line, "%*s%n %n%*s%n HTTP/%u.%u",
              &vend, &dstart, &dend, &vmajor, &vminor) != 2)
      || (vmajor != 1)) {
    return HE_PROTOCOL;
  }
  if (vminor == 0) {
    version = HVER_1_0;
  } else if (vminor == 1) {
    version = HVER_1_1;
  } else {
    return HE_PROTOCOL;
  }
  std::string sverb(line, vend);
  if (!FromString(verb, sverb.c_str())) {
    return HE_PROTOCOL; // !?! HC_METHOD_NOT_SUPPORTED?
  }
  path.assign(line + dstart, line + dend);
  return HE_NONE;
}

bool HttpRequestData::getAbsoluteUri(std::string* uri) const {
  if (HV_CONNECT == verb)
    return false;
  Url<char> url(path);
  if (url.valid()) {
    uri->assign(path);
    return true;
  }
  std::string host;
  if (!hasHeader(HH_HOST, &host))
    return false;
  url.set_address(host);
  url.set_full_path(path);
  uri->assign(url.url());
  return url.valid();
}

bool HttpRequestData::getRelativeUri(std::string* host,
                                     std::string* path) const
{
  if (HV_CONNECT == verb)
    return false;
  Url<char> url(this->path);
  if (url.valid()) {
    host->assign(url.address());
    path->assign(url.full_path());
    return true;
  }
  if (!hasHeader(HH_HOST, host))
    return false;
  path->assign(this->path);
  return true;
}

//
// HttpResponseData
//

void
HttpResponseData::clear(bool release_document) {
  scode = HC_INTERNAL_SERVER_ERROR;
  message.clear();
  HttpData::clear(release_document);
}

void
HttpResponseData::copy(const HttpResponseData& src) {
  scode = src.scode;
  message = src.message;
  HttpData::copy(src);
}

void HttpResponseData::set_success(uint32_t scode) {
  this->scode = scode;
  message.clear();
  setHeader(HH_CONTENT_LENGTH, "0", false);
}

void HttpResponseData::set_success(const std::string& content_type,
                                   StreamInterface* document,
                                   uint32_t scode) {
  this->scode = scode;
  message.erase(message.begin(), message.end());
  setContent(content_type, document);
}

void HttpResponseData::set_redirect(const std::string& location,
                                    uint32_t scode) {
  this->scode = scode;
  message.clear();
  setHeader(HH_LOCATION, location);
  setHeader(HH_CONTENT_LENGTH, "0", false);
}

void HttpResponseData::set_error(uint32_t scode) {
  this->scode = scode;
  message.clear();
  setHeader(HH_CONTENT_LENGTH, "0", false);
}

size_t
HttpResponseData::formatLeader(char* buffer, size_t size) const {
  size_t len = sprintfn(buffer, size, "HTTP/%s %lu", ToString(version), scode);
  if (!message.empty()) {
    len += sprintfn(buffer + len, size - len, " %.*s",
                    message.size(), message.data());
  }
  return len;
}

HttpError
HttpResponseData::parseLeader(const char* line, size_t len) {
  size_t pos = 0;
  unsigned int vmajor, vminor, temp_scode;
  int temp_pos;
  // sscanf isn't safe with strings that aren't null-terminated, and there is
  // no guarantee that |line| is. Create a local copy that is null-terminated.
  std::string line_str(line, len);
  line = line_str.c_str();
  if (sscanf(line, "HTTP %u%n",
             &temp_scode, &temp_pos) == 1) {
    // This server's response has no version. :( NOTE: This happens for every
    // response to requests made from Chrome plugins, regardless of the server's
    // behaviour.
    RTC_LOG(LS_VERBOSE) << "HTTP version missing from response";
    version = HVER_UNKNOWN;
  } else if ((sscanf(line, "HTTP/%u.%u %u%n",
                     &vmajor, &vminor, &temp_scode, &temp_pos) == 3)
             && (vmajor == 1)) {
    // This server's response does have a version.
    if (vminor == 0) {
      version = HVER_1_0;
    } else if (vminor == 1) {
      version = HVER_1_1;
    } else {
      return HE_PROTOCOL;
    }
  } else {
    return HE_PROTOCOL;
  }
  scode = temp_scode;
  pos = static_cast<size_t>(temp_pos);
  while ((pos < len) && isspace(static_cast<unsigned char>(line[pos]))) ++pos;
  message.assign(line + pos, len - pos);
  return HE_NONE;
}

//////////////////////////////////////////////////////////////////////
// Http Authentication
//////////////////////////////////////////////////////////////////////

std::string quote(const std::string& str) {
  std::string result;
  result.push_back('"');
  for (size_t i=0; i<str.size(); ++i) {
    if ((str[i] == '"') || (str[i] == '\\'))
      result.push_back('\\');
    result.push_back(str[i]);
  }
  result.push_back('"');
  return result;
}

#if defined(WEBRTC_WIN)
struct NegotiateAuthContext : public HttpAuthContext {
  CredHandle cred;
  CtxtHandle ctx;
  size_t steps;
  bool specified_credentials;

  NegotiateAuthContext(const std::string& auth, CredHandle c1, CtxtHandle c2)
  : HttpAuthContext(auth), cred(c1), ctx(c2), steps(0),
    specified_credentials(false)
  { }

  ~NegotiateAuthContext() override {
    DeleteSecurityContext(&ctx);
    FreeCredentialsHandle(&cred);
  }
};
#endif // WEBRTC_WIN

HttpAuthResult HttpAuthenticate(
  const char * challenge, size_t len,
  const SocketAddress& server,
  const std::string& method, const std::string& uri,
  const std::string& username, const CryptString& password,
  HttpAuthContext *& context, std::string& response, std::string& auth_method)
{
  HttpAttributeList args;
  HttpParseAttributes(challenge, len, args);
  HttpHasNthAttribute(args, 0, &auth_method, nullptr);

  if (context && (context->auth_method != auth_method))
    return HAR_IGNORE;

  // BASIC
  if (_stricmp(auth_method.c_str(), "basic") == 0) {
    if (context)
      return HAR_CREDENTIALS; // Bad credentials
    if (username.empty())
      return HAR_CREDENTIALS; // Missing credentials

    context = new HttpAuthContext(auth_method);

    // TODO: convert sensitive to a secure buffer that gets securely deleted
    //std::string decoded = username + ":" + password;
    size_t len = username.size() + password.GetLength() + 2;
    char * sensitive = new char[len];
    size_t pos = strcpyn(sensitive, len, username.data(), username.size());
    pos += strcpyn(sensitive + pos, len - pos, ":");
    password.CopyTo(sensitive + pos, true);

    response = auth_method;
    response.append(" ");
    // TODO: create a sensitive-source version of Base64::encode
    response.append(Base64::Encode(sensitive));
    memset(sensitive, 0, len);
    delete [] sensitive;
    return HAR_RESPONSE;
  }

  // DIGEST
  if (_stricmp(auth_method.c_str(), "digest") == 0) {
    if (context)
      return HAR_CREDENTIALS; // Bad credentials
    if (username.empty())
      return HAR_CREDENTIALS; // Missing credentials

    context = new HttpAuthContext(auth_method);

    std::string cnonce, ncount;
    char buffer[256];
    sprintf(buffer, "%d", static_cast<int>(time(0)));
    cnonce = MD5(buffer);
    ncount = "00000001";

    std::string realm, nonce, qop, opaque;
    HttpHasAttribute(args, "realm", &realm);
    HttpHasAttribute(args, "nonce", &nonce);
    bool has_qop = HttpHasAttribute(args, "qop", &qop);
    bool has_opaque = HttpHasAttribute(args, "opaque", &opaque);

    // TODO: convert sensitive to be secure buffer
    //std::string A1 = username + ":" + realm + ":" + password;
    size_t len = username.size() + realm.size() + password.GetLength() + 3;
    char * sensitive = new char[len];  // A1
    size_t pos = strcpyn(sensitive, len, username.data(), username.size());
    pos += strcpyn(sensitive + pos, len - pos, ":");
    pos += strcpyn(sensitive + pos, len - pos, realm.c_str());
    pos += strcpyn(sensitive + pos, len - pos, ":");
    password.CopyTo(sensitive + pos, true);

    std::string A2 = method + ":" + uri;
    std::string middle;
    if (has_qop) {
      qop = "auth";
      middle = nonce + ":" + ncount + ":" + cnonce + ":" + qop;
    } else {
      middle = nonce;
    }
    std::string HA1 = MD5(sensitive);
    memset(sensitive, 0, len);
    delete [] sensitive;
    std::string HA2 = MD5(A2);
    std::string dig_response = MD5(HA1 + ":" + middle + ":" + HA2);

    std::stringstream ss;
    ss << auth_method;
    ss << " username=" << quote(username);
    ss << ", realm=" << quote(realm);
    ss << ", nonce=" << quote(nonce);
    ss << ", uri=" << quote(uri);
    if (has_qop) {
      ss << ", qop=" << qop;
      ss << ", nc="  << ncount;
      ss << ", cnonce=" << quote(cnonce);
    }
    ss << ", response=\"" << dig_response << "\"";
    if (has_opaque) {
      ss << ", opaque=" << quote(opaque);
    }
    response = ss.str();
    return HAR_RESPONSE;
  }

#if defined(WEBRTC_WIN)
#if 1
  bool want_negotiate = (_stricmp(auth_method.c_str(), "negotiate") == 0);
  bool want_ntlm = (_stricmp(auth_method.c_str(), "ntlm") == 0);
  // SPNEGO & NTLM
  if (want_negotiate || want_ntlm) {
    const size_t MAX_MESSAGE = 12000, MAX_SPN = 256;
    char out_buf[MAX_MESSAGE], spn[MAX_SPN];

#if 0 // Requires funky windows versions
    DWORD len = MAX_SPN;
    if (DsMakeSpn("HTTP", server.HostAsURIString().c_str(), nullptr,
                  server.port(),
                  0, &len, spn) != ERROR_SUCCESS) {
      RTC_LOG_F(WARNING) << "(Negotiate) - DsMakeSpn failed";
      return HAR_IGNORE;
    }
#else
    sprintfn(spn, MAX_SPN, "HTTP/%s", server.ToString().c_str());
#endif

    SecBuffer out_sec;
    out_sec.pvBuffer   = out_buf;
    out_sec.cbBuffer   = sizeof(out_buf);
    out_sec.BufferType = SECBUFFER_TOKEN;

    SecBufferDesc out_buf_desc;
    out_buf_desc.ulVersion = 0;
    out_buf_desc.cBuffers  = 1;
    out_buf_desc.pBuffers  = &out_sec;

    const ULONG NEG_FLAGS_DEFAULT =
      //ISC_REQ_ALLOCATE_MEMORY
      ISC_REQ_CONFIDENTIALITY
      //| ISC_REQ_EXTENDED_ERROR
      //| ISC_REQ_INTEGRITY
      | ISC_REQ_REPLAY_DETECT
      | ISC_REQ_SEQUENCE_DETECT
      //| ISC_REQ_STREAM
      //| ISC_REQ_USE_SUPPLIED_CREDS
      ;

    ::TimeStamp lifetime;
    SECURITY_STATUS ret = S_OK;
    ULONG ret_flags = 0, flags = NEG_FLAGS_DEFAULT;

    bool specify_credentials = !username.empty();
    size_t steps = 0;

    // uint32_t now = Time();

    NegotiateAuthContext * neg = static_cast<NegotiateAuthContext *>(context);
    if (neg) {
      const size_t max_steps = 10;
      if (++neg->steps >= max_steps) {
        RTC_LOG(WARNING) << "AsyncHttpsProxySocket::Authenticate(Negotiate) "
                            "too many retries";
        return HAR_ERROR;
      }
      steps = neg->steps;

      std::string challenge, decoded_challenge;
      if (HttpHasNthAttribute(args, 1, &challenge, nullptr) &&
          Base64::Decode(challenge, Base64::DO_STRICT, &decoded_challenge,
                         nullptr)) {
        SecBuffer in_sec;
        in_sec.pvBuffer   = const_cast<char *>(decoded_challenge.data());
        in_sec.cbBuffer   = static_cast<unsigned long>(decoded_challenge.size());
        in_sec.BufferType = SECBUFFER_TOKEN;

        SecBufferDesc in_buf_desc;
        in_buf_desc.ulVersion = 0;
        in_buf_desc.cBuffers  = 1;
        in_buf_desc.pBuffers  = &in_sec;

        ret = InitializeSecurityContextA(&neg->cred, &neg->ctx, spn, flags, 0, SECURITY_NATIVE_DREP, &in_buf_desc, 0, &neg->ctx, &out_buf_desc, &ret_flags, &lifetime);
        // RTC_LOG(INFO) << "$$$ InitializeSecurityContext @ " <<
        // TimeSince(now);
        if (FAILED(ret)) {
          RTC_LOG(LS_ERROR) << "InitializeSecurityContext returned: "
                            << ErrorName(ret, SECURITY_ERRORS);
          return HAR_ERROR;
        }
      } else if (neg->specified_credentials) {
        // Try again with default credentials
        specify_credentials = false;
        delete context;
        context = neg = 0;
      } else {
        return HAR_CREDENTIALS;
      }
    }

    if (!neg) {
      unsigned char userbuf[256], passbuf[256], domainbuf[16];
      SEC_WINNT_AUTH_IDENTITY_A auth_id, * pauth_id = 0;
      if (specify_credentials) {
        memset(&auth_id, 0, sizeof(auth_id));
        size_t len = password.GetLength()+1;
        char * sensitive = new char[len];
        password.CopyTo(sensitive, true);
        std::string::size_type pos = username.find('\\');
        if (pos == std::string::npos) {
          auth_id.UserLength = static_cast<unsigned long>(
              std::min(sizeof(userbuf) - 1, username.size()));
          memcpy(userbuf, username.c_str(), auth_id.UserLength);
          userbuf[auth_id.UserLength] = 0;
          auth_id.DomainLength = 0;
          domainbuf[auth_id.DomainLength] = 0;
          auth_id.PasswordLength = static_cast<unsigned long>(
              std::min(sizeof(passbuf) - 1, password.GetLength()));
          memcpy(passbuf, sensitive, auth_id.PasswordLength);
          passbuf[auth_id.PasswordLength] = 0;
        } else {
          auth_id.UserLength = static_cast<unsigned long>(
              std::min(sizeof(userbuf) - 1, username.size() - pos - 1));
          memcpy(userbuf, username.c_str() + pos + 1, auth_id.UserLength);
          userbuf[auth_id.UserLength] = 0;
          auth_id.DomainLength =
              static_cast<unsigned long>(std::min(sizeof(domainbuf) - 1, pos));
          memcpy(domainbuf, username.c_str(), auth_id.DomainLength);
          domainbuf[auth_id.DomainLength] = 0;
          auth_id.PasswordLength = static_cast<unsigned long>(
              std::min(sizeof(passbuf) - 1, password.GetLength()));
          memcpy(passbuf, sensitive, auth_id.PasswordLength);
          passbuf[auth_id.PasswordLength] = 0;
        }
        memset(sensitive, 0, len);
        delete [] sensitive;
        auth_id.User = userbuf;
        auth_id.Domain = domainbuf;
        auth_id.Password = passbuf;
        auth_id.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;
        pauth_id = &auth_id;
        RTC_LOG(LS_VERBOSE)
            << "Negotiate protocol: Using specified credentials";
      } else {
        RTC_LOG(LS_VERBOSE) << "Negotiate protocol: Using default credentials";
      }

      CredHandle cred;
      ret = AcquireCredentialsHandleA(
          0, const_cast<char*>(want_negotiate ? NEGOSSP_NAME_A : NTLMSP_NAME_A),
          SECPKG_CRED_OUTBOUND, 0, pauth_id, 0, 0, &cred, &lifetime);
      // RTC_LOG(INFO) << "$$$ AcquireCredentialsHandle @ " << TimeSince(now);
      if (ret != SEC_E_OK) {
        RTC_LOG(LS_ERROR) << "AcquireCredentialsHandle error: "
                          << ErrorName(ret, SECURITY_ERRORS);
        return HAR_IGNORE;
      }

      //CSecBufferBundle<5, CSecBufferBase::FreeSSPI> sb_out;

      CtxtHandle ctx;
      ret = InitializeSecurityContextA(&cred, 0, spn, flags, 0, SECURITY_NATIVE_DREP, 0, 0, &ctx, &out_buf_desc, &ret_flags, &lifetime);
      // RTC_LOG(INFO) << "$$$ InitializeSecurityContext @ " << TimeSince(now);
      if (FAILED(ret)) {
        RTC_LOG(LS_ERROR) << "InitializeSecurityContext returned: "
                          << ErrorName(ret, SECURITY_ERRORS);
        FreeCredentialsHandle(&cred);
        return HAR_IGNORE;
      }

      RTC_DCHECK(!context);
      context = neg = new NegotiateAuthContext(auth_method, cred, ctx);
      neg->specified_credentials = specify_credentials;
      neg->steps = steps;
    }

    if ((ret == SEC_I_COMPLETE_NEEDED) || (ret == SEC_I_COMPLETE_AND_CONTINUE)) {
      ret = CompleteAuthToken(&neg->ctx, &out_buf_desc);
      // RTC_LOG(INFO) << "$$$ CompleteAuthToken @ " << TimeSince(now);
      RTC_LOG(LS_VERBOSE) << "CompleteAuthToken returned: "
                          << ErrorName(ret, SECURITY_ERRORS);
      if (FAILED(ret)) {
        return HAR_ERROR;
      }
    }

    // RTC_LOG(INFO) << "$$$ NEGOTIATE took " << TimeSince(now) << "ms";

    std::string decoded(out_buf, out_buf + out_sec.cbBuffer);
    response = auth_method;
    response.append(" ");
    response.append(Base64::Encode(decoded));
    return HAR_RESPONSE;
  }
#endif
#endif // WEBRTC_WIN

  return HAR_IGNORE;
}

//////////////////////////////////////////////////////////////////////

} // namespace rtc
