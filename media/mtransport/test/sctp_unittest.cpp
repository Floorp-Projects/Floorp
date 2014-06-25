/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include <iostream>
#include <string>
#include <map>

#include "sigslot.h"

#include "logging.h"
#include "nsNetCID.h"
#include "nsITimer.h"
#include "nsComponentManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"

#include "transportflow.h"
#include "transportlayer.h"
#include "transportlayerloopback.h"

#include "mtransport_test_utils.h"
#include "runnable_utils.h"
#include "usrsctp.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"


using namespace mozilla;

MtransportTestUtils *test_utils;
static bool sctp_logging = false;
static int port_number = 5000;

namespace {

class TransportTestPeer;

class SendPeriodic : public nsITimerCallback {
 public:
  SendPeriodic(TransportTestPeer *peer, int to_send) :
      peer_(peer),
      to_send_(to_send) {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

 protected:
  virtual ~SendPeriodic() {}

  TransportTestPeer *peer_;
  int to_send_;
};

NS_IMPL_ISUPPORTS(SendPeriodic, nsITimerCallback)


class TransportTestPeer : public sigslot::has_slots<> {
 public:
  TransportTestPeer(std::string name, int local_port, int remote_port)
      : name_(name), connected_(false),
        sent_(0), received_(0),
        flow_(new TransportFlow()),
        loopback_(new TransportLayerLoopback()),
        sctp_(usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP, receive_cb, nullptr, 0, nullptr)),
        timer_(do_CreateInstance(NS_TIMER_CONTRACTID)),
        periodic_(nullptr) {
    std::cerr << "Creating TransportTestPeer; flow=" <<
        static_cast<void *>(flow_.get()) <<
        " local=" << local_port <<
        " remote=" << remote_port << std::endl;

    usrsctp_register_address(static_cast<void *>(this));
    int r = usrsctp_set_non_blocking(sctp_, 1);
    EXPECT_GE(r, 0);

    struct linger l;
    l.l_onoff = 1;
    l.l_linger = 0;
    r = usrsctp_setsockopt(sctp_, SOL_SOCKET, SO_LINGER, &l,
                       (socklen_t)sizeof(l));
    EXPECT_GE(r, 0);

    struct sctp_event subscription;
    memset(&subscription, 0, sizeof(subscription));
    subscription.se_assoc_id = SCTP_ALL_ASSOC;
    subscription.se_on = 1;
    subscription.se_type = SCTP_ASSOC_CHANGE;
    r = usrsctp_setsockopt(sctp_, IPPROTO_SCTP, SCTP_EVENT, &subscription,
                           sizeof(subscription));
    EXPECT_GE(r, 0);

    memset(&local_addr_, 0, sizeof(local_addr_));
    local_addr_.sconn_family = AF_CONN;
#if !defined(__Userspace_os_Linux) && !defined(__Userspace_os_Windows) && !defined(__Userspace_os_Android)
    local_addr_.sconn_len = sizeof(struct sockaddr_conn);
#endif
    local_addr_.sconn_port = htons(local_port);
    local_addr_.sconn_addr = static_cast<void *>(this);


    memset(&remote_addr_, 0, sizeof(remote_addr_));
    remote_addr_.sconn_family = AF_CONN;
#if !defined(__Userspace_os_Linux) && !defined(__Userspace_os_Windows) && !defined(__Userspace_os_Android)
    remote_addr_.sconn_len = sizeof(struct sockaddr_conn);
#endif
    remote_addr_.sconn_port = htons(remote_port);
    remote_addr_.sconn_addr = static_cast<void *>(this);

    nsresult res;
    res = loopback_->Init();
    EXPECT_EQ((nsresult)NS_OK, res);
  }

  ~TransportTestPeer() {
    std::cerr << "Destroying sctp connection flow=" <<
        static_cast<void *>(flow_.get()) << std::endl;
    usrsctp_close(sctp_);
    usrsctp_deregister_address(static_cast<void *>(this));

    test_utils->sts_target()->Dispatch(WrapRunnable(this,
                                                   &TransportTestPeer::Disconnect_s),
                                      NS_DISPATCH_SYNC);

    std::cerr << "~TransportTestPeer() completed" << std::endl;
  }

