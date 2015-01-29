/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>
#include "webrtc/base/asynchttprequest.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/httpserver.h"
#include "webrtc/base/socketstream.h"
#include "webrtc/base/thread.h"
#include "webrtc/test/testsupport/gtest_disable.h"

namespace rtc {

static const SocketAddress kServerAddr("127.0.0.1", 0);
static const SocketAddress kServerHostnameAddr("localhost", 0);
static const char kServerGetPath[] = "/get";
static const char kServerPostPath[] = "/post";
static const char kServerResponse[] = "This is a test";

class TestHttpServer : public HttpServer, public sigslot::has_slots<> {
 public:
  TestHttpServer(Thread* thread, const SocketAddress& addr) :
      socket_(thread->socketserver()->CreateAsyncSocket(addr.family(),
                                                        SOCK_STREAM)) {
    socket_->Bind(addr);
    socket_->Listen(5);
    socket_->SignalReadEvent.connect(this, &TestHttpServer::OnAccept);
  }

  SocketAddress address() const { return socket_->GetLocalAddress(); }
  void Close() const { socket_->Close(); }

 private:
  void OnAccept(AsyncSocket* socket) {
    AsyncSocket* new_socket = socket_->Accept(NULL);
    if (new_socket) {
      HandleConnection(new SocketStream(new_socket));
    }
  }
  rtc::scoped_ptr<AsyncSocket> socket_;
};

class AsyncHttpRequestTest : public testing::Test,
                             public sigslot::has_slots<> {
 public:
  AsyncHttpRequestTest()
      : started_(false),
        done_(false),
        server_(Thread::Current(), kServerAddr) {
    server_.SignalHttpRequest.connect(this, &AsyncHttpRequestTest::OnRequest);
  }

  bool started() const { return started_; }
  bool done() const { return done_; }

  AsyncHttpRequest* CreateGetRequest(const std::string& host, int port,
                                     const std::string& path) {
    rtc::AsyncHttpRequest* request =
        new rtc::AsyncHttpRequest("unittest");
    request->SignalWorkDone.connect(this,
        &AsyncHttpRequestTest::OnRequestDone);
    request->request().verb = rtc::HV_GET;
    request->set_host(host);
    request->set_port(port);
    request->request().path = path;
    request->response().document.reset(new MemoryStream());
    return request;
  }
  AsyncHttpRequest* CreatePostRequest(const std::string& host, int port,
                                      const std::string& path,
                                      const std::string content_type,
                                      StreamInterface* content) {
    rtc::AsyncHttpRequest* request =
        new rtc::AsyncHttpRequest("unittest");
    request->SignalWorkDone.connect(this,
        &AsyncHttpRequestTest::OnRequestDone);
    request->request().verb = rtc::HV_POST;
    request->set_host(host);
    request->set_port(port);
    request->request().path = path;
    request->request().setContent(content_type, content);
    request->response().document.reset(new MemoryStream());
    return request;
  }

  const TestHttpServer& server() const { return server_; }

 protected:
  void OnRequest(HttpServer* server, HttpServerTransaction* t) {
    started_ = true;

    if (t->request.path == kServerGetPath) {
      t->response.set_success("text/plain", new MemoryStream(kServerResponse));
    } else if (t->request.path == kServerPostPath) {
      // reverse the data and reply
      size_t size;
      StreamInterface* in = t->request.document.get();
      StreamInterface* out = new MemoryStream();
      in->GetSize(&size);
      for (size_t i = 0; i < size; ++i) {
        char ch;
        in->SetPosition(size - i - 1);
        in->Read(&ch, 1, NULL, NULL);
        out->Write(&ch, 1, NULL, NULL);
      }
      out->Rewind();
      t->response.set_success("text/plain", out);
    } else {
      t->response.set_error(404);
    }
    server_.Respond(t);
  }
  void OnRequestDone(SignalThread* thread) {
    done_ = true;
  }

 private:
  bool started_;
  bool done_;
  TestHttpServer server_;
};

TEST_F(AsyncHttpRequestTest, TestGetSuccess) {
  AsyncHttpRequest* req = CreateGetRequest(
      kServerHostnameAddr.hostname(), server().address().port(),
      kServerGetPath);
  EXPECT_FALSE(started());
  req->Start();
  EXPECT_TRUE_WAIT(started(), 5000);  // Should have started by now.
  EXPECT_TRUE_WAIT(done(), 5000);
  std::string response;
  EXPECT_EQ(200U, req->response().scode);
  ASSERT_TRUE(req->response().document);
  req->response().document->Rewind();
  req->response().document->ReadLine(&response);
  EXPECT_EQ(kServerResponse, response);
  req->Release();
}

TEST_F(AsyncHttpRequestTest, TestGetNotFound) {
  AsyncHttpRequest* req = CreateGetRequest(
      kServerHostnameAddr.hostname(), server().address().port(),
      "/bad");
  req->Start();
  EXPECT_TRUE_WAIT(done(), 5000);
  size_t size;
  EXPECT_EQ(404U, req->response().scode);
  ASSERT_TRUE(req->response().document);
  req->response().document->GetSize(&size);
  EXPECT_EQ(0U, size);
  req->Release();
}

TEST_F(AsyncHttpRequestTest, TestGetToNonServer) {
  AsyncHttpRequest* req = CreateGetRequest(
      "127.0.0.1", server().address().port(),
      kServerGetPath);
  // Stop the server before we send the request.
  server().Close();
  req->Start();
  EXPECT_TRUE_WAIT(done(), 10000);
  size_t size;
  EXPECT_EQ(500U, req->response().scode);
  ASSERT_TRUE(req->response().document);
  req->response().document->GetSize(&size);
  EXPECT_EQ(0U, size);
  req->Release();
}

TEST_F(AsyncHttpRequestTest, DISABLED_TestGetToInvalidHostname) {
  AsyncHttpRequest* req = CreateGetRequest(
      "invalid", server().address().port(),
      kServerGetPath);
  req->Start();
  EXPECT_TRUE_WAIT(done(), 5000);
  size_t size;
  EXPECT_EQ(500U, req->response().scode);
  ASSERT_TRUE(req->response().document);
  req->response().document->GetSize(&size);
  EXPECT_EQ(0U, size);
  req->Release();
}

TEST_F(AsyncHttpRequestTest, TestPostSuccess) {
  AsyncHttpRequest* req = CreatePostRequest(
      kServerHostnameAddr.hostname(), server().address().port(),
      kServerPostPath, "text/plain", new MemoryStream("abcd1234"));
  req->Start();
  EXPECT_TRUE_WAIT(done(), 5000);
  std::string response;
  EXPECT_EQ(200U, req->response().scode);
  ASSERT_TRUE(req->response().document);
  req->response().document->Rewind();
  req->response().document->ReadLine(&response);
  EXPECT_EQ("4321dcba", response);
  req->Release();
}

// Ensure that we shut down properly even if work is outstanding.
TEST_F(AsyncHttpRequestTest, TestCancel) {
  AsyncHttpRequest* req = CreateGetRequest(
      kServerHostnameAddr.hostname(), server().address().port(),
      kServerGetPath);
  req->Start();
  req->Destroy(true);
}

TEST_F(AsyncHttpRequestTest, TestGetSuccessDelay) {
  AsyncHttpRequest* req = CreateGetRequest(
      kServerHostnameAddr.hostname(), server().address().port(),
      kServerGetPath);
  req->set_start_delay(10);  // Delay 10ms.
  req->Start();
  Thread::SleepMs(5);
  EXPECT_FALSE(started());  // Should not have started immediately.
  EXPECT_TRUE_WAIT(started(), 5000);  // Should have started by now.
  EXPECT_TRUE_WAIT(done(), 5000);
  std::string response;
  EXPECT_EQ(200U, req->response().scode);
  ASSERT_TRUE(req->response().document);
  req->response().document->Rewind();
  req->response().document->ReadLine(&response);
  EXPECT_EQ(kServerResponse, response);
  req->Release();
}

}  // namespace rtc
