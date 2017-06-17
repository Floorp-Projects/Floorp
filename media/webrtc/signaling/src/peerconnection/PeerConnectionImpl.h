/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _PEER_CONNECTION_IMPL_H_
#define _PEER_CONNECTION_IMPL_H_

#include <deque>
#include <string>
#include <vector>
#include <map>
#include <cmath>

#include "prlock.h"
#include "mozilla/RefPtr.h"
#include "nsWeakPtr.h"
#include "nsAutoPtr.h"
#include "nsIWeakReferenceUtils.h" // for the definition of nsWeakPtr
#include "IPeerConnection.h"
#include "sigslot.h"
#include "nricectx.h"
#include "nricemediastream.h"
#include "nsComponentManagerUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIUUIDGenerator.h"
#include "nsIThread.h"

#include "signaling/src/jsep/JsepSession.h"
#include "signaling/src/jsep/JsepSessionImpl.h"
#include "signaling/src/sdp/SdpMediaSection.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/PeerConnectionImplEnumsBinding.h"
#include "PrincipalChangeObserver.h"
#include "StreamTracks.h"

#include "mozilla/TimeStamp.h"
#include "mozilla/net/DataChannel.h"
#include "VideoUtils.h"
#include "VideoSegment.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "nsIPrincipal.h"
#include "mozilla/PeerIdentity.h"

namespace test {
#ifdef USE_FAKE_PCOBSERVER
class AFakePCObserver;
#endif
}

class nsGlobalWindow;
class nsDOMDataChannel;

namespace mozilla {
class DataChannel;
class DtlsIdentity;
class NrIceCtx;
class NrIceMediaStream;
class NrIceStunServer;
class NrIceTurnServer;
class MediaPipeline;

class DOMMediaStream;

namespace dom {
class RTCCertificate;
struct RTCConfiguration;
class RTCDTMFSender;
struct RTCIceServer;
struct RTCOfferOptions;
struct RTCRtpParameters;
class RTCRtpSender;
class MediaStreamTrack;

#ifdef USE_FAKE_PCOBSERVER
typedef test::AFakePCObserver PeerConnectionObserver;
typedef const char *PCObserverString;
#else
class PeerConnectionObserver;
typedef NS_ConvertUTF8toUTF16 PCObserverString;
#endif
}
}

#if defined(__cplusplus) && __cplusplus >= 201103L
typedef struct Timecard Timecard;
#else
#include "timecard.h"
#endif

// To preserve blame, convert nsresult to ErrorResult with wrappers. These macros
// help declare wrappers w/function being wrapped when there are no differences.

#define NS_IMETHODIMP_TO_ERRORRESULT(func, rv, ...) \
NS_IMETHODIMP func(__VA_ARGS__);                    \
void func (__VA_ARGS__, rv)

#define NS_IMETHODIMP_TO_ERRORRESULT_RETREF(resulttype, func, rv, ...) \
NS_IMETHODIMP func(__VA_ARGS__, resulttype **result);                  \
already_AddRefed<resulttype> func (__VA_ARGS__, rv)

struct MediaStreamTable;

namespace mozilla {

using mozilla::dom::PeerConnectionObserver;
using mozilla::dom::RTCConfiguration;
using mozilla::dom::RTCIceServer;
using mozilla::dom::RTCOfferOptions;
using mozilla::DOMMediaStream;
using mozilla::NrIceCtx;
using mozilla::NrIceMediaStream;
using mozilla::DtlsIdentity;
using mozilla::ErrorResult;
using mozilla::NrIceStunServer;
using mozilla::NrIceTurnServer;
using mozilla::PeerIdentity;

class PeerConnectionWrapper;
class PeerConnectionMedia;
class RemoteSourceStreamInfo;

// Uuid Generator
class PCUuidGenerator : public mozilla::JsepUuidGenerator {
 public:
  virtual bool Generate(std::string* idp) override;

 private:
  nsCOMPtr<nsIUUIDGenerator> mGenerator;
};

class PeerConnectionConfiguration
{
public:
  PeerConnectionConfiguration()
  : mBundlePolicy(kBundleBalanced),
    mIceTransportPolicy(NrIceCtx::ICE_POLICY_ALL) {}

