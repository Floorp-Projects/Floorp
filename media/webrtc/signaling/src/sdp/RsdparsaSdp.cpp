/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signaling/src/sdp/RsdparsaSdp.h"

#include <cstdlib>
#include "mozilla/UniquePtr.h"
#include "mozilla/Assertions.h"
#include "nsError.h"


#include "signaling/src/sdp/SdpErrorHolder.h"
#include "signaling/src/sdp/RsdparsaSdpInc.h"
#include "signaling/src/sdp/RsdparsaSdpMediaSection.h"

#ifdef CRLF
#undef CRLF
#endif
#define CRLF "\r\n"

namespace mozilla
{

RsdparsaSdp::RsdparsaSdp(RsdparsaSessionHandle session, const SdpOrigin& origin)
  : mSession(std::move(session))
  , mOrigin(origin)
{
  RsdparsaSessionHandle attributeSession(sdp_new_reference(mSession.get()));
  mAttributeList.reset(new RsdparsaSdpAttributeList(std::move(attributeSession)));

  size_t section_count = sdp_media_section_count(mSession.get());
  for (size_t level = 0; level < section_count; level++) {
    RustMediaSection* mediaSection = sdp_get_media_section(mSession.get(),
                                                           level);
    if (!mediaSection) {
      MOZ_ASSERT(false, "sdp_get_media_section failed because level was out of"
                        " bounds, but we did a bounds check!");
      break;
    }
    RsdparsaSessionHandle newSession(sdp_new_reference(mSession.get()));
    RsdparsaSdpMediaSection* sdpMediaSection;
    sdpMediaSection = new RsdparsaSdpMediaSection(level,
                                                  std::move(newSession),
                                                  mediaSection,
                                                  mAttributeList.get());
    mMediaSections.values.push_back(sdpMediaSection);
  }
}

const SdpOrigin&
RsdparsaSdp::GetOrigin() const
{
  return mOrigin;
}

uint32_t
RsdparsaSdp::GetBandwidth(const std::string& type) const
{
  return get_sdp_bandwidth(mSession.get(), type.c_str());
}

const SdpMediaSection&
RsdparsaSdp::GetMediaSection(size_t level) const
{
  if (level > mMediaSections.values.size()) {
    MOZ_CRASH();
  }
  return *mMediaSections.values[level];
}

SdpMediaSection&
RsdparsaSdp::GetMediaSection(size_t level)
{
  if (level > mMediaSections.values.size()) {
    MOZ_CRASH();
  }
  return *mMediaSections.values[level];
}

SdpMediaSection&
RsdparsaSdp::AddMediaSection(SdpMediaSection::MediaType mediaType,
                             SdpDirectionAttribute::Direction dir,
                             uint16_t port,
                             SdpMediaSection::Protocol protocol,
                             sdp::AddrType addrType, const std::string& addr)
{
  //TODO: See Bug 1436080
  MOZ_CRASH("Method not implemented");
}

void
RsdparsaSdp::Serialize(std::ostream& os) const
{
  os << "v=0" << CRLF << mOrigin << "s=-" << CRLF;

  // We don't support creating i=, u=, e=, p=
  // We don't generate c= at the session level (only in media)

  BandwidthVec* bwVec = sdp_get_session_bandwidth_vec(mSession.get());
  char *bwString = sdp_serialize_bandwidth(bwVec);
  if (bwString) {
    os << bwString;
    sdp_free_string(bwString);
  }

  os << "t=0 0" << CRLF;

  // We don't support r= or z=

  // attributes
  os << *mAttributeList;

  // media sections
  for (const SdpMediaSection* msection : mMediaSections.values) {
    os << *msection;
  }
}

} // namespace mozilla