  void ConnectSocket(TransportTestPeer *peer) {
    test_utils->sts_target()->Dispatch(WrapRunnable(
        this, &TransportTestPeer::ConnectSocket_s, peer),
                                       NS_DISPATCH_SYNC);
  }

  void ConnectSocket_s(TransportTestPeer *peer) {
    loopback_->Connect(peer->loopback_);

    ASSERT_EQ((nsresult)NS_OK, flow_->PushLayer(loopback_));

    flow_->SignalPacketReceived.connect(this, &TransportTestPeer::PacketReceived);

    // SCTP here!
    ASSERT_TRUE(sctp_);
    std::cerr << "Calling usrsctp_bind()" << std::endl;
    int r = usrsctp_bind(sctp_, reinterpret_cast<struct sockaddr *>(
        &local_addr_), sizeof(local_addr_));
    ASSERT_GE(0, r);

    std::cerr << "Calling usrsctp_connect()" << std::endl;
    r = usrsctp_connect(sctp_, reinterpret_cast<struct sockaddr *>(
        &remote_addr_), sizeof(remote_addr_));
    ASSERT_GE(0, r);
  }

  void Disconnect_s() {
    if (flow_) {
      flow_ = nullptr;
    }
  }

  void Disconnect() {
    loopback_->Disconnect();
  }


  void StartTransfer(size_t to_send) {
    periodic_ = new SendPeriodic(this, to_send);
    timer_->SetTarget(test_utils->sts_target());
    timer_->InitWithCallback(periodic_, 10, nsITimer::TYPE_REPEATING_SLACK);
  }

  void SendOne() {
    unsigned char buf[100];
    memset(buf, sent_ & 0xff, sizeof(buf));

    struct sctp_sndinfo info;
    info.snd_sid = 1;
    info.snd_flags = 0;
    info.snd_ppid = 50;  // What the heck is this?
    info.snd_context = 0;
    info.snd_assoc_id = 0;

    int r = usrsctp_sendv(sctp_, buf, sizeof(buf), nullptr, 0,
                          static_cast<void *>(&info),
                          sizeof(info), SCTP_SENDV_SNDINFO, 0);
    ASSERT_TRUE(r >= 0);
    ASSERT_EQ(sizeof(buf), (size_t)r);

    ++sent_;
  }

  int sent() const { return sent_; }
  int received() const { return received_; }
  bool connected() const { return connected_; }

  static TransportResult SendPacket_s(const unsigned char* data, size_t len,
                                      const mozilla::RefPtr<TransportFlow>& flow) {
    TransportResult res = flow->SendPacket(data, len);
    delete data; // we always allocate
    return res;
  }

  TransportResult SendPacket(const unsigned char* data, size_t len) {
    unsigned char *buffer = new unsigned char[len];
    memcpy(buffer, data, len);

    // Uses DISPATCH_NORMAL to avoid possible deadlocks when we're called
    // from MainThread especially during shutdown (same as DataChannels).
    // RUN_ON_THREAD short-circuits if already on the STS thread, which is
    // normal for most transfers outside of connect() and close().  Passes
    // a refptr to flow_ to avoid any async deletion issues (since we can't
    // make 'this' into a refptr as it isn't refcounted)
    RUN_ON_THREAD(test_utils->sts_target(), WrapRunnableNM(
        &TransportTestPeer::SendPacket_s, buffer, len, flow_),
                  NS_DISPATCH_NORMAL);

    return 0;
  }

  void PacketReceived(TransportFlow * flow, const unsigned char* data,
                      size_t len) {
    std::cerr << "Received " << len << " bytes" << std::endl;

    // Pass the data to SCTP

    usrsctp_conninput(static_cast<void *>(this), data, len, 0);
  }

  // Process SCTP notification
  void Notification(union sctp_notification *msg, size_t len) {
    ASSERT_EQ(msg->sn_header.sn_length, len);

    if (msg->sn_header.sn_type == SCTP_ASSOC_CHANGE) {
      struct sctp_assoc_change *change = &msg->sn_assoc_change;

      if (change->sac_state == SCTP_COMM_UP) {
        std::cerr << "Connection up" << std::endl;
        SetConnected(true);
      } else {
        std::cerr << "Connection down" << std::endl;
        SetConnected(false);
      }
    }
  }