  bool addStunServer(const std::string& addr, uint16_t port,
                     const char* transport)
  {
    UniquePtr<NrIceStunServer> server(NrIceStunServer::Create(addr, port, transport));
    if (!server) {
      return false;
    }
    addStunServer(*server);
    return true;
  }
  bool addTurnServer(const std::string& addr, uint16_t port,
                     const std::string& username,
                     const std::string& pwd,
                     const char* transport)
  {
    // TODO(ekr@rtfm.com): Need support for SASLprep for
    // username and password. Bug # ???
    std::vector<unsigned char> password(pwd.begin(), pwd.end());

    UniquePtr<NrIceTurnServer> server(NrIceTurnServer::Create(addr, port, username, password,
							      transport));
    if (!server) {
      return false;
    }
    addTurnServer(*server);
    return true;
  }
  void addStunServer(const NrIceStunServer& server) { mStunServers.push_back (server); }
  void addTurnServer(const NrIceTurnServer& server) { mTurnServers.push_back (server); }
  const std::vector<NrIceStunServer>& getStunServers() const { return mStunServers; }
  const std::vector<NrIceTurnServer>& getTurnServers() const { return mTurnServers; }
  void setBundlePolicy(JsepBundlePolicy policy) { mBundlePolicy = policy;}
  JsepBundlePolicy getBundlePolicy() const { return mBundlePolicy; }
  void setIceTransportPolicy(NrIceCtx::Policy policy) { mIceTransportPolicy = policy;}
  NrIceCtx::Policy getIceTransportPolicy() const { return mIceTransportPolicy; }

  nsresult Init(const RTCConfiguration& aSrc);
  nsresult AddIceServer(const RTCIceServer& aServer);

private:
  std::vector<NrIceStunServer> mStunServers;
  std::vector<NrIceTurnServer> mTurnServers;
  JsepBundlePolicy mBundlePolicy;
  NrIceCtx::Policy mIceTransportPolicy;
};

// Not an inner class so we can forward declare.
class RTCStatsQuery {
  public:
    explicit RTCStatsQuery(bool internalStats);
    ~RTCStatsQuery();

    nsAutoPtr<mozilla::dom::RTCStatsReportInternal> report;
    std::string error;
    // A timestamp to help with telemetry.
    mozilla::TimeStamp iceStartTime;
    // Just for convenience, maybe integrate into the report later
    bool failed;

  private:
    friend class PeerConnectionImpl;
    std::string pcName;
    bool internalStats;
    nsTArray<RefPtr<mozilla::MediaPipeline>> pipelines;
    RefPtr<NrIceCtx> iceCtx;
    bool grabAllLevels;
    DOMHighResTimeStamp now;
};

// Enter an API call and check that the state is OK,
// the PC isn't closed, etc.
#define PC_AUTO_ENTER_API_CALL(assert_ice_ready) \
    do { \
      /* do/while prevents res from conflicting with locals */    \
      nsresult res = CheckApiState(assert_ice_ready);             \
      if (NS_FAILED(res)) return res; \
    } while(0)
#define PC_AUTO_ENTER_API_CALL_VOID_RETURN(assert_ice_ready) \
    do { \
      /* do/while prevents res from conflicting with locals */    \
      nsresult res = CheckApiState(assert_ice_ready);             \
      if (NS_FAILED(res)) return; \
    } while(0)
#define PC_AUTO_ENTER_API_CALL_NO_CHECK() CheckThread()

class PeerConnectionImpl final : public nsISupports,
                                 public mozilla::DataChannelConnection::DataConnectionListener,
                                 public dom::PrincipalChangeObserver<dom::MediaStreamTrack>,
                                 public sigslot::has_slots<>
{
  struct Internal; // Avoid exposing c includes to bindings

public:
  explicit PeerConnectionImpl(const mozilla::dom::GlobalObject* aGlobal = nullptr);

  enum Error {
    kNoError                          = 0,
    kInvalidCandidate                 = 2,
    kInvalidMediastreamTrack          = 3,
    kInvalidState                     = 4,
    kInvalidSessionDescription        = 5,
    kIncompatibleSessionDescription   = 6,
    kIncompatibleMediaStreamTrack     = 8,
    kInternalError                    = 9
  };

  NS_DECL_THREADSAFE_ISUPPORTS

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, JS::MutableHandle<JSObject*> aReflector);

