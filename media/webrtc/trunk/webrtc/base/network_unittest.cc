/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/network.h"

#include <vector>
#if defined(WEBRTC_POSIX)
#include <sys/types.h>
#if !defined(WEBRTC_ANDROID)
#include <ifaddrs.h>
#else
#include "webrtc/base/ifaddrs-android.h"
#endif
#endif
#include "webrtc/base/gunit.h"
#if defined(WEBRTC_WIN)
#include "webrtc/base/logging.h"  // For LOG_GLE
#endif

namespace rtc {

class NetworkTest : public testing::Test, public sigslot::has_slots<>  {
 public:
  NetworkTest() : callback_called_(false) {}

  void OnNetworksChanged() {
    callback_called_ = true;
  }

  NetworkManager::Stats MergeNetworkList(
      BasicNetworkManager& network_manager,
      const NetworkManager::NetworkList& list,
      bool* changed) {
    NetworkManager::Stats stats;
    network_manager.MergeNetworkList(list, changed, &stats);
    return stats;
  }

  bool IsIgnoredNetwork(BasicNetworkManager& network_manager,
                        const Network& network) {
    return network_manager.IsIgnoredNetwork(network);
  }

  NetworkManager::NetworkList GetNetworks(
      const BasicNetworkManager& network_manager, bool include_ignored) {
    NetworkManager::NetworkList list;
    network_manager.CreateNetworks(include_ignored, &list);
    return list;
  }

#if defined(WEBRTC_POSIX)
  // Separated from CreateNetworks for tests.
  static void CallConvertIfAddrs(const BasicNetworkManager& network_manager,
                                 struct ifaddrs* interfaces,
                                 bool include_ignored,
                                 NetworkManager::NetworkList* networks) {
    network_manager.ConvertIfAddrs(interfaces, include_ignored, networks);
  }
#endif  // defined(WEBRTC_POSIX)

