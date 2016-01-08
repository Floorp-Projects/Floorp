/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

         ,-----.                  ,--.  ,--.
        '  .--./ ,--,--.,--.,--.,-'  '-.`--' ,---. ,--,--,
        |  |    ' ,-.  ||  ||  |'-.  .-',--.| .-. ||      `
        '  '--'\\ '-'  |'  ''  '  |  |  |  |' '-' '|  ||  |
         `-----' `--`--' `----'   `--'  `--' `---' `--''--'

                        :+o+-
                      -dNNNNNd.
                      yNNNNNNNs
                      :mNNNNNm-
                       `/sso/``-://-
                        .:+sydNNNNNNms:                      `://`
                 `-/+shmNNNNNNNNNNNNNNNms-                  :mNNNm/
           `-/oydmNNNNNNNNNNNNNNNNNNNNNNNNdo-              +NNNNNN+
       .shmNNNNNNNNNNNmdyo/:dNNNNNNNNNNNNNNNNdo.         `sNNNNNm+
       hNNNNNNNNmhs+:-`   .dNNNNNNNNNNNNNNNNNNNNh+-`    `hNNNNNm:
       -yddyo/:.         -dNNNNm::ymNNNNNNNNNNNNNNNmdy+/dNNNNNd.
                        :mNNNNd.   `/ymNNNNNNNNNNNNNNNNNNNNNNh`
                       +NNNNNh`       `+hNNNNNNNNNNNNNNNNNNNs
                      sNNNNNy`           .yNNNNNm`-/oymNNNm+
                    `yNNNNNo              oNNNNNm`     `-.
                   .dNNNNm/               oNNNNNm`
                   oNNNNm:                +NNNNNm`
                   `+yho.                 +NNNNNm`
                                          +NNNNNNs.
                                          `yNNNNNNmy-
                                            -smNNNNNNh:
                                              .smNNNNNNh/
                                                `omNNNNNNd:
                                                  `+dNNNNNd
                             ````......````          /hmdy-
            `.:/+osyhddmNNMMMMMMMMMMMMMMMMMMMMNNmddhyso+/:.`
     `-+shmNMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMNmhs+-`
  -smMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMds-
 hMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMh
 yMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMs
  .ohNMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMNh+.
      ./oydmMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMmhyo:.
             `.:/+osyyhddmmNNMMMMMMMMMMMMMMNNmmddhyyso+/:.`

            ,--------.,--.     ,--.           ,--.
            '--.  .--'|  ,---. `--' ,---.     |  | ,---.
               |  |   |  .-.  |,--.(  .-'     |  |(  .-'
               |  |   |  | |  ||  |.-'  `)    |  |.-'  `)
               `--'   `--' `--'`--'`----'     `--'`----'
                                                                ,--.
       ,---.  ,------.  ,------.                  ,--.          |  |
      '   .-' |  .-.  \ |  .--. ' ,--,--.,--.--.,-'  '-. ,--,--.|  |
      `.  `-. |  |  \  :|  '--' |' ,-.  ||  .--''-.  .-'' ,-.  ||  |
      .-'    ||  '--'  /|  | --' \ '-'  ||  |     |  |  \ '-'  |`--'
      `-----' `-------' `--'      `--`--'`--'     `--'   `--`--'.--.
                                                                '__'
*/

#ifndef _SDP_H_
#define _SDP_H_

#include <ostream>
#include <vector>
#include <sstream>
#include "mozilla/UniquePtr.h"
#include "mozilla/Maybe.h"
#include "signaling/src/sdp/SdpMediaSection.h"
#include "signaling/src/sdp/SdpAttributeList.h"
#include "signaling/src/sdp/SdpEnum.h"

namespace mozilla
{

class SdpOrigin;
class SdpEncryptionKey;
class SdpMediaSection;

/**
 * Base class for an SDP
 */
class Sdp
{
public:
  Sdp(){};
  virtual ~Sdp(){};

  virtual const SdpOrigin& GetOrigin() const = 0;
  // Note: connection information is always retrieved from media sections
  virtual uint32_t GetBandwidth(const std::string& type) const = 0;

  virtual const SdpAttributeList& GetAttributeList() const = 0;
  virtual SdpAttributeList& GetAttributeList() = 0;

  virtual size_t GetMediaSectionCount() const = 0;
  virtual const SdpMediaSection& GetMediaSection(size_t level) const = 0;
  virtual SdpMediaSection& GetMediaSection(size_t level) = 0;

  virtual SdpMediaSection& AddMediaSection(SdpMediaSection::MediaType media,
                                           SdpDirectionAttribute::Direction dir,
                                           uint16_t port,
                                           SdpMediaSection::Protocol proto,
                                           sdp::AddrType addrType,
                                           const std::string& addr) = 0;

  virtual void Serialize(std::ostream&) const = 0;

  std::string ToString() const;
};

inline std::ostream& operator<<(std::ostream& os, const Sdp& sdp)
{
  sdp.Serialize(os);
  return os;
}

inline std::string
Sdp::ToString() const
{
  std::stringstream s;
  s << *this;
  return s.str();
}

class SdpOrigin
{
public:
  SdpOrigin(const std::string& username, uint64_t sessId, uint64_t sessVer,
            sdp::AddrType addrType, const std::string& addr)
      : mUsername(username),
        mSessionId(sessId),
        mSessionVersion(sessVer),
        mAddrType(addrType),
        mAddress(addr)
  {
  }

  const std::string&
  GetUsername() const
  {
    return mUsername;
  }

  uint64_t
  GetSessionId() const
  {
    return mSessionId;
  }

  uint64_t
  GetSessionVersion() const
  {
    return mSessionVersion;
  }

  sdp::AddrType
  GetAddrType() const
  {
    return mAddrType;
  }

  const std::string&
  GetAddress() const
  {
    return mAddress;
  }

  void
  Serialize(std::ostream& os) const
  {
    sdp::NetType netType = sdp::kInternet;
    os << "o=" << mUsername << " " << mSessionId << " " << mSessionVersion
       << " " << netType << " " << mAddrType << " " << mAddress << "\r\n";
  }

private:
  std::string mUsername;
  uint64_t mSessionId;
  uint64_t mSessionVersion;
  sdp::AddrType mAddrType;
  std::string mAddress;
};

inline std::ostream& operator<<(std::ostream& os, const SdpOrigin& origin)
{
  origin.Serialize(os);
  return os;
}

} // namespace mozilla

#endif