  static already_AddRefed<PeerConnectionImpl>
      Constructor(const mozilla::dom::GlobalObject& aGlobal, ErrorResult& rv);
  static PeerConnectionImpl* CreatePeerConnection();
  already_AddRefed<DOMMediaStream> MakeMediaStream();

  nsresult CreateRemoteSourceStreamInfo(RefPtr<RemoteSourceStreamInfo>* aInfo,
                                        const std::string& aId);

  // DataConnection observers
  void NotifyDataChannel(already_AddRefed<mozilla::DataChannel> aChannel)
    // PeerConnectionImpl only inherits from mozilla::DataChannelConnection
    // inside libxul.
    override
    ;

  // Get the media object
  const RefPtr<PeerConnectionMedia>& media() const {
    PC_AUTO_ENTER_API_CALL_NO_CHECK();
    return mMedia;
  }

  // Configure the ability to use localhost.
  void SetAllowIceLoopback(bool val) { mAllowIceLoopback = val; }
  bool GetAllowIceLoopback() const { return mAllowIceLoopback; }

  // Configure the ability to use IPV6 link-local addresses.
  void SetAllowIceLinkLocal(bool val) { mAllowIceLinkLocal = val; }
  bool GetAllowIceLinkLocal() const { return mAllowIceLinkLocal; }

  // Handle system to allow weak references to be passed through C code
  virtual const std::string& GetHandle();

  // Name suitable for exposing to content
  virtual const std::string& GetName();

  // ICE events
  void IceConnectionStateChange(NrIceCtx* ctx,
                                NrIceCtx::ConnectionState state);
  void IceGatheringStateChange(NrIceCtx* ctx,
                               NrIceCtx::GatheringState state);
  void UpdateDefaultCandidate(const std::string& defaultAddr,
                              uint16_t defaultPort,
                              const std::string& defaultRtcpAddr,
                              uint16_t defaultRtcpPort,
                              uint16_t level);
  void EndOfLocalCandidates(uint16_t level);
  void IceStreamReady(NrIceMediaStream *aStream);

  static void ListenThread(void *aData);
  static void ConnectThread(void *aData);

  // Get the main thread
  nsCOMPtr<nsIThread> GetMainThread() {
    PC_AUTO_ENTER_API_CALL_NO_CHECK();
    return mThread;
  }

  // Get the STS thread
  nsIEventTarget* GetSTSThread() {
    PC_AUTO_ENTER_API_CALL_NO_CHECK();
    return mSTSThread;
  }

  nsPIDOMWindowInner* GetWindow() const {
    PC_AUTO_ENTER_API_CALL_NO_CHECK();
    return mWindow;
  }

  // Initialize PeerConnection from a PeerConnectionConfiguration object
  // (used directly by unit-tests, and indirectly by the JS entry point)
  // This is necessary because RTCConfiguration can't be used by unit-tests
  nsresult Initialize(PeerConnectionObserver& aObserver,
                      nsGlobalWindow* aWindow,
                      const PeerConnectionConfiguration& aConfiguration,
                      nsISupports* aThread);

  // Initialize PeerConnection from an RTCConfiguration object (JS entrypoint)
  void Initialize(PeerConnectionObserver& aObserver,
                  nsGlobalWindow& aWindow,
                  const RTCConfiguration& aConfiguration,
                  nsISupports* aThread,
                  ErrorResult &rv);

  void SetCertificate(mozilla::dom::RTCCertificate& aCertificate);
  const RefPtr<mozilla::dom::RTCCertificate>& Certificate() const;
  // This is a hack to support external linkage.
  RefPtr<DtlsIdentity> Identity() const;

  NS_IMETHODIMP_TO_ERRORRESULT(CreateOffer, ErrorResult &rv,
                               const RTCOfferOptions& aOptions)
  {
    rv = CreateOffer(aOptions);
  }

  NS_IMETHODIMP CreateAnswer();
  void CreateAnswer(ErrorResult &rv)
  {
    rv = CreateAnswer();
  }

  NS_IMETHODIMP CreateOffer(
      const mozilla::JsepOfferOptions& aConstraints);

  NS_IMETHODIMP SetLocalDescription (int32_t aAction, const char* aSDP);

  void SetLocalDescription (int32_t aAction, const nsAString& aSDP, ErrorResult &rv)
  {
    rv = SetLocalDescription(aAction, NS_ConvertUTF16toUTF8(aSDP).get());
  }

  nsresult CreateNewRemoteTracks(RefPtr<PeerConnectionObserver>& aPco);

