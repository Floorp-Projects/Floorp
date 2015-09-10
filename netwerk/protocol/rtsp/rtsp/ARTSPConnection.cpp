/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "ARTSPConnection"
#include "RtspPrlog.h"

#include "ARTSPConnection.h"

#include <cutils/properties.h>

#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/base64.h>
#include <media/stagefright/MediaErrors.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsNetCID.h"
#include "ClosingService.h"
#include "nsIServiceManager.h"
#include "nsICryptoHash.h"

#include "prnetdb.h"
#include "prerr.h"
#include "prerror.h"
#include "NetworkActivityMonitor.h"

using namespace mozilla::net;

namespace android {

// static
const uint32_t ARTSPConnection::kSocketPollTimeoutUs = 1000;
const uint32_t ARTSPConnection::kSocketPollTimeoutRetries = 10000;
const uint32_t ARTSPConnection::kSocketBlokingRecvTimeout = 10;

ARTSPConnection::ARTSPConnection(bool uidValid, uid_t uid)
    : mUIDValid(uidValid),
      mUID(uid),
      mState(DISCONNECTED),
      mAuthType(NONE),
      mConnectionID(0),
      mNextCSeq(0),
      mReceiveResponseEventPending(false),
      mSocket(nullptr),
      mNumSocketPollTimeoutRetries(0) {
}

ARTSPConnection::~ARTSPConnection() {
    if (mSocket) {
        LOGE("Connection is still open, closing the socket.");
        closeSocket();
    }
}

void ARTSPConnection::connect(const char *url, const sp<AMessage> &reply) {
    sp<AMessage> msg = new AMessage(kWhatConnect, id());
    msg->setString("url", url);
    msg->setMessage("reply", reply);
    msg->post();
}

void ARTSPConnection::disconnect(const sp<AMessage> &reply) {
    int32_t result;
    sp<AMessage> msg = new AMessage(kWhatDisconnect, id());
    msg->setMessage("reply", reply);
    if (reply->findInt32("result", &result)) {
      msg->setInt32("result", result);
    } else {
      msg->setInt32("result", OK);
    }
    msg->post();
}

void ARTSPConnection::sendRequest(
        const char *request, const sp<AMessage> &reply) {
    sp<AMessage> msg = new AMessage(kWhatSendRequest, id());
    msg->setString("request", request);
    msg->setMessage("reply", reply);
    msg->post();
}

void ARTSPConnection::observeBinaryData(const sp<AMessage> &reply) {
    sp<AMessage> msg = new AMessage(kWhatObserveBinaryData, id());
    msg->setMessage("reply", reply);
    msg->post();
}

void ARTSPConnection::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatConnect:
            onConnect(msg);
            break;

        case kWhatDisconnect:
            onDisconnect(msg);
            break;

        case kWhatCompleteConnection:
            onCompleteConnection(msg);
            break;

        case kWhatSendRequest:
            onSendRequest(msg);
            break;

        case kWhatReceiveResponse:
            onReceiveResponse();
            break;

        case kWhatObserveBinaryData:
        {
            CHECK(msg->findMessage("reply", &mObserveBinaryMessage));
            break;
        }

        default:
            TRESPASS();
            break;
    }
}

// static
bool ARTSPConnection::ParseURL(
        const char *url, AString *host, uint16_t *port, AString *path,
        AString *user, AString *pass) {
    host->clear();
    *port = 0;
    path->clear();
    user->clear();
    pass->clear();

    if (strncasecmp("rtsp://", url, 7)) {
        return false;
    }

    const char *slashPos = strchr(&url[7], '/');

    if (slashPos == NULL) {
        host->setTo(&url[7]);
        path->setTo("/");
    } else {
        host->setTo(&url[7], slashPos - &url[7]);
        path->setTo(slashPos);
    }

    ssize_t atPos = host->find("@");

    if (atPos >= 0) {
        // Split of user:pass@ from hostname.

        AString userPass(*host, 0, atPos);
        host->erase(0, atPos + 1);

        ssize_t colonPos = userPass.find(":");

        if (colonPos < 0) {
            *user = userPass;
        } else {
            user->setTo(userPass, 0, colonPos);
            pass->setTo(userPass, colonPos + 1, userPass.size() - colonPos - 1);
        }
    }

    const char *colonPos = strchr(host->c_str(), ':');

    if (colonPos != NULL) {
        unsigned long x;
        if (!ParseSingleUnsignedLong(colonPos + 1, &x) || x >= 65536) {
            return false;
        }

        *port = x;

        size_t colonOffset = colonPos - host->c_str();
        size_t trailing = host->size() - colonOffset;
        host->erase(colonOffset, trailing);
    } else {
        *port = 554;
    }

    return true;
}