 protected:
  bool callback_called_;
};

// Test that the Network ctor works properly.
TEST_F(NetworkTest, TestNetworkConstruct) {
  Network ipv4_network1("test_eth0", "Test Network Adapter 1",
                        IPAddress(0x12345600U), 24);
  EXPECT_EQ("test_eth0", ipv4_network1.name());
  EXPECT_EQ("Test Network Adapter 1", ipv4_network1.description());
  EXPECT_EQ(IPAddress(0x12345600U), ipv4_network1.prefix());
  EXPECT_EQ(24, ipv4_network1.prefix_length());
  EXPECT_FALSE(ipv4_network1.ignored());
}

// Tests that our ignore function works properly.
TEST_F(NetworkTest, TestIsIgnoredNetworkIgnoresOnlyLoopbackByDefault) {
  Network ipv4_network1("test_eth0", "Test Network Adapter 1",
                        IPAddress(0x12345600U), 24, ADAPTER_TYPE_ETHERNET);
  Network ipv4_network2("test_wlan0", "Test Network Adapter 2",
                        IPAddress(0x12345601U), 16, ADAPTER_TYPE_WIFI);
  Network ipv4_network3("test_cell0", "Test Network Adapter 3",
                        IPAddress(0x12345602U), 16, ADAPTER_TYPE_CELLULAR);
  Network ipv4_network4("test_vpn0", "Test Network Adapter 4",
                        IPAddress(0x12345603U), 16, ADAPTER_TYPE_VPN);
  Network ipv4_network5("test_lo", "Test Network Adapter 5",
                        IPAddress(0x12345604U), 16, ADAPTER_TYPE_LOOPBACK);
  BasicNetworkManager network_manager;
  EXPECT_FALSE(IsIgnoredNetwork(network_manager, ipv4_network1));
  EXPECT_FALSE(IsIgnoredNetwork(network_manager, ipv4_network2));
  EXPECT_FALSE(IsIgnoredNetwork(network_manager, ipv4_network3));
  EXPECT_FALSE(IsIgnoredNetwork(network_manager, ipv4_network4));
  EXPECT_TRUE(IsIgnoredNetwork(network_manager, ipv4_network5));
}

TEST_F(NetworkTest, TestIsIgnoredNetworkIgnoresIPsStartingWith0) {
  Network ipv4_network1("test_eth0", "Test Network Adapter 1",
                        IPAddress(0x12345600U), 24, ADAPTER_TYPE_ETHERNET);
  Network ipv4_network2("test_eth1", "Test Network Adapter 2",
                        IPAddress(0x010000U), 24, ADAPTER_TYPE_ETHERNET);
  BasicNetworkManager network_manager;
  EXPECT_FALSE(IsIgnoredNetwork(network_manager, ipv4_network1));
  EXPECT_TRUE(IsIgnoredNetwork(network_manager, ipv4_network2));
}

TEST_F(NetworkTest, TestIsIgnoredNetworkIgnoresNetworksAccordingToIgnoreMask) {
  Network ipv4_network1("test_eth0", "Test Network Adapter 1",
                        IPAddress(0x12345600U), 24, ADAPTER_TYPE_ETHERNET);
  Network ipv4_network2("test_wlan0", "Test Network Adapter 2",
                        IPAddress(0x12345601U), 16, ADAPTER_TYPE_WIFI);
  Network ipv4_network3("test_cell0", "Test Network Adapter 3",
                        IPAddress(0x12345602U), 16, ADAPTER_TYPE_CELLULAR);
  BasicNetworkManager network_manager;
  network_manager.set_network_ignore_mask(
      ADAPTER_TYPE_ETHERNET | ADAPTER_TYPE_LOOPBACK | ADAPTER_TYPE_WIFI);
  EXPECT_TRUE(IsIgnoredNetwork(network_manager, ipv4_network1));
  EXPECT_TRUE(IsIgnoredNetwork(network_manager, ipv4_network2));
  EXPECT_FALSE(IsIgnoredNetwork(network_manager, ipv4_network3));
}

// TODO(phoglund): Remove when ignore list goes away.
TEST_F(NetworkTest, TestIgnoreList) {
  Network ignore_me("ignore_me", "Ignore me please!",
                    IPAddress(0x12345600U), 24);
  Network include_me("include_me", "Include me please!",
                     IPAddress(0x12345600U), 24);
  BasicNetworkManager network_manager;
  EXPECT_FALSE(IsIgnoredNetwork(network_manager, ignore_me));
  EXPECT_FALSE(IsIgnoredNetwork(network_manager, include_me));
  std::vector<std::string> ignore_list;
  ignore_list.push_back("ignore_me");
  network_manager.set_network_ignore_list(ignore_list);
  EXPECT_TRUE(IsIgnoredNetwork(network_manager, ignore_me));
  EXPECT_FALSE(IsIgnoredNetwork(network_manager, include_me));
}

// Test is failing on Windows opt: b/11288214
TEST_F(NetworkTest, DISABLED_TestCreateNetworks) {
  BasicNetworkManager manager;
  NetworkManager::NetworkList result = GetNetworks(manager, true);
  // We should be able to bind to any addresses we find.
  NetworkManager::NetworkList::iterator it;
  for (it = result.begin();
       it != result.end();
       ++it) {
    sockaddr_storage storage;
    memset(&storage, 0, sizeof(storage));
    IPAddress ip = (*it)->GetBestIP();
    SocketAddress bindaddress(ip, 0);
    bindaddress.SetScopeID((*it)->scope_id());
    // TODO(thaloun): Use rtc::AsyncSocket once it supports IPv6.
    int fd = static_cast<int>(socket(ip.family(), SOCK_STREAM, IPPROTO_TCP));
    if (fd > 0) {
      size_t ipsize = bindaddress.ToSockAddrStorage(&storage);
      EXPECT_GE(ipsize, 0U);
      int success = ::bind(fd,
                           reinterpret_cast<sockaddr*>(&storage),
                           static_cast<int>(ipsize));
#if defined(WEBRTC_WIN)
      if (success) LOG_GLE(LS_ERROR) << "Socket bind failed.";
#endif
      EXPECT_EQ(0, success);
#if defined(WEBRTC_WIN)
      closesocket(fd);
#else
      close(fd);
#endif
    }
    delete (*it);
  }
}

// Test that UpdateNetworks succeeds.
TEST_F(NetworkTest, TestUpdateNetworks) {
  BasicNetworkManager manager;
  manager.SignalNetworksChanged.connect(
      static_cast<NetworkTest*>(this), &NetworkTest::OnNetworksChanged);
  manager.StartUpdating();
  Thread::Current()->ProcessMessages(0);
  EXPECT_TRUE(callback_called_);
  callback_called_ = false;
  // Callback should be triggered immediately when StartUpdating
  // is called, after network update signal is already sent.
  manager.StartUpdating();
  EXPECT_TRUE(manager.started());
  Thread::Current()->ProcessMessages(0);
  EXPECT_TRUE(callback_called_);
  manager.StopUpdating();
  EXPECT_TRUE(manager.started());
  manager.StopUpdating();
  EXPECT_FALSE(manager.started());
  manager.StopUpdating();
  EXPECT_FALSE(manager.started());
  callback_called_ = false;
  // Callback should be triggered immediately after StartUpdating is called
  // when start_count_ is reset to 0.
  manager.StartUpdating();
  Thread::Current()->ProcessMessages(0);
  EXPECT_TRUE(callback_called_);
}

// Verify that MergeNetworkList() merges network lists properly.
TEST_F(NetworkTest, TestBasicMergeNetworkList) {
  Network ipv4_network1("test_eth0", "Test Network Adapter 1",
                        IPAddress(0x12345600U), 24);
  Network ipv4_network2("test_eth1", "Test Network Adapter 2",
                        IPAddress(0x00010000U), 16);
  ipv4_network1.AddIP(IPAddress(0x12345678));
  ipv4_network2.AddIP(IPAddress(0x00010004));
  BasicNetworkManager manager;

  // Add ipv4_network1 to the list of networks.
  NetworkManager::NetworkList list;
  list.push_back(new Network(ipv4_network1));
  bool changed;
  NetworkManager::Stats stats = MergeNetworkList(manager, list, &changed);
  EXPECT_TRUE(changed);
  EXPECT_EQ(stats.ipv6_network_count, 0);
  EXPECT_EQ(stats.ipv4_network_count, 1);
  list.clear();

  manager.GetNetworks(&list);
  EXPECT_EQ(1U, list.size());
  EXPECT_EQ(ipv4_network1.ToString(), list[0]->ToString());
  Network* net1 = list[0];
  list.clear();

  // Replace ipv4_network1 with ipv4_network2.
  list.push_back(new Network(ipv4_network2));
  stats = MergeNetworkList(manager, list, &changed);
  EXPECT_TRUE(changed);
  EXPECT_EQ(stats.ipv6_network_count, 0);
  EXPECT_EQ(stats.ipv4_network_count, 1);
  list.clear();

  manager.GetNetworks(&list);
  EXPECT_EQ(1U, list.size());
  EXPECT_EQ(ipv4_network2.ToString(), list[0]->ToString());
  Network* net2 = list[0];
  list.clear();

  // Add Network2 back.
  list.push_back(new Network(ipv4_network1));
  list.push_back(new Network(ipv4_network2));
  stats = MergeNetworkList(manager, list, &changed);
  EXPECT_TRUE(changed);
  EXPECT_EQ(stats.ipv6_network_count, 0);
  EXPECT_EQ(stats.ipv4_network_count, 2);
  list.clear();

  // Verify that we get previous instances of Network objects.
  manager.GetNetworks(&list);
  EXPECT_EQ(2U, list.size());
  EXPECT_TRUE((net1 == list[0] && net2 == list[1]) ||
              (net1 == list[1] && net2 == list[0]));
  list.clear();

  // Call MergeNetworkList() again and verify that we don't get update
  // notification.
  list.push_back(new Network(ipv4_network2));
  list.push_back(new Network(ipv4_network1));
  stats = MergeNetworkList(manager, list, &changed);
  EXPECT_FALSE(changed);
  EXPECT_EQ(stats.ipv6_network_count, 0);
  EXPECT_EQ(stats.ipv4_network_count, 2);
  list.clear();

  // Verify that we get previous instances of Network objects.
  manager.GetNetworks(&list);
  EXPECT_EQ(2U, list.size());
  EXPECT_TRUE((net1 == list[0] && net2 == list[1]) ||
              (net1 == list[1] && net2 == list[0]));
  list.clear();
}

// Sets up some test IPv6 networks and appends them to list.
// Four networks are added - public and link local, for two interfaces.
void SetupNetworks(NetworkManager::NetworkList* list) {
  IPAddress ip;
  IPAddress prefix;
  EXPECT_TRUE(IPFromString("abcd::1234:5678:abcd:ef12", &ip));
  EXPECT_TRUE(IPFromString("abcd::", &prefix));
  // First, fake link-locals.
  Network ipv6_eth0_linklocalnetwork("test_eth0", "Test NetworkAdapter 1",
                                     prefix, 64);
  ipv6_eth0_linklocalnetwork.AddIP(ip);
  EXPECT_TRUE(IPFromString("abcd::5678:abcd:ef12:3456", &ip));
  Network ipv6_eth1_linklocalnetwork("test_eth1", "Test NetworkAdapter 2",
                                     prefix, 64);
  ipv6_eth1_linklocalnetwork.AddIP(ip);
  // Public networks:
  EXPECT_TRUE(IPFromString("2401:fa00:4:1000:be30:5bff:fee5:c3", &ip));
  prefix = TruncateIP(ip, 64);
  Network ipv6_eth0_publicnetwork1_ip1("test_eth0", "Test NetworkAdapter 1",
                                       prefix, 64);
  ipv6_eth0_publicnetwork1_ip1.AddIP(ip);
  EXPECT_TRUE(IPFromString("2400:4030:1:2c00:be30:abcd:efab:cdef", &ip));
  prefix = TruncateIP(ip, 64);
  Network ipv6_eth1_publicnetwork1_ip1("test_eth1", "Test NetworkAdapter 1",
                                       prefix, 64);
  ipv6_eth1_publicnetwork1_ip1.AddIP(ip);
  list->push_back(new Network(ipv6_eth0_linklocalnetwork));
  list->push_back(new Network(ipv6_eth1_linklocalnetwork));
  list->push_back(new Network(ipv6_eth0_publicnetwork1_ip1));
  list->push_back(new Network(ipv6_eth1_publicnetwork1_ip1));
}

// Test that the basic network merging case works.
TEST_F(NetworkTest, TestIPv6MergeNetworkList) {
  BasicNetworkManager manager;
  manager.SignalNetworksChanged.connect(
      static_cast<NetworkTest*>(this), &NetworkTest::OnNetworksChanged);
  NetworkManager::NetworkList original_list;
  SetupNetworks(&original_list);
  bool changed = false;
  NetworkManager::Stats stats =
      MergeNetworkList(manager, original_list, &changed);
  EXPECT_TRUE(changed);
  EXPECT_EQ(stats.ipv6_network_count, 4);
  EXPECT_EQ(stats.ipv4_network_count, 0);
  NetworkManager::NetworkList list;
  manager.GetNetworks(&list);
  EXPECT_EQ(original_list.size(), list.size());
  // Verify that the original members are in the merged list.
  for (NetworkManager::NetworkList::iterator it = original_list.begin();
       it != original_list.end(); ++it) {
    EXPECT_NE(list.end(), std::find(list.begin(), list.end(), *it));
  }
}

// Test that no more than manager.max_ipv6_networks() IPv6 networks get
// returned.
TEST_F(NetworkTest, TestIPv6MergeNetworkListTrimExcessive) {
  BasicNetworkManager manager;
  manager.SignalNetworksChanged.connect(static_cast<NetworkTest*>(this),
                                        &NetworkTest::OnNetworksChanged);
  NetworkManager::NetworkList original_list;

  // Add twice the allowed number of IPv6 networks.
  for (int i = 0; i < 2 * manager.max_ipv6_networks(); i++) {
    // Make a network with different prefix length.
    IPAddress ip;
    EXPECT_TRUE(IPFromString("2401:fa01:4:1000:be30:faa:fee:faa", &ip));
    IPAddress prefix = TruncateIP(ip, 64 - i);
    Network* ipv6_network =
        new Network("test_eth0", "Test Network Adapter 1", prefix, 64 - i);
    ipv6_network->AddIP(ip);
    original_list.push_back(ipv6_network);
  }

  // Add one IPv4 network.
  Network* ipv4_network = new Network("test_eth0", "Test Network Adapter 1",
                                      IPAddress(0x12345600U), 24);
  ipv4_network->AddIP(IPAddress(0x12345600U));
  original_list.push_back(ipv4_network);

  bool changed = false;
  MergeNetworkList(manager, original_list, &changed);
  EXPECT_TRUE(changed);
  NetworkManager::NetworkList list;
  manager.GetNetworks(&list);

  // List size should be the max allowed IPv6 networks plus one IPv4 network.
  EXPECT_EQ(manager.max_ipv6_networks() + 1, (int)list.size());

  // Verify that the IPv4 network is in the list.
  EXPECT_NE(list.end(), std::find(list.begin(), list.end(), ipv4_network));
}

// Tests that when two network lists that describe the same set of networks are
// merged, that the changed callback is not called, and that the original
// objects remain in the result list.
TEST_F(NetworkTest, TestNoChangeMerge) {
  BasicNetworkManager manager;
  manager.SignalNetworksChanged.connect(
      static_cast<NetworkTest*>(this), &NetworkTest::OnNetworksChanged);
  NetworkManager::NetworkList original_list;
  SetupNetworks(&original_list);
  bool changed = false;
  MergeNetworkList(manager, original_list, &changed);
  EXPECT_TRUE(changed);
  // Second list that describes the same networks but with new objects.
  NetworkManager::NetworkList second_list;
  SetupNetworks(&second_list);
  changed = false;
  MergeNetworkList(manager, second_list, &changed);
  EXPECT_FALSE(changed);
  NetworkManager::NetworkList resulting_list;
  manager.GetNetworks(&resulting_list);
  EXPECT_EQ(original_list.size(), resulting_list.size());
  // Verify that the original members are in the merged list.
  for (NetworkManager::NetworkList::iterator it = original_list.begin();
       it != original_list.end(); ++it) {
    EXPECT_NE(resulting_list.end(),
              std::find(resulting_list.begin(), resulting_list.end(), *it));
  }
  // Doublecheck that the new networks aren't in the list.
  for (NetworkManager::NetworkList::iterator it = second_list.begin();
       it != second_list.end(); ++it) {
    EXPECT_EQ(resulting_list.end(),
              std::find(resulting_list.begin(), resulting_list.end(), *it));
  }
}

// Test that we can merge a network that is the same as another network but with
// a different IP. The original network should remain in the list, but have its
// IP changed.
TEST_F(NetworkTest, MergeWithChangedIP) {
  BasicNetworkManager manager;
  manager.SignalNetworksChanged.connect(
      static_cast<NetworkTest*>(this), &NetworkTest::OnNetworksChanged);
  NetworkManager::NetworkList original_list;
  SetupNetworks(&original_list);
  // Make a network that we're going to change.
  IPAddress ip;
  EXPECT_TRUE(IPFromString("2401:fa01:4:1000:be30:faa:fee:faa", &ip));
  IPAddress prefix = TruncateIP(ip, 64);
  Network* network_to_change = new Network("test_eth0",
                                          "Test Network Adapter 1",
                                          prefix, 64);
  Network* changed_network = new Network(*network_to_change);
  network_to_change->AddIP(ip);
  IPAddress changed_ip;
  EXPECT_TRUE(IPFromString("2401:fa01:4:1000:be30:f00:f00:f00", &changed_ip));
  changed_network->AddIP(changed_ip);
  original_list.push_back(network_to_change);
  bool changed = false;
  MergeNetworkList(manager, original_list, &changed);
  NetworkManager::NetworkList second_list;
  SetupNetworks(&second_list);
  second_list.push_back(changed_network);
  changed = false;
  MergeNetworkList(manager, second_list, &changed);
  EXPECT_TRUE(changed);
  NetworkManager::NetworkList list;
  manager.GetNetworks(&list);
  EXPECT_EQ(original_list.size(), list.size());
  // Make sure the original network is still in the merged list.
  EXPECT_NE(list.end(),
            std::find(list.begin(), list.end(), network_to_change));
  EXPECT_EQ(changed_ip, network_to_change->GetIPs().at(0));
}

// Testing a similar case to above, but checking that a network can be updated
// with additional IPs (not just a replacement).
TEST_F(NetworkTest, TestMultipleIPMergeNetworkList) {
  BasicNetworkManager manager;
  manager.SignalNetworksChanged.connect(
      static_cast<NetworkTest*>(this), &NetworkTest::OnNetworksChanged);
  NetworkManager::NetworkList original_list;
  SetupNetworks(&original_list);
  bool changed = false;
  MergeNetworkList(manager, original_list, &changed);
  EXPECT_TRUE(changed);
  IPAddress ip;
  IPAddress check_ip;
  IPAddress prefix;
  // Add a second IP to the public network on eth0 (2401:fa00:4:1000/64).
  EXPECT_TRUE(IPFromString("2401:fa00:4:1000:be30:5bff:fee5:c6", &ip));
  prefix = TruncateIP(ip, 64);
  Network ipv6_eth0_publicnetwork1_ip2("test_eth0", "Test NetworkAdapter 1",
                                       prefix, 64);
  // This is the IP that already existed in the public network on eth0.
  EXPECT_TRUE(IPFromString("2401:fa00:4:1000:be30:5bff:fee5:c3", &check_ip));
  ipv6_eth0_publicnetwork1_ip2.AddIP(ip);
  original_list.push_back(new Network(ipv6_eth0_publicnetwork1_ip2));
  changed = false;
  MergeNetworkList(manager, original_list, &changed);
  EXPECT_TRUE(changed);
  // There should still be four networks.
  NetworkManager::NetworkList list;
  manager.GetNetworks(&list);
  EXPECT_EQ(4U, list.size());
  // Check the gathered IPs.
  int matchcount = 0;
  for (NetworkManager::NetworkList::iterator it = list.begin();
       it != list.end(); ++it) {
    if ((*it)->ToString() == original_list[2]->ToString()) {
      ++matchcount;
      EXPECT_EQ(1, matchcount);
      // This should be the same network object as before.
      EXPECT_EQ((*it), original_list[2]);
      // But with two addresses now.
      EXPECT_EQ(2U, (*it)->GetIPs().size());
      EXPECT_NE((*it)->GetIPs().end(),
                std::find((*it)->GetIPs().begin(),
                          (*it)->GetIPs().end(),
                          check_ip));
      EXPECT_NE((*it)->GetIPs().end(),
                std::find((*it)->GetIPs().begin(),
                          (*it)->GetIPs().end(),
                          ip));
    } else {
      // Check the IP didn't get added anywhere it wasn't supposed to.
      EXPECT_EQ((*it)->GetIPs().end(),
                std::find((*it)->GetIPs().begin(),
                          (*it)->GetIPs().end(),
                          ip));
    }
  }
}

// Test that merge correctly distinguishes multiple networks on an interface.
TEST_F(NetworkTest, TestMultiplePublicNetworksOnOneInterfaceMerge) {
  BasicNetworkManager manager;
  manager.SignalNetworksChanged.connect(
      static_cast<NetworkTest*>(this), &NetworkTest::OnNetworksChanged);
  NetworkManager::NetworkList original_list;
  SetupNetworks(&original_list);
  bool changed = false;
  MergeNetworkList(manager, original_list, &changed);
  EXPECT_TRUE(changed);
  IPAddress ip;
  IPAddress prefix;
  // A second network for eth0.
  EXPECT_TRUE(IPFromString("2400:4030:1:2c00:be30:5bff:fee5:c3", &ip));
  prefix = TruncateIP(ip, 64);
  Network ipv6_eth0_publicnetwork2_ip1("test_eth0", "Test NetworkAdapter 1",
                                       prefix, 64);
  ipv6_eth0_publicnetwork2_ip1.AddIP(ip);
  original_list.push_back(new Network(ipv6_eth0_publicnetwork2_ip1));
  changed = false;
  MergeNetworkList(manager, original_list, &changed);
  EXPECT_TRUE(changed);
  // There should be five networks now.
  NetworkManager::NetworkList list;
  manager.GetNetworks(&list);
  EXPECT_EQ(5U, list.size());
  // Check the resulting addresses.
  for (NetworkManager::NetworkList::iterator it = list.begin();
       it != list.end(); ++it) {
    if ((*it)->prefix() == ipv6_eth0_publicnetwork2_ip1.prefix() &&
        (*it)->name() == ipv6_eth0_publicnetwork2_ip1.name()) {
      // Check the new network has 1 IP and that it's the correct one.
      EXPECT_EQ(1U, (*it)->GetIPs().size());
      EXPECT_EQ(ip, (*it)->GetIPs().at(0));
    } else {
      // Check the IP didn't get added anywhere it wasn't supposed to.
      EXPECT_EQ((*it)->GetIPs().end(),
                std::find((*it)->GetIPs().begin(),
                          (*it)->GetIPs().end(),
                          ip));
    }
  }
}

// Test that DumpNetworks works.
TEST_F(NetworkTest, TestDumpNetworks) {
  BasicNetworkManager manager;
  manager.DumpNetworks(true);
}

// Test that we can toggle IPv6 on and off.
TEST_F(NetworkTest, TestIPv6Toggle) {
  BasicNetworkManager manager;
  bool ipv6_found = false;
  NetworkManager::NetworkList list;
#if !defined(WEBRTC_WIN)
  // There should be at least one IPv6 network (fe80::/64 should be in there).
  // TODO(thaloun): Disabling this test on windows for the moment as the test
  // machines don't seem to have IPv6 installed on them at all.
  manager.set_ipv6_enabled(true);
  list = GetNetworks(manager, true);
  for (NetworkManager::NetworkList::iterator it = list.begin();
       it != list.end(); ++it) {
    if ((*it)->prefix().family() == AF_INET6) {
      ipv6_found = true;
      break;
    }
  }
  EXPECT_TRUE(ipv6_found);
  for (NetworkManager::NetworkList::iterator it = list.begin();
       it != list.end(); ++it) {
    delete (*it);
  }
#endif
  ipv6_found = false;
  manager.set_ipv6_enabled(false);
  list = GetNetworks(manager, true);
  for (NetworkManager::NetworkList::iterator it = list.begin();
       it != list.end(); ++it) {
    if ((*it)->prefix().family() == AF_INET6) {
      ipv6_found = true;
      break;
    }
  }
  EXPECT_FALSE(ipv6_found);
  for (NetworkManager::NetworkList::iterator it = list.begin();
       it != list.end(); ++it) {
    delete (*it);
  }
}

TEST_F(NetworkTest, TestNetworkListSorting) {
  BasicNetworkManager manager;
  Network ipv4_network1("test_eth0", "Test Network Adapter 1",
                        IPAddress(0x12345600U), 24);
  ipv4_network1.AddIP(IPAddress(0x12345600U));

  IPAddress ip;
  IPAddress prefix;
  EXPECT_TRUE(IPFromString("2400:4030:1:2c00:be30:abcd:efab:cdef", &ip));
  prefix = TruncateIP(ip, 64);
  Network ipv6_eth1_publicnetwork1_ip1("test_eth1", "Test NetworkAdapter 2",
                                       prefix, 64);
  ipv6_eth1_publicnetwork1_ip1.AddIP(ip);

  NetworkManager::NetworkList list;
  list.push_back(new Network(ipv4_network1));
  list.push_back(new Network(ipv6_eth1_publicnetwork1_ip1));
  Network* net1 = list[0];
  Network* net2 = list[1];

  bool changed = false;
  MergeNetworkList(manager, list, &changed);
  ASSERT_TRUE(changed);
  // After sorting IPv6 network should be higher order than IPv4 networks.
  EXPECT_TRUE(net1->preference() < net2->preference());
}

TEST_F(NetworkTest, TestNetworkAdapterTypes) {
  Network wifi("wlan0", "Wireless Adapter", IPAddress(0x12345600U), 24,
               ADAPTER_TYPE_WIFI);
  EXPECT_EQ(ADAPTER_TYPE_WIFI, wifi.type());
  Network ethernet("eth0", "Ethernet", IPAddress(0x12345600U), 24,
                   ADAPTER_TYPE_ETHERNET);
  EXPECT_EQ(ADAPTER_TYPE_ETHERNET, ethernet.type());
  Network cellular("test_cell", "Cellular Adapter", IPAddress(0x12345600U), 24,
                   ADAPTER_TYPE_CELLULAR);
  EXPECT_EQ(ADAPTER_TYPE_CELLULAR, cellular.type());
  Network vpn("bridge_test", "VPN Adapter", IPAddress(0x12345600U), 24,
              ADAPTER_TYPE_VPN);
  EXPECT_EQ(ADAPTER_TYPE_VPN, vpn.type());
  Network unknown("test", "Test Adapter", IPAddress(0x12345600U), 24,
                  ADAPTER_TYPE_UNKNOWN);
  EXPECT_EQ(ADAPTER_TYPE_UNKNOWN, unknown.type());
}

#if defined(WEBRTC_POSIX)
// Verify that we correctly handle interfaces with no address.
TEST_F(NetworkTest, TestConvertIfAddrsNoAddress) {
  ifaddrs list;
  memset(&list, 0, sizeof(list));
  list.ifa_name = const_cast<char*>("test_iface");

  NetworkManager::NetworkList result;
  BasicNetworkManager manager;
  CallConvertIfAddrs(manager, &list, true, &result);
  EXPECT_TRUE(result.empty());
}
#endif  // defined(WEBRTC_POSIX)

#if defined(WEBRTC_LINUX) && !defined(WEBRTC_ANDROID)
// If you want to test non-default routes, you can do the following on a linux
// machine:
// 1) Load the dummy network driver:
// sudo modprobe dummy
// sudo ifconfig dummy0 127.0.0.1
// 2) Run this test and confirm the output says it found a dummy route (and
// passes).
// 3) When done:
// sudo rmmmod dummy
TEST_F(NetworkTest, TestIgnoreNonDefaultRoutes) {
  BasicNetworkManager manager;
  NetworkManager::NetworkList list;
  list = GetNetworks(manager, false);
  bool found_dummy = false;
  LOG(LS_INFO) << "Looking for dummy network: ";
  for (NetworkManager::NetworkList::iterator it = list.begin();
       it != list.end(); ++it) {
    LOG(LS_INFO) << "  Network name: " << (*it)->name();
    found_dummy |= (*it)->name().find("dummy0") != std::string::npos;
  }
  for (NetworkManager::NetworkList::iterator it = list.begin();
       it != list.end(); ++it) {
    delete (*it);
  }
  if (!found_dummy) {
    LOG(LS_INFO) << "No dummy found, quitting.";
    return;
  }
  LOG(LS_INFO) << "Found dummy, running again while ignoring non-default "
               << "routes.";
  manager.set_ignore_non_default_routes(true);
  list = GetNetworks(manager, false);
  for (NetworkManager::NetworkList::iterator it = list.begin();
       it != list.end(); ++it) {
    LOG(LS_INFO) << "  Network name: " << (*it)->name();
    EXPECT_TRUE((*it)->name().find("dummy0") == std::string::npos);
  }
  for (NetworkManager::NetworkList::iterator it = list.begin();
       it != list.end(); ++it) {
    delete (*it);
  }
}
#endif

// Test MergeNetworkList successfully combines all IPs for the same
// prefix/length into a single Network.
TEST_F(NetworkTest, TestMergeNetworkList) {
  BasicNetworkManager manager;
  NetworkManager::NetworkList list;

  // Create 2 IPAddress classes with only last digit different.
  IPAddress ip1, ip2;
  EXPECT_TRUE(IPFromString("2400:4030:1:2c00:be30:0:0:1", &ip1));
  EXPECT_TRUE(IPFromString("2400:4030:1:2c00:be30:0:0:2", &ip2));

  // Create 2 networks with the same prefix and length.
  Network* net1 = new Network("em1", "em1", TruncateIP(ip1, 64), 64);
  Network* net2 = new Network("em1", "em1", TruncateIP(ip1, 64), 64);

  // Add different IP into each.
  net1->AddIP(ip1);
  net2->AddIP(ip2);

  list.push_back(net1);
  list.push_back(net2);
  bool changed;
  MergeNetworkList(manager, list, &changed);
  EXPECT_TRUE(changed);

  NetworkManager::NetworkList list2;
  manager.GetNetworks(&list2);

  // Make sure the resulted networklist has only 1 element and 2
  // IPAddresses.
  EXPECT_EQ(list2.size(), 1uL);
  EXPECT_EQ(list2[0]->GetIPs().size(), 2uL);
  EXPECT_EQ(list2[0]->GetIPs()[0], ip1);
  EXPECT_EQ(list2[0]->GetIPs()[1], ip2);
}

// Test that the filtering logic follows the defined ruleset in network.h.
TEST_F(NetworkTest, TestIPv6Selection) {
  InterfaceAddress ip;
  std::string ipstr;

  ipstr = "2401:fa00:4:1000:be30:5bff:fee5:c3";
  ASSERT_TRUE(IPFromString(ipstr, IPV6_ADDRESS_FLAG_DEPRECATED, &ip));

  // Create a network with this prefix.
  Network ipv6_network(
      "test_eth0", "Test NetworkAdapter", TruncateIP(ip, 64), 64);

  // When there is no address added, it should return an unspecified
  // address.
  EXPECT_EQ(ipv6_network.GetBestIP(), IPAddress());
  EXPECT_TRUE(IPIsUnspec(ipv6_network.GetBestIP()));

  // Deprecated one should not be returned.
  ipv6_network.AddIP(ip);
  EXPECT_EQ(ipv6_network.GetBestIP(), IPAddress());

  // Add ULA one. ULA is unique local address which is starting either
  // with 0xfc or 0xfd.
  ipstr = "fd00:fa00:4:1000:be30:5bff:fee5:c4";
  ASSERT_TRUE(IPFromString(ipstr, IPV6_ADDRESS_FLAG_NONE, &ip));
  ipv6_network.AddIP(ip);
  EXPECT_EQ(ipv6_network.GetBestIP(), static_cast<IPAddress>(ip));

  // Add global one.
  ipstr = "2401:fa00:4:1000:be30:5bff:fee5:c5";
  ASSERT_TRUE(IPFromString(ipstr, IPV6_ADDRESS_FLAG_NONE, &ip));
  ipv6_network.AddIP(ip);
  EXPECT_EQ(ipv6_network.GetBestIP(), static_cast<IPAddress>(ip));

  // Add global dynamic temporary one.
  ipstr = "2401:fa00:4:1000:be30:5bff:fee5:c6";
  ASSERT_TRUE(IPFromString(ipstr, IPV6_ADDRESS_FLAG_TEMPORARY, &ip));
  ipv6_network.AddIP(ip);
  EXPECT_EQ(ipv6_network.GetBestIP(), static_cast<IPAddress>(ip));
}

}  // namespace rtc