  void RemoveOldRemoteTracks(RefPtr<PeerConnectionObserver>& aPco);

  NS_IMETHODIMP SetRemoteDescription (int32_t aAction, const char* aSDP);

  void SetRemoteDescription (int32_t aAction, const nsAString& aSDP, ErrorResult &rv)
  {
    rv = SetRemoteDescription(aAction, NS_ConvertUTF16toUTF8(aSDP).get());
  }

  NS_IMETHODIMP_TO_ERRORRESULT(GetStats, ErrorResult &rv,
                               mozilla::dom::MediaStreamTrack *aSelector)
  {
    rv = GetStats(aSelector);
  }

  NS_IMETHODIMP AddIceCandidate(const char* aCandidate, const char* aMid,
                                unsigned short aLevel);

  void AddIceCandidate(const nsAString& aCandidate, const nsAString& aMid,
                       unsigned short aLevel, ErrorResult &rv)
  {
    rv = AddIceCandidate(NS_ConvertUTF16toUTF8(aCandidate).get(),
                         NS_ConvertUTF16toUTF8(aMid).get(), aLevel);
  }

  void UpdateNetworkState(bool online);

  NS_IMETHODIMP CloseStreams();

  void CloseStreams(ErrorResult &rv)
  {
    rv = CloseStreams();
  }

  NS_IMETHODIMP_TO_ERRORRESULT(AddTrack, ErrorResult &rv,
      mozilla::dom::MediaStreamTrack& aTrack,
      const mozilla::dom::Sequence<mozilla::OwningNonNull<DOMMediaStream>>& aStreams)
  {
    rv = AddTrack(aTrack, aStreams);
  }

  NS_IMETHODIMP_TO_ERRORRESULT(RemoveTrack, ErrorResult &rv,
                               mozilla::dom::MediaStreamTrack& aTrack)
  {
    rv = RemoveTrack(aTrack);
  }

  nsresult
  AddTrack(mozilla::dom::MediaStreamTrack& aTrack, DOMMediaStream& aStream);

  NS_IMETHODIMP_TO_ERRORRESULT(InsertDTMF, ErrorResult &rv,
                               dom::RTCRtpSender& sender,
                               const nsAString& tones,
                               uint32_t duration, uint32_t interToneGap) {
    rv = InsertDTMF(sender, tones, duration, interToneGap);
  }

  NS_IMETHODIMP_TO_ERRORRESULT(GetDTMFToneBuffer, ErrorResult &rv,
                               dom::RTCRtpSender& sender,
                               nsAString& outToneBuffer) {
    rv = GetDTMFToneBuffer(sender, outToneBuffer);
  }

  NS_IMETHODIMP_TO_ERRORRESULT(ReplaceTrack, ErrorResult &rv,
                               mozilla::dom::MediaStreamTrack& aThisTrack,
                               mozilla::dom::MediaStreamTrack& aWithTrack)
  {
    rv = ReplaceTrack(aThisTrack, aWithTrack);
  }

  NS_IMETHODIMP_TO_ERRORRESULT(SetParameters, ErrorResult &rv,
                               dom::MediaStreamTrack& aTrack,
                               const dom::RTCRtpParameters& aParameters)
  {
    rv = SetParameters(aTrack, aParameters);
  }

  NS_IMETHODIMP_TO_ERRORRESULT(GetParameters, ErrorResult &rv,
                               dom::MediaStreamTrack& aTrack,
                               dom::RTCRtpParameters& aOutParameters)
  {
    rv = GetParameters(aTrack, aOutParameters);
  }

  nsresult
  SetParameters(dom::MediaStreamTrack& aTrack,
                const std::vector<JsepTrack::JsConstraints>& aConstraints);

  nsresult
  GetParameters(dom::MediaStreamTrack& aTrack,
                std::vector<JsepTrack::JsConstraints>* aOutConstraints);

  // test-only: called from simulcast mochitests.
  NS_IMETHODIMP_TO_ERRORRESULT(AddRIDExtension, ErrorResult &rv,
                               dom::MediaStreamTrack& aRecvTrack,
                               unsigned short aExtensionId)
  {
    rv = AddRIDExtension(aRecvTrack, aExtensionId);
  }