void ARTSPConnection::MakeUserAgent(const char *userAgent) {
    mUserAgent.clear();
    mUserAgent.setTo("User-Agent: ");
    mUserAgent.append(userAgent);
    mUserAgent.append("\r\n");
}

static status_t MakeSocketBlocking(PRFileDesc *fd, bool blocking) {
    // Check if socket is closed.
    if (!fd) {
        return UNKNOWN_ERROR;
    }

    PRStatus rv = PR_FAILURE;
    PRSocketOptionData opt;

    opt.option = PR_SockOpt_Nonblocking;
    opt.value.non_blocking = !blocking;
    rv = PR_SetSocketOption(fd, &opt);

    return rv == PR_SUCCESS ? OK : UNKNOWN_ERROR;
}

void ARTSPConnection::onConnect(const sp<AMessage> &msg) {
    ++mConnectionID;

    if (mState != DISCONNECTED) {
        closeSocket();

        flushPendingRequests();
    }

    mState = CONNECTING;

    AString url;
    CHECK(msg->findString("url", &url));

    sp<AMessage> reply;
    CHECK(msg->findMessage("reply", &reply));

    AString host, path;
    uint16_t port;
    if (!ParseURL(url.c_str(), &host, &port, &path, &mUser, &mPass)
            || (mUser.size() > 0 && mPass.size() == 0)) {
        // If we have a user name but no password we have to give up
        // right here, since we currently have no way of asking the user
        // for this information.

        LOGE("Malformed rtsp url %s", url.c_str());

        reply->setInt32("result", ERROR_MALFORMED);
        reply->post();

        mState = DISCONNECTED;
        return;
    }

    if (mUser.size() > 0) {
        LOGV("user = '%s', pass = '%s'", mUser.c_str(), mPass.c_str());
    }

    PRStatus status = PR_FAILURE;
    PRHostEnt hostentry;
    char buffer[PR_NETDB_BUF_SIZE];

    status = PR_GetHostByName(
        host.c_str(), buffer, PR_NETDB_BUF_SIZE, &hostentry);
    if (status == PR_FAILURE) {
        LOGE("Unknown host %s", host.c_str());

        PRErrorCode code = PR_GetError();
        reply->setInt32("result", -code);
        reply->post();

        mState = DISCONNECTED;
        return;
    }

    mSocket = PR_OpenTCPSocket(PR_AF_INET);
    if (!mSocket) {
        TRESPASS();
    }

    NetworkActivityMonitor::AttachIOLayer(mSocket);
    ClosingService::AttachIOLayer(mSocket);

    MakeSocketBlocking(mSocket, false);

    PRNetAddr remote;
    remote.inet.family = PR_AF_INET;
    remote.inet.ip = *((uint32_t *) hostentry.h_addr_list[0]);
    remote.inet.port = PR_htons(port);

    status = PR_Connect(mSocket, &remote, PR_INTERVAL_NO_TIMEOUT);

    reply->setInt32("server-ip", PR_ntohl(remote.inet.ip));

    if (status != PR_SUCCESS) {
        PRErrorCode code = PR_GetError();
        if (code == PR_IN_PROGRESS_ERROR) {
            mNumSocketPollTimeoutRetries = 0;
            sp<AMessage> msg = new AMessage(kWhatCompleteConnection, id());
            msg->setMessage("reply", reply);
            msg->setInt32("connection-id", mConnectionID);
            msg->post();
            return;
        }

        reply->setInt32("result", -code);
        mState = DISCONNECTED;

        closeSocket();
    } else {
        reply->setInt32("result", OK);
        mState = CONNECTED;
        mNextCSeq = 1;

        postReceiveResponseEvent();
    }

    reply->post();
}

