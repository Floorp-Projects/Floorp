/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NeqoHttp3Conn_h__
#define NeqoHttp3Conn_h__

#include <cstdint>
#include "mozilla/net/neqo_glue_ffi_generated.h"

namespace mozilla {
namespace net {

class NeqoHttp3Conn final {
 public:
  static nsresult Init(const nsACString& aOrigin, const nsACString& aAlpn,
                       const NetAddr& aLocalAddr, const NetAddr& aRemoteAddr,
                       uint32_t aMaxTableSize, uint16_t aMaxBlockedStreams,
                       uint64_t aMaxData, uint64_t aMaxStreamData,
                       bool aVersionNegotiation, bool aWebTransport,
                       const nsACString& aQlogDir, uint32_t aDatagramSize,
                       NeqoHttp3Conn** aConn) {
    return neqo_http3conn_new(&aOrigin, &aAlpn, &aLocalAddr, &aRemoteAddr,
                              aMaxTableSize, aMaxBlockedStreams, aMaxData,
                              aMaxStreamData, aVersionNegotiation,
                              aWebTransport, &aQlogDir, aDatagramSize,
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

  nsresult ProcessInput(const NetAddr& aRemoteAddr,
                        const nsTArray<uint8_t>& aPacket) {
    return neqo_http3conn_process_input(this, &aRemoteAddr, &aPacket);
  }

  bool ProcessOutput(nsACString* aRemoteAddr, uint16_t* aPort,
                     nsTArray<uint8_t>& aData, uint64_t* aTimeout) {
    aData.TruncateLength(0);
    return neqo_http3conn_process_output(this, aRemoteAddr, aPort, &aData,
                                         aTimeout);
  }

  nsresult GetEvent(Http3Event* aEvent, nsTArray<uint8_t>& aData) {
    return neqo_http3conn_event(this, aEvent, &aData);
  }

  nsresult Fetch(const nsACString& aMethod, const nsACString& aScheme,
                 const nsACString& aHost, const nsACString& aPath,
                 const nsACString& aHeaders, uint64_t* aStreamId,
                 uint8_t aUrgency, bool aIncremental) {
    return neqo_http3conn_fetch(this, &aMethod, &aScheme, &aHost, &aPath,
                                &aHeaders, aStreamId, aUrgency, aIncremental);
  }

  nsresult PriorityUpdate(uint64_t aStreamId, uint8_t aUrgency,
                          bool aIncremental) {
    return neqo_http3conn_priority_update(this, aStreamId, aUrgency,
                                          aIncremental);
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

  void CancelFetch(uint64_t aStreamId, uint64_t aError) {
    neqo_http3conn_cancel_fetch(this, aStreamId, aError);
  }

  void ResetStream(uint64_t aStreamId, uint64_t aError) {
    neqo_http3conn_reset_stream(this, aStreamId, aError);
  }

  void StreamStopSending(uint64_t aStreamId, uint64_t aError) {
    neqo_http3conn_stream_stop_sending(this, aStreamId, aError);
  }

  void SetResumptionToken(nsTArray<uint8_t>& aToken) {
    neqo_http3conn_set_resumption_token(this, &aToken);
  }

  void SetEchConfig(nsTArray<uint8_t>& aEchConfig) {
    neqo_http3conn_set_ech_config(this, &aEchConfig);
  }

  bool IsZeroRtt() { return neqo_http3conn_is_zero_rtt(this); }

  void AddRef() { neqo_http3conn_addref(this); }
  void Release() { neqo_http3conn_release(this); }

  void GetStats(Http3Stats* aStats) {
    return neqo_http3conn_get_stats(this, aStats);
  }

  nsresult CreateWebTransport(const nsACString& aHost, const nsACString& aPath,
                              const nsACString& aHeaders,
                              uint64_t* aSessionId) {
    return neqo_http3conn_webtransport_create_session(this, &aHost, &aPath,
                                                      &aHeaders, aSessionId);
  }

  nsresult CloseWebTransport(uint64_t aSessionId, uint32_t aError,
                             const nsACString& aMessage) {
    return neqo_http3conn_webtransport_close_session(this, aSessionId, aError,
                                                     &aMessage);
  }

  nsresult CreateWebTransportStream(uint64_t aSessionId,
                                    WebTransportStreamType aStreamType,
                                    uint64_t* aStreamId) {
    return neqo_http3conn_webtransport_create_stream(this, aSessionId,
                                                     aStreamType, aStreamId);
  }

  nsresult WebTransportSendDatagram(uint64_t aSessionId,
                                    nsTArray<uint8_t>& aData,
                                    uint64_t aTrackingId) {
    return neqo_http3conn_webtransport_send_datagram(this, aSessionId, &aData,
                                                     aTrackingId);
  }

  nsresult WebTransportMaxDatagramSize(uint64_t aSessionId, uint64_t* aResult) {
    return neqo_http3conn_webtransport_max_datagram_size(this, aSessionId,
                                                         aResult);
  }

  nsresult WebTransportSetSendOrder(uint64_t aSessionId,
                                    Maybe<int64_t> aSendOrder) {
    return neqo_http3conn_webtransport_set_sendorder(this, aSessionId,
                                                     aSendOrder.ptrOr(nullptr));
  }

 private:
  NeqoHttp3Conn() = delete;
  ~NeqoHttp3Conn() = delete;
  NeqoHttp3Conn(const NeqoHttp3Conn&) = delete;
  NeqoHttp3Conn& operator=(const NeqoHttp3Conn&) = delete;
};

}  // namespace net
}  // namespace mozilla

#endif
