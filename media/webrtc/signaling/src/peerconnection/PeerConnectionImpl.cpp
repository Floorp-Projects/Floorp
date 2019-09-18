/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>
#include <cerrno>
#include <deque>
#include <set>
#include <sstream>
#include <vector>

#include "CSFLog.h"
#include "base/histogram.h"
#include "timecard.h"

#include "jsapi.h"
#include "nspr.h"
#include "nss.h"
#include "pk11pub.h"

#include "nsNetCID.h"
#include "nsILoadContext.h"
#include "nsIProperty.h"
#include "nsIPropertyBag2.h"
#include "nsIServiceManager.h"
#include "nsISimpleEnumerator.h"
#include "nsServiceManagerUtils.h"
#include "nsISocketTransportService.h"
#include "nsIConsoleService.h"
#include "nsThreadUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsProxyRelease.h"
#include "prtime.h"

#include "AudioConduit.h"
#include "VideoConduit.h"
#include "MediaStreamGraph.h"
#include "runnable_utils.h"
#include "PeerConnectionCtx.h"
#include "PeerConnectionImpl.h"
#include "PeerConnectionMedia.h"
#include "RemoteTrackSource.h"
#include "nsDOMDataChannelDeclarations.h"
#include "dtlsidentity.h"
#include "signaling/src/sdp/SdpAttribute.h"

#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/jsep/JsepSession.h"
#include "signaling/src/jsep/JsepSessionImpl.h"

#include "signaling/src/mediapipeline/MediaPipeline.h"

#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Sprintf.h"

#ifdef XP_WIN
// We need to undef the MS macro for Document::CreateEvent
#  ifdef CreateEvent
#    undef CreateEvent
#  endif
#endif  // XP_WIN

#include "mozilla/dom/Document.h"
#include "nsGlobalWindow.h"
#include "nsDOMDataChannel.h"
#include "mozilla/dom/Location.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Preferences.h"
#include "mozilla/PublicSSL.h"
#include "nsXULAppAPI.h"
#include "nsContentUtils.h"
#include "nsDOMJSUtils.h"
#include "nsIScriptError.h"
#include "nsPrintfCString.h"
#include "nsURLHelper.h"
#include "nsNetUtil.h"
#include "nsIURLParser.h"
#include "js/ArrayBuffer.h"    // JS::NewArrayBufferWithContents
#include "js/GCAnnotations.h"  // JS_HAZ_ROOTED
#include "js/RootingAPI.h"     // JS::{{,Mutable}Handle,Rooted}
#include "mozilla/PeerIdentity.h"
#include "mozilla/dom/RTCCertificate.h"
#include "mozilla/dom/RTCDTMFSenderBinding.h"
#include "mozilla/dom/RTCDTMFToneChangeEvent.h"
#include "mozilla/dom/RTCRtpReceiverBinding.h"
#include "mozilla/dom/RTCRtpSenderBinding.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "mozilla/dom/RTCPeerConnectionBinding.h"
#include "mozilla/dom/PeerConnectionImplBinding.h"
#include "mozilla/dom/RTCDataChannelBinding.h"
#include "mozilla/dom/PerformanceTiming.h"
#include "mozilla/dom/PluginCrashedEvent.h"
#include "MediaStreamTrack.h"
#include "AudioStreamTrack.h"
#include "VideoStreamTrack.h"
#include "nsIScriptGlobalObject.h"
#include "MediaStreamGraph.h"
#include "DOMMediaStream.h"
#include "WebrtcGlobalInformation.h"
#include "mozilla/dom/Event.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/net/DataChannelProtocol.h"
#include "MediaManager.h"

#include "MediaStreamGraphImpl.h"

#ifdef XP_WIN
// We need to undef the MS macro again in case the windows include file
// got imported after we included mozilla/dom/Document.h
#  ifdef CreateEvent
#    undef CreateEvent
#  endif
#endif  // XP_WIN

#include "MediaSegment.h"

#ifdef USE_FAKE_PCOBSERVER
#  include "FakePCObserver.h"
#else
#  include "mozilla/dom/PeerConnectionObserverBinding.h"
#endif
#include "mozilla/dom/PeerConnectionObserverEnumsBinding.h"

#define ICE_PARSING \
  "In RTCConfiguration passed to RTCPeerConnection constructor"

using namespace mozilla;
using namespace mozilla::dom;

typedef PCObserverString ObString;

static const char* pciLogTag = "PeerConnectionImpl";
#ifdef LOGTAG
#  undef LOGTAG
#endif
#define LOGTAG pciLogTag

static mozilla::LazyLogModule logModuleInfo("signaling");

// Getting exceptions back down from PCObserver is generally not harmful.
namespace {
// This is a terrible hack.  The problem is that SuppressException is not
// inline, and we link this file without libxul in some cases (e.g. for our test
// setup).  So we can't use ErrorResult or IgnoredErrorResult because those call
// SuppressException...  And we can't use FastErrorResult because we can't
// include BindingUtils.h, because our linking is completely broken. Use
// BaseErrorResult directly.  Please do not let me see _anyone_ doing this
// without really careful review from someone who knows what they are doing.
class JSErrorResult : public binding_danger::TErrorResult<
                          binding_danger::JustAssertCleanupPolicy> {
 public:
  ~JSErrorResult() { SuppressException(); }
} JS_HAZ_ROOTED;

// The WrapRunnable() macros copy passed-in args and passes them to the function
// later on the other thread. ErrorResult cannot be passed like this because it
// disallows copy-semantics.
//
// This WrappableJSErrorResult hack solves this by not actually copying the
// ErrorResult, but creating a new one instead, which works because we don't
// care about the result.
//
// Since this is for JS-calls, these can only be dispatched to the main thread.

class WrappableJSErrorResult {
 public:
  WrappableJSErrorResult() : mRv(MakeUnique<JSErrorResult>()), isCopy(false) {}
  WrappableJSErrorResult(const WrappableJSErrorResult& other)
      : mRv(MakeUnique<JSErrorResult>()), isCopy(true) {}
  ~WrappableJSErrorResult() {
    if (isCopy) {
      MOZ_ASSERT(NS_IsMainThread());
    }
  }
  operator ErrorResult&() { return *mRv; }

 private:
  mozilla::UniquePtr<JSErrorResult> mRv;
  bool isCopy;
} JS_HAZ_ROOTED;

}  // namespace

static nsresult InitNSSInContent() {
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_NOT_SAME_THREAD);

  if (!XRE_IsContentProcess()) {
    MOZ_ASSERT_UNREACHABLE("Must be called in content process");
    return NS_ERROR_FAILURE;
  }

  static bool nssStarted = false;
  if (nssStarted) {
    return NS_OK;
  }

  if (NSS_NoDB_Init(nullptr) != SECSuccess) {
    CSFLogError(LOGTAG, "NSS_NoDB_Init failed.");
    return NS_ERROR_FAILURE;
  }

  if (NS_FAILED(mozilla::psm::InitializeCipherSuite())) {
    CSFLogError(LOGTAG, "Fail to set up nss cipher suite.");
    return NS_ERROR_FAILURE;
  }

  mozilla::psm::DisableMD5();

  nssStarted = true;

  return NS_OK;
}

namespace mozilla {
class DataChannel;
}

namespace mozilla {

RTCStatsQuery::RTCStatsQuery(bool aInternal, bool aRecordTelemetry)
    : internalStats(aInternal),
      recordTelemetry(aRecordTelemetry),
      grabAllLevels(false),
      now(0.0) {}

RTCStatsQuery::~RTCStatsQuery() {}

NS_IMPL_ISUPPORTS0(PeerConnectionImpl)

already_AddRefed<PeerConnectionImpl> PeerConnectionImpl::Constructor(
    const dom::GlobalObject& aGlobal) {
  RefPtr<PeerConnectionImpl> pc = new PeerConnectionImpl(&aGlobal);

  CSFLogDebug(LOGTAG, "Created PeerConnection: %p", pc.get());

  return pc.forget();
}

PeerConnectionImpl* PeerConnectionImpl::CreatePeerConnection() {
  PeerConnectionImpl* pc = new PeerConnectionImpl();

  CSFLogDebug(LOGTAG, "Created PeerConnection: %p", pc);

  return pc;
}

bool PeerConnectionImpl::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto,
                                    JS::MutableHandle<JSObject*> aReflector) {
  return PeerConnectionImpl_Binding::Wrap(aCx, this, aGivenProto, aReflector);
}

bool PCUuidGenerator::Generate(std::string* idp) {
  nsresult rv;

  if (!mGenerator) {
    mGenerator = do_GetService("@mozilla.org/uuid-generator;1", &rv);
    if (NS_FAILED(rv)) {
      return false;
    }
    if (!mGenerator) {
      return false;
    }
  }

  nsID id;
  rv = mGenerator->GenerateUUIDInPlace(&id);
  if (NS_FAILED(rv)) {
    return false;
  }
  char buffer[NSID_LENGTH];
  id.ToProvidedString(buffer);
  idp->assign(buffer);

  return true;
}

bool IsPrivateBrowsing(nsPIDOMWindowInner* aWindow) {
  if (!aWindow) {
    return false;
  }

  Document* doc = aWindow->GetExtantDoc();
  if (!doc) {
    return false;
  }

  nsILoadContext* loadContext = doc->GetLoadContext();
  return loadContext && loadContext->UsePrivateBrowsing();
}

PeerConnectionImpl::PeerConnectionImpl(const GlobalObject* aGlobal)
    : mTimeCard(MOZ_LOG_TEST(logModuleInfo, LogLevel::Error) ? create_timecard()
                                                             : nullptr),
      mSignalingState(RTCSignalingState::Stable),
      mIceConnectionState(RTCIceConnectionState::New),
      mIceGatheringState(RTCIceGatheringState::New),
      mWindow(nullptr),
      mCertificate(nullptr),
      mSTSThread(nullptr),
      mForceIceTcp(false),
      mMedia(nullptr),
      mTransportHandler(nullptr),
      mUuidGen(MakeUnique<PCUuidGenerator>()),
      mIceRestartCount(0),
      mIceRollbackCount(0),
      mHaveConfiguredCodecs(false),
      mTrickle(true)  // TODO(ekr@rtfm.com): Use pref
      ,
      mPrivateWindow(false),
      mActiveOnWindow(false),
      mPacketDumpEnabled(false),
      mPacketDumpFlagsMutex("Packet dump flags mutex"),
      listenPort(0),
      connectPort(0),
      connectStr(nullptr) {
  MOZ_ASSERT(NS_IsMainThread());
  if (aGlobal) {
    mWindow = do_QueryInterface(aGlobal->GetAsSupports());
    if (IsPrivateBrowsing(mWindow)) {
      mPrivateWindow = true;
    }
    mWindow->AddPeerConnection();
    mActiveOnWindow = true;
  }
  CSFLogInfo(LOGTAG, "%s: PeerConnectionImpl constructor for %s", __FUNCTION__,
             mHandle.c_str());
  STAMP_TIMECARD(mTimeCard, "Constructor Completed");
  mForceIceTcp =
      Preferences::GetBool("media.peerconnection.ice.force_ice_tcp", false);
  memset(mMaxReceiving, 0, sizeof(mMaxReceiving));
  memset(mMaxSending, 0, sizeof(mMaxSending));
}

PeerConnectionImpl::~PeerConnectionImpl() {
  if (mTimeCard) {
    STAMP_TIMECARD(mTimeCard, "Destructor Invoked");
    print_timecard(mTimeCard);
    destroy_timecard(mTimeCard);
    mTimeCard = nullptr;
  }
  // This aborts if not on main thread (in Debug builds)
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  if (mWindow && mActiveOnWindow) {
    mWindow->RemovePeerConnection();
    // No code is supposed to observe the assignment below, but
    // hopefully it makes looking at this object in a debugger
    // make more sense.
    mActiveOnWindow = false;
  }

  if (mPrivateWindow && mTransportHandler) {
    mTransportHandler->ExitPrivateMode();
  }
  if (PeerConnectionCtx::isActive()) {
    PeerConnectionCtx::GetInstance()->mPeerConnections.erase(mHandle);
  } else {
    CSFLogError(LOGTAG, "PeerConnectionCtx is already gone. Ignoring...");
  }

  CSFLogInfo(LOGTAG, "%s: PeerConnectionImpl destructor invoked for %s",
             __FUNCTION__, mHandle.c_str());

  // Since this and Initialize() occur on MainThread, they can't both be
  // running at once

  // Right now, we delete PeerConnectionCtx at XPCOM shutdown only, but we
  // probably want to shut it down more aggressively to save memory.  We
  // could shut down here when there are no uses.  It might be more optimal
  // to release off a timer (and XPCOM Shutdown) to avoid churn
  ShutdownMedia();
}