void ARTSPConnection::performDisconnect() {
    closeSocket();

    flushPendingRequests();

    mUser.clear();
    mPass.clear();
    mAuthType = NONE;
    mNonce.clear();
    mNumSocketPollTimeoutRetries = 0;

    mState = DISCONNECTED;
}

void ARTSPConnection::onDisconnect(const sp<AMessage> &msg) {
    int32_t result;
    if (mState == CONNECTED || mState == CONNECTING) {
        performDisconnect();
    }

    sp<AMessage> reply;
    CHECK(msg->findMessage("reply", &reply));
    CHECK(msg->findInt32("result", &result));

    reply->setInt32("result", result);
    reply->post();
}

void ARTSPConnection::onCompleteConnection(const sp<AMessage> &msg) {
    sp<AMessage> reply;
    CHECK(msg->findMessage("reply", &reply));

    int32_t connectionID;
    CHECK(msg->findInt32("connection-id", &connectionID));

    if ((connectionID != mConnectionID) || mState != CONNECTING) {
        // While we were attempting to connect, the attempt was
        // cancelled.
        reply->setInt32("result", -ECONNABORTED);
        reply->post();
        return;
    }

    PRPollDesc writePollDesc;

    writePollDesc.fd = mSocket;
    writePollDesc.in_flags = PR_POLL_WRITE;

    int32_t numSocketsReadyToRead = PR_Poll(&writePollDesc, 1,
        PR_MicrosecondsToInterval(kSocketPollTimeoutUs));

    CHECK_GE(numSocketsReadyToRead, 0);

    if (numSocketsReadyToRead == 0) {
        // PR_Poll() timed out. Not yet connected.
        if (mNumSocketPollTimeoutRetries < kSocketPollTimeoutRetries) {
            mNumSocketPollTimeoutRetries++;
            msg->post();
        } else {
            // Connection timeout here.
            // We cannot establish TCP connection, abort the connect
            // and reply an error to RTSPConnectionHandler.
            LOGE("Connection timeout. Failed to connect to the server.");
            mNumSocketPollTimeoutRetries = 0;
            reply->setInt32("result", -ETIMEDOUT);
            reply->post();
        }
        return;
    }

    if (numSocketsReadyToRead < 0) {
        reply->setInt32("result", -PR_GetError());

        mState = DISCONNECTED;
        closeSocket();
    } else {
        reply->setInt32("result", OK);
        mState = CONNECTED;
        mNextCSeq = 1;

        postReceiveResponseEvent();
    }

    reply->post();
}

void ARTSPConnection::onSendRequest(const sp<AMessage> &msg) {
    sp<AMessage> reply;
    CHECK(msg->findMessage("reply", &reply));

    if (mState != CONNECTED) {
        reply->setInt32("result", -ENOTCONN);
        reply->post();
        return;
    }

    AString request;
    CHECK(msg->findString("request", &request));

    // Just in case we need to re-issue the request with proper authentication
    // later, stash it away.
    reply->setString("original-request", request.c_str(), request.size());

    addAuthentication(&request);
    addUserAgent(&request);

    // Find the boundary between headers and the body.
    ssize_t i = request.find("\r\n\r\n");
    CHECK_GE(i, 0);

    int32_t cseq = mNextCSeq++;

    AString cseqHeader = "CSeq: ";
    cseqHeader.append(cseq);
    cseqHeader.append("\r\n");

    request.insert(cseqHeader, i + 2);

    LOGV("request: '%s'", request.c_str());

    size_t numBytesSent = 0;
    while (numBytesSent < request.size()) {
        ssize_t n =
            PR_Send(mSocket, request.c_str() + numBytesSent,
                    request.size() - numBytesSent, 0, PR_INTERVAL_NO_WAIT);

        PRErrorCode errCode = PR_GetError();
        if (n < 0 && errCode == PR_PENDING_INTERRUPT_ERROR) {
            continue;
        }

        if (n <= 0) {
            performDisconnect();

            if (n == 0) {
                // Server closed the connection.
                LOGE("Server unexpectedly closed the connection.");

                reply->setInt32("result", ERROR_IO);
                reply->post();
            } else {
                LOGE("Error sending rtsp request. (%d)", errCode);
                reply->setInt32("result", -errCode);
                reply->post();
            }

            return;
        }

        numBytesSent += (size_t)n;
    }

    mPendingRequests.add(cseq, reply);
}

