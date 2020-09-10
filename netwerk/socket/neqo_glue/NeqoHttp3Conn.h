/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NeqoHttp3Conn_h__
#define NeqoHttp3Conn_h__

#include "mozilla/net/neqo_glue_ffi_generated.h"

namespace mozilla {
namespace net {

class NeqoHttp3Conn final {
 public:
  static nsresult Init(const nsACString& aOrigin, const nsACString& aAlpn,
                       const nsACString& aLocalAddr,
                       const nsACString& aRemoteAddr, uint32_t aMaxTableSize,
                       uint16_t aMaxBlockedStreams, NeqoHttp3Conn** aConn) {
    return neqo_http3conn_new(&aOrigin, &aAlpn, &aLocalAddr, &aRemoteAddr,
                              aMaxTableSize, aMaxBlockedStreams,
                              (const mozilla::net::NeqoHttp3Conn**)aConn);
  }

  void Close(uint64_t aError) { neqo_http3conn_close(this, aError); }

  nsresult GetSecInfo(NeqoSecretInfo* aSecInfo) {
    return neqo_http3conn_tls_info(this, aSecInfo);
  }

  nsresult PeerCertificateInfo(NeqoCertificateInfo* aCertInfo) {
    return neqo_http3conn_peer_certificate_info(this, aCertInfo);
  }

  void PeerAuthenticated(PRErrorCode aError) {
    neqo_http3conn_authenticated(this, aError);
  }

  void ProcessInput(uint8_t* aPacket, uint32_t aLen) {
    neqo_http3conn_process_input(this, aPacket, aLen);
  }

  uint64_t ProcessOutput() { return neqo_http3conn_process_output(this); }

  bool HasDataToSend() { return neqo_http3conn_has_data_to_send(this); }

  nsresult GetDataToSend(nsTArray<uint8_t>& aData) {
    aData.TruncateLength(0);
    return neqo_http3conn_get_data_to_send(this, &aData);
  }

  nsresult GetEvent(Http3Event* aEvent, nsTArray<uint8_t>& aHeaderData) {
    return neqo_http3conn_event(this, aEvent, &aHeaderData);
  }

  nsresult Fetch(const nsACString& aMethod, const nsACString& aScheme,
                 const nsACString& aHost, const nsACString& aPath,
                 const nsACString& aHeaders, uint64_t* aStreamId) {
    return neqo_http3conn_fetch(this, &aMethod, &aScheme, &aHost, &aPath,
                                &aHeaders, aStreamId);
  }

  nsresult SendRequestBody(uint64_t aStreamId, const uint8_t* aBuf,
                           uint32_t aCount, uint32_t* aCountRead) {
    return neqo_htttp3conn_send_request_body(this, aStreamId, aBuf, aCount,
                                             aCountRead);
  }

  // This closes only the sending side of a stream.
  nsresult CloseStream(uint64_t aStreamId) {
    return neqo_http3conn_close_stream(this, aStreamId);
  }

  nsresult ReadResponseData(uint64_t aStreamId, uint8_t* aBuf, uint32_t aLen,
                            uint32_t* aRead, bool* aFin) {
    return neqo_http3conn_read_response_data(this, aStreamId, aBuf, aLen, aRead,
                                             aFin);
  }

  void ResetStream(uint64_t aStreamId, uint64_t aError) {
    neqo_http3conn_reset_stream(this, aStreamId, aError);
  }

  void GetResumptionToken(nsTArray<uint8_t>& aToken) {
    neqo_http3conn_resumption_token(this, &aToken);
  }

  void SetResumptionToken(nsTArray<uint8_t>& aToken) {
    neqo_http3conn_set_resumption_token(this, &aToken);
  }

  bool IsZeroRtt() {
    return neqo_http3conn_is_zero_rtt(this);
  }

  nsrefcnt AddRef() { return neqo_http3conn_addref(this); }
  nsrefcnt Release() { return neqo_http3conn_release(this); }

 private:
  NeqoHttp3Conn() = delete;
  ~NeqoHttp3Conn() = delete;
  NeqoHttp3Conn(const NeqoHttp3Conn&) = delete;
  NeqoHttp3Conn& operator=(const NeqoHttp3Conn&) = delete;
};

}  // namespace net
}  // namespace mozilla

#endif