  // test-only: called from simulcast mochitests.
  NS_IMETHODIMP_TO_ERRORRESULT(AddRIDFilter, ErrorResult& rv,
                               dom::MediaStreamTrack& aRecvTrack,
                               const nsAString& aRid)
  {
    rv = AddRIDFilter(aRecvTrack, aRid);
  }

  nsresult GetPeerIdentity(nsAString& peerIdentity)
  {
    if (mPeerIdentity) {
      peerIdentity = mPeerIdentity->ToString();
      return NS_OK;
    }

    peerIdentity.SetIsVoid(true);
    return NS_OK;
  }

  const PeerIdentity* GetPeerIdentity() const { return mPeerIdentity; }
  nsresult SetPeerIdentity(const nsAString& peerIdentity);

  const std::string& GetIdAsAscii() const
  {
    return mName;
  }

  nsresult GetId(nsAString& id)
  {
    id = NS_ConvertASCIItoUTF16(mName.c_str());
    return NS_OK;
  }

  nsresult SetId(const nsAString& id)
  {
    mName = NS_ConvertUTF16toUTF8(id).get();
    return NS_OK;
  }

  // this method checks to see if we've made a promise to protect media.
  bool PrivacyRequested() const { return mPrivacyRequested; }

  NS_IMETHODIMP GetFingerprint(char** fingerprint);
  void GetFingerprint(nsAString& fingerprint)
  {
    char *tmp;
    GetFingerprint(&tmp);
    fingerprint.AssignASCII(tmp);
    delete[] tmp;
  }

  NS_IMETHODIMP GetLocalDescription(char** aSDP);

  void GetLocalDescription(nsAString& aSDP)
  {
    char *tmp;
    GetLocalDescription(&tmp);
    aSDP.AssignASCII(tmp);
    delete[] tmp;
  }

  NS_IMETHODIMP GetRemoteDescription(char** aSDP);

  void GetRemoteDescription(nsAString& aSDP)
  {
    char *tmp;
    GetRemoteDescription(&tmp);
    aSDP.AssignASCII(tmp);
    delete[] tmp;
  }

  NS_IMETHODIMP SignalingState(mozilla::dom::PCImplSignalingState* aState);

  mozilla::dom::PCImplSignalingState SignalingState()
  {
    mozilla::dom::PCImplSignalingState state;
    SignalingState(&state);
    return state;
  }

  NS_IMETHODIMP IceConnectionState(
      mozilla::dom::PCImplIceConnectionState* aState);

  mozilla::dom::PCImplIceConnectionState IceConnectionState()
  {
    mozilla::dom::PCImplIceConnectionState state;
    IceConnectionState(&state);
    return state;
  }

  NS_IMETHODIMP IceGatheringState(
      mozilla::dom::PCImplIceGatheringState* aState);

  mozilla::dom::PCImplIceGatheringState IceGatheringState()
  {
    mozilla::dom::PCImplIceGatheringState state;
    IceGatheringState(&state);
    return state;
  }

  NS_IMETHODIMP Close();

  void Close(ErrorResult &rv)
  {
    rv = Close();
  }

  bool PluginCrash(uint32_t aPluginID,
                   const nsAString& aPluginName);

  void RecordEndOfCallTelemetry() const;

  nsresult InitializeDataChannel();

  NS_IMETHODIMP_TO_ERRORRESULT_RETREF(nsDOMDataChannel,
                                      CreateDataChannel, ErrorResult &rv,
                                      const nsAString& aLabel,
                                      const nsAString& aProtocol,
                                      uint16_t aType,
                                      bool outOfOrderAllowed,
                                      uint16_t aMaxTime,
                                      uint16_t aMaxNum,
                                      bool aExternalNegotiated,
                                      uint16_t aStream);

  NS_IMETHODIMP_TO_ERRORRESULT(GetLocalStreams, ErrorResult &rv,
                               nsTArray<RefPtr<DOMMediaStream > >& result)
  {
    rv = GetLocalStreams(result);
  }

  NS_IMETHODIMP_TO_ERRORRESULT(GetRemoteStreams, ErrorResult &rv,
                               nsTArray<RefPtr<DOMMediaStream > >& result)
  {
    rv = GetRemoteStreams(result);
  }

  // Called whenever something is unrecognized by the parser
  // May be called more than once and does not necessarily mean
  // that parsing was stopped, only that something was unrecognized.
  void OnSdpParseError(const char* errorMessage);