void ARTSPConnection::onReceiveResponse() {
    mReceiveResponseEventPending = false;

    if (mState != CONNECTED) {
        return;
    }

    PRPollDesc readPollDesc;
    readPollDesc.fd = mSocket;
    readPollDesc.in_flags = PR_POLL_READ;

    int32_t numSocketsReadyToRead = PR_Poll(&readPollDesc, 1,
        PR_MicrosecondsToInterval(kSocketPollTimeoutUs));

    CHECK_GE(numSocketsReadyToRead, 0);

    // Number of ready-to-read sockets is not expected to be greater than 1.
    if (numSocketsReadyToRead > 1) {
        return;
    }

    if (numSocketsReadyToRead == 1) {
        MakeSocketBlocking(mSocket, true);

        bool success = receiveRTSPResponse();

        MakeSocketBlocking(mSocket, false);

        if (!success) {
            // Something horrible, irreparable has happened.
            flushPendingRequests();
            return;
        }
    }

    postReceiveResponseEvent();
}

void ARTSPConnection::flushPendingRequests() {
    for (size_t i = 0; i < mPendingRequests.size(); ++i) {
        sp<AMessage> reply = mPendingRequests.valueAt(i);

        reply->setInt32("result", -ECONNABORTED);
        reply->post();
    }

    mPendingRequests.clear();
}

void ARTSPConnection::postReceiveResponseEvent() {
    if (mReceiveResponseEventPending) {
        return;
    }

    sp<AMessage> msg = new AMessage(kWhatReceiveResponse, id());
    msg->post();

    mReceiveResponseEventPending = true;
}

status_t ARTSPConnection::receive(void *data, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        ssize_t n = PR_Recv(mSocket, (uint8_t *)data + offset, size - offset,
                            0, PR_SecondsToInterval(kSocketBlokingRecvTimeout));

        PRErrorCode errCode = PR_GetError();
        if (n < 0 && errCode == PR_PENDING_INTERRUPT_ERROR) {
            continue;
        }

        if (n <= 0) {
            performDisconnect();

            if (n == 0) {
                // Server closed the connection.
                LOGE("Server unexpectedly closed the connection.");
                return ERROR_IO;
            } else {
                LOGE("Error reading rtsp response. (%d)", errCode);
                return -errCode;
            }
        }

        offset += (size_t)n;
    }

    return OK;
}

bool ARTSPConnection::receiveLine(AString *line) {
    line->clear();

    bool sawCR = false;
    for (;;) {
        char c;
        if (receive(&c, 1) != OK) {
            return false;
        }

        if (sawCR && c == '\n') {
            line->erase(line->size() - 1, 1);
            return true;
        }

        line->append(&c, 1);

        if (c == '$' && line->size() == 1) {
            // Special-case for interleaved binary data.
            return true;
        }

        sawCR = (c == '\r');
    }
}

sp<ABuffer> ARTSPConnection::receiveBinaryData() {
    uint8_t x[3];
    if (receive(x, 3) != OK) {
        return NULL;
    }

    sp<ABuffer> buffer = new ABuffer((x[1] << 8) | x[2]);
    if (receive(buffer->data(), buffer->size()) != OK) {
        return NULL;
    }

    buffer->meta()->setInt32("index", (int32_t)x[0]);

    return buffer;
}

static bool IsRTSPVersion(const AString &s) {
    return s == "RTSP/1.0";
}

