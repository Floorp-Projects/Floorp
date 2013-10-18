/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _PEER_CONNECTION_IMPL_H_
#define _PEER_CONNECTION_IMPL_H_

#include <string>
#include <vector>
#include <map>
#include <cmath>

#include "prlock.h"
#include "mozilla/RefPtr.h"
#include "sigslot.h"
#include "nricectx.h"
#include "nricemediastream.h"
#include "nsComponentManagerUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIThread.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/PeerConnectionImplEnumsBinding.h"

#ifdef MOZILLA_INTERNAL_API
#include "mozilla/TimeStamp.h"
#include "mozilla/net/DataChannel.h"
#include "VideoUtils.h"
#include "VideoSegment.h"
#include "nsNSSShutDown.h"
#endif

namespace test {
#ifdef USE_FAKE_PCOBSERVER
class AFakePCObserver;
#endif
}

#ifdef USE_FAKE_MEDIA_STREAMS
class Fake_DOMMediaStream;
#endif

class nsIDOMMediaStream;
class nsDOMDataChannel;

namespace mozilla {
class DataChannel;
class DtlsIdentity;
class NrIceCtx;
class NrIceMediaStream;
class NrIceStunServer;
class NrIceTurnServer;

#ifdef USE_FAKE_MEDIA_STREAMS
typedef Fake_DOMMediaStream DOMMediaStream;
#else
class DOMMediaStream;
#endif

namespace dom {
class RTCConfiguration;
class MediaConstraintsInternal;
class MediaStreamTrack;
class RTCStatsReportInternal;

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

namespace sipcc {

using mozilla::dom::PeerConnectionObserver;
using mozilla::dom::RTCConfiguration;
using mozilla::dom::MediaConstraintsInternal;
using mozilla::DOMMediaStream;
using mozilla::NrIceCtx;
using mozilla::NrIceMediaStream;
using mozilla::DtlsIdentity;
using mozilla::ErrorResult;
using mozilla::NrIceStunServer;
using mozilla::NrIceTurnServer;

class PeerConnectionWrapper;
class PeerConnectionMedia;
class RemoteSourceStreamInfo;
class MediaConstraintsExternal;
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
                     const std::string& pwd)
  {
    // TODO(ekr@rtfm.com): Need support for SASLprep for
    // username and password. Bug # ???
    std::vector<unsigned char> password(pwd.begin(), pwd.end());

    NrIceTurnServer* server(NrIceTurnServer::Create(addr, port, username, password));
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
#endif
                                     public sigslot::has_slots<>
{
  class Internal; // Avoid exposing c includes to bindings

public:
  PeerConnectionImpl(const mozilla::dom::GlobalObject* aGlobal = nullptr);
  virtual ~PeerConnectionImpl();

  enum Error {
    kNoError                          = 0,
    kInvalidConstraintsType           = 1,
    kInvalidCandidateType             = 2,
    kInvalidMediastreamTrack          = 3,
    kInvalidState                     = 4,
    kInvalidSessionDescription        = 5,
    kIncompatibleSessionDescription   = 6,
    kIncompatibleConstraints          = 7,
    kIncompatibleMediaStreamTrack     = 8,
    kInternalError                    = 9
  };

  NS_DECL_THREADSAFE_ISUPPORTS

#ifdef MOZILLA_INTERNAL_API
  virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> scope);
#endif

  static already_AddRefed<PeerConnectionImpl>
      Constructor(const mozilla::dom::GlobalObject& aGlobal, ErrorResult& rv);
  static PeerConnectionImpl* CreatePeerConnection();
  static nsresult ConvertRTCConfiguration(const RTCConfiguration& aSrc,
                                          IceConfiguration *aDst);
  static already_AddRefed<DOMMediaStream> MakeMediaStream(nsPIDOMWindow* aWindow,
                                                          uint32_t aHint);

  nsresult CreateRemoteSourceStreamInfo(nsRefPtr<RemoteSourceStreamInfo>* aInfo);

  // Implementation of the only observer we need
  void onCallEvent(const OnCallEventArgs &args);

  // DataConnection observers
  void NotifyConnection();
  void NotifyClosedConnection();
  void NotifyDataChannel(already_AddRefed<mozilla::DataChannel> aChannel);

  // Get the media object
  const nsRefPtr<PeerConnectionMedia>& media() const {
    PC_AUTO_ENTER_API_CALL_NO_CHECK();
    return mMedia;
  }

  // Handle system to allow weak references to be passed through C code
  virtual const std::string& GetHandle();

  // ICE events
  void IceGatheringCompleted(NrIceCtx *aCtx);
  void IceCompleted(NrIceCtx *aCtx);
  void IceFailed(NrIceCtx *aCtx);
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

  // Get the DTLS identity
  mozilla::RefPtr<DtlsIdentity> const GetIdentity();

  // Create a fake media stream
  nsresult CreateFakeMediaStream(uint32_t hint, nsIDOMMediaStream** retval);

  nsPIDOMWindow* GetWindow() const {
    PC_AUTO_ENTER_API_CALL_NO_CHECK();
    return mWindow;
  }

  // Initialize PeerConnection from an IceConfiguration object (unit-tests)
  nsresult Initialize(PeerConnectionObserver& aObserver,
                      nsIDOMWindow* aWindow,
                      const IceConfiguration& aConfiguration,
                      nsIThread* aThread) {
    return Initialize(aObserver, aWindow, &aConfiguration, nullptr, aThread);
  }

  // Initialize PeerConnection from an RTCConfiguration object (JS entrypoint)
  void Initialize(PeerConnectionObserver& aObserver,
                  nsIDOMWindow* aWindow,
                  const RTCConfiguration& aConfiguration,
                  nsISupports* aThread,
                  ErrorResult &rv)
  {
    nsresult r = Initialize(aObserver, aWindow, nullptr, &aConfiguration, aThread);
    if (NS_FAILED(r)) {
      rv.Throw(r);
    }
  }

  NS_IMETHODIMP_TO_ERRORRESULT(CreateOffer, ErrorResult &rv,
                               const MediaConstraintsInternal& aConstraints)
  {
    rv = CreateOffer(aConstraints);
  }

  NS_IMETHODIMP_TO_ERRORRESULT(CreateAnswer, ErrorResult &rv,
                               const MediaConstraintsInternal& aConstraints)
  {
    rv = CreateAnswer(aConstraints);
  }

  NS_IMETHODIMP CreateOffer(const MediaConstraintsExternal& aConstraints);
  NS_IMETHODIMP CreateAnswer(const MediaConstraintsExternal& aConstraints);

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

  NS_IMETHODIMP ReadyState(mozilla::dom::PCImplReadyState* aState);
  mozilla::dom::PCImplReadyState ReadyState()
  {
    mozilla::dom::PCImplReadyState state;
    ReadyState(&state);
    return state;
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

  NS_IMETHODIMP IceState(mozilla::dom::PCImplIceState* aState);
  mozilla::dom::PCImplIceState IceState()
  {
    mozilla::dom::PCImplIceState state;
    IceState(&state);
    return state;
  }

  NS_IMETHODIMP Close();
  void Close(ErrorResult &rv)
  {
    rv = Close();
  }

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

  // Called to retreive the list of parsing errors.
  const std::vector<std::string> &GetSdpParseErrors();

  // Sets the RTC Signaling State
  void SetSignalingState_m(mozilla::dom::PCImplSignalingState aSignalingState);

#ifdef MOZILLA_INTERNAL_API
  // initialize telemetry for when calls start
  void startCallTelem();
#endif

private:
  PeerConnectionImpl(const PeerConnectionImpl&rhs);
  PeerConnectionImpl& operator=(PeerConnectionImpl);
  NS_IMETHODIMP Initialize(PeerConnectionObserver& aObserver,
                           nsIDOMWindow* aWindow,
                           const IceConfiguration* aConfiguration,
                           const RTCConfiguration* aRTCConfiguration,
                           nsISupports* aThread);

  NS_IMETHODIMP EnsureDataConnection(uint16_t aNumstreams);

  nsresult CloseInt();
  void ChangeReadyState(mozilla::dom::PCImplReadyState aReadyState);
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
  nsresult GetTimeSinceEpoch(DOMHighResTimeStamp *result);
#endif

  // Shut down media - called on main thread only
  void ShutdownMedia();

  // ICE callbacks run on the right thread.
  nsresult IceStateChange_m(mozilla::dom::PCImplIceState aState);

#ifdef MOZILLA_INTERNAL_API
  // Fills in an RTCStatsReportInternal. Must be run on STS.
  void GetStats_s(uint32_t trackId,
                  DOMHighResTimeStamp now);

  // Sends an RTCStatsReport to JS. Must run on main thread.
  void OnStatsReport_m(uint32_t trackId,
                       nsresult result,
                       nsAutoPtr<mozilla::dom::RTCStatsReportInternal> report);
#endif

  // Timecard used to measure processing time. This should be the first class
  // attribute so that we accurately measure the time required to instantiate
  // any other attributes of this class.
  Timecard *mTimeCard;

  // The call
  mozilla::ScopedDeletePtr<Internal> mInternal;
  mozilla::dom::PCImplReadyState mReadyState;
  mozilla::dom::PCImplSignalingState mSignalingState;

  // ICE State
  mozilla::dom::PCImplIceState mIceState;

  nsCOMPtr<nsIThread> mThread;
  // We hold a raw pointer to PeerConnectionObserver (no WeakRefs to concretes!)
  // which is an invariant guaranteed to exist between Initialize() and Close().
  // We explicitly clear it in Close(). We wrap it in a helper, to encourage
  // testing against nullptr before use. Use in Runnables requires wrapping
  // access in RefPtr<> since they may execute after close. This is only safe
  // to use on the main thread
  //
  // TODO: Remove if we ever properly wire PeerConnection for cycle-collection.
  class WeakReminder
  {
  public:
    WeakReminder() : mObserver(nullptr) {}
    void Init(PeerConnectionObserver *aObserver) {
      mObserver = aObserver;
    }
    void Close() {
      mObserver = nullptr;
    }
    PeerConnectionObserver *MayGet() {
      return mObserver;
    }
  private:
    PeerConnectionObserver *mObserver;
  } mPCObserver;
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

  // The DTLS identity
  mozilla::RefPtr<DtlsIdentity> mIdentity;

  // A handle to refer to this PC with
  std::string mHandle;

  // The target to run stuff on
  nsCOMPtr<nsIEventTarget> mSTSThread;

#ifdef MOZILLA_INTERNAL_API
  // DataConnection that's used to get all the DataChannels
	nsRefPtr<mozilla::DataChannelConnection> mDataConnection;
#endif

  nsRefPtr<PeerConnectionMedia> mMedia;

#ifdef MOZILLA_INTERNAL_API
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