  // Called when OnLocal/RemoteDescriptionSuccess/Error
  // is called to start the list over.
  void ClearSdpParseErrorMessages();

  // Called to retreive the list of parsing errors.
  const std::vector<std::string> &GetSdpParseErrors();

  // Sets the RTC Signaling State
  void SetSignalingState_m(mozilla::dom::PCImplSignalingState aSignalingState,
                           bool rollback = false);

  // Updates the RTC signaling state based on the JsepSession state
  void UpdateSignalingState(bool rollback = false);

  bool IsClosed() const;
  // called when DTLS connects; we only need this once
  nsresult SetDtlsConnected(bool aPrivacyRequested);

  bool HasMedia() const;

  // initialize telemetry for when calls start
  void startCallTelem();

  nsresult BuildStatsQuery_m(
      mozilla::dom::MediaStreamTrack *aSelector,
      RTCStatsQuery *query);

  static nsresult ExecuteStatsQuery_s(RTCStatsQuery *query);

  // for monitoring changes in track ownership
  // PeerConnectionMedia can't do it because it doesn't know about principals
  virtual void PrincipalChanged(dom::MediaStreamTrack* aTrack) override;

  static std::string GetStreamId(const DOMMediaStream& aStream);
  static std::string GetTrackId(const dom::MediaStreamTrack& track);

  void OnMediaError(const std::string& aError);

private:
  virtual ~PeerConnectionImpl();
  PeerConnectionImpl(const PeerConnectionImpl&rhs);
  PeerConnectionImpl& operator=(PeerConnectionImpl);
  nsresult CalculateFingerprint(const std::string& algorithm,
                                std::vector<uint8_t>* fingerprint) const;
  nsresult ConfigureJsepSessionCodecs();

  NS_IMETHODIMP EnsureDataConnection(uint16_t aLocalPort, uint16_t aNumstreams,
                                     uint32_t aMaxMessageSize, bool aMMSSet);

  nsresult CloseInt();
  nsresult CheckApiState(bool assert_ice_ready) const;
  void CheckThread() const {
    MOZ_ASSERT(CheckThreadInt(), "Wrong thread");
  }
  bool CheckThreadInt() const {
    bool on;
    NS_ENSURE_SUCCESS(mThread->IsOnCurrentThread(&on), false);
    NS_ENSURE_TRUE(on, false);
    return true;
  }

  // test-only: called from AddRIDExtension and AddRIDFilter
  // for simulcast mochitests.
  RefPtr<MediaPipeline> GetMediaPipelineForTrack(
      dom::MediaStreamTrack& aRecvTrack);

  nsresult GetTimeSinceEpoch(DOMHighResTimeStamp *result);

  // Shut down media - called on main thread only
  void ShutdownMedia();

  void CandidateReady(const std::string& candidate, uint16_t level);
  void SendLocalIceCandidateToContent(uint16_t level,
                                      const std::string& mid,
                                      const std::string& candidate);

  nsresult GetDatachannelParameters(
      uint32_t* channels,
      uint16_t* localport,
      uint16_t* remoteport,
      uint32_t* maxmessagesize,
      bool*     mmsset,
      uint16_t* level) const;

  static void DeferredAddTrackToJsepSession(const std::string& pcHandle,
                                            SdpMediaSection::MediaType type,
                                            const std::string& streamId,
                                            const std::string& trackId);

  nsresult AddTrackToJsepSession(SdpMediaSection::MediaType type,
                                 const std::string& streamId,
                                 const std::string& trackId);

  nsresult SetupIceRestart();
  nsresult RollbackIceRestart();
  void FinalizeIceRestart();

  static void GetStatsForPCObserver_s(
      const std::string& pcHandle,
      nsAutoPtr<RTCStatsQuery> query);

  // Sends an RTCStatsReport to JS. Must run on main thread.
  static void DeliverStatsReportToPCObserver_m(
      const std::string& pcHandle,
      nsresult result,
      nsAutoPtr<RTCStatsQuery> query);

  // When ICE completes, we record a bunch of statistics that outlive the
  // PeerConnection. This is just telemetry right now, but this can also
  // include things like dumping the RLogConnector somewhere, saving away
  // an RTCStatsReport somewhere so it can be inspected after the call is over,
  // or other things.
  void RecordLongtermICEStatistics();