bool ARTSPConnection::receiveRTSPResponse() {
    AString statusLine;

    if (!receiveLine(&statusLine)) {
        return false;
    }

    if (statusLine == "$") {
        sp<ABuffer> buffer = receiveBinaryData();

        if (buffer == NULL) {
            return false;
        }

        if (mObserveBinaryMessage != NULL) {
            sp<AMessage> notify = mObserveBinaryMessage->dup();
            notify->setObject("buffer", buffer);
            notify->post();
        } else {
            LOGW("received binary data, but no one cares.");
        }

        return true;
    }

    /*
     * Status-Line = RTSP-Version SP Status-Code SP Reason-Phrase CRLF
     */
    sp<ARTSPResponse> response = new ARTSPResponse;
    response->mStatusLine = statusLine;

    LOGI("status: %s", response->mStatusLine.c_str());

    ssize_t space1 = response->mStatusLine.find(" ");
    if (space1 < 0) {
        return false;
    }
    ssize_t space2 = response->mStatusLine.find(" ", space1 + 1);
    if (space2 < 0) {
        return false;
    }

    bool isRequest = false;

    if (!IsRTSPVersion(AString(response->mStatusLine, 0, space1))) {
        /*
         * Request-Line = Method SP Request-URI SP RTSP-Version CRLF
         */
        if (!IsRTSPVersion(AString(response->mStatusLine, space2 + 1,
                                   response->mStatusLine.size() - space2 - 1))) {
            /* Neither an RTSP response or request */
            return false;
        }

        isRequest = true;
        response->mStatusCode = 0;
    } else {
        AString statusCodeStr(
                response->mStatusLine, space1 + 1, space2 - space1 - 1);

        if (!ParseSingleUnsignedLong(
                    statusCodeStr.c_str(), &response->mStatusCode)
                || response->mStatusCode < 100 || response->mStatusCode > 999) {
            return false;
        }
    }

    AString line;
    ssize_t lastDictIndex = -1;
    for (;;) {
        if (!receiveLine(&line)) {
            break;
        }

        if (line.empty()) {
            break;
        }

        LOGV("line: '%s'", line.c_str());

        if (line.c_str()[0] == ' ' || line.c_str()[0] == '\t') {
            // Support for folded header values.

            if (lastDictIndex < 0) {
                // First line cannot be a continuation of the previous one.
                return false;
            }

            AString &value = response->mHeaders.editValueAt(lastDictIndex);
            value.append(line);

            continue;
        }

        ssize_t colonPos = line.find(":");
        if (colonPos < 0) {
            // Malformed header line.
            return false;
        }

        AString key(line, 0, colonPos);
        key.trim();
        key.tolower();

        line.erase(0, colonPos + 1);

        lastDictIndex = response->mHeaders.add(key, line);
    }

    for (size_t i = 0; i < response->mHeaders.size(); ++i) {
        response->mHeaders.editValueAt(i).trim();
    }

    unsigned long contentLength = 0;

    ssize_t i = response->mHeaders.indexOfKey("content-length");

    if (i >= 0) {
        AString value = response->mHeaders.valueAt(i);
        if (!ParseSingleUnsignedLong(value.c_str(), &contentLength)) {
            return false;
        }
    }

    if (contentLength > 0) {
        response->mContent = new ABuffer(contentLength);

        if (receive(response->mContent->data(), contentLength) != OK) {
            return false;
        }
    }

    if (response->mStatusCode == 401) {
        if (mAuthType == NONE && mUser.size() > 0
                && parseAuthMethod(response)) {
            ssize_t i;
            if ((status_t)OK != findPendingRequest(response, &i) || i < 0) {
                return false;
            }

            sp<AMessage> reply = mPendingRequests.valueAt(i);
            mPendingRequests.removeItemsAt(i);

            AString request;
            CHECK(reply->findString("original-request", &request));

            sp<AMessage> msg = new AMessage(kWhatSendRequest, id());
            msg->setMessage("reply", reply);
            msg->setString("request", request.c_str(), request.size());

            LOGI("re-sending request with authentication headers...");
            onSendRequest(msg);

            return true;
        }
    }

    return isRequest
        ? handleServerRequest(response)
        : notifyResponseListener(response);
}

