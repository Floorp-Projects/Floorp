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
#include "nsIThread.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/PeerConnectionImplEnumsBinding.h"
#include "StreamBuffer.h"

#ifdef MOZILLA_INTERNAL_API
#include "mozilla/TimeStamp.h"
#include "mozilla/net/DataChannel.h"
#include "VideoUtils.h"
#include "VideoSegment.h"
#include "nsNSSShutDown.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "nsIPrincipal.h"
#include "mozilla/PeerIdentity.h"
#ifndef USE_FAKE_MEDIA_STREAMS
#include "DOMMediaStream.h"
#endif
#endif

namespace test {
#ifdef USE_FAKE_PCOBSERVER
class AFakePCObserver;
#endif
}

#ifdef USE_FAKE_MEDIA_STREAMS
class Fake_DOMMediaStream;
#endif

class nsGlobalWindow;
class nsIDOMMediaStream;
class nsDOMDataChannel;

namespace mozilla {
class DataChannel;
class DtlsIdentity;
class NrIceCtx;
class NrIceMediaStream;
class NrIceStunServer;
class NrIceTurnServer;
class MediaPipeline;

#ifdef USE_FAKE_MEDIA_STREAMS
typedef Fake_DOMMediaStream DOMMediaStream;
#else
class DOMMediaStream;
#endif

namespace dom {
struct RTCConfiguration;
struct RTCOfferOptions;
class MediaStreamTrack;

#ifdef USE_FAKE_PCOBSERVER
typedef test::AFakePCObserver PeerConnectionObserver;
typedef const char *PCObserverString;
#else
class PeerConnectionObserver;
typedef NS_ConvertUTF8toUTF16 PCObserverString;
#endif
}
class SipccOfferOptions;
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

namespace sipcc {

using mozilla::dom::PeerConnectionObserver;
using mozilla::dom::RTCConfiguration;
using mozilla::dom::RTCOfferOptions;
using mozilla::DOMMediaStream;
using mozilla::NrIceCtx;
using mozilla::NrIceMediaStream;
using mozilla::DtlsIdentity;
using mozilla::ErrorResult;
using mozilla::NrIceStunServer;
using mozilla::NrIceTurnServer;
#ifdef MOZILLA_INTERNAL_API
using mozilla::PeerIdentity;
#endif

class PeerConnectionWrapper;
class PeerConnectionMedia;
class RemoteSourceStreamInfo;
class OnCallEventArgs;

class IceConfiguration
{
public:
  bool addStunServer(const std::string& addr, uint16_t port)
  {
    NrIceStunServer* server(NrIceStunServer::Create(addr, port));
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

    NrIceTurnServer* server(NrIceTurnServer::Create(addr, port, username, password,
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
private:
  std::vector<NrIceStunServer> mStunServers;
  std::vector<NrIceTurnServer> mTurnServers;
};

#ifdef MOZILLA_INTERNAL_API
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
    nsTArray<mozilla::RefPtr<mozilla::MediaPipeline>> pipelines;
    mozilla::RefPtr<NrIceCtx> iceCtx;
    nsTArray<mozilla::RefPtr<NrIceMediaStream>> streams;
    DOMHighResTimeStamp now;
};
#endif // MOZILLA_INTERNAL_API

// Enter an API call and check that the state is OK,
// the PC isn't closed, etc.
#define PC_AUTO_ENTER_API_CALL(assert_ice_ready) \
    do { \
      /* do/while prevents res from conflicting with locals */    \
      nsresult res = CheckApiState(assert_ice_ready);             \
      if (NS_FAILED(res)) return res; \
    } while(0)
#define PC_AUTO_ENTER_API_CALL_NO_CHECK() CheckThread()

class PeerConnectionImpl MOZ_FINAL : public nsISupports,
#ifdef MOZILLA_INTERNAL_API
                                     public mozilla::DataChannelConnection::DataConnectionListener,
                                     public nsNSSShutDownObject,
                                     public DOMMediaStream::PrincipalChangeObserver,
#endif
                                     public sigslot::has_slots<>
{
  struct Internal; // Avoid exposing c includes to bindings

public:
  PeerConnectionImpl(const mozilla::dom::GlobalObject* aGlobal = nullptr);

  enum Error {
    kNoError                          = 0,
    kInvalidCandidateType             = 2,
    kInvalidMediastreamTrack          = 3,
    kInvalidState                     = 4,
    kInvalidSessionDescription        = 5,
    kIncompatibleSessionDescription   = 6,
    kIncompatibleMediaStreamTrack     = 8,
    kInternalError                    = 9
  };

  NS_DECL_THREADSAFE_ISUPPORTS

#ifdef MOZILLA_INTERNAL_API
  virtual JSObject* WrapObject(JSContext* cx);
#endif

  static already_AddRefed<PeerConnectionImpl>
      Constructor(const mozilla::dom::GlobalObject& aGlobal, ErrorResult& rv);
  static PeerConnectionImpl* CreatePeerConnection();
  static nsresult ConvertRTCConfiguration(const RTCConfiguration& aSrc,
                                          IceConfiguration *aDst);
  already_AddRefed<DOMMediaStream> MakeMediaStream(uint32_t aHint);

  nsresult CreateRemoteSourceStreamInfo(nsRefPtr<RemoteSourceStreamInfo>* aInfo);

  // Implementation of the only observer we need
  void onCallEvent(const OnCallEventArgs &args);

  // DataConnection observers
  void NotifyDataChannel(already_AddRefed<mozilla::DataChannel> aChannel);

  // Get the media object
  const nsRefPtr<PeerConnectionMedia>& media() const {
    PC_AUTO_ENTER_API_CALL_NO_CHECK();
    return mMedia;
  }

  // Handle system to allow weak references to be passed through C code
  virtual const std::string& GetHandle();

  // Name suitable for exposing to content
  virtual const std::string& GetName();

  // ICE events
  void IceConnectionStateChange(NrIceCtx* ctx,
                                NrIceCtx::ConnectionState state);
  void IceGatheringStateChange(NrIceCtx* ctx,
                               NrIceCtx::GatheringState state);
  void IceStreamReady(NrIceMediaStream *aStream);

  static void ListenThread(void *aData);
  static void ConnectThread(void *aData);

  // Get the main thread
  nsCOMPtr<nsIThread> GetMainThread() {
    PC_AUTO_ENTER_API_CALL_NO_CHECK();
    return mThread;
  }

  // Get the STS thread
  nsCOMPtr<nsIEventTarget> GetSTSThread() {
    PC_AUTO_ENTER_API_CALL_NO_CHECK();
    return mSTSThread;
  }

  // Get the DTLS identity (local side)
  mozilla::RefPtr<DtlsIdentity> const GetIdentity() const;
  std::string GetFingerprint() const;
  std::string GetFingerprintAlgorithm() const;
  std::string GetFingerprintHexValue() const;

  // Create a fake media stream
  nsresult CreateFakeMediaStream(uint32_t hint, nsIDOMMediaStream** retval);

  nsPIDOMWindow* GetWindow() const {
    PC_AUTO_ENTER_API_CALL_NO_CHECK();
    return mWindow;
  }

  // Initialize PeerConnection from an IceConfiguration object (unit-tests)
  nsresult Initialize(PeerConnectionObserver& aObserver,
                      nsGlobalWindow* aWindow,
                      const IceConfiguration& aConfiguration,
                      nsIThread* aThread) {
    return Initialize(aObserver, aWindow, &aConfiguration, nullptr, aThread);
  }

  // Initialize PeerConnection from an RTCConfiguration object (JS entrypoint)
  void Initialize(PeerConnectionObserver& aObserver,
                  nsGlobalWindow& aWindow,
                  const RTCConfiguration& aConfiguration,
                  nsISupports* aThread,
                  ErrorResult &rv)
  {
    nsresult r = Initialize(aObserver, &aWindow, nullptr, &aConfiguration, aThread);
    if (NS_FAILED(r)) {
      rv.Throw(r);
    }
  }

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

  NS_IMETHODIMP CreateOffer(const mozilla::SipccOfferOptions& aConstraints);

  NS_IMETHODIMP SetLocalDescription (int32_t aAction, const char* aSDP);

  void SetLocalDescription (int32_t aAction, const nsAString& aSDP, ErrorResult &rv)
  {
    rv = SetLocalDescription(aAction, NS_ConvertUTF16toUTF8(aSDP).get());
  }

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

  NS_IMETHODIMP CloseStreams();

  void CloseStreams(ErrorResult &rv)
  {
    rv = CloseStreams();
  }

  NS_IMETHODIMP_TO_ERRORRESULT(AddStream, ErrorResult &rv,
                               DOMMediaStream& aMediaStream)
  {
    rv = AddStream(aMediaStream);
  }

  NS_IMETHODIMP_TO_ERRORRESULT(RemoveStream, ErrorResult &rv,
                               DOMMediaStream& aMediaStream)
  {
    rv = RemoveStream(aMediaStream);
  }


  nsresult GetPeerIdentity(nsAString& peerIdentity)
  {
#ifdef MOZILLA_INTERNAL_API
    if (mPeerIdentity) {
      peerIdentity = mPeerIdentity->ToString();
      return NS_OK;
    }
#endif

    peerIdentity.SetIsVoid(true);
    return NS_OK;
  }

#ifdef MOZILLA_INTERNAL_API
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
#endif

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
    delete tmp;
  }

  NS_IMETHODIMP GetRemoteDescription(char** aSDP);

  void GetRemoteDescription(nsAString& aSDP)
  {
    char *tmp;
    GetRemoteDescription(&tmp);
    aSDP.AssignASCII(tmp);
    delete tmp;
  }

  NS_IMETHODIMP SignalingState(mozilla::dom::PCImplSignalingState* aState);

  mozilla::dom::PCImplSignalingState SignalingState()
  {
    mozilla::dom::PCImplSignalingState state;
    SignalingState(&state);
    return state;
  }

  NS_IMETHODIMP SipccState(mozilla::dom::PCImplSipccState* aState);

  mozilla::dom::PCImplSipccState SipccState()
  {
    mozilla::dom::PCImplSipccState state;
    SipccState(&state);
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

  bool PluginCrash(uint64_t aPluginID,
                   const nsAString& aPluginName,
                   const nsAString& aPluginDumpID);

  nsresult InitializeDataChannel(int track_id, uint16_t aLocalport,
                                 uint16_t aRemoteport, uint16_t aNumstreams);

  NS_IMETHODIMP_TO_ERRORRESULT(ConnectDataConnection, ErrorResult &rv,
                               uint16_t aLocalport,
                               uint16_t aRemoteport,
                               uint16_t aNumstreams)
  {
    rv = ConnectDataConnection(aLocalport, aRemoteport, aNumstreams);
  }

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
                               nsTArray<nsRefPtr<DOMMediaStream > >& result)
  {
    rv = GetLocalStreams(result);
  }

  NS_IMETHODIMP_TO_ERRORRESULT(GetRemoteStreams, ErrorResult &rv,
                               nsTArray<nsRefPtr<DOMMediaStream > >& result)
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

  void OnAddIceCandidateError() {
    ++mAddCandidateErrorCount;
  }

  // Called to retreive the list of parsing errors.
  const std::vector<std::string> &GetSdpParseErrors();

  // Sets the RTC Signaling State
  void SetSignalingState_m(mozilla::dom::PCImplSignalingState aSignalingState);

  bool IsClosed() const;
  // called when DTLS connects; we only need this once
  nsresult SetDtlsConnected(bool aPrivacyRequested);

  bool HasMedia() const;

#ifdef MOZILLA_INTERNAL_API
  // initialize telemetry for when calls start
  void startCallTelem();

  nsresult BuildStatsQuery_m(
      mozilla::dom::MediaStreamTrack *aSelector,
      RTCStatsQuery *query);

  static nsresult ExecuteStatsQuery_s(RTCStatsQuery *query);

  // for monitoring changes in stream ownership
  // PeerConnectionMedia can't do it because it doesn't know about principals
  virtual void PrincipalChanged(DOMMediaStream* aMediaStream) MOZ_OVERRIDE;
#endif

private:
  virtual ~PeerConnectionImpl();
  PeerConnectionImpl(const PeerConnectionImpl&rhs);
  PeerConnectionImpl& operator=(PeerConnectionImpl);
  NS_IMETHODIMP Initialize(PeerConnectionObserver& aObserver,
                           nsGlobalWindow* aWindow,
                           const IceConfiguration* aConfiguration,
                           const RTCConfiguration* aRTCConfiguration,
                           nsISupports* aThread);

  NS_IMETHODIMP EnsureDataConnection(uint16_t aNumstreams);

  nsresult CloseInt();
  nsresult CheckApiState(bool assert_ice_ready) const;
  void CheckThread() const {
    NS_ABORT_IF_FALSE(CheckThreadInt(), "Wrong thread");
  }
  bool CheckThreadInt() const {
#ifdef MOZILLA_INTERNAL_API
    // Thread assertions are disabled in the C++ unit tests because those
    // make API calls off the main thread.
    // TODO(ekr@rtfm.com): Fix the unit tests so they don't do that.
    bool on;
    NS_ENSURE_SUCCESS(mThread->IsOnCurrentThread(&on), false);
    NS_ENSURE_TRUE(on, false);
#endif
    return true;
  }

#ifdef MOZILLA_INTERNAL_API
  void virtualDestroyNSSReference() MOZ_FINAL;
  void destructorSafeDestroyNSSReference();
  nsresult GetTimeSinceEpoch(DOMHighResTimeStamp *result);
#endif

  // Shut down media - called on main thread only
  void ShutdownMedia();

  // ICE callbacks run on the right thread.
  nsresult IceConnectionStateChange_m(
      mozilla::dom::PCImplIceConnectionState aState);
  nsresult IceGatheringStateChange_m(
      mozilla::dom::PCImplIceGatheringState aState);

  NS_IMETHOD FingerprintSplitHelper(
      std::string& fingerprint, size_t& spaceIdx) const;


#ifdef MOZILLA_INTERNAL_API
  static void GetStatsForPCObserver_s(
      const std::string& pcHandle,
      nsAutoPtr<RTCStatsQuery> query);

  // Sends an RTCStatsReport to JS. Must run on main thread.
  static void DeliverStatsReportToPCObserver_m(
      const std::string& pcHandle,
      nsresult result,
      nsAutoPtr<RTCStatsQuery> query);
#endif

  // When ICE completes, we record a bunch of statistics that outlive the
  // PeerConnection. This is just telemetry right now, but this can also
  // include things like dumping the RLogRingbuffer somewhere, saving away
  // an RTCStatsReport somewhere so it can be inspected after the call is over,
  // or other things.
  void RecordLongtermICEStatistics();

  // Timecard used to measure processing time. This should be the first class
  // attribute so that we accurately measure the time required to instantiate
  // any other attributes of this class.
  Timecard *mTimeCard;

  // The call
  mozilla::ScopedDeletePtr<Internal> mInternal;
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

  nsCOMPtr<nsPIDOMWindow> mWindow;

  // The SDP sent in from JS - here for debugging.
  std::string mLocalRequestedSDP;
  std::string mRemoteRequestedSDP;
  // The SDP we are using.
  std::string mLocalSDP;
  std::string mRemoteSDP;

  // DTLS fingerprint
  std::string mFingerprint;
  std::string mRemoteFingerprint;

  // identity-related fields
  mozilla::RefPtr<DtlsIdentity> mIdentity;
#ifdef MOZILLA_INTERNAL_API
  // The entity on the other end of the peer-to-peer connection;
  // void if they are not yet identified, and no identity setting has been set
  nsAutoPtr<PeerIdentity> mPeerIdentity;
#endif
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

#ifdef MOZILLA_INTERNAL_API
  // DataConnection that's used to get all the DataChannels
  nsRefPtr<mozilla::DataChannelConnection> mDataConnection;
#endif

  nsRefPtr<PeerConnectionMedia> mMedia;

#ifdef MOZILLA_INTERNAL_API
  // Start time of ICE, used for telemetry
  mozilla::TimeStamp mIceStartTime;
  // Start time of call used for Telemetry
  mozilla::TimeStamp mStartTime;
#endif

  // Temporary: used to prevent multiple audio streams or multiple video streams
  // in a single PC. This is tied up in the IETF discussion around proper
  // representation of multiple streams in SDP, and strongly related to
  // Bug 840728.
  int mNumAudioStreams;
  int mNumVideoStreams;

  bool mHaveDataStream;

  // Holder for error messages from parsing SDP
  std::vector<std::string> mSDPParseErrorMessages;
  unsigned int mAddCandidateErrorCount;

  bool mTrickle;

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
  PeerConnectionWrapper(const std::string& handle);

  PeerConnectionImpl *impl() { return impl_; }

 private:
  nsRefPtr<PeerConnectionImpl> impl_;
};

}  // end sipcc namespace

#undef NS_IMETHODIMP_TO_ERRORRESULT
#undef NS_IMETHODIMP_TO_ERRORRESULT_RETREF
#endif  // _PEER_CONNECTION_IMPL_H_