  void OnNegotiationNeeded();
  static void MaybeFireNegotiationNeeded_static(const std::string& pcHandle);
  void MaybeFireNegotiationNeeded();

  // Timecard used to measure processing time. This should be the first class
  // attribute so that we accurately measure the time required to instantiate
  // any other attributes of this class.
  Timecard *mTimeCard;

  mozilla::dom::PCImplSignalingState mSignalingState;

  // ICE State
  mozilla::dom::PCImplIceConnectionState mIceConnectionState;
  mozilla::dom::PCImplIceGatheringState mIceGatheringState;

  // DTLS
  // this is true if we have been connected ever, see SetDtlsConnected
  bool mDtlsConnected;

  nsCOMPtr<nsIThread> mThread;
  // TODO: Remove if we ever properly wire PeerConnection for cycle-collection.
  nsWeakPtr mPCObserver;

  nsCOMPtr<nsPIDOMWindowInner> mWindow;

  // The SDP sent in from JS
  std::string mLocalRequestedSDP;
  std::string mRemoteRequestedSDP;

  // DTLS fingerprint
  std::string mFingerprint;
  std::string mRemoteFingerprint;

  // identity-related fields
  // The entity on the other end of the peer-to-peer connection;
  // void if they are not yet identified, and no identity setting has been set
  RefPtr<PeerIdentity> mPeerIdentity;
  // The certificate we are using.
  RefPtr<mozilla::dom::RTCCertificate> mCertificate;
  // Whether an app should be prevented from accessing media produced by the PC
  // If this is true, then media will not be sent until mPeerIdentity matches
  // local streams PeerIdentity; and remote streams are protected from content
  //
  // This can be false if mPeerIdentity is set, in the case where identity is
  // provided, but the media is not protected from the app on either side
  bool mPrivacyRequested;

  // A handle to refer to this PC with
  std::string mHandle;

  // A name for this PC that we are willing to expose to content.
  std::string mName;

  // The target to run stuff on
  nsCOMPtr<nsIEventTarget> mSTSThread;

  // DataConnection that's used to get all the DataChannels
  RefPtr<mozilla::DataChannelConnection> mDataConnection;

  bool mAllowIceLoopback;
  bool mAllowIceLinkLocal;
  bool mForceIceTcp;
  RefPtr<PeerConnectionMedia> mMedia;

  // The JSEP negotiation session.
  mozilla::UniquePtr<PCUuidGenerator> mUuidGen;
  mozilla::UniquePtr<mozilla::JsepSession> mJsepSession;
  std::string mPreviousIceUfrag; // used during rollback of ice restart
  std::string mPreviousIcePwd; // used during rollback of ice restart

  // Start time of ICE, used for telemetry
  mozilla::TimeStamp mIceStartTime;
  // Start time of call used for Telemetry
  mozilla::TimeStamp mStartTime;

  bool mHaveConfiguredCodecs;

  bool mHaveDataStream;

  unsigned int mAddCandidateErrorCount;

  bool mTrickle;

  bool mNegotiationNeeded;

  bool mPrivateWindow;

  // storage for Telemetry data
  uint16_t mMaxReceiving[SdpMediaSection::kMediaTypes];
  uint16_t mMaxSending[SdpMediaSection::kMediaTypes];

  // DTMF
  struct DTMFState {
    PeerConnectionImpl* mPeerConnectionImpl;
    nsCOMPtr<nsITimer> mSendTimer;
    nsString mTrackId;
    nsString mTones;
    size_t mLevel;
    uint32_t mDuration;
    uint32_t mInterToneGap;
  };

  static void
  DTMFSendTimerCallback_m(nsITimer* timer, void*);

  nsTArray<DTMFState> mDTMFStates;

public:
  //these are temporary until the DataChannel Listen/Connect API is removed
  unsigned short listenPort;
  unsigned short connectPort;
  char *connectStr; // XXX ownership/free
};

// This is what is returned when you acquire on a handle
class PeerConnectionWrapper
{
 public:
  explicit PeerConnectionWrapper(const std::string& handle);

  PeerConnectionImpl *impl() { return impl_; }

 private:
  RefPtr<PeerConnectionImpl> impl_;
};

}  // end mozilla namespace

#undef NS_IMETHODIMP_TO_ERRORRESULT
#undef NS_IMETHODIMP_TO_ERRORRESULT_RETREF
#endif  // _PEER_CONNECTION_IMPL_H_