bool ARTSPConnection::handleServerRequest(const sp<ARTSPResponse> &request) {
    // Implementation of server->client requests is optional for all methods
    // but we do need to respond, even if it's just to say that we don't
    // support the method.

    ssize_t space1 = request->mStatusLine.find(" ");
    if (space1 < 0) {
        return false;
    }

    AString response;
    response.append("RTSP/1.0 501 Not Implemented\r\n");

    ssize_t i = request->mHeaders.indexOfKey("cseq");

    if (i >= 0) {
        AString value = request->mHeaders.valueAt(i);

        unsigned long cseq;
        if (!ParseSingleUnsignedLong(value.c_str(), &cseq)) {
            return false;
        }

        response.append("CSeq: ");
        response.append(cseq);
        response.append("\r\n");
    }

    response.append("\r\n");

    size_t numBytesSent = 0;
    while (numBytesSent < response.size()) {
        ssize_t n =
            PR_Send(mSocket, response.c_str() + numBytesSent,
                 response.size() - numBytesSent, 0, PR_INTERVAL_NO_WAIT);

        PRErrorCode errCode = PR_GetError();
        if (n < 0 && errCode == PR_PENDING_INTERRUPT_ERROR) {
            continue;
        }

        if (n <= 0) {
            if (n == 0) {
                // Server closed the connection.
                LOGE("Server unexpectedly closed the connection.");
            } else {
                LOGE("Error sending rtsp response. (%d)", errCode);
            }

            performDisconnect();

            return false;
        }

        numBytesSent += (size_t)n;
    }

    return true;
}

// static
bool ARTSPConnection::ParseSingleUnsignedLong(
        const char *from, unsigned long *x) {
    char *end;
    *x = strtoul(from, &end, 10);

    if (end == from || *end != '\0') {
        return false;
    }

    return true;
}

status_t ARTSPConnection::findPendingRequest(
        const sp<ARTSPResponse> &response, ssize_t *index) const {
    *index = 0;

    ssize_t i = response->mHeaders.indexOfKey("cseq");

    if (i < 0) {
        // This is an unsolicited server->client message.
        return OK;
    }

    AString value = response->mHeaders.valueAt(i);

    unsigned long cseq;
    if (!ParseSingleUnsignedLong(value.c_str(), &cseq)) {
        return ERROR_MALFORMED;
    }

    i = mPendingRequests.indexOfKey(cseq);

    if (i < 0) {
        return -ENOENT;
    }

    *index = i;

    return OK;
}

bool ARTSPConnection::notifyResponseListener(
        const sp<ARTSPResponse> &response) {
    ssize_t i;
    status_t err = findPendingRequest(response, &i);

    if (err == OK && i < 0) {
        // An unsolicited server response is not a problem.
        return true;
    }

    if (err != OK) {
        return false;
    }

    sp<AMessage> reply = mPendingRequests.valueAt(i);
    mPendingRequests.removeItemsAt(i);

    reply->setInt32("result", OK);
    reply->setObject("response", response);
    reply->post();

    return true;
}

bool ARTSPConnection::parseAuthMethod(const sp<ARTSPResponse> &response) {
    ssize_t i = response->mHeaders.indexOfKey("www-authenticate");

    if (i < 0) {
        return false;
    }

    AString value = response->mHeaders.valueAt(i);

    if (!strncmp(value.c_str(), "Basic", 5)) {
        mAuthType = BASIC;
    } else {
        if (strncmp(value.c_str(), "Digest", 6)) {
            return false;
        }
        mAuthType = DIGEST;

        i = value.find("nonce=");
        if (i < 0 || value.c_str()[i + 6] != '\"') {
            return false;
        }
        ssize_t j = value.find("\"", i + 7);
        if (j < 0) {
            return false;
        }

        mNonce.setTo(value, i + 7, j - i - 7);
    }

    return true;
}

