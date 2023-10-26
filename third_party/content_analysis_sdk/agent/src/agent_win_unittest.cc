// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <latch>
#include <memory>
#include <thread>

#include "agent/src/agent_win.h"
#include "agent/src/event_win.h"
#include "browser/src/client_win.h"
#include "gtest/gtest.h"

namespace content_analysis {
namespace sdk {
namespace testing {

// A handler that counts the number of times the callback methods are invoked.
// Also remembers the last BrowserInfo structure passed to it from any of the
// callbacks.
struct TestHandler : public AgentEventHandler {
  void OnBrowserConnected(const BrowserInfo& info) override {
    last_info_ = info;
    ++connect_count_;
  }
  void OnBrowserDisconnected(const BrowserInfo& info) override {
    last_info_ = info;
    ++disconnect_count_;
  }
  void OnAnalysisRequested(
      std::unique_ptr<ContentAnalysisEvent> event) override {
    ++request_count_;
    ResultCode ret = event->Send();
    ASSERT_EQ(ResultCode::OK, ret);
  }
  void OnResponseAcknowledged(
      const ContentAnalysisAcknowledgement& ack) override {
    ++ack_count_;
  }
  void OnCancelRequests(
      const ContentAnalysisCancelRequests& cancel) override {
    ++cancel_count_;
  }

  int connect_count_ = 0;
  int disconnect_count_ = 0;
  int request_count_ = 0;
  int ack_count_ = 0;
  int cancel_count_ = 0;
  BrowserInfo last_info_;
};

// A test handler that closes its event before sending the response.
struct CloseEventTestHandler : public TestHandler {
  void OnAnalysisRequested(
    std::unique_ptr<ContentAnalysisEvent> event) override {
    ++request_count_;

    // Closing the event before sending should generate an error.
    ResultCode ret = event->Close();
    ASSERT_EQ(ResultCode::OK, ret);

    ret = event->Send();
    ASSERT_NE(ResultCode::OK, ret);
  }
};

// A test handler that attempts to send two responses for a given request.
struct DoubleSendTestHandler : public TestHandler {
  void OnAnalysisRequested(
      std::unique_ptr<ContentAnalysisEvent> event) override {
    ++request_count_;

    ResultCode ret = event->Send();
    ASSERT_EQ(ResultCode::OK, ret);

    // Trying to send again fails.
    ret = event->Send();
    ASSERT_NE(ResultCode::OK, ret);
  }
};

// A test handler that signals a latch after a client connects.
// Can only be used with one client.
struct SignalClientConnectedTestHandler : public TestHandler {
  void OnBrowserConnected(const BrowserInfo& info) override {
    TestHandler::OnBrowserConnected(info);
    wait_for_client.count_down();
  }

  std::latch wait_for_client{ 1 };
};

// A test handler that signals a latch after a request is processed.
// Can only be used with one request.
struct SignalClientRequestedTestHandler : public TestHandler {
  void OnAnalysisRequested(
      std::unique_ptr<ContentAnalysisEvent> event) override {
    TestHandler::OnAnalysisRequested(std::move(event));
    wait_for_request.count_down();
  }

