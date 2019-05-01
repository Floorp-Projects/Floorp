/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "logging.h"
#include "nrinterfaceprioritizer.h"
#include "nsCOMPtr.h"

MOZ_MTLOG_MODULE("mtransport")

namespace {

class LocalAddress {
 public:
  LocalAddress()
      : ifname_(),
        addr_(),
        key_(),
        is_vpn_(-1),
        estimated_speed_(-1),
        type_preference_(-1),
        ip_version_(-1) {}

  bool Init(const nr_local_addr& local_addr) {
    ifname_ = local_addr.addr.ifname;

    char buf[MAXIFNAME + 47];
    int r = nr_transport_addr_fmt_ifname_addr_string(&local_addr.addr, buf,
                                                     sizeof(buf));
    if (r) {
      MOZ_MTLOG(ML_ERROR, "Error formatting interface key.");
      return false;
    }
    key_ = buf;

    r = nr_transport_addr_get_addrstring(&local_addr.addr, buf, sizeof(buf));
    if (r) {
      MOZ_MTLOG(ML_ERROR, "Error formatting address string.");
      return false;
    }
    addr_ = buf;

    is_vpn_ = (local_addr.interface.type & NR_INTERFACE_TYPE_VPN) != 0 ? 1 : 0;
    estimated_speed_ = local_addr.interface.estimated_speed;
    type_preference_ = GetNetworkTypePreference(local_addr.interface.type);
    ip_version_ = local_addr.addr.ip_version;
    return true;
  }

  bool operator<(const LocalAddress& rhs) const {
    // Interface that is "less" here is preferred.
    // If type preferences are different, we should simply sort by
    // |type_preference_|.
    if (type_preference_ != rhs.type_preference_) {
      return type_preference_ < rhs.type_preference_;
    }

    // If type preferences are the same, the next thing we use to sort is vpn.
    // If two LocalAddress are different in |is_vpn_|, the LocalAddress that is
    // not in vpn gets priority.
    if (is_vpn_ != rhs.is_vpn_) {
      return is_vpn_ < rhs.is_vpn_;
    }

    // Compare estimated speed.
    if (estimated_speed_ != rhs.estimated_speed_) {
      return estimated_speed_ > rhs.estimated_speed_;
    }

    // See if our hard-coded pref list helps us.
    auto thisindex = std::find(interface_preference_list().begin(),
                               interface_preference_list().end(), ifname_);
    auto rhsindex = std::find(interface_preference_list().begin(),
                              interface_preference_list().end(), rhs.ifname_);
    if (thisindex != rhsindex) {
      return thisindex < rhsindex;
    }

    // Prefer IPV6 over IPV4
    if (ip_version_ != rhs.ip_version_) {
      return ip_version_ > rhs.ip_version_;
    }

    // Now we start getting into arbitrary stuff
    if (ifname_ != rhs.ifname_) {
      return ifname_ < rhs.ifname_;
    }

    return addr_ < rhs.addr_;
  }

  const std::string& GetKey() const { return key_; }

 private:
  // Getting the preference corresponding to a type. Getting lower number here
  // means the type of network is preferred.
  static inline int GetNetworkTypePreference(int type) {
    if (type & NR_INTERFACE_TYPE_WIRED) {
      return 1;
    }
    if (type & NR_INTERFACE_TYPE_WIFI) {
      return 2;
    }
    if (type & NR_INTERFACE_TYPE_MOBILE) {
      return 3;
    }
    if (type & NR_INTERFACE_TYPE_TEREDO) {
      // Teredo gets penalty because it's IP relayed
      return 5;
    }
    return 4;
  }

  // TODO(bug 895790): Once we can get useful interface properties on Darwin,
  // we should remove this stuff.
  static const std::vector<std::string>& interface_preference_list() {
    static std::vector<std::string> list(build_interface_preference_list());
    return list;
  }

  static std::vector<std::string> build_interface_preference_list() {
    std::vector<std::string> result;
    result.push_back("rl0");
    result.push_back("wi0");
    result.push_back("en0");
    result.push_back("enp2s0");
    result.push_back("enp3s0");
    result.push_back("en1");
    result.push_back("en2");
    result.push_back("en3");
    result.push_back("eth0");
    result.push_back("eth1");
    result.push_back("eth2");
    result.push_back("em1");
    result.push_back("em0");
    result.push_back("ppp");
    result.push_back("ppp0");
    result.push_back("vmnet1");
    result.push_back("vmnet0");
    result.push_back("vmnet3");
    result.push_back("vmnet4");
    result.push_back("vmnet5");
    result.push_back("vmnet6");
    result.push_back("vmnet7");
    result.push_back("vmnet8");
    result.push_back("virbr0");
    result.push_back("wlan0");
    result.push_back("lo0");
    return result;
  }

  std::string ifname_;
  std::string addr_;
  std::string key_;
  int is_vpn_;
  int estimated_speed_;
  int type_preference_;
  int ip_version_;
};

class InterfacePrioritizer {
 public:
  InterfacePrioritizer() : local_addrs_(), preference_map_(), sorted_(false) {}

  int add(const nr_local_addr* iface) {
    LocalAddress addr;
    if (!addr.Init(*iface)) {
      return R_FAILED;
    }
    std::pair<std::set<LocalAddress>::iterator, bool> r =
        local_addrs_.insert(addr);
    if (!r.second) {
      return R_ALREADY;  // This address is already in the set.
    }
    sorted_ = false;
    return 0;
  }

  int sort() {
    UCHAR tmp_pref = 127;
    preference_map_.clear();
    for (const auto& local_addr : local_addrs_) {
      if (tmp_pref == 0) {
        return R_FAILED;
      }
      preference_map_.insert(make_pair(local_addr.GetKey(), tmp_pref--));
    }
    sorted_ = true;
    return 0;
  }

  int getPreference(const char* key, UCHAR* pref) {
    if (!sorted_) {
      return R_FAILED;
    }
    std::map<std::string, UCHAR>::iterator i = preference_map_.find(key);
    if (i == preference_map_.end()) {
      return R_NOT_FOUND;
    }
    *pref = i->second;
    return 0;
  }

 private:
  std::set<LocalAddress> local_addrs_;
  std::map<std::string, UCHAR> preference_map_;
  bool sorted_;
};

}  // anonymous namespace

static int add_interface(void* obj, nr_local_addr* iface) {
  InterfacePrioritizer* ip = static_cast<InterfacePrioritizer*>(obj);
  return ip->add(iface);
}

static int get_priority(void* obj, const char* key, UCHAR* pref) {
  InterfacePrioritizer* ip = static_cast<InterfacePrioritizer*>(obj);
  return ip->getPreference(key, pref);
}

static int sort_preference(void* obj) {
  InterfacePrioritizer* ip = static_cast<InterfacePrioritizer*>(obj);
  return ip->sort();
}

static int destroy(void** objp) {
  if (!objp || !*objp) {
    return 0;
  }

  InterfacePrioritizer* ip = static_cast<InterfacePrioritizer*>(*objp);
  *objp = nullptr;
  delete ip;

  return 0;
}

static nr_interface_prioritizer_vtbl priorizer_vtbl = {
    add_interface, get_priority, sort_preference, destroy};

namespace mozilla {

nr_interface_prioritizer* CreateInterfacePrioritizer() {
  nr_interface_prioritizer* ip;
  int r = nr_interface_prioritizer_create_int(new InterfacePrioritizer(),
                                              &priorizer_vtbl, &ip);
  if (r != 0) {
    return nullptr;
  }
  return ip;
}

}  // namespace mozilla