static void H(const AString &s, AString *out) {
    nsresult rv;
    nsCOMPtr<nsICryptoHash> cryptoHash;

    out->clear();

    cryptoHash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
        LOGE(("Failed to create crypto hash."));
        return;
    }
    rv = cryptoHash->Init(nsICryptoHash::MD5);
    if (NS_FAILED(rv)) {
        LOGE(("Failed to init md5 hash."));
        return;
    }

    rv = cryptoHash->Update((unsigned char*)s.c_str(), s.size());
    if (NS_FAILED(rv)) {
        LOGE(("Failed to update md5 hash."));
        return;
    }

    nsAutoCString hashString;
    rv = cryptoHash->Finish(false, hashString);
    if (NS_FAILED(rv)) {
        LOGE(("Failed to finish md5 hash."));
        return;
    }

    const uint8_t kHashKeyLength = 16;
    if (hashString.Length() != kHashKeyLength) {
      LOGE(("Invalid hash key length."));
      return;
    }

    const char *key;
    hashString.GetData(&key);

    for (size_t i = 0; i < 16; ++i) {
        char nibble = key[i] >> 4;
        if (nibble <= 9) {
            nibble += '0';
        } else {
            nibble += 'a' - 10;
        }
        out->append(&nibble, 1);

        nibble = key[i] & 0x0f;
        if (nibble <= 9) {
            nibble += '0';
        } else {
            nibble += 'a' - 10;
        }
        out->append(&nibble, 1);
    }
}

static bool GetMethodAndURL(
        const AString &request, AString *method, AString *url) {
    ssize_t space1 = request.find(" ");
    if (space1 < 0) {
        return false;
    }

    ssize_t space2 = request.find(" ", space1 + 1);
    if (space2 < 0) {
        return false;
    }

    method->setTo(request, 0, space1);
    url->setTo(request, space1 + 1, space2 - space1);
    return true;
}

void ARTSPConnection::addAuthentication(AString *request) {
    if (mAuthType == NONE) {
        return;
    }

    // Find the boundary between headers and the body.
    ssize_t i = request->find("\r\n\r\n");
    if (i < 0) {
        LOGE("Failed to find the boundary between headers and the body");
        return;
    }

    if (mAuthType == BASIC) {
        AString tmp;
        tmp.append(mUser);
        tmp.append(":");
        tmp.append(mPass);

        AString out;
        encodeBase64(tmp.c_str(), tmp.size(), &out);

        AString fragment;
        fragment.append("Authorization: Basic ");
        fragment.append(out);
        fragment.append("\r\n");

        request->insert(fragment, i + 2);

        return;
    }

    CHECK_EQ((int)mAuthType, (int)DIGEST);

    AString method, url;
    if (!GetMethodAndURL(*request, &method, &url)) {
        LOGE("Fail to get method and url");
        return;
    }

    AString A1;
    A1.append(mUser);
    A1.append(":");
    A1.append("Streaming Server");
    A1.append(":");
    A1.append(mPass);

    AString A2;
    A2.append(method);
    A2.append(":");
    A2.append(url);

    AString HA1, HA2;
    H(A1, &HA1);
    H(A2, &HA2);

    AString tmp;
    tmp.append(HA1);
    tmp.append(":");
    tmp.append(mNonce);
    tmp.append(":");
    tmp.append(HA2);

    AString digest;
    H(tmp, &digest);

    AString fragment;
    fragment.append("Authorization: Digest ");
    fragment.append("nonce=\"");
    fragment.append(mNonce);
    fragment.append("\", ");
    fragment.append("username=\"");
    fragment.append(mUser);
    fragment.append("\", ");
    fragment.append("uri=\"");
    fragment.append(url);
    fragment.append("\", ");
    fragment.append("response=\"");
    fragment.append(digest);
    fragment.append("\"");
    fragment.append("\r\n");

    request->insert(fragment, i + 2);
}

void ARTSPConnection::addUserAgent(AString *request) const {
    // Find the boundary between headers and the body.
    ssize_t i = request->find("\r\n\r\n");
    if (i < 0) {
        LOGE("Failed to find the boundary between headers and the body");
    }

    request->insert(mUserAgent, i + 2);
}

void ARTSPConnection::closeSocket() {
    PR_Close(mSocket);
    mSocket = nullptr;
}

}  // namespace android
