/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SDPHELPER_H_
#define _SDPHELPER_H_

#include "nsError.h"

#include "signaling/src/sdp/SdpMediaSection.h"
#include "signaling/src/sdp/SdpAttribute.h"

#include "m_cpp_utils.h"

#include <string>
#include <map>
#include <vector>

namespace mozilla {
class SdpMediaSection;
class Sdp;

class SdpHelper {
  public:
    enum MsectionBundleType {
      kNoBundle,
      kSlaveBundle,
      kMasterBundle
    };

    // Takes a std::string* into which error strings will be written for the
    // lifetime of the SdpHelper.
    explicit SdpHelper(std::string* errorDest) : mLastError(*errorDest) {}
    ~SdpHelper() {}

    nsresult GetComponent(const std::string& candidate, size_t* component);
    nsresult CopyTransportParams(size_t numComponents,
                                 const SdpMediaSection& source,
                                 SdpMediaSection* dest);
    bool AreOldTransportParamsValid(const Sdp& oldAnswer,
                                    const Sdp& newOffer,
                                    size_t level);

    bool MsectionIsDisabled(const SdpMediaSection& msection) const;
    static void DisableMsection(Sdp* sdp, SdpMediaSection* msection);

    // Maps each mid to the m-section that is the master of its bundle.
    // Mids that do not appear in an a=group:BUNDLE do not appear here.
    typedef std::map<std::string, const SdpMediaSection*> BundledMids;

    nsresult GetBundledMids(const Sdp& sdp, BundledMids* bundledMids);

    bool IsBundleSlave(const Sdp& localSdp, uint16_t level);
    void GetBundleGroups(
        const Sdp& sdp,
        std::vector<SdpGroupAttributeList::Group>* groups) const;

    nsresult GetMidFromLevel(const Sdp& sdp,
                             uint16_t level,
                             std::string* mid);
    nsresult GetIdsFromMsid(const Sdp& sdp,
                            const SdpMediaSection& msection,
                            std::string* streamId,
                            std::string* trackId);
    nsresult GetMsids(const SdpMediaSection& msection,
                      std::vector<SdpMsidAttributeList::Msid>* msids);
    nsresult ParseMsid(const std::string& msidAttribute,
                       std::string* streamId,
                       std::string* trackId);
    nsresult AddCandidateToSdp(Sdp* sdp,
                               const std::string& candidate,
                               const std::string& mid,
                               uint16_t level);
    void SetIceGatheringComplete(Sdp* sdp,
                                 uint16_t level,
                                 BundledMids bundledMids);
    void SetDefaultAddresses(const std::string& defaultCandidateAddr,
                             uint16_t defaultCandidatePort,
                             const std::string& defaultRtcpCandidateAddr,
                             uint16_t defaultRtcpCandidatePort,
                             Sdp* sdp,
                             uint16_t level,
                             BundledMids bundledMids);
    void SetDefaultAddresses(const std::string& defaultCandidateAddr,
                             uint16_t defaultCandidatePort,
                             const std::string& defaultRtcpCandidateAddr,
                             uint16_t defaultRtcpCandidatePort,
                             SdpMediaSection* msection);
    void SetupMsidSemantic(const std::vector<std::string>& msids,
                           Sdp* sdp) const;
    MsectionBundleType GetMsectionBundleType(const Sdp& sdp,
                                             uint16_t level,
                                             BundledMids& bundledMids,
                                             std::string* masterMid) const;

    std::string GetCNAME(const SdpMediaSection& msection) const;

    SdpMediaSection* FindMsectionByMid(Sdp& sdp,
                                       const std::string& mid) const;

    const SdpMediaSection* FindMsectionByMid(const Sdp& sdp,
                                             const std::string& mid) const;

    nsresult CopyStickyParams(const SdpMediaSection& source,
                              SdpMediaSection* dest);
    bool HasRtcp(SdpMediaSection::Protocol proto) const;
    static SdpMediaSection::Protocol GetProtocolForMediaType(
        SdpMediaSection::MediaType type);
    void appendSdpParseErrors(
          const std::vector<std::pair<size_t, std::string> >& aErrors,
          std::string* aErrorString);

    static bool GetPtAsInt(const std::string& ptString, uint16_t* ptOutparam);

    void AddCommonExtmaps(
        const SdpMediaSection& remoteMsection,
        const std::vector<SdpExtmapAttributeList::Extmap>& localExtensions,
        SdpMediaSection* localMsection);

  private:
    std::string& mLastError;

    DISALLOW_COPY_ASSIGN(SdpHelper);
};
} // namespace mozilla

#endif // _SDPHELPER_H_

