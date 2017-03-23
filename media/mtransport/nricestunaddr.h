/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nricestunaddr_h__
#define nricestunaddr_h__

#include "nsError.h" // for nsresult

typedef struct nr_local_addr_ nr_local_addr;

namespace mozilla {

class NrIceStunAddr {
public:
  NrIceStunAddr(); // needed for IPC deserialization
  explicit NrIceStunAddr(const nr_local_addr* addr);
  NrIceStunAddr(const NrIceStunAddr& rhs);

  ~NrIceStunAddr();

  const nr_local_addr& localAddr() const { return *localAddr_; }

  // serialization/deserialization helper functions for use
  // in media/mtransport/ipc/NrIceStunAddrMessagUtils.h
  size_t SerializationBufferSize() const;
  nsresult Serialize(char* buffer, size_t buffer_size) const;
  nsresult Deserialize(const char* buffer, size_t buffer_size);

private:
  nr_local_addr* localAddr_;

};

} // namespace mozilla

#endif // nricestunaddr_h__