nsresult PeerConnectionImpl::Initialize(PeerConnectionObserver& aObserver,
                                        nsGlobalWindowInner* aWindow,
                                        const RTCConfiguration& aConfiguration,
                                        nsISupports* aThread) {
  nsresult res;

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aThread);
  if (!mThread) {
    mThread = do_QueryInterface(aThread);
    MOZ_ASSERT(mThread);
  }
  CheckThread();

  mPCObserver = &aObserver;

  // Find the STS thread

  mSTSThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &res);
  MOZ_ASSERT(mSTSThread);

  // We do callback handling on STS instead of main to avoid media jank.
  // Someday, we may have a dedicated thread for this.
  mTransportHandler = MediaTransportHandler::Create(mSTSThread);
  if (mPrivateWindow) {
    mTransportHandler->EnterPrivateMode();
  }

  // Initialize NSS if we are in content process. For chrome process, NSS should
  // already been initialized.
  if (XRE_IsParentProcess()) {
    // This code interferes with the C++ unit test startup code.
    nsCOMPtr<nsISupports> nssDummy = do_GetService("@mozilla.org/psm;1", &res);
    NS_ENSURE_SUCCESS(res, res);
  } else {
    NS_ENSURE_SUCCESS(res = InitNSSInContent(), res);
  }

  // Currently no standalone unit tests for DataChannel,
  // which is the user of mWindow
  MOZ_ASSERT(aWindow);
  mWindow = aWindow;
  NS_ENSURE_STATE(mWindow);

  PRTime timestamp = PR_Now();
  // Ok if we truncate this.
  char temp[128];

  nsAutoCString locationCStr;

  RefPtr<Location> location = mWindow->Location();
  nsAutoString locationAStr;
  res = location->ToString(locationAStr);
  NS_ENSURE_SUCCESS(res, res);

  CopyUTF16toUTF8(locationAStr, locationCStr);

  SprintfLiteral(temp, "%" PRIu64 " (id=%" PRIu64 " url=%s)",
                 static_cast<uint64_t>(timestamp),
                 static_cast<uint64_t>(mWindow ? mWindow->WindowID() : 0),
                 locationCStr.get() ? locationCStr.get() : "NULL");

  mName = temp;

  // Generate a random handle
  unsigned char handle_bin[8];
  SECStatus rv;
  rv = PK11_GenerateRandom(handle_bin, sizeof(handle_bin));
  if (rv != SECSuccess) {
    MOZ_CRASH();
    return NS_ERROR_UNEXPECTED;
  }

  char hex[17];
  SprintfLiteral(hex, "%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x", handle_bin[0],
                 handle_bin[1], handle_bin[2], handle_bin[3], handle_bin[4],
                 handle_bin[5], handle_bin[6], handle_bin[7]);

  mHandle = hex;

  STAMP_TIMECARD(mTimeCard, "Initializing PC Ctx");
  res = PeerConnectionCtx::InitializeGlobal(mThread, mSTSThread);
  NS_ENSURE_SUCCESS(res, res);

  nsTArray<dom::RTCIceServer> iceServers;
  if (aConfiguration.mIceServers.WasPassed()) {
    iceServers = aConfiguration.mIceServers.Value();
  }

  res = mTransportHandler->CreateIceCtx("PC:" + GetName(), iceServers,
                                        aConfiguration.mIceTransportPolicy);
  if (NS_FAILED(res)) {
    CSFLogError(LOGTAG, "%s: Failed to init mtransport", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  mJsepSession =
      MakeUnique<JsepSessionImpl>(mName, MakeUnique<PCUuidGenerator>());

  res = mJsepSession->Init();
  if (NS_FAILED(res)) {
    CSFLogError(LOGTAG, "%s: Couldn't init JSEP Session, res=%u", __FUNCTION__,
                static_cast<unsigned>(res));
    return res;
  }

  JsepBundlePolicy bundlePolicy;
  switch (aConfiguration.mBundlePolicy) {
    case dom::RTCBundlePolicy::Balanced:
      bundlePolicy = kBundleBalanced;
      break;
    case dom::RTCBundlePolicy::Max_compat:
      bundlePolicy = kBundleMaxCompat;
      break;
    case dom::RTCBundlePolicy::Max_bundle:
      bundlePolicy = kBundleMaxBundle;
      break;
    default:
      MOZ_CRASH();
  }

  res = mJsepSession->SetBundlePolicy(bundlePolicy);
  if (NS_FAILED(res)) {
    CSFLogError(LOGTAG, "%s: Couldn't set bundle policy, res=%u, error=%s",
                __FUNCTION__, static_cast<unsigned>(res),
                mJsepSession->GetLastError().c_str());
    return res;
  }

  mMedia = new PeerConnectionMedia(this);

  // Initialize the media object.
  res = mMedia->Init();
  if (NS_FAILED(res)) {
    CSFLogError(LOGTAG, "%s: Couldn't initialize media object", __FUNCTION__);
    ShutdownMedia();
    return res;
  }

  PeerConnectionCtx::GetInstance()->mPeerConnections[mHandle] = this;

  return NS_OK;
}

void PeerConnectionImpl::Initialize(PeerConnectionObserver& aObserver,
                                    nsGlobalWindowInner& aWindow,
                                    const RTCConfiguration& aConfiguration,
                                    nsISupports* aThread, ErrorResult& rv) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aThread);
  mThread = do_QueryInterface(aThread);

  nsresult res = Initialize(aObserver, &aWindow, aConfiguration, aThread);
  if (NS_FAILED(res)) {
    rv.Throw(res);
    return;
  }

  if (!aConfiguration.mPeerIdentity.IsEmpty()) {
    mPeerIdentity = new PeerIdentity(aConfiguration.mPeerIdentity);
    mPrivacyRequested = Some(true);
  }
}

void PeerConnectionImpl::SetCertificate(
    mozilla::dom::RTCCertificate& aCertificate) {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(!mCertificate, "This can only be called once");
  mCertificate = &aCertificate;

  std::vector<uint8_t> fingerprint;
  nsresult rv =
      CalculateFingerprint(DtlsIdentity::DEFAULT_HASH_ALGORITHM, &fingerprint);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: Couldn't calculate fingerprint, rv=%u",
                __FUNCTION__, static_cast<unsigned>(rv));
    mCertificate = nullptr;
    return;
  }
  rv = mJsepSession->AddDtlsFingerprint(DtlsIdentity::DEFAULT_HASH_ALGORITHM,
                                        fingerprint);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: Couldn't set DTLS credentials, rv=%u",
                __FUNCTION__, static_cast<unsigned>(rv));
    mCertificate = nullptr;
  }
}

const RefPtr<mozilla::dom::RTCCertificate>& PeerConnectionImpl::Certificate()
    const {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  return mCertificate;
}

RefPtr<DtlsIdentity> PeerConnectionImpl::Identity() const {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(mCertificate);
  return mCertificate->CreateDtlsIdentity();
}

class CompareCodecPriority {
 public:
  void SetPreferredCodec(int32_t preferredCodec) {
    // This pref really ought to be a string, preferably something like
    // "H264" or "VP8" instead of a payload type.
    // Bug 1101259.
    std::ostringstream os;
    os << preferredCodec;
    mPreferredCodec = os.str();
  }

  bool operator()(const UniquePtr<JsepCodecDescription>& lhs,
                  const UniquePtr<JsepCodecDescription>& rhs) const {
    if (!mPreferredCodec.empty() && lhs->mDefaultPt == mPreferredCodec &&
        rhs->mDefaultPt != mPreferredCodec) {
      return true;
    }

    if (lhs->mStronglyPreferred && !rhs->mStronglyPreferred) {
      return true;
    }

    return false;
  }

 private:
  std::string mPreferredCodec;
};

class ConfigureCodec {
 public:
  explicit ConfigureCodec(nsCOMPtr<nsIPrefBranch>& branch)
      : mHardwareH264Supported(false),
        mSoftwareH264Enabled(false),
        mH264Enabled(false),
        mVP9Enabled(true),
        mVP9Preferred(false),
        mH264Level(13),   // minimum suggested for WebRTC spec
        mH264MaxBr(0),    // Unlimited
        mH264MaxMbps(0),  // Unlimited
        mVP8MaxFs(0),
        mVP8MaxFr(0),
        mUseTmmbr(false),
        mUseRemb(false),
        mUseAudioFec(false),
        mRedUlpfecEnabled(false),
        mDtmfEnabled(false) {
    mSoftwareH264Enabled = PeerConnectionCtx::GetInstance()->gmpHasH264();

    mH264Enabled = mHardwareH264Supported || mSoftwareH264Enabled;

    branch->GetIntPref("media.navigator.video.h264.level", &mH264Level);
    mH264Level &= 0xFF;

    branch->GetIntPref("media.navigator.video.h264.max_br", &mH264MaxBr);

    branch->GetIntPref("media.navigator.video.h264.max_mbps", &mH264MaxMbps);

    branch->GetBoolPref("media.peerconnection.video.vp9_enabled", &mVP9Enabled);

    branch->GetBoolPref("media.peerconnection.video.vp9_preferred",
                        &mVP9Preferred);

    branch->GetIntPref("media.navigator.video.max_fs", &mVP8MaxFs);
    if (mVP8MaxFs <= 0) {
      mVP8MaxFs = 12288;  // We must specify something other than 0
    }

    branch->GetIntPref("media.navigator.video.max_fr", &mVP8MaxFr);
    if (mVP8MaxFr <= 0) {
      mVP8MaxFr = 60;  // We must specify something other than 0
    }

    // TMMBR is enabled from a pref in about:config
    branch->GetBoolPref("media.navigator.video.use_tmmbr", &mUseTmmbr);

    // REMB is enabled by default, but can be disabled from about:config
    branch->GetBoolPref("media.navigator.video.use_remb", &mUseRemb);

    branch->GetBoolPref("media.navigator.audio.use_fec", &mUseAudioFec);

    branch->GetBoolPref("media.navigator.video.red_ulpfec_enabled",
                        &mRedUlpfecEnabled);

    // media.peerconnection.dtmf.enabled controls both sdp generation for
    // DTMF support as well as DTMF exposure to DOM
    branch->GetBoolPref("media.peerconnection.dtmf.enabled", &mDtmfEnabled);
  }

  void operator()(UniquePtr<JsepCodecDescription>& codec) const {
    switch (codec->mType) {
      case SdpMediaSection::kAudio: {
        JsepAudioCodecDescription& audioCodec =
            static_cast<JsepAudioCodecDescription&>(*codec);
        if (audioCodec.mName == "opus") {
          audioCodec.mFECEnabled = mUseAudioFec;
        } else if (audioCodec.mName == "telephone-event") {
          audioCodec.mEnabled = mDtmfEnabled;
        }
      } break;
      case SdpMediaSection::kVideo: {
        JsepVideoCodecDescription& videoCodec =
            static_cast<JsepVideoCodecDescription&>(*codec);

        if (videoCodec.mName == "H264") {
          // Override level
          videoCodec.mProfileLevelId &= 0xFFFF00;
          videoCodec.mProfileLevelId |= mH264Level;

          videoCodec.mConstraints.maxBr = mH264MaxBr;

          videoCodec.mConstraints.maxMbps = mH264MaxMbps;

          // Might disable it, but we set up other params anyway
          videoCodec.mEnabled = mH264Enabled;

          if (videoCodec.mPacketizationMode == 0 && !mSoftwareH264Enabled) {
            // We're assuming packetization mode 0 is unsupported by
            // hardware.
            videoCodec.mEnabled = false;
          }

          if (mHardwareH264Supported) {
            videoCodec.mStronglyPreferred = true;
          }
        } else if (videoCodec.mName == "red") {
          videoCodec.mEnabled = mRedUlpfecEnabled;
        } else if (videoCodec.mName == "ulpfec") {
          videoCodec.mEnabled = mRedUlpfecEnabled;
        } else if (videoCodec.mName == "VP8" || videoCodec.mName == "VP9") {
          if (videoCodec.mName == "VP9") {
            if (!mVP9Enabled) {
              videoCodec.mEnabled = false;
              break;
            }
            if (mVP9Preferred) {
              videoCodec.mStronglyPreferred = true;
            }
          }
          videoCodec.mConstraints.maxFs = mVP8MaxFs;
          videoCodec.mConstraints.maxFps = mVP8MaxFr;
        }

        if (mUseTmmbr) {
          videoCodec.EnableTmmbr();
        }
        if (mUseRemb) {
          videoCodec.EnableRemb();
        }
      } break;
      case SdpMediaSection::kText:
      case SdpMediaSection::kApplication:
      case SdpMediaSection::kMessage: {
      }  // Nothing to configure for these.
    }
  }

 private:
  bool mHardwareH264Supported;
  bool mSoftwareH264Enabled;
  bool mH264Enabled;
  bool mVP9Enabled;
  bool mVP9Preferred;
  int32_t mH264Level;
  int32_t mH264MaxBr;
  int32_t mH264MaxMbps;
  int32_t mVP8MaxFs;
  int32_t mVP8MaxFr;
  bool mUseTmmbr;
  bool mUseRemb;
  bool mUseAudioFec;
  bool mRedUlpfecEnabled;
  bool mDtmfEnabled;
};

class ConfigureRedCodec {
 public:
  explicit ConfigureRedCodec(nsCOMPtr<nsIPrefBranch>& branch,
                             std::vector<uint8_t>* redundantEncodings)
      : mRedundantEncodings(redundantEncodings) {
    // if we wanted to override or modify which encodings are considered
    // for redundant encodings, we'd probably want to handle it here by
    // checking prefs modifying the operator() code below
  }

  void operator()(UniquePtr<JsepCodecDescription>& codec) const {
    if (codec->mType == SdpMediaSection::kVideo && codec->mEnabled == false) {
      uint8_t pt = (uint8_t)strtoul(codec->mDefaultPt.c_str(), nullptr, 10);
      // don't search for the codec payload type unless we have a valid
      // conversion (non-zero)
      if (pt != 0) {
        std::vector<uint8_t>::iterator it = std::find(
            mRedundantEncodings->begin(), mRedundantEncodings->end(), pt);
        if (it != mRedundantEncodings->end()) {
          mRedundantEncodings->erase(it);
        }
      }
    }
  }

 private:
  std::vector<uint8_t>* mRedundantEncodings;
};