  std::latch wait_for_request{ 1 };
};

std::unique_ptr<AgentWin> CreateAgent(
    Agent::Config config,
    TestHandler** handler_ptr,
    ResultCode expected_rc=ResultCode::OK) {
  ResultCode rc;
  auto handler = std::make_unique<TestHandler>();
  *handler_ptr = handler.get();
  auto agent = std::make_unique<AgentWin>(
      std::move(config), std::move(handler), &rc);
  EXPECT_EQ(expected_rc, rc);
  return agent;
}

std::unique_ptr<ClientWin> CreateClient(
    Client::Config config) {
  int rc;
  auto client = std::make_unique<ClientWin>(std::move(config), &rc);
  return rc == 0 ? std::move(client) : nullptr;
}

ContentAnalysisRequest BuildRequest(std::string content=std::string()) {
  ContentAnalysisRequest request;
  request.set_request_token("req-token");
  *request.add_tags() = "dlp";
  request.set_text_content(content);  // Moved.
  return request;
}

TEST(AgentTest, Create) {
  const Agent::Config config{"test", false};
  TestHandler* handler_ptr;
  auto agent = CreateAgent(config, &handler_ptr);
  ASSERT_TRUE(agent);
  ASSERT_TRUE(handler_ptr);

  ASSERT_EQ(config.name, agent->GetConfig().name);
  ASSERT_EQ(config.user_specific, agent->GetConfig().user_specific);
}

TEST(AgentTest, Create_InvalidPipename) {
  // TODO(rogerta): The win32 docs say that a backslash is an invalid
  // character for a pipename, but it seemed to work correctly in testing.
  // Using an empty name was the easiest way to generate an invalid pipe
  // name.
  const Agent::Config config{"", false};
  TestHandler* handler_ptr;
  auto agent = CreateAgent(config, &handler_ptr,
      ResultCode::ERR_INVALID_CHANNEL_NAME);
  ASSERT_TRUE(agent);

  ASSERT_EQ(ResultCode::ERR_AGENT_NOT_INITIALIZED,
            agent->HandleOneEventForTesting());
}

// Can't create two agents with the same name.
TEST(AgentTest, Create_SecondFails) {
  const Agent::Config config{ "test", false };
  TestHandler* handler_ptr1;
  auto agent1 = CreateAgent(config, &handler_ptr1);
  ASSERT_TRUE(agent1);

  TestHandler* handler_ptr2;
  auto agent2 = CreateAgent(config, &handler_ptr2,
      ResultCode::ERR_AGENT_ALREADY_EXISTS);
  ASSERT_TRUE(agent2);

  ASSERT_EQ(ResultCode::ERR_AGENT_NOT_INITIALIZED,
            agent2->HandleOneEventForTesting());
}

TEST(AgentTest, Stop) {
  TestHandler* handler_ptr;
  auto agent = CreateAgent({ "test", false }, &handler_ptr);
  ASSERT_TRUE(agent);

  // Create a separate thread to stop the agent.
  std::thread thread([&agent]() {
    agent->Stop();
  });

  agent->HandleEvents();
  thread.join();
}

TEST(AgentTest, ConnectAndStop) {
  ResultCode rc;
  auto handler = std::make_unique<SignalClientConnectedTestHandler>();
  auto* handler_ptr = handler.get();
  auto agent = std::make_unique<AgentWin>(
    Agent::Config{"test", false}, std::move(handler), &rc);
  ASSERT_TRUE(agent);
  ASSERT_EQ(ResultCode::OK, rc);

  // Client thread waits until latch reaches zero.
  std::latch stop_client{ 1 };

  // Create a thread to handle the client.  Since the client is sync, it can't
  // run in the same thread as the agent.
  std::thread client_thread([&stop_client]() {
    auto client = CreateClient({ "test", false });
    ASSERT_TRUE(client);
    stop_client.wait();
  });

  // A thread that stops the agent after one client connects.
  std::thread stop_agent([&handler_ptr, &agent]() {
    handler_ptr->wait_for_client.wait();
    agent->Stop();
  });

  agent->HandleEvents();

  stop_client.count_down();
  client_thread.join();
  stop_agent.join();
}

TEST(AgentTest, Connect_UserSpecific) {
  ResultCode rc;
  auto handler = std::make_unique<SignalClientConnectedTestHandler>();
  auto* handler_ptr = handler.get();
  auto agent = std::make_unique<AgentWin>(
    Agent::Config{ "test", true }, std::move(handler), &rc);
  ASSERT_TRUE(agent);
  ASSERT_EQ(ResultCode::OK, rc);

  // Create a thread to handle the client.  Since the client is sync, it can't
  // run in the same thread as the agent.
  std::thread client_thread([]() {
    // If the user_specific does not match the agent, the client should not
    // connect.
    auto client = CreateClient({ "test", false });
    ASSERT_FALSE(client);

    auto client2 = CreateClient({ "test", true });
    ASSERT_TRUE(client2);
  });

  // A thread that stops the agent after one client connects.
  std::thread stop_agent([&handler_ptr, &agent]() {
    handler_ptr->wait_for_client.wait();
    agent->Stop();
  });

  agent->HandleEvents();

  client_thread.join();
  stop_agent.join();
}

TEST(AgentTest, ConnectRequestAndStop) {
  ResultCode rc;
  auto handler = std::make_unique<SignalClientRequestedTestHandler>();
  auto* handler_ptr = handler.get();
  auto agent = std::make_unique<AgentWin>(
      Agent::Config{"test", false}, std::move(handler), &rc);
  ASSERT_TRUE(agent);
  ASSERT_EQ(ResultCode::OK, rc);

  // Create a thread to handle the client.  Since the client is sync, it can't
  // run in the same thread as the agent.
  std::thread client_thread([]() {
    auto client = CreateClient({ "test", false });
    ASSERT_TRUE(client);

    ContentAnalysisRequest request = BuildRequest("test");
    ContentAnalysisResponse response;
    client->Send(request, &response);
  });

  // A thread that stops the agent after one client connects.
  std::thread stop_agent([&handler_ptr, &agent]() {
    handler_ptr->wait_for_request.wait();
    agent->Stop();
  });

  agent->HandleEvents();

  client_thread.join();
  stop_agent.join();
}

TEST(AgentTest, ConnectAndClose) {
  const Agent::Config aconfig{ "test", false };
  const Client::Config cconfig{ "test", false };

  // Create an agent and client that connects to it.
  TestHandler* handler_ptr;
  auto agent = CreateAgent(aconfig, &handler_ptr);
  ASSERT_TRUE(agent);
  auto client = CreateClient(cconfig);
  ASSERT_TRUE(client);
  ASSERT_EQ(cconfig.name, client->GetConfig().name);
  ASSERT_EQ(cconfig.user_specific, client->GetConfig().user_specific);

  agent->HandleOneEventForTesting();
  ASSERT_EQ(1, handler_ptr->connect_count_);
  ASSERT_EQ(0, handler_ptr->disconnect_count_);
  ASSERT_EQ(0, handler_ptr->cancel_count_);
  ASSERT_EQ(GetCurrentProcessId(), handler_ptr->last_info_.pid);

  // Close the client and make sure a disconnect is received.
  client.reset();
  agent->HandleOneEventForTesting();
  ASSERT_EQ(1, handler_ptr->connect_count_);
  ASSERT_EQ(1, handler_ptr->disconnect_count_);
  ASSERT_EQ(0, handler_ptr->cancel_count_);
  ASSERT_EQ(GetCurrentProcessId(), handler_ptr->last_info_.pid);
}

TEST(AgentTest, Request) {
  TestHandler* handler_ptr;
  auto agent = CreateAgent({"test", false}, &handler_ptr);
  ASSERT_TRUE(agent);

  bool done = false;

  // Create a thread to handle the client.  Since the client is sync, it can't
  // run in the same thread as the agent.
  std::thread client_thread([&done]() {
    auto client = CreateClient({"test", false});
    ASSERT_TRUE(client);

    // Send a request and wait for a response.
    ContentAnalysisRequest request = BuildRequest();
    ContentAnalysisResponse response;
    int ret = client->Send(request, &response);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(request.request_token(), response.request_token());

    done = true;
  });

  while (!done) {
    agent->HandleOneEventForTesting();
  }
  ASSERT_EQ(1, handler_ptr->request_count_);
  ASSERT_EQ(0, handler_ptr->ack_count_);
  ASSERT_EQ(0, handler_ptr->cancel_count_);

  client_thread.join();
}

TEST(AgentTest, Request_Large) {
  TestHandler* handler_ptr;
  auto agent = CreateAgent({"test", false}, &handler_ptr);
  ASSERT_TRUE(agent);

  bool done = false;

  // Create a thread to handle the client.  Since the client is sync, it can't
  // run in the same thread as the agent.
  std::thread client_thread([&done]() {
    auto client = CreateClient({"test", false});
    ASSERT_TRUE(client);

    // Send a request and wait for a response.  Create a large string, which
    // means larger than the initial mesasge buffer size specified when
    // creating the pipes (4096 bytes).
    ContentAnalysisRequest request = BuildRequest(std::string(5000, 'a'));
    ContentAnalysisResponse response;
    int ret = client->Send(request, &response);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(request.request_token(), response.request_token());

    done = true;
  });

  while (!done) {
    agent->HandleOneEventForTesting();
  }
  ASSERT_EQ(1, handler_ptr->request_count_);
  ASSERT_EQ(0, handler_ptr->ack_count_);
  ASSERT_EQ(0, handler_ptr->cancel_count_);

  client_thread.join();
}

TEST(AgentTest, Request_DoubleSend) {
  ResultCode rc;
  auto handler = std::make_unique<DoubleSendTestHandler>();
  DoubleSendTestHandler* handler_ptr = handler.get();
  auto agent = std::make_unique<AgentWin>(
    Agent::Config{"test", false}, std::move(handler), &rc);
  ASSERT_TRUE(agent);
  ASSERT_EQ(ResultCode::OK, rc);

  bool done = false;

  // Create a thread to handle the client.  Since the client is sync, it can't
  // run in the same thread as the agent.
  std::thread client_thread([&done]() {
    auto client = CreateClient({ "test", false });
    ASSERT_TRUE(client);

    // Send a request and wait for a response.
    ContentAnalysisRequest request = BuildRequest();
    ContentAnalysisResponse response;
    int ret = client->Send(request, &response);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(request.request_token(), response.request_token());

    done = true;
    });

  while (!done) {
    agent->HandleOneEventForTesting();
  }
  ASSERT_EQ(1, handler_ptr->request_count_);
  ASSERT_EQ(0, handler_ptr->ack_count_);
  ASSERT_EQ(0, handler_ptr->cancel_count_);

  client_thread.join();
}

TEST(AgentTest, Request_CloseEvent) {
  ResultCode rc;
  auto handler = std::make_unique<CloseEventTestHandler>();
  CloseEventTestHandler* handler_ptr = handler.get();
  auto agent = std::make_unique<AgentWin>(
      Agent::Config{"test", false}, std::move(handler), &rc);
  ASSERT_TRUE(agent);
  ASSERT_EQ(ResultCode::OK, rc);

  bool done = false;

  // Create a thread to handle the client.  Since the client is sync, it can't
  // run in the same thread as the agent.
  std::thread client_thread([&done]() {
    auto client = CreateClient({"test", false});
    ASSERT_TRUE(client);

    // Send a request and wait for a response.
    ContentAnalysisRequest request = BuildRequest();
    ContentAnalysisResponse response;
    int ret = client->Send(request, &response);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(request.request_token(), response.request_token());

    done = true;
  });

  while (!done) {
    agent->HandleOneEventForTesting();
  }
  ASSERT_EQ(1, handler_ptr->request_count_);

  client_thread.join();
}

TEST(AgentTest, Ack) {
  TestHandler* handler_ptr;
  auto agent = CreateAgent({ "test", false }, &handler_ptr);
  ASSERT_TRUE(agent);

  bool done = false;

  // Create a thread to handle the client.  Since the client is sync, it can't
  // run in the same thread as the agent.
  std::thread client_thread([&done]() {
    auto client = CreateClient({"test", false});
    ASSERT_TRUE(client);

    // Send a request and wait for a response.
    ContentAnalysisRequest request = BuildRequest();
    ContentAnalysisResponse response;
    int ret = client->Send(request, &response);
    ASSERT_EQ(0, ret);

    ContentAnalysisAcknowledgement ack;
    ack.set_request_token(request.request_token());
    ret = client->Acknowledge(ack);
    ASSERT_EQ(0, ret);

    done = true;
  });

  while (!done) {
    agent->HandleOneEventForTesting();
  }
  ASSERT_EQ(1, handler_ptr->request_count_);
  ASSERT_EQ(1, handler_ptr->ack_count_);
  ASSERT_EQ(0, handler_ptr->cancel_count_);

  client_thread.join();
}

TEST(AgentTest, Cancel) {
  TestHandler* handler_ptr;
  auto agent = CreateAgent({ "test", false }, &handler_ptr);
  ASSERT_TRUE(agent);

  // Create a thread to handle the client.  Since the client is sync, it can't
  // run in the same thread as the agent.
  std::thread client_thread([]() {
    auto client = CreateClient({"test", false});
    ASSERT_TRUE(client);

    ContentAnalysisCancelRequests cancel;
    cancel.set_user_action_id("1234567890");
    int ret = client->CancelRequests(cancel);
    ASSERT_EQ(0, ret);
  });

  while (handler_ptr->cancel_count_ == 0) {
    agent->HandleOneEventForTesting();
  }
  ASSERT_EQ(0, handler_ptr->request_count_);
  ASSERT_EQ(0, handler_ptr->ack_count_);

  client_thread.join();
}

}  // namespace testing
}  // namespace sdk
}  // namespace content_analysis