  void SetConnected(bool state) {
    connected_ = state;
  }

  static int conn_output(void *addr, void *buffer, size_t length, uint8_t tos, uint8_t set_df) {
    TransportTestPeer *peer = static_cast<TransportTestPeer *>(addr);

    peer->SendPacket(static_cast<unsigned char *>(buffer), length);

    return 0;
  }

  static int receive_cb(struct socket* sock, union sctp_sockstore addr,
                        void *data, size_t datalen,
                        struct sctp_rcvinfo rcv, int flags, void *ulp_info) {
    TransportTestPeer *me = static_cast<TransportTestPeer *>(
        addr.sconn.sconn_addr);
    MOZ_ASSERT(me);

    if (flags & MSG_NOTIFICATION) {
      union sctp_notification *notif =
          static_cast<union sctp_notification *>(data);

      me->Notification(notif, datalen);
      return 0;
    }

    me->received_ += datalen;

    std::cerr << "receive_cb: sock " << sock << " data " << data << "(" << datalen << ") total received bytes = " << me->received_ << std::endl;

    return 0;
  }


 private:
  std::string name_;
  bool connected_;
  size_t sent_;
  size_t received_;
  mozilla::RefPtr<TransportFlow> flow_;
  TransportLayerLoopback *loopback_;

  struct sockaddr_conn local_addr_;
  struct sockaddr_conn remote_addr_;
  struct socket *sctp_;
  nsCOMPtr<nsITimer> timer_;
  nsRefPtr<SendPeriodic> periodic_;
};


// Implemented here because it calls a method of TransportTestPeer
NS_IMETHODIMP SendPeriodic::Notify(nsITimer *timer) {
  peer_->SendOne();
  --to_send_;
  if (!to_send_) {
    timer->Cancel();
  }
  return NS_OK;
}

class TransportTest : public ::testing::Test {
 public:
  TransportTest() {
  }

  ~TransportTest() {
    if (p1_)
      p1_->Disconnect();
    if (p2_)
      p2_->Disconnect();
    delete p1_;
    delete p2_;
  }

  static void debug_printf(const char *format, ...) {
    va_list ap;

    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
  }


  static void SetUpTestCase() {
    if (sctp_logging) {
      usrsctp_init(0, &TransportTestPeer::conn_output, debug_printf);
      usrsctp_sysctl_set_sctp_debug_on(0xffffffff);
    } else {
      usrsctp_init(0, &TransportTestPeer::conn_output, nullptr);
    }
  }

  void SetUp() {
  }

  void ConnectSocket(int p1port = 0, int p2port = 0) {
    if (!p1port)
      p1port = port_number++;
    if (!p2port)
      p2port = port_number++;

    p1_ = new TransportTestPeer("P1", p1port, p2port);
    p2_ = new TransportTestPeer("P2", p2port, p1port);

    p1_->ConnectSocket(p2_);
    p2_->ConnectSocket(p1_);
    ASSERT_TRUE_WAIT(p1_->connected(), 2000);
    ASSERT_TRUE_WAIT(p2_->connected(), 2000);
  }

  void TestTransfer(int expected = 1) {
    std::cerr << "Starting trasnsfer test" << std::endl;
    p1_->StartTransfer(expected);
    ASSERT_TRUE_WAIT(p1_->sent() == expected, 10000);
    ASSERT_TRUE_WAIT(p2_->received() == (expected * 100), 10000);
    std::cerr << "P2 received " << p2_->received() << std::endl;
  }

 protected:
  TransportTestPeer *p1_;
  TransportTestPeer *p2_;
};

TEST_F(TransportTest, TestConnect) {
  ConnectSocket();
}

TEST_F(TransportTest, TestConnectSymmetricalPorts) {
  ConnectSocket(5002,5002);
}

TEST_F(TransportTest, TestTransfer) {
  ConnectSocket();
  TestTransfer(50);
}


}  // end namespace

int main(int argc, char **argv)
{
  test_utils = new MtransportTestUtils();
  // Start the tests
  ::testing::InitGoogleTest(&argc, argv);

  for(int i=0; i<argc; i++) {
    if (!strcmp(argv[i],"-v")) {
      sctp_logging = true;
    }
  }

  int rv = RUN_ALL_TESTS();
  delete test_utils;
  return rv;
}