nsresult PeerConnectionImpl::ConfigureJsepSessionCodecs() {
  nsresult res;
  nsCOMPtr<nsIPrefService> prefs =
      do_GetService("@mozilla.org/preferences-service;1", &res);

  if (NS_FAILED(res)) {
    CSFLogError(LOGTAG, "%s: Couldn't get prefs service, res=%u", __FUNCTION__,
                static_cast<unsigned>(res));
    return res;
  }

  nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);
  if (!branch) {
    CSFLogError(LOGTAG, "%s: Couldn't get prefs branch", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  ConfigureCodec configurer(branch);
  mJsepSession->ForEachCodec(configurer);

  // if red codec is enabled, configure it for the other enabled codecs
  for (auto& codec : mJsepSession->Codecs()) {
    if (codec->mName == "red" && codec->mEnabled) {
      JsepVideoCodecDescription* redCodec =
          static_cast<JsepVideoCodecDescription*>(codec.get());
      ConfigureRedCodec configureRed(branch, &(redCodec->mRedundantEncodings));
      mJsepSession->ForEachCodec(configureRed);
      break;
    }
  }

  // We use this to sort the list of codecs once everything is configured
  CompareCodecPriority comparator;

  // Sort by priority
  int32_t preferredCodec = 0;
  branch->GetIntPref("media.navigator.video.preferred_codec", &preferredCodec);

  if (preferredCodec) {
    comparator.SetPreferredCodec(preferredCodec);
  }

  mJsepSession->SortCodecs(comparator);
  return NS_OK;
}

// Data channels won't work without a window, so in order for the C++ unit
// tests to work (it doesn't have a window available) we ifdef the following
// two implementations.
//
// Note: 'media.peerconnection.sctp.force_maximum_message_size' changes
// behaviour triggered by these parameters.
NS_IMETHODIMP
PeerConnectionImpl::EnsureDataConnection(uint16_t aLocalPort,
                                         uint16_t aNumstreams,
                                         uint32_t aMaxMessageSize,
                                         bool aMMSSet) {
  PC_AUTO_ENTER_API_CALL(false);

  if (mDataConnection) {
    CSFLogDebug(LOGTAG, "%s DataConnection already connected", __FUNCTION__);
    mDataConnection->SetMaxMessageSize(aMMSSet, aMaxMessageSize);
    return NS_OK;
  }

  nsCOMPtr<nsIEventTarget> target =
      mWindow ? mWindow->EventTargetFor(TaskCategory::Other) : nullptr;
  Maybe<uint64_t> mms = aMMSSet ? Some(aMaxMessageSize) : Nothing();
  if (auto res = DataChannelConnection::Create(this, target, mTransportHandler,
                                               aLocalPort, aNumstreams, mms)) {
    mDataConnection = res.value();
    CSFLogDebug(LOGTAG, "%s DataChannelConnection %p attached to %s",
                __FUNCTION__, (void*)mDataConnection.get(), mHandle.c_str());
    return NS_OK;
  }
  CSFLogError(LOGTAG, "%s DataConnection Create Failed", __FUNCTION__);
  return NS_ERROR_FAILURE;
}

nsresult PeerConnectionImpl::GetDatachannelParameters(
    uint32_t* channels, uint16_t* localport, uint16_t* remoteport,
    uint32_t* remotemaxmessagesize, bool* mmsset, std::string* transportId,
    bool* client) const {
  for (const auto& transceiver : mJsepSession->GetTransceivers()) {
    bool dataChannel =
        transceiver->GetMediaType() == SdpMediaSection::kApplication;

    if (dataChannel && transceiver->mSendTrack.GetNegotiatedDetails()) {
      // This will release assert if there is no such index, and that's ok
      const JsepTrackEncoding& encoding =
          transceiver->mSendTrack.GetNegotiatedDetails()->GetEncoding(0);

      if (encoding.GetCodecs().empty()) {
        CSFLogError(LOGTAG,
                    "%s: Negotiated m=application with no codec. "
                    "This is likely to be broken.",
                    __FUNCTION__);
        return NS_ERROR_FAILURE;
      }

      for (const auto& codec : encoding.GetCodecs()) {
        if (codec->mType != SdpMediaSection::kApplication) {
          CSFLogError(LOGTAG,
                      "%s: Codec type for m=application was %u, this "
                      "is a bug.",
                      __FUNCTION__, static_cast<unsigned>(codec->mType));
          MOZ_ASSERT(false, "Codec for m=application was not \"application\"");
          return NS_ERROR_FAILURE;
        }

        if (codec->mName != "webrtc-datachannel") {
          CSFLogWarn(LOGTAG,
                     "%s: Codec for m=application was not "
                     "webrtc-datachannel (was instead %s). ",
                     __FUNCTION__, codec->mName.c_str());
          continue;
        }

        if (codec->mChannels) {
          *channels = codec->mChannels;
        } else {
          *channels = WEBRTC_DATACHANNEL_STREAMS_DEFAULT;
        }
        const JsepApplicationCodecDescription* appCodec =
            static_cast<const JsepApplicationCodecDescription*>(codec.get());
        *localport = appCodec->mLocalPort;
        *remoteport = appCodec->mRemotePort;
        *remotemaxmessagesize = appCodec->mRemoteMaxMessageSize;
        *mmsset = appCodec->mRemoteMMSSet;
        MOZ_ASSERT(!transceiver->mTransport.mTransportId.empty());
        *transportId = transceiver->mTransport.mTransportId;
        *client = transceiver->mTransport.mDtls->GetRole() ==
                  JsepDtlsTransport::kJsepDtlsClient;
        return NS_OK;
      }
    }
  }

  *channels = 0;
  *localport = 0;
  *remoteport = 0;
  *remotemaxmessagesize = 0;
  *mmsset = false;
  transportId->clear();
  return NS_ERROR_FAILURE;
}

nsresult PeerConnectionImpl::AddRtpTransceiverToJsepSession(
    RefPtr<JsepTransceiver>& transceiver) {
  nsresult res = ConfigureJsepSessionCodecs();
  if (NS_FAILED(res)) {
    CSFLogError(LOGTAG, "Failed to configure codecs");
    return res;
  }

  res = mJsepSession->AddTransceiver(transceiver);

  if (NS_FAILED(res)) {
    std::string errorString = mJsepSession->GetLastError();
    CSFLogError(LOGTAG, "%s (%s) : pc = %s, error = %s", __FUNCTION__,
                transceiver->GetMediaType() == SdpMediaSection::kAudio
                    ? "audio"
                    : "video",
                mHandle.c_str(), errorString.c_str());
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

already_AddRefed<TransceiverImpl> PeerConnectionImpl::CreateTransceiverImpl(
    JsepTransceiver* aJsepTransceiver, dom::MediaStreamTrack* aSendTrack,
    ErrorResult& aRv) {
  // TODO: Maybe this should be done in PeerConnectionMedia?
  if (aSendTrack) {
    aSendTrack->AddPrincipalChangeObserver(this);
  }

  OwningNonNull<dom::MediaStreamTrack> receiveTrack =
      CreateReceiveTrack(aJsepTransceiver->GetMediaType());

  RefPtr<TransceiverImpl> transceiverImpl;

  aRv = mMedia->AddTransceiver(aJsepTransceiver, *receiveTrack, aSendTrack,
                               &transceiverImpl);

  return transceiverImpl.forget();
}

already_AddRefed<TransceiverImpl> PeerConnectionImpl::CreateTransceiverImpl(
    const nsAString& aKind, dom::MediaStreamTrack* aSendTrack,
    ErrorResult& jrv) {
  SdpMediaSection::MediaType type;
  if (aKind.EqualsASCII("audio")) {
    type = SdpMediaSection::MediaType::kAudio;
  } else if (aKind.EqualsASCII("video")) {
    type = SdpMediaSection::MediaType::kVideo;
  } else {
    MOZ_ASSERT(false);
    jrv = NS_ERROR_INVALID_ARG;
    return nullptr;
  }

  RefPtr<JsepTransceiver> jsepTransceiver = new JsepTransceiver(type);

  RefPtr<TransceiverImpl> transceiverImpl =
      CreateTransceiverImpl(jsepTransceiver, aSendTrack, jrv);

  if (jrv.Failed()) {
    // Would be nice if we could peek at the rv without stealing it, so we
    // could log...
    CSFLogError(LOGTAG, "%s: failed", __FUNCTION__);
    return nullptr;
  }

  // Do this last, since it is not possible to roll back.
  nsresult rv = AddRtpTransceiverToJsepSession(jsepTransceiver);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: AddRtpTransceiverToJsepSession failed, res=%u",
                __FUNCTION__, static_cast<unsigned>(rv));
    jrv = rv;
    return nullptr;
  }

  return transceiverImpl.forget();
}

bool PeerConnectionImpl::CheckNegotiationNeeded(ErrorResult& rv) {
  MOZ_ASSERT(mSignalingState == RTCSignalingState::Stable);
  return mJsepSession->CheckNegotiationNeeded();
}

nsresult PeerConnectionImpl::InitializeDataChannel() {
  PC_AUTO_ENTER_API_CALL(false);
  CSFLogDebug(LOGTAG, "%s", __FUNCTION__);

  uint32_t channels = 0;
  uint16_t localport = 0;
  uint16_t remoteport = 0;
  uint32_t remotemaxmessagesize = 0;
  bool mmsset = false;
  std::string transportId;
  bool client = false;
  nsresult rv = GetDatachannelParameters(&channels, &localport, &remoteport,
                                         &remotemaxmessagesize, &mmsset,
                                         &transportId, &client);

  if (NS_FAILED(rv)) {
    CSFLogDebug(LOGTAG, "%s: We did not negotiate datachannel", __FUNCTION__);
    return NS_OK;
  }

  if (channels > MAX_NUM_STREAMS) {
    channels = MAX_NUM_STREAMS;
  }

  rv = EnsureDataConnection(localport, channels, remotemaxmessagesize, mmsset);
  if (NS_SUCCEEDED(rv)) {
    if (mDataConnection->ConnectToTransport(transportId, client, localport,
                                            remoteport)) {
      return NS_OK;
    }
    // If we inited the DataConnection, call Destroy() before releasing it
    mDataConnection->Destroy();
  }
  mDataConnection = nullptr;
  return NS_ERROR_FAILURE;
}

already_AddRefed<nsDOMDataChannel> PeerConnectionImpl::CreateDataChannel(
    const nsAString& aLabel, const nsAString& aProtocol, uint16_t aType,
    bool ordered, uint16_t aMaxTime, uint16_t aMaxNum, bool aExternalNegotiated,
    uint16_t aStream, ErrorResult& rv) {
  RefPtr<nsDOMDataChannel> result;
  rv = CreateDataChannel(aLabel, aProtocol, aType, ordered, aMaxTime, aMaxNum,
                         aExternalNegotiated, aStream, getter_AddRefs(result));
  return result.forget();
}

NS_IMETHODIMP
PeerConnectionImpl::CreateDataChannel(
    const nsAString& aLabel, const nsAString& aProtocol, uint16_t aType,
    bool ordered, uint16_t aMaxTime, uint16_t aMaxNum, bool aExternalNegotiated,
    uint16_t aStream, nsDOMDataChannel** aRetval) {
  PC_AUTO_ENTER_API_CALL(false);
  MOZ_ASSERT(aRetval);

  // WebRTC is not enabled when recording/replaying. See bug 1304149.
  if (recordreplay::IsRecordingOrReplaying()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<DataChannel> dataChannel;
  DataChannelConnection::Type theType =
      static_cast<DataChannelConnection::Type>(aType);

  nsresult rv = EnsureDataConnection(
      WEBRTC_DATACHANNEL_PORT_DEFAULT, WEBRTC_DATACHANNEL_STREAMS_DEFAULT,
      WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_REMOTE_DEFAULT, false);
  if (NS_FAILED(rv)) {
    return rv;
  }
  dataChannel = mDataConnection->Open(
      NS_ConvertUTF16toUTF8(aLabel), NS_ConvertUTF16toUTF8(aProtocol), theType,
      ordered,
      aType == DataChannelConnection::PARTIAL_RELIABLE_REXMIT
          ? aMaxNum
          : (aType == DataChannelConnection::PARTIAL_RELIABLE_TIMED ? aMaxTime
                                                                    : 0),
      nullptr, nullptr, aExternalNegotiated, aStream);
  NS_ENSURE_TRUE(dataChannel, NS_ERROR_FAILURE);

  CSFLogDebug(LOGTAG, "%s: making DOMDataChannel", __FUNCTION__);

  RefPtr<JsepTransceiver> dcTransceiver;
  for (auto& transceiver : mJsepSession->GetTransceivers()) {
    if (transceiver->GetMediaType() == SdpMediaSection::kApplication) {
      dcTransceiver = transceiver;
      break;
    }
  }

  if (!dcTransceiver) {
    dcTransceiver =
        new JsepTransceiver(SdpMediaSection::MediaType::kApplication);
    mJsepSession->AddTransceiver(dcTransceiver);
  }

  dcTransceiver->RestartDatachannelTransceiver();

  RefPtr<nsDOMDataChannel> retval;
  rv = NS_NewDOMDataChannel(dataChannel.forget(), mWindow,
                            getter_AddRefs(retval));
  if (NS_FAILED(rv)) {
    return rv;
  }
  retval.forget(aRetval);
  return NS_OK;
}

void PeerConnectionImpl::NotifyDataChannel(
    already_AddRefed<DataChannel> aChannel) {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  RefPtr<DataChannel> channel(aChannel);
  MOZ_ASSERT(channel);
  CSFLogDebug(LOGTAG, "%s: channel: %p", __FUNCTION__, channel.get());

  RefPtr<nsDOMDataChannel> domchannel;
  nsresult rv = NS_NewDOMDataChannel(channel.forget(), mWindow,
                                     getter_AddRefs(domchannel));
  NS_ENSURE_SUCCESS_VOID(rv);

  JSErrorResult jrv;
  mPCObserver->NotifyDataChannel(*domchannel, jrv);
}

NS_IMETHODIMP
PeerConnectionImpl::CreateOffer(const RTCOfferOptions& aOptions) {
  JsepOfferOptions options;
  // convert the RTCOfferOptions to JsepOfferOptions
  if (aOptions.mOfferToReceiveAudio.WasPassed()) {
    options.mOfferToReceiveAudio =
        mozilla::Some(size_t(aOptions.mOfferToReceiveAudio.Value()));
  }

  if (aOptions.mOfferToReceiveVideo.WasPassed()) {
    options.mOfferToReceiveVideo =
        mozilla::Some(size_t(aOptions.mOfferToReceiveVideo.Value()));
  }

  options.mIceRestart = mozilla::Some(aOptions.mIceRestart);

  return CreateOffer(options);
}

static void DeferredCreateOffer(const std::string& aPcHandle,
                                const JsepOfferOptions& aOptions) {
  PeerConnectionWrapper wrapper(aPcHandle);

  if (wrapper.impl()) {
    if (!PeerConnectionCtx::GetInstance()->isReady()) {
      MOZ_CRASH(
          "Why is DeferredCreateOffer being executed when the "
          "PeerConnectionCtx isn't ready?");
    }
    wrapper.impl()->CreateOffer(aOptions);
  }
}

// Have to use unique_ptr because webidl enums are generated without a
// copy c'tor.
static std::unique_ptr<dom::PCErrorData> buildJSErrorData(
    const JsepSession::Result& aResult, const std::string& aMessage) {
  std::unique_ptr<dom::PCErrorData> result(new dom::PCErrorData);
  result->mName = *aResult.mError;
  result->mMessage = NS_ConvertASCIItoUTF16(aMessage.c_str());
  return result;
}

// Used by unit tests and the IDL CreateOffer.
NS_IMETHODIMP
PeerConnectionImpl::CreateOffer(const JsepOfferOptions& aOptions) {
  PC_AUTO_ENTER_API_CALL(true);

  if (!PeerConnectionCtx::GetInstance()->isReady()) {
    // Uh oh. We're not ready yet. Enqueue this operation.
    PeerConnectionCtx::GetInstance()->queueJSEPOperation(
        WrapRunnableNM(DeferredCreateOffer, mHandle, aOptions));
    STAMP_TIMECARD(mTimeCard, "Deferring CreateOffer (not ready)");
    return NS_OK;
  }

  CSFLogDebug(LOGTAG, "CreateOffer()");

  nsresult nrv = ConfigureJsepSessionCodecs();
  if (NS_FAILED(nrv)) {
    CSFLogError(LOGTAG, "Failed to configure codecs");
    return nrv;
  }

  STAMP_TIMECARD(mTimeCard, "Create Offer");

  std::string offer;

  JsepSession::Result result = mJsepSession->CreateOffer(aOptions, &offer);
  JSErrorResult rv;
  if (result.mError.isSome()) {
    std::string errorString = mJsepSession->GetLastError();

    CSFLogError(LOGTAG, "%s: pc = %s, error = %s", __FUNCTION__,
                mHandle.c_str(), errorString.c_str());

    mPCObserver->OnCreateOfferError(*buildJSErrorData(result, errorString), rv);
  } else {
    UpdateSignalingState();
    mPCObserver->OnCreateOfferSuccess(ObString(offer.c_str()), rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::CreateAnswer() {
  PC_AUTO_ENTER_API_CALL(true);

  CSFLogDebug(LOGTAG, "CreateAnswer()");

  STAMP_TIMECARD(mTimeCard, "Create Answer");
  // TODO(bug 1098015): Once RTCAnswerOptions is standardized, we'll need to
  // add it as a param to CreateAnswer, and convert it here.
  JsepAnswerOptions options;
  std::string answer;

  JsepSession::Result result = mJsepSession->CreateAnswer(options, &answer);
  JSErrorResult rv;
  if (result.mError.isSome()) {
    std::string errorString = mJsepSession->GetLastError();

    CSFLogError(LOGTAG, "%s: pc = %s, error = %s", __FUNCTION__,
                mHandle.c_str(), errorString.c_str());

    mPCObserver->OnCreateAnswerError(*buildJSErrorData(result, errorString),
                                     rv);
  } else {
    UpdateSignalingState();
    mPCObserver->OnCreateAnswerSuccess(ObString(answer.c_str()), rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::SetLocalDescription(int32_t aAction, const char* aSDP) {
  PC_AUTO_ENTER_API_CALL(true);

  if (!aSDP) {
    CSFLogError(LOGTAG, "%s - aSDP is NULL", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  JSErrorResult rv;
  STAMP_TIMECARD(mTimeCard, "Set Local Description");

  if (mMedia->AnyLocalTrackHasPeerIdentity()) {
    mPrivacyRequested = Some(true);
  }

  mLocalRequestedSDP = aSDP;

  bool wasRestartingIce = mJsepSession->IsIceRestarting();
  JsepSdpType sdpType;
  switch (aAction) {
    case IPeerConnection::kActionOffer:
      sdpType = mozilla::kJsepSdpOffer;
      break;
    case IPeerConnection::kActionAnswer:
      sdpType = mozilla::kJsepSdpAnswer;
      break;
    case IPeerConnection::kActionPRAnswer:
      sdpType = mozilla::kJsepSdpPranswer;
      break;
    case IPeerConnection::kActionRollback:
      sdpType = mozilla::kJsepSdpRollback;
      break;
    default:
      MOZ_ASSERT(false);
      return NS_ERROR_FAILURE;
  }
  JsepSession::Result result =
      mJsepSession->SetLocalDescription(sdpType, mLocalRequestedSDP);
  if (result.mError.isSome()) {
    std::string errorString = mJsepSession->GetLastError();
    CSFLogError(LOGTAG, "%s: pc = %s, error = %s", __FUNCTION__,
                mHandle.c_str(), errorString.c_str());
    mPCObserver->OnSetLocalDescriptionError(
        *buildJSErrorData(result, errorString), rv);
  } else {
    if (wasRestartingIce) {
      RecordIceRestartStatistics(sdpType);
    }
    mPCObserver->SyncTransceivers(rv);
    UpdateSignalingState(sdpType == mozilla::kJsepSdpRollback);
    mPCObserver->OnSetLocalDescriptionSuccess(rv);
  }

  return NS_OK;
}

static void DeferredSetRemote(const std::string& aPcHandle, int32_t aAction,
                              const std::string& aSdp) {
  PeerConnectionWrapper wrapper(aPcHandle);

  if (wrapper.impl()) {
    if (!PeerConnectionCtx::GetInstance()->isReady()) {
      MOZ_CRASH(
          "Why is DeferredSetRemote being executed when the "
          "PeerConnectionCtx isn't ready?");
    }
    wrapper.impl()->SetRemoteDescription(aAction, aSdp.c_str());
  }
}

NS_IMETHODIMP
PeerConnectionImpl::SetRemoteDescription(int32_t action, const char* aSDP) {
  PC_AUTO_ENTER_API_CALL(true);

  if (!aSDP) {
    CSFLogError(LOGTAG, "%s - aSDP is NULL", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  JSErrorResult jrv;
  if (action == IPeerConnection::kActionOffer) {
    if (!PeerConnectionCtx::GetInstance()->isReady()) {
      // Uh oh. We're not ready yet. Enqueue this operation. (This must be a
      // remote offer, or else we would not have gotten this far)
      PeerConnectionCtx::GetInstance()->queueJSEPOperation(WrapRunnableNM(
          DeferredSetRemote, mHandle, action, std::string(aSDP)));
      STAMP_TIMECARD(mTimeCard, "Deferring SetRemote (not ready)");
      return NS_OK;
    }

    nsresult nrv = ConfigureJsepSessionCodecs();
    if (NS_FAILED(nrv)) {
      CSFLogError(LOGTAG, "Failed to configure codecs");
      return nrv;
    }
  }

  STAMP_TIMECARD(mTimeCard, "Set Remote Description");

  mRemoteRequestedSDP = aSDP;
  bool wasRestartingIce = mJsepSession->IsIceRestarting();
  JsepSdpType sdpType;
  switch (action) {
    case IPeerConnection::kActionOffer:
      sdpType = mozilla::kJsepSdpOffer;
      break;
    case IPeerConnection::kActionAnswer:
      sdpType = mozilla::kJsepSdpAnswer;
      break;
    case IPeerConnection::kActionPRAnswer:
      sdpType = mozilla::kJsepSdpPranswer;
      break;
    case IPeerConnection::kActionRollback:
      sdpType = mozilla::kJsepSdpRollback;
      break;
    default:
      MOZ_ASSERT(false);
      return NS_ERROR_FAILURE;
  }

  size_t originalTransceiverCount = mJsepSession->GetTransceivers().size();
  JsepSession::Result result =
      mJsepSession->SetRemoteDescription(sdpType, mRemoteRequestedSDP);
  if (result.mError.isSome()) {
    std::string errorString = mJsepSession->GetLastError();
    CSFLogError(LOGTAG, "%s: pc = %s, error = %s", __FUNCTION__,
                mHandle.c_str(), errorString.c_str());
    mPCObserver->OnSetRemoteDescriptionError(
        *buildJSErrorData(result, errorString), jrv);
  } else {
    // Iterate over the JSEP transceivers that were just created
    for (size_t i = originalTransceiverCount;
         i < mJsepSession->GetTransceivers().size(); ++i) {
      RefPtr<JsepTransceiver> jsepTransceiver =
          mJsepSession->GetTransceivers()[i];

      if (jsepTransceiver->GetMediaType() ==
          SdpMediaSection::MediaType::kApplication) {
        continue;
      }

      // Audio or video transceiver, need to tell JS about it.
      RefPtr<TransceiverImpl> transceiverImpl =
          CreateTransceiverImpl(jsepTransceiver, nullptr, jrv);
      if (jrv.Failed()) {
        return NS_ERROR_FAILURE;
      }

      const JsepTrack& receiving(jsepTransceiver->mRecvTrack);
      CSFLogInfo(LOGTAG, "%s: pc = %s, asking JS to create transceiver",
                 __FUNCTION__, mHandle.c_str());
      switch (receiving.GetMediaType()) {
        case SdpMediaSection::MediaType::kAudio:
          mPCObserver->OnTransceiverNeeded(NS_ConvertASCIItoUTF16("audio"),
                                           *transceiverImpl, jrv);
          break;
        case SdpMediaSection::MediaType::kVideo:
          mPCObserver->OnTransceiverNeeded(NS_ConvertASCIItoUTF16("video"),
                                           *transceiverImpl, jrv);
          break;
        default:
          MOZ_RELEASE_ASSERT(false);
      }

      if (jrv.Failed()) {
        nsresult rv = jrv.StealNSResult();
        CSFLogError(LOGTAG,
                    "%s: pc = %s, OnTransceiverNeeded failed. "
                    "This should never happen. rv = %d",
                    __FUNCTION__, mHandle.c_str(), static_cast<int>(rv));
        MOZ_CRASH();
        return NS_ERROR_FAILURE;
      }
    }

    if (wasRestartingIce) {
      RecordIceRestartStatistics(sdpType);
    }

    mPCObserver->SyncTransceivers(jrv);

    UpdateSignalingState(sdpType == mozilla::kJsepSdpRollback);

    mPCObserver->OnSetRemoteDescriptionSuccess(jrv);

    startCallTelem();
  }

  return NS_OK;
}

// WebRTC uses highres time relative to the UNIX epoch (Jan 1, 1970, UTC).

nsresult PeerConnectionImpl::GetTimeSinceEpoch(DOMHighResTimeStamp* result) {
  MOZ_ASSERT(NS_IsMainThread());
  Performance* perf = mWindow->GetPerformance();
  NS_ENSURE_TRUE(perf && perf->Timing(), NS_ERROR_UNEXPECTED);
  *result = perf->Now() + perf->Timing()->NavigationStart();
  return NS_OK;
}

class RTCStatsReportInternalConstruct : public RTCStatsReportInternal {
 public:
  RTCStatsReportInternalConstruct(const nsString& pcid,
                                  DOMHighResTimeStamp now) {
    mPcid = pcid;
    mRtpContributingSourceStats.Construct();
    mInboundRtpStreamStats.Construct();
    mOutboundRtpStreamStats.Construct();
    mRemoteInboundRtpStreamStats.Construct();
    mRemoteOutboundRtpStreamStats.Construct();
    mIceCandidatePairStats.Construct();
    mIceCandidateStats.Construct();
    mTimestamp.Construct(now);
    mTrickledIceCandidateStats.Construct();
    mRawLocalCandidates.Construct();
    mRawRemoteCandidates.Construct();
  }
};

NS_IMETHODIMP
PeerConnectionImpl::GetStats(MediaStreamTrack* aSelector) {
  PC_AUTO_ENTER_API_CALL(true);

  GetStats(aSelector, false, false)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [handle = mHandle](UniquePtr<RTCStatsQuery>&& aQuery) {
            DeliverStatsReportToPCObserver_m(
                handle, NS_OK, nsAutoPtr<RTCStatsQuery>(aQuery.release()));
          },
          [handle = mHandle](nsresult aError) {
            DeliverStatsReportToPCObserver_m(handle, aError,
                                             nsAutoPtr<RTCStatsQuery>());
          });

  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::AddIceCandidate(
    const char* aCandidate, const char* aMid, const char* aUfrag,
    const dom::Nullable<unsigned short>& aLevel) {
  PC_AUTO_ENTER_API_CALL(true);

  if (mForceIceTcp &&
      std::string::npos != std::string(aCandidate).find(" UDP ")) {
    CSFLogError(LOGTAG, "Blocking remote UDP candidate: %s", aCandidate);
    return NS_OK;
  }

  JSErrorResult rv;
  STAMP_TIMECARD(mTimeCard, "Add Ice Candidate");

  CSFLogDebug(LOGTAG, "AddIceCandidate: %s %s", aCandidate, aUfrag);

  std::string transportId;
  Maybe<unsigned short> level;
  if (!aLevel.IsNull()) {
    level = Some(aLevel.Value());
  }
  JsepSession::Result result = mJsepSession->AddRemoteIceCandidate(
      aCandidate, aMid, level, aUfrag, &transportId);

  if (!result.mError.isSome()) {
    // We do not bother PCMedia about this before offer/answer concludes.
    // Once offer/answer concludes, PCMedia will extract these candidates from
    // the remote SDP.
    if (mSignalingState == RTCSignalingState::Stable && !transportId.empty()) {
      mMedia->AddIceCandidate(aCandidate, transportId, aUfrag);
      mRawTrickledCandidates.push_back(aCandidate);
    }
    mPCObserver->OnAddIceCandidateSuccess(rv);
  } else {
    std::string errorString = mJsepSession->GetLastError();

    CSFLogError(LOGTAG,
                "Failed to incorporate remote candidate into SDP:"
                " res = %u, candidate = %s, level = %i, error = %s",
                static_cast<unsigned>(*result.mError), aCandidate,
                level.valueOr(-1), errorString.c_str());

    mPCObserver->OnAddIceCandidateError(*buildJSErrorData(result, errorString),
                                        rv);
  }

  return NS_OK;
}

void PeerConnectionImpl::UpdateNetworkState(bool online) {
  if (!mMedia) {
    return;
  }
  mMedia->UpdateNetworkState(online);
}

NS_IMETHODIMP
PeerConnectionImpl::CloseStreams() {
  PC_AUTO_ENTER_API_CALL(false);

  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::SetPeerIdentity(const nsAString& aPeerIdentity) {
  PC_AUTO_ENTER_API_CALL(true);
  MOZ_ASSERT(!aPeerIdentity.IsEmpty());

  // once set, this can't be changed
  if (mPeerIdentity) {
    if (!mPeerIdentity->Equals(aPeerIdentity)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    mPeerIdentity = new PeerIdentity(aPeerIdentity);
    Document* doc = GetWindow()->GetExtantDoc();
    if (!doc) {
      CSFLogInfo(LOGTAG, "Can't update principal on streams; document gone");
      return NS_ERROR_FAILURE;
    }
    MediaStreamTrack* allTracks = nullptr;
    mMedia->UpdateSinkIdentity_m(allTracks, doc->NodePrincipal(),
                                 mPeerIdentity);
  }
  return NS_OK;
}

nsresult PeerConnectionImpl::OnAlpnNegotiated(const std::string& aAlpn) {
  PC_AUTO_ENTER_API_CALL(false);
  if (mPrivacyRequested.isSome()) {
    return NS_OK;
  }

  mPrivacyRequested = Some(aAlpn == "c-webrtc");

  // For this, as with mPrivacyRequested, once we've connected to a peer, we
  // fixate on that peer.  Dealing with multiple peers or connections is more
  // than this run-down wreck of an object can handle.
  // Besides, this is only used to say if we have been connected ever.
  if (!*mPrivacyRequested) {
    // Neither side wants privacy
    Document* doc = GetWindow()->GetExtantDoc();
    if (!doc) {
      CSFLogInfo(LOGTAG, "Can't update principal on streams; document gone");
      return NS_ERROR_FAILURE;
    }
    mMedia->UpdateRemoteStreamPrincipals_m(doc->NodePrincipal());
  }

  return NS_OK;
}

void PeerConnectionImpl::PrincipalChanged(MediaStreamTrack* aTrack) {
  Document* doc = GetWindow()->GetExtantDoc();
  if (doc) {
    mMedia->UpdateSinkIdentity_m(aTrack, doc->NodePrincipal(), mPeerIdentity);
  } else {
    CSFLogInfo(LOGTAG, "Can't update sink principal; document gone");
  }
}

void PeerConnectionImpl::OnMediaError(const std::string& aError) {
  CSFLogError(LOGTAG, "Encountered media error! %s", aError.c_str());
  // TODO: Let content know about this somehow.
}

bool PeerConnectionImpl::ShouldDumpPacket(size_t level,
                                          dom::mozPacketDumpType type,
                                          bool sending) const {
  if (!mPacketDumpEnabled) {
    return false;
  }

  MutexAutoLock lock(mPacketDumpFlagsMutex);

  const std::vector<unsigned>* packetDumpFlags;

  if (sending) {
    packetDumpFlags = &mSendPacketDumpFlags;
  } else {
    packetDumpFlags = &mRecvPacketDumpFlags;
  }

  if (level < packetDumpFlags->size()) {
    unsigned flag = 1 << (unsigned)type;
    return flag & packetDumpFlags->at(level);
  }

  return false;
}

void PeerConnectionImpl::DumpPacket_m(size_t level, dom::mozPacketDumpType type,
                                      bool sending,
                                      UniquePtr<uint8_t[]>& packet,
                                      size_t size) {
  if (IsClosed()) {
    return;
  }

  if (!ShouldDumpPacket(level, type, sending)) {
    return;
  }

  // TODO: Is this efficient? Should we try grabbing our JS ctx from somewhere
  // else?
  AutoJSAPI jsapi;
  if (!jsapi.Init(GetWindow())) {
    return;
  }

  JS::Rooted<JSObject*> jsobj(
      jsapi.cx(),
      JS::NewArrayBufferWithContents(jsapi.cx(), size, packet.release()));

  RootedSpiderMonkeyInterface<ArrayBuffer> arrayBuffer(jsapi.cx());
  if (!arrayBuffer.Init(jsobj)) {
    return;
  }

  JSErrorResult jrv;
  mPCObserver->OnPacket(level, type, sending, arrayBuffer, jrv);
}

NS_IMETHODIMP
PeerConnectionImpl::GetRtpSources(
    MediaStreamTrack& aRecvTrack, DOMHighResTimeStamp aRtpSourceTimeNow,
    nsTArray<dom::RTCRtpSourceEntry>& outRtpSources) {
  PC_AUTO_ENTER_API_CALL(true);
  outRtpSources.Clear();
  std::vector<RefPtr<TransceiverImpl>>& transceivers =
      mMedia->GetTransceivers();
  for (RefPtr<TransceiverImpl>& transceiver : transceivers) {
    if (transceiver->HasReceiveTrack(&aRecvTrack)) {
      transceiver->GetRtpSources(aRtpSourceTimeNow, outRtpSources);
      break;
    }
  }
  return NS_OK;
}

DOMHighResTimeStamp PeerConnectionImpl::GetNowInRtpSourceReferenceTime() {
  return RtpSourceObserver::NowInReportClockTime();
}

// test-only: adds fake CSRCs and audio data
nsresult PeerConnectionImpl::InsertAudioLevelForContributingSource(
    dom::MediaStreamTrack& aRecvTrack, unsigned long aSource,
    DOMHighResTimeStamp aTimestamp, bool aHasLevel, uint8_t aLevel) {
  PC_AUTO_ENTER_API_CALL(true);
  std::vector<RefPtr<TransceiverImpl>>& transceivers =
      mMedia->GetTransceivers();
  for (RefPtr<TransceiverImpl>& transceiver : transceivers) {
    if (transceiver->HasReceiveTrack(&aRecvTrack)) {
      transceiver->InsertAudioLevelForContributingSource(aSource, aTimestamp,
                                                         aHasLevel, aLevel);
      break;
    }
  }

  return NS_OK;
}

nsresult PeerConnectionImpl::AddRIDExtension(MediaStreamTrack& aRecvTrack,
                                             unsigned short aExtensionId) {
  return mMedia->AddRIDExtension(aRecvTrack, aExtensionId);
}

nsresult PeerConnectionImpl::AddRIDFilter(MediaStreamTrack& aRecvTrack,
                                          const nsAString& aRid) {
  return mMedia->AddRIDFilter(aRecvTrack, aRid);
}

nsresult PeerConnectionImpl::EnablePacketDump(unsigned long level,
                                              dom::mozPacketDumpType type,
                                              bool sending) {
  mPacketDumpEnabled = true;
  std::vector<unsigned>* packetDumpFlags;
  if (sending) {
    packetDumpFlags = &mSendPacketDumpFlags;
  } else {
    packetDumpFlags = &mRecvPacketDumpFlags;
  }

  unsigned flag = 1 << (unsigned)type;

  MutexAutoLock lock(mPacketDumpFlagsMutex);
  if (level >= packetDumpFlags->size()) {
    packetDumpFlags->resize(level + 1);
  }

  (*packetDumpFlags)[level] |= flag;
  return NS_OK;
}

nsresult PeerConnectionImpl::DisablePacketDump(unsigned long level,
                                               dom::mozPacketDumpType type,
                                               bool sending) {
  std::vector<unsigned>* packetDumpFlags;
  if (sending) {
    packetDumpFlags = &mSendPacketDumpFlags;
  } else {
    packetDumpFlags = &mRecvPacketDumpFlags;
  }

  unsigned flag = 1 << (unsigned)type;

  MutexAutoLock lock(mPacketDumpFlagsMutex);
  if (level < packetDumpFlags->size()) {
    (*packetDumpFlags)[level] &= ~flag;
  }

  return NS_OK;
}

static int GetDTMFToneCode(uint16_t c) {
  const char* DTMF_TONECODES = "0123456789*#ABCD";

  if (c == ',') {
    // , is a special character indicating a 2 second delay
    return -1;
  }

  const char* i = strchr(DTMF_TONECODES, c);
  MOZ_ASSERT(i);
  return i - DTMF_TONECODES;
}

OwningNonNull<dom::MediaStreamTrack> PeerConnectionImpl::CreateReceiveTrack(
    SdpMediaSection::MediaType type) {
  bool audio = (type == SdpMediaSection::MediaType::kAudio);

  // Set the principal used for creating the tracks. This makes the track
  // data (audio/video samples) accessible to the receiving page. We're
  // only certain that privacy hasn't been requested if we're connected.
  nsCOMPtr<nsIPrincipal> principal;
  Document* doc = GetWindow()->GetExtantDoc();
  MOZ_ASSERT(doc);
  if (mPrivacyRequested.isSome() && !*mPrivacyRequested) {
    principal = doc->NodePrincipal();
  } else {
    // we're either certain that we need isolation for the tracks, OR
    // we're not sure and we can fix the track in SetDtlsConnected
    principal =
        NullPrincipal::CreateWithInheritedAttributes(doc->NodePrincipal());
  }

  MediaStreamGraph* graph = MediaStreamGraph::GetInstance(
      audio ? MediaStreamGraph::AUDIO_THREAD_DRIVER
            : MediaStreamGraph::SYSTEM_THREAD_DRIVER,
      GetWindow(), MediaStreamGraph::REQUEST_DEFAULT_SAMPLE_RATE);

  RefPtr<MediaStreamTrack> track;
  RefPtr<RemoteTrackSource> trackSource;
  RefPtr<SourceMediaStream> source = graph->CreateSourceStream();
  if (audio) {
    trackSource = new RemoteTrackSource(source, principal,
                                        NS_ConvertASCIItoUTF16("remote audio"));
    track = new AudioStreamTrack(GetWindow(), source,
                                 333,  // Use a constant TrackID. Dependents
                                       // read this from the DOM track.
                                 trackSource);
  } else {
    trackSource = new RemoteTrackSource(source, principal,
                                        NS_ConvertASCIItoUTF16("remote video"));
    track = new VideoStreamTrack(GetWindow(), source,
                                 666,  // Use a constant TrackID. Dependents
                                       // read this from the DOM track.
                                 trackSource);
  }

  CSFLogDebug(LOGTAG, "Created %s track %p, inner: %p",
              audio ? "audio" : "video", track.get(), track->GetStream());

  // Spec says remote tracks start out muted.
  trackSource->SetMuted(true);

  return OwningNonNull<dom::MediaStreamTrack>(*track);
}

NS_IMETHODIMP
PeerConnectionImpl::InsertDTMF(TransceiverImpl& transceiver,
                               const nsAString& tones, uint32_t duration,
                               uint32_t interToneGap) {
  PC_AUTO_ENTER_API_CALL(false);

  // Check values passed in from PeerConnection.js
  MOZ_ASSERT(duration >= 40, "duration must be at least 40");
  MOZ_ASSERT(duration <= 6000, "duration must be at most 6000");
  MOZ_ASSERT(interToneGap >= 30, "interToneGap must be at least 30");

  JSErrorResult jrv;

  // TODO(bug 1401983): Move DTMF stuff to TransceiverImpl
  // Attempt to locate state for the DTMFSender
  RefPtr<DTMFState> state;
  for (auto& dtmfState : mDTMFStates) {
    if (dtmfState->mTransceiver.get() == &transceiver) {
      state = dtmfState;
      break;
    }
  }

  // No state yet, create a new one
  if (!state) {
    state = *mDTMFStates.AppendElement(new DTMFState);
    state->mPCObserver = mPCObserver;
    state->mTransceiver = &transceiver;
  }
  MOZ_ASSERT(state);

  state->mTones = tones;
  state->mDuration = duration;
  state->mInterToneGap = interToneGap;
  if (!state->mTones.IsEmpty()) {
    state->StartPlayout(0);
  }
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::GetDTMFToneBuffer(mozilla::dom::RTCRtpSender& sender,
                                      nsAString& outToneBuffer) {
  PC_AUTO_ENTER_API_CALL(false);

  JSErrorResult jrv;

  // Retrieve track
  RefPtr<MediaStreamTrack> mst = sender.GetTrack(jrv);
  if (jrv.Failed()) {
    NS_WARNING("Failed to retrieve track for RTCRtpSender!");
    return jrv.StealNSResult();
  }

  // TODO(bug 1401983): Move DTMF stuff to TransceiverImpl
  // Attempt to locate state for the DTMFSender
  for (auto& dtmfState : mDTMFStates) {
    if (dtmfState->mTransceiver->HasSendTrack(mst)) {
      outToneBuffer = dtmfState->mTones;
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::ReplaceTrackNoRenegotiation(TransceiverImpl& aTransceiver,
                                                MediaStreamTrack* aWithTrack) {
  PC_AUTO_ENTER_API_CALL(true);

  RefPtr<dom::MediaStreamTrack> oldSendTrack(aTransceiver.GetSendTrack());
  if (oldSendTrack) {
    oldSendTrack->RemovePrincipalChangeObserver(this);
  }

  nsresult rv = aTransceiver.UpdateSendTrack(aWithTrack);

  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "Failed to update transceiver: %d",
                static_cast<int>(rv));
    return rv;
  }

  // TODO(bug 1401983): Move DTMF stuff to TransceiverImpl
  for (size_t i = 0; i < mDTMFStates.Length(); ++i) {
    if (mDTMFStates[i]->mTransceiver.get() == &aTransceiver) {
      mDTMFStates[i]->StopPlayout();
      mDTMFStates.RemoveElementAt(i);
      break;
    }
  }

  if (aWithTrack) {
    aWithTrack->AddPrincipalChangeObserver(this);
    PrincipalChanged(aWithTrack);
  }

  // We update the media pipelines here so we can apply different codec
  // settings for different sources (e.g. screensharing as opposed to camera.)
  // TODO: We should probably only do this if the source has in fact changed.

  if (NS_FAILED((rv = mMedia->UpdateMediaPipelines()))) {
    CSFLogError(LOGTAG, "Error Updating MediaPipelines");
    return rv;
  }

  return NS_OK;
}

nsresult PeerConnectionImpl::CalculateFingerprint(
    const std::string& algorithm, std::vector<uint8_t>* fingerprint) const {
  DtlsDigest digest(algorithm);

  MOZ_ASSERT(fingerprint);
  const UniqueCERTCertificate& cert = mCertificate->Certificate();
  nsresult rv = DtlsIdentity::ComputeFingerprint(cert, &digest);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "Unable to calculate certificate fingerprint, rv=%u",
                static_cast<unsigned>(rv));
    return rv;
  }
  *fingerprint = digest.value_;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::GetFingerprint(char** fingerprint) {
  MOZ_ASSERT(fingerprint);
  MOZ_ASSERT(mCertificate);
  std::vector<uint8_t> fp;
  nsresult rv = CalculateFingerprint(DtlsIdentity::DEFAULT_HASH_ALGORITHM, &fp);
  NS_ENSURE_SUCCESS(rv, rv);
  std::ostringstream os;
  os << DtlsIdentity::DEFAULT_HASH_ALGORITHM << ' '
     << SdpFingerprintAttributeList::FormatFingerprint(fp);
  std::string fpStr = os.str();

  char* tmp = new char[fpStr.size() + 1];
  std::copy(fpStr.begin(), fpStr.end(), tmp);
  tmp[fpStr.size()] = '\0';

  *fingerprint = tmp;
  return NS_OK;
}

void PeerConnectionImpl::GetCurrentLocalDescription(nsAString& aSDP) {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  std::string localSdp =
      mJsepSession->GetLocalDescription(kJsepDescriptionCurrent);
  aSDP = NS_ConvertASCIItoUTF16(localSdp.c_str());
}

void PeerConnectionImpl::GetPendingLocalDescription(nsAString& aSDP) {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  std::string localSdp =
      mJsepSession->GetLocalDescription(kJsepDescriptionPending);
  aSDP = NS_ConvertASCIItoUTF16(localSdp.c_str());
}

void PeerConnectionImpl::GetCurrentRemoteDescription(nsAString& aSDP) {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  std::string remoteSdp =
      mJsepSession->GetRemoteDescription(kJsepDescriptionCurrent);
  aSDP = NS_ConvertASCIItoUTF16(remoteSdp.c_str());
}

void PeerConnectionImpl::GetPendingRemoteDescription(nsAString& aSDP) {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  std::string remoteSdp =
      mJsepSession->GetRemoteDescription(kJsepDescriptionPending);
  aSDP = NS_ConvertASCIItoUTF16(remoteSdp.c_str());
}

NS_IMETHODIMP
PeerConnectionImpl::SignalingState(RTCSignalingState* aState) {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aState);

  *aState = mSignalingState;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::IceConnectionState(RTCIceConnectionState* aState) {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aState);

  *aState = mIceConnectionState;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::IceGatheringState(RTCIceGatheringState* aState) {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aState);

  *aState = mIceGatheringState;
  return NS_OK;
}

nsresult PeerConnectionImpl::CheckApiState(bool assert_ice_ready) const {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(mTrickle || !assert_ice_ready ||
             (mIceGatheringState == RTCIceGatheringState::Complete));

  if (IsClosed()) {
    CSFLogError(LOGTAG, "%s: called API while closed", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }
  if (!mMedia) {
    CSFLogError(LOGTAG, "%s: called API with disposed mMedia", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::Close() {
  CSFLogDebug(LOGTAG, "%s: for %s", __FUNCTION__, mHandle.c_str());
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  SetSignalingState_m(RTCSignalingState::Closed);

  return NS_OK;
}

bool PeerConnectionImpl::PluginCrash(uint32_t aPluginID,
                                     const nsAString& aPluginName) {
  // fire an event to the DOM window if this is "ours"
  bool result = mMedia ? mMedia->AnyCodecHasPluginID(aPluginID) : false;
  if (!result) {
    return false;
  }

  CSFLogError(LOGTAG, "%s: Our plugin %llu crashed", __FUNCTION__,
              static_cast<unsigned long long>(aPluginID));

  RefPtr<Document> doc = mWindow->GetExtantDoc();
  if (!doc) {
    NS_WARNING("Couldn't get document for PluginCrashed event!");
    return true;
  }

  PluginCrashedEventInit init;
  init.mPluginID = aPluginID;
  init.mPluginName = aPluginName;
  init.mSubmittedCrashReport = false;
  init.mGmpPlugin = true;
  init.mBubbles = true;
  init.mCancelable = true;

  RefPtr<PluginCrashedEvent> event = PluginCrashedEvent::Constructor(
      doc, NS_LITERAL_STRING("PluginCrashed"), init);

  event->SetTrusted(true);
  event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;

  EventDispatcher::DispatchDOMEvent(mWindow, nullptr, event, nullptr, nullptr);

  return true;
}

void PeerConnectionImpl::RecordEndOfCallTelemetry() const {
  if (!mJsepSession) {
    return;
  }

  // Exit early if no connection information was ever exchanged,
  // This prevents distortion of telemetry data.
  if (mLocalRequestedSDP.empty() && mRemoteRequestedSDP.empty()) {
    return;
  }

  // Bitmask used for WEBRTC/LOOP_CALL_TYPE telemetry reporting
  static const uint32_t kAudioTypeMask = 1;
  static const uint32_t kVideoTypeMask = 2;
  static const uint32_t kDataChannelTypeMask = 4;

  // Report end-of-call Telemetry
  if (mJsepSession->GetNegotiations() > 0) {
    Telemetry::Accumulate(Telemetry::WEBRTC_RENEGOTIATIONS,
                          mJsepSession->GetNegotiations() - 1);
  }
  Telemetry::Accumulate(Telemetry::WEBRTC_MAX_VIDEO_SEND_TRACK,
                        mMaxSending[SdpMediaSection::MediaType::kVideo]);
  Telemetry::Accumulate(Telemetry::WEBRTC_MAX_VIDEO_RECEIVE_TRACK,
                        mMaxReceiving[SdpMediaSection::MediaType::kVideo]);
  Telemetry::Accumulate(Telemetry::WEBRTC_MAX_AUDIO_SEND_TRACK,
                        mMaxSending[SdpMediaSection::MediaType::kAudio]);
  Telemetry::Accumulate(Telemetry::WEBRTC_MAX_AUDIO_RECEIVE_TRACK,
                        mMaxReceiving[SdpMediaSection::MediaType::kAudio]);
  // DataChannels appear in both Sending and Receiving
  Telemetry::Accumulate(Telemetry::WEBRTC_DATACHANNEL_NEGOTIATED,
                        mMaxSending[SdpMediaSection::MediaType::kApplication]);
  // Enumerated/bitmask: 1 = Audio, 2 = Video, 4 = DataChannel
  // A/V = 3, A/V/D = 7, etc
  uint32_t type = 0;
  if (mMaxSending[SdpMediaSection::MediaType::kAudio] ||
      mMaxReceiving[SdpMediaSection::MediaType::kAudio]) {
    type = kAudioTypeMask;
  }
  if (mMaxSending[SdpMediaSection::MediaType::kVideo] ||
      mMaxReceiving[SdpMediaSection::MediaType::kVideo]) {
    type |= kVideoTypeMask;
  }
  if (mMaxSending[SdpMediaSection::MediaType::kApplication]) {
    type |= kDataChannelTypeMask;
  }
  Telemetry::Accumulate(Telemetry::WEBRTC_CALL_TYPE, type);
}

nsresult PeerConnectionImpl::CloseInt() {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  // TODO(bug 1401983): Move DTMF stuff to TransceiverImpl
  for (auto& dtmfState : mDTMFStates) {
    dtmfState->StopPlayout();
  }

  // We do this at the end of the call because we want to make sure we've waited
  // for all trickle ICE candidates to come in; this can happen well after we've
  // transitioned to connected. As a bonus, this allows us to detect race
  // conditions where a stats dispatch happens right as the PC closes.
  if (!mPrivateWindow) {
    RecordLongtermICEStatistics();
  }
  RecordEndOfCallTelemetry();
  CSFLogInfo(LOGTAG,
             "%s: Closing PeerConnectionImpl %s; "
             "ending call",
             __FUNCTION__, mHandle.c_str());
  if (mJsepSession) {
    mJsepSession->Close();
  }
  if (mDataConnection) {
    CSFLogInfo(LOGTAG, "%s: Destroying DataChannelConnection %p for %s",
               __FUNCTION__, (void*)mDataConnection.get(), mHandle.c_str());
    mDataConnection->Destroy();
    mDataConnection =
        nullptr;  // it may not go away until the runnables are dead
  }
  ShutdownMedia();

  // DataConnection will need to stay alive until all threads/runnables exit

  return NS_OK;
}

void PeerConnectionImpl::ShutdownMedia() {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  if (!mMedia) return;

  // before we destroy references to local tracks, detach from them
  for (RefPtr<TransceiverImpl>& transceiver : mMedia->GetTransceivers()) {
    RefPtr<dom::MediaStreamTrack> track = transceiver->GetSendTrack();
    if (track) {
      track->RemovePrincipalChangeObserver(this);
    }
  }

  // End of call to be recorded in Telemetry
  if (!mStartTime.IsNull()) {
    TimeDuration timeDelta = TimeStamp::Now() - mStartTime;
    Telemetry::Accumulate(Telemetry::WEBRTC_CALL_DURATION,
                          timeDelta.ToSeconds());
  }

  // Forget the reference so that we can transfer it to
  // SelfDestruct().
  mMedia.forget().take()->SelfDestruct();
}

void PeerConnectionImpl::SetSignalingState_m(RTCSignalingState aSignalingState,
                                             bool rollback) {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  if (mSignalingState == RTCSignalingState::Closed) {
    return;
  }

  if (aSignalingState == RTCSignalingState::Have_local_offer ||
      (aSignalingState == RTCSignalingState::Stable &&
       mSignalingState == RTCSignalingState::Have_remote_offer && !rollback)) {
    mMedia->EnsureTransports(*mJsepSession);
  }

  if (mSignalingState == aSignalingState) {
    return;
  }

  mSignalingState = aSignalingState;

  if (mSignalingState == RTCSignalingState::Stable) {
    // If we're rolling back a local offer, we might need to remove some
    // transports, and stomp some MediaPipeline setup, but nothing further
    // needs to be done.
    mMedia->UpdateTransports(*mJsepSession, mForceIceTcp);
    if (NS_FAILED(mMedia->UpdateMediaPipelines())) {
      CSFLogError(LOGTAG, "Error Updating MediaPipelines");
      NS_ASSERTION(false,
                   "Error Updating MediaPipelines in SetSignalingState_m()");
      // XXX what now?  Not much we can do but keep going, without major
      // restructuring
    }

    if (!rollback) {
      InitializeDataChannel();
      mMedia->StartIceChecks(*mJsepSession);
    }

    // Telemetry: record info on the current state of streams/renegotiations/etc
    // Note: this code gets run on rollbacks as well!

    // Update the max channels used with each direction for each type
    uint16_t receiving[SdpMediaSection::kMediaTypes];
    uint16_t sending[SdpMediaSection::kMediaTypes];
    mJsepSession->CountTracks(receiving, sending);
    for (size_t i = 0; i < SdpMediaSection::kMediaTypes; i++) {
      if (mMaxReceiving[i] < receiving[i]) {
        mMaxReceiving[i] = receiving[i];
      }
      if (mMaxSending[i] < sending[i]) {
        mMaxSending[i] = sending[i];
      }
    }
  }

  if (mSignalingState == RTCSignalingState::Closed) {
    CloseInt();
    // Uncount this connection as active on the inner window upon close.
    if (mWindow && mActiveOnWindow) {
      mWindow->RemovePeerConnection();
      mActiveOnWindow = false;
    }
  }

  JSErrorResult rv;
  mPCObserver->OnStateChange(PCObserverStateType::SignalingState, rv);
}

void PeerConnectionImpl::UpdateSignalingState(bool rollback) {
  mozilla::JsepSignalingState state = mJsepSession->GetState();

  RTCSignalingState newState;

  switch (state) {
    case kJsepStateStable:
      newState = RTCSignalingState::Stable;
      break;
    case kJsepStateHaveLocalOffer:
      newState = RTCSignalingState::Have_local_offer;
      break;
    case kJsepStateHaveRemoteOffer:
      newState = RTCSignalingState::Have_remote_offer;
      break;
    case kJsepStateHaveLocalPranswer:
      newState = RTCSignalingState::Have_local_pranswer;
      break;
    case kJsepStateHaveRemotePranswer:
      newState = RTCSignalingState::Have_remote_pranswer;
      break;
    case kJsepStateClosed:
      newState = RTCSignalingState::Closed;
      break;
    default:
      MOZ_CRASH();
  }

  SetSignalingState_m(newState, rollback);
}

bool PeerConnectionImpl::IsClosed() const {
  return mSignalingState == RTCSignalingState::Closed;
}

bool PeerConnectionImpl::HasMedia() const { return mMedia; }

PeerConnectionWrapper::PeerConnectionWrapper(const std::string& handle)
    : impl_(nullptr) {
  if (!PeerConnectionCtx::isActive() ||
      (PeerConnectionCtx::GetInstance()->mPeerConnections.find(handle) ==
       PeerConnectionCtx::GetInstance()->mPeerConnections.end())) {
    return;
  }

  PeerConnectionImpl* impl =
      PeerConnectionCtx::GetInstance()->mPeerConnections[handle];

  if (!impl->media()) return;

  impl_ = impl;
}

const RefPtr<MediaTransportHandler> PeerConnectionImpl::GetTransportHandler()
    const {
  return mTransportHandler;
}

const std::string& PeerConnectionImpl::GetHandle() {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  return mHandle;
}

const std::string& PeerConnectionImpl::GetName() {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  return mName;
}

void PeerConnectionImpl::CandidateReady(const std::string& candidate,
                                        const std::string& transportId,
                                        const std::string& ufrag) {
  PC_AUTO_ENTER_API_CALL_VOID_RETURN(false);

  if (mForceIceTcp && std::string::npos != candidate.find(" UDP ")) {
    CSFLogWarn(LOGTAG, "Blocking local UDP candidate: %s", candidate.c_str());
    return;
  }

  // One of the very few places we still use level; required by the JSEP API
  uint16_t level = 0;
  std::string mid;
  bool skipped = false;
  nsresult res = mJsepSession->AddLocalIceCandidate(
      candidate, transportId, ufrag, &level, &mid, &skipped);

  if (NS_FAILED(res)) {
    std::string errorString = mJsepSession->GetLastError();

    CSFLogError(LOGTAG,
                "Failed to incorporate local candidate into SDP:"
                " res = %u, candidate = %s, transport-id = %s,"
                " error = %s",
                static_cast<unsigned>(res), candidate.c_str(),
                transportId.c_str(), errorString.c_str());
    return;
  }

  if (skipped) {
    CSFLogDebug(LOGTAG,
                "Skipped adding local candidate %s (transport-id %s) "
                "to SDP, this typically happens because the m-section "
                "is bundled, which means it doesn't make sense for it "
                "to have its own transport-related attributes.",
                candidate.c_str(), transportId.c_str());
    return;
  }

  CSFLogDebug(LOGTAG, "Passing local candidate to content: %s",
              candidate.c_str());
  SendLocalIceCandidateToContent(level, mid, candidate, ufrag);
}

void PeerConnectionImpl::SendLocalIceCandidateToContent(
    uint16_t level, const std::string& mid, const std::string& candidate,
    const std::string& ufrag) {
  JSErrorResult rv;
  mPCObserver->OnIceCandidate(level, ObString(mid.c_str()),
                              ObString(candidate.c_str()),
                              ObString(ufrag.c_str()), rv);
}

void PeerConnectionImpl::IceConnectionStateChange(
    dom::RTCIceConnectionState domState) {
  PC_AUTO_ENTER_API_CALL_VOID_RETURN(false);

  CSFLogDebug(LOGTAG, "%s: %d", __FUNCTION__, static_cast<int>(domState));

  if (domState == mIceConnectionState) {
    // no work to be done since the states are the same.
    // this can happen during ICE rollback situations.
    return;
  }

  mIceConnectionState = domState;

  // Uncount this connection as active on the inner window upon close.
  if (mWindow && mActiveOnWindow &&
      mIceConnectionState == RTCIceConnectionState::Closed) {
    mWindow->RemovePeerConnection();
    mActiveOnWindow = false;
  }

  // Would be nice if we had a means of converting one of these dom enums
  // to a string that wasn't almost as much text as this switch statement...
  switch (mIceConnectionState) {
    case RTCIceConnectionState::New:
      STAMP_TIMECARD(mTimeCard, "Ice state: new");
      break;
    case RTCIceConnectionState::Checking:
      // For telemetry
      mIceStartTime = TimeStamp::Now();
      STAMP_TIMECARD(mTimeCard, "Ice state: checking");
      break;
    case RTCIceConnectionState::Connected:
      STAMP_TIMECARD(mTimeCard, "Ice state: connected");
      break;
    case RTCIceConnectionState::Completed:
      STAMP_TIMECARD(mTimeCard, "Ice state: completed");
      break;
    case RTCIceConnectionState::Failed:
      STAMP_TIMECARD(mTimeCard, "Ice state: failed");
      break;
    case RTCIceConnectionState::Disconnected:
      STAMP_TIMECARD(mTimeCard, "Ice state: disconnected");
      break;
    case RTCIceConnectionState::Closed:
      STAMP_TIMECARD(mTimeCard, "Ice state: closed");
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected mIceConnectionState!");
  }

  // Collect telemetry for situations where hostname obfuscation would be used
  uint64_t winId = GetWindow()->WindowID();
  bool iceJustFailed =
      mIceConnectionState == dom::RTCIceConnectionState::Failed;
  bool iceJustSucceeded =
      mIceConnectionState == dom::RTCIceConnectionState::Connected;
  if (!MediaManager::Get()->IsActivelyCapturingOrHasAPermission(winId)) {
    bool enabled = Preferences::GetBool(
        "media.peerconnection.ice.obfuscate_host_addresses", false);
    if (enabled) {
      if (!mIceFinished && !mIceStartTime.IsNull() &&
          (iceJustFailed || iceJustSucceeded)) {
        Telemetry::AccumulateTimeDelta(
            Telemetry::WEBRTC_HOSTNAME_OBFUSCATION_ENABLED_ICE_DURATION_MS,
            mIceStartTime, TimeStamp::Now());
      }
      if (iceJustSucceeded) {
        Telemetry::ScalarAdd(
            Telemetry::ScalarID::WEBRTC_HOSTNAMEOBFUSCATION_ENABLED_SUCCEEDED,
            1);
      } else if (iceJustFailed) {
        Telemetry::ScalarAdd(
            Telemetry::ScalarID::WEBRTC_HOSTNAMEOBFUSCATION_ENABLED_FAILED, 1);
      }
    } else {
      if (!mIceFinished && !mIceStartTime.IsNull() &&
          (iceJustFailed || iceJustSucceeded)) {
        Telemetry::AccumulateTimeDelta(
            Telemetry::WEBRTC_HOSTNAME_OBFUSCATION_DISABLED_ICE_DURATION_MS,
            mIceStartTime, TimeStamp::Now());
      }
      if (iceJustSucceeded) {
        Telemetry::ScalarAdd(
            Telemetry::ScalarID::WEBRTC_HOSTNAMEOBFUSCATION_DISABLED_SUCCEEDED,
            1);
      } else if (iceJustFailed) {
        Telemetry::ScalarAdd(
            Telemetry::ScalarID::WEBRTC_HOSTNAMEOBFUSCATION_DISABLED_FAILED, 1);
      }
    }
    if (iceJustFailed || iceJustSucceeded) {
      mIceFinished = true;
    }
  }

  WrappableJSErrorResult rv;
  mPCObserver->OnStateChange(PCObserverStateType::IceConnectionState, rv);
}

void PeerConnectionImpl::OnCandidateFound(const std::string& aTransportId,
                                          const CandidateInfo& aCandidateInfo) {
  if (!aCandidateInfo.mDefaultHostRtp.empty()) {
    UpdateDefaultCandidate(aCandidateInfo.mDefaultHostRtp,
                           aCandidateInfo.mDefaultPortRtp,
                           aCandidateInfo.mDefaultHostRtcp,
                           aCandidateInfo.mDefaultPortRtcp, aTransportId);
  }
  CandidateReady(aCandidateInfo.mCandidate, aTransportId,
                 aCandidateInfo.mUfrag);
}

void PeerConnectionImpl::IceGatheringStateChange(
    dom::RTCIceGatheringState state) {
  PC_AUTO_ENTER_API_CALL_VOID_RETURN(false);

  CSFLogDebug(LOGTAG, "%s %d", __FUNCTION__, static_cast<int>(state));
  if (mIceGatheringState == state) {
    return;
  }

  mIceGatheringState = state;

  // Would be nice if we had a means of converting one of these dom enums
  // to a string that wasn't almost as much text as this switch statement...
  switch (mIceGatheringState) {
    case RTCIceGatheringState::New:
      STAMP_TIMECARD(mTimeCard, "Ice gathering state: new");
      break;
    case RTCIceGatheringState::Gathering:
      STAMP_TIMECARD(mTimeCard, "Ice gathering state: gathering");
      break;
    case RTCIceGatheringState::Complete:
      STAMP_TIMECARD(mTimeCard, "Ice gathering state: complete");
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected mIceGatheringState!");
  }

  JSErrorResult rv;
  mPCObserver->OnStateChange(PCObserverStateType::IceGatheringState, rv);
}

void PeerConnectionImpl::UpdateDefaultCandidate(
    const std::string& defaultAddr, uint16_t defaultPort,
    const std::string& defaultRtcpAddr, uint16_t defaultRtcpPort,
    const std::string& transportId) {
  CSFLogDebug(LOGTAG, "%s", __FUNCTION__);
  mJsepSession->UpdateDefaultCandidate(
      defaultAddr, defaultPort, defaultRtcpAddr, defaultRtcpPort, transportId);
}

RefPtr<RTCStatsQueryPromise> PeerConnectionImpl::GetStats(
    dom::MediaStreamTrack* aSelector, bool aInternalStats,
    bool aRecordTelemetry) {
  UniquePtr<RTCStatsQuery> query(
      new RTCStatsQuery(aInternalStats, aRecordTelemetry));
  nsresult rv = BuildStatsQuery_m(aSelector, query.get());
  if (NS_FAILED(rv)) {
    return RTCStatsQueryPromise::CreateAndReject(rv, __func__);
  }

  nsTArray<RefPtr<MediaPipeline>> pipelines;
  // Gather up pipelines from mMedia so they may be inspected on STS
  mMedia->GetTransmitPipelinesMatching(aSelector, &pipelines);
  mMedia->GetReceivePipelinesMatching(aSelector, &pipelines);
  if (!pipelines.Length()) {
    CSFLogError(LOGTAG, "%s: Found no pipelines matching selector.",
                __FUNCTION__);
  }

  return InvokeAsync(mSTSThread, __func__,
                     [transportHandler = mMedia->mTransportHandler, pipelines,
                      aQuery = std::move(query)]() mutable {
                       return PeerConnectionImpl::ExecuteStatsQuery_s(
                           std::move(aQuery), pipelines, transportHandler);
                     });
}

nsresult PeerConnectionImpl::BuildStatsQuery_m(
    mozilla::dom::MediaStreamTrack* aSelector, RTCStatsQuery* query) {
  if (!HasMedia()) {
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = GetTimeSinceEpoch(&(query->now));
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "Could not build stats query, could not get timestamp");
    return rv;
  }

  // We do not use the pcHandle here, since that's risky to expose to content.
  query->report.reset(new RTCStatsReportInternalConstruct(
      NS_ConvertASCIItoUTF16(mName.c_str()), query->now));

  query->iceStartTime = mIceStartTime;
  query->report->mIceRestarts.Construct(mIceRestartCount);
  query->report->mIceRollbacks.Construct(mIceRollbackCount);

  // Populate SDP on main
  if (query->internalStats) {
    if (mJsepSession) {
      // TODO we probably should report Current and Pending SDPs here
      // separately. Plus the raw SDP we got from JS (mLocalRequestedSDP).
      // And if it's the offer or answer would also be nice.
      std::string localDescription =
          mJsepSession->GetLocalDescription(kJsepDescriptionPendingOrCurrent);
      std::string remoteDescription =
          mJsepSession->GetRemoteDescription(kJsepDescriptionPendingOrCurrent);
      query->report->mLocalSdp.Construct(
          NS_ConvertASCIItoUTF16(localDescription.c_str()));
      query->report->mRemoteSdp.Construct(
          NS_ConvertASCIItoUTF16(remoteDescription.c_str()));
      query->report->mOfferer.Construct(mJsepSession->IsOfferer());
      for (const auto& candidate : mRawTrickledCandidates) {
        query->report->mRawRemoteCandidates.Value().AppendElement(
            NS_ConvertASCIItoUTF16(candidate.c_str()), fallible);
      }
    }
  }

  if (aSelector) {
    query->transportId = mMedia->GetTransportIdMatching(*aSelector);
  }

  if (!aSelector) {
    query->grabAllLevels = true;
  }

  return NS_OK;
}

RefPtr<RTCStatsQueryPromise> PeerConnectionImpl::ExecuteStatsQuery_s(
    UniquePtr<RTCStatsQuery>&& query,
    const nsTArray<RefPtr<MediaPipeline>>& aPipelines,
    const RefPtr<MediaTransportHandler>& aTransportHandler) {
  // Gather stats from pipelines provided (can't touch mMedia + stream on STS)

  for (size_t p = 0; p < aPipelines.Length(); ++p) {
    MOZ_ASSERT(aPipelines[p]);
    MOZ_ASSERT(aPipelines[p]->Conduit());
    if (!aPipelines[p] || !aPipelines[p]->Conduit()) {
      // continue if we don't have a valid conduit
      continue;
    }
    const MediaPipeline& mp = *aPipelines[p];
    auto asVideo = mp.Conduit()->AsVideoSessionConduit();

    if (query->recordTelemetry && asVideo) {
      asVideo.value()->RecordTelemetry();
    }

    nsString kind = asVideo.isNothing() ? NS_LITERAL_STRING("audio")
                                        : NS_LITERAL_STRING("video");
    nsString idstr = kind + NS_LITERAL_STRING("_");
    idstr.AppendInt(static_cast<uint32_t>(p));

    // TODO(@@NG):ssrcs handle Conduits having multiple stats at the same level
    // This is pending spec work
    // Gather pipeline stats.
    switch (mp.Direction()) {
      case MediaPipeline::DirectionType::TRANSMIT: {
        nsString localId = NS_LITERAL_STRING("outbound_rtp_") + idstr;
        nsString remoteId;
        Maybe<uint32_t> ssrc;
        std::vector<unsigned int> ssrcvals = mp.Conduit()->GetLocalSSRCs();
        if (!ssrcvals.empty()) {
          ssrc = Some(ssrcvals[0]);
        }
        {
          // First, fill in remote stat with rtcp receiver data, if present.
          // ReceiverReports have less information than SenderReports,
          // so fill in what we can.
          uint32_t jitterMs;
          uint32_t packetsReceived;
          uint64_t bytesReceived;
          uint32_t packetsLost;
          Maybe<double> rtt;
          if (mp.Conduit()->GetRTCPReceiverReport(&jitterMs, &packetsReceived,
                                                  &bytesReceived, &packetsLost,
                                                  &rtt)) {
            remoteId = NS_LITERAL_STRING("outbound_rtcp_") + idstr;
            RTCRemoteInboundRtpStreamStats s;
            // TODO Bug 1496533 - use reception time not query time
            s.mTimestamp.Construct(query->now);
            s.mId.Construct(remoteId);
            s.mType.Construct(RTCStatsType::Remote_inbound_rtp);
            ssrc.apply([&s](uint32_t aSsrc) { s.mSsrc.Construct(aSsrc); });
            s.mMediaType.Construct(
                kind);  // mediaType is the old name for kind.
            s.mKind.Construct(kind);
            s.mJitter.Construct(double(jitterMs) / 1000);
            s.mLocalId.Construct(localId);
            s.mPacketsReceived.Construct(packetsReceived);
            s.mBytesReceived.Construct(bytesReceived);
            s.mPacketsLost.Construct(packetsLost);
            rtt.apply([&s](auto r) { s.mRoundTripTime.Construct(r); });
            query->report->mRemoteInboundRtpStreamStats.Value().AppendElement(
                s, fallible);
          }
        }
        // Then, fill in local side (with cross-link to remote only if present)
        {
          RTCOutboundRtpStreamStats s;
          // TODO Bug 1496533 - use reception time not query time
          s.mTimestamp.Construct(query->now);
          s.mId.Construct(localId);
          s.mType.Construct(RTCStatsType::Outbound_rtp);
          ssrc.apply([&s](uint32_t aSsrc) { s.mSsrc.Construct(aSsrc); });
          s.mMediaType.Construct(kind);  // mediaType is the old name for kind.
          s.mKind.Construct(kind);
          s.mRemoteId.Construct(remoteId);
          s.mPacketsSent.Construct(mp.RtpPacketsSent());
          s.mBytesSent.Construct(mp.RtpBytesSent());

          // Fill in packet type statistics
          webrtc::RtcpPacketTypeCounter counters;
          if (mp.Conduit()->GetSendPacketTypeStats(&counters)) {
            s.mNackCount.Construct(counters.nack_packets);
            // Fill in video only packet type stats
            if (asVideo) {
              s.mFirCount.Construct(counters.fir_packets);
              s.mPliCount.Construct(counters.pli_packets);
            }
          }

          // Lastly, fill in video encoder stats if this is video
          asVideo.apply([&s](auto conduit) {
            double framerateMean;
            double framerateStdDev;
            double bitrateMean;
            double bitrateStdDev;
            uint32_t droppedFrames;
            uint32_t framesEncoded;
            Maybe<uint64_t> qpSum;
            if (conduit->GetVideoEncoderStats(
                    &framerateMean, &framerateStdDev, &bitrateMean,
                    &bitrateStdDev, &droppedFrames, &framesEncoded, &qpSum)) {
              s.mFramerateMean.Construct(framerateMean);
              s.mFramerateStdDev.Construct(framerateStdDev);
              s.mBitrateMean.Construct(bitrateMean);
              s.mBitrateStdDev.Construct(bitrateStdDev);
              s.mDroppedFrames.Construct(droppedFrames);
              s.mFramesEncoded.Construct(framesEncoded);
              qpSum.apply([&s](uint64_t aQp) { s.mQpSum.Construct(aQp); });
            }
          });
          query->report->mOutboundRtpStreamStats.Value().AppendElement(
              s, fallible);
        }
        break;
      }
      case MediaPipeline::DirectionType::RECEIVE: {
        nsString localId = NS_LITERAL_STRING("inbound_rtp_") + idstr;
        nsString remoteId;
        Maybe<uint32_t> ssrc;
        unsigned int ssrcval;
        if (mp.Conduit()->GetRemoteSSRC(&ssrcval)) {
          ssrc = Some(ssrcval);
        }
        {
          // First, fill in remote stat with rtcp sender data, if present.
          uint32_t packetsSent;
          uint64_t bytesSent;
          if (mp.Conduit()->GetRTCPSenderReport(&packetsSent, &bytesSent)) {
            remoteId = NS_LITERAL_STRING("inbound_rtcp_") + idstr;
            RTCRemoteOutboundRtpStreamStats s;
            // TODO Bug 1496533 - use reception time not query time
            s.mTimestamp.Construct(query->now);
            s.mId.Construct(remoteId);
            s.mType.Construct(RTCStatsType::Remote_outbound_rtp);
            ssrc.apply([&s](uint32_t aSsrc) { s.mSsrc.Construct(aSsrc); });
            s.mMediaType.Construct(
                kind);  // mediaType is the old name for kind.
            s.mKind.Construct(kind);
            s.mLocalId.Construct(localId);
            s.mPacketsSent.Construct(packetsSent);
            s.mBytesSent.Construct(bytesSent);
            query->report->mRemoteOutboundRtpStreamStats.Value().AppendElement(
                s, fallible);
          }
        }
        // Then, fill in local side (with cross-link to remote only if present)
        RTCInboundRtpStreamStats s;
        s.mTimestamp.Construct(query->now);
        s.mId.Construct(localId);
        s.mType.Construct(RTCStatsType::Inbound_rtp);
        ssrc.apply([&s](uint32_t aSsrc) { s.mSsrc.Construct(aSsrc); });
        s.mMediaType.Construct(kind);  // mediaType is the old name for kind.
        s.mKind.Construct(kind);
        unsigned int jitterMs, packetsLost;
        if (mp.Conduit()->GetRTPReceiverStats(&jitterMs, &packetsLost)) {
          s.mJitter.Construct(double(jitterMs) / 1000);
          s.mPacketsLost.Construct(packetsLost);
        }
        if (remoteId.Length()) {
          s.mRemoteId.Construct(remoteId);
        }
        s.mPacketsReceived.Construct(mp.RtpPacketsReceived());
        s.mBytesReceived.Construct(mp.RtpBytesReceived());

        // Fill in packet type statistics
        webrtc::RtcpPacketTypeCounter counters;
        if (mp.Conduit()->GetRecvPacketTypeStats(&counters)) {
          s.mNackCount.Construct(counters.nack_packets);
          // Fill in video only packet type stats
          if (asVideo) {
            s.mFirCount.Construct(counters.fir_packets);
            s.mPliCount.Construct(counters.pli_packets);
          }
        }
        // Lastly, fill in video decoder stats if this is video
        asVideo.apply([&s](auto conduit) {
          double framerateMean;
          double framerateStdDev;
          double bitrateMean;
          double bitrateStdDev;
          uint32_t discardedPackets;
          uint32_t framesDecoded;
          if (conduit->GetVideoDecoderStats(
                  &framerateMean, &framerateStdDev, &bitrateMean,
                  &bitrateStdDev, &discardedPackets, &framesDecoded)) {
            s.mFramerateMean.Construct(framerateMean);
            s.mFramerateStdDev.Construct(framerateStdDev);
            s.mBitrateMean.Construct(bitrateMean);
            s.mBitrateStdDev.Construct(bitrateStdDev);
            s.mDiscardedPackets.Construct(discardedPackets);
            s.mFramesDecoded.Construct(framesDecoded);
          }
        });
        query->report->mInboundRtpStreamStats.Value().AppendElement(s,
                                                                    fallible);
        // Fill in Contributing Source statistics
        mp.GetContributingSourceStats(
            localId, query->report->mRtpContributingSourceStats.Value());
        break;
      }
    }
  }

  std::string transportId;
  if (!query->grabAllLevels) {
    transportId = query->transportId;
  }
  auto report = std::move(query->report);
  auto now = query->now;

  return aTransportHandler->GetIceStats(transportId, now, std::move(report))
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [query = std::move(query)](
              std::unique_ptr<dom::RTCStatsReportInternal>&& aReport) mutable {
            query->report = std::move(aReport);
            return RTCStatsQueryPromise::CreateAndResolve(std::move(query),
                                                          __func__);
          },
          [](nsresult aError) {
            return RTCStatsQueryPromise::CreateAndReject(aError, __func__);
          });
}

void PeerConnectionImpl::DeliverStatsReportToPCObserver_m(
    const std::string& pcHandle, nsresult result,
    const nsAutoPtr<RTCStatsQuery>& query) {
  // Is the PeerConnectionImpl still around?
  PeerConnectionWrapper pcw(pcHandle);
  if (pcw.impl()) {
    JSErrorResult rv;
    if (NS_SUCCEEDED(result)) {
      pcw.impl()->mPCObserver->OnGetStatsSuccess(*query->report, rv);
    } else {
      pcw.impl()->mPCObserver->OnGetStatsError(
          ObString("Failed to fetch statistics"), rv);
    }

    if (rv.Failed()) {
      CSFLogError(LOGTAG, "Error firing stats observer callback");
    }
  }
}

void PeerConnectionImpl::RecordLongtermICEStatistics() {
  WebrtcGlobalInformation::StoreLongTermICEStatistics(*this);
}

void PeerConnectionImpl::RecordIceRestartStatistics(JsepSdpType type) {
  switch (type) {
    case mozilla::kJsepSdpOffer:
    case mozilla::kJsepSdpPranswer:
      break;
    case mozilla::kJsepSdpAnswer:
      ++mIceRestartCount;
      break;
    case mozilla::kJsepSdpRollback:
      ++mIceRollbackCount;
      break;
  }
}

// Telemetry for when calls start
void PeerConnectionImpl::startCallTelem() {
  if (!mStartTime.IsNull()) {
    return;
  }

  // Start time for calls
  mStartTime = TimeStamp::Now();

  // Increment session call counter
  // If we want to track Loop calls independently here, we need two histograms.
  Telemetry::Accumulate(Telemetry::WEBRTC_CALL_COUNT_2, 1);
}

void PeerConnectionImpl::DTMFState::StopPlayout() {
  if (mSendTimer) {
    mSendTimer->Cancel();
    mSendTimer = nullptr;
  }
}

void PeerConnectionImpl::DTMFState::StartPlayout(uint32_t aDelay) {
  if (!mSendTimer) {
    mSendTimer = NS_NewTimer();
    mSendTimer->InitWithCallback(this, aDelay, nsITimer::TYPE_ONE_SHOT);
  }
}

nsresult PeerConnectionImpl::DTMFState::Notify(nsITimer* timer) {
  MOZ_ASSERT(NS_IsMainThread());
  StopPlayout();

  if (!mTransceiver->IsSending()) {
    return NS_OK;
  }

  nsString eventTone;
  if (!mTones.IsEmpty()) {
    uint16_t toneChar = mTones.CharAt(0);
    int tone = GetDTMFToneCode(toneChar);

    eventTone.Assign(toneChar);

    mTones.Cut(0, 1);

    if (tone == -1) {
      StartPlayout(2000);
    } else {
      // Reset delay if necessary
      StartPlayout(mDuration + mInterToneGap);
      mTransceiver->InsertDTMFTone(tone, mDuration);
    }
  }

  RefPtr<dom::MediaStreamTrack> sendTrack = mTransceiver->GetSendTrack();
  if (!sendTrack) {
    NS_WARNING("Failed to dispatch the RTCDTMFToneChange event!");
    return NS_OK;  // Return is ignored anyhow
  }

  JSErrorResult jrv;
  mPCObserver->OnDTMFToneChange(*sendTrack, eventTone, jrv);

  if (jrv.Failed()) {
    NS_WARNING("Failed to dispatch the RTCDTMFToneChange event!");
  }

  return NS_OK;
}

PeerConnectionImpl::DTMFState::DTMFState() = default;
PeerConnectionImpl::DTMFState::~DTMFState() { StopPlayout(); }

NS_IMPL_ISUPPORTS(PeerConnectionImpl::DTMFState, nsITimerCallback)

}  // namespace mozilla
