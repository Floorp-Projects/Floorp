# Http3Session and streams

The HTTP/3 and QUIC protocol are implemented in the neqo library. Http3Session, Http3Steam, and Http3WebTransportStream are added to integrate the library into the existing necko code.

The following classes are necessary:
- HttpConnectionUDP - this is the object that is registered in nsHttpConnectionMgr and it is also used as an async listener to socket events (it implements nsIUDPSocketSyncListener)
- nsUDPSocket - represent a UDP socket and implements nsASocketHandler. nsSocketTransportService manages UDP and TCP sockets and calls the corresponding nsASocketHandler when the socket encounters an error or has data to be read, etc.
- NeqoHttp3Conn is a c++ object that maps to the rust object Http3Client.
- Http3Session manages NeqoHttp3Conn/Http3Client and provides bridge between the rust implementation and necko legacy code, i.e. HttpConnectionUDP and nsHttpTransaction.
- Http3Streams are used to map reading and writing from/into a nsHttpTransaction onto the NeqoHttp3Conn/Http3Client API (e.g. nsHttpTransaction::OnWriteSegment will call Http3Client::read_data). NeqoHttp3Conn is only accessed by Http3Sesson and NeqoHttp3Conn functions are exposed through Http3Session where needed.

```{mermaid}
graph TD
   A[HttpConnectionMgr] --> B[HttpConnectionUDP]
   B --> C[nsUDPSocket]
   C --> B
   D[nsSocketTransportService] --> C
   B --> E[NeqoHttp3Conn]
   B --> F[Http3Stream]
   F -->|row| B
   F --> G[nsHttpTransport]
   G --> B
   B --> G
```

## Interactions with sockets and driving neqo

As described in [this docs](https://github.com/mozilla/neqo/blob/main/neqo-http3/src/lib.rs), neqo does not create a socket,  it produces, i.e. encodes, data that should be sent as a payload in a UDP packet and consumes data received on the UDP socket. Therefore the necko is responsible for creating a socket and reading and writing data from/into the socket. Necko uses nsUDPSocket and nsSocketTransportService for this.
The UDP socket is constantly polled for reading. It is not polled for writing, we let QUIC control to not overload the network path and buffers.

When the UDP socket has an available packet, nsSocketTransportService will return from the polling function and call nsUDPSocket::OnSocketReady, which calls HttpConnectionUDP::OnPacketReceived, HttpConnectionUDP::RecvData and further Http3Session::RecvData. For writing data
HttpConnectionUDP::SendData is called which calls Http3Session::SendData.

Neqo needs an external timer. The timer is managed by Http3Session. When the timer expires HttpConnectionUDP::OnQuicTimeoutExpired is executed that calls Http3Session::ProcessOutputAndEvents.

HttpConnectionUDP::RecvData, HttpConnectionUDP::SendData or HttpConnectionUDP::OnQuicTimeoutExpired must be on the stack when we interact with neqo. The reason is that they are responsible for proper clean-up in case of an error. For example, if there is a slow reader that is ready to read, it will call Http3Session::TransactionHasDataToRecv to be registered in a list and HttpConnectionUDP::ForceRecv will be called that will call the same function chain as in the case a new packet is received, i.e. HttpConnectionUDP::RecvData and further Http3Session::RecvData. The other example is when a new HTTP transaction is added to the session, the transaction needs to send data. The transaction will be registered in a list and HttpConnectionUDP::ResumeSend will be called which further calls HttpConnectionUDP::SendData.

Http3Session holds a reference to a ConnectionHandler object which is a wrapper object around HttpConnectionUDP. The destructor of ConnectionHandler calls nsHttpHandler::ReclaimConnection which is responsible for removing the connection from nsHttpConnectionMgr.
HttpConnectionUDP::RecvData, HttpConnectionUDP::SendData or HttpConnectionUDP::OnQuicTimeoutExpired call HttpConnectionUDP::CloseTransaction which will cause Http3Session to remove the reference to the ConnectionHandler object. The ConnectionHandler object will be destroyed and nsHttpHandler::ReclaimConnection will be called.
This behavior is historical and it is also used for HTTP/2 and older versions. In the case of the older versions, nsHttpHandler::ReclaimConnection may actually reuse a connection instead of removing it from nsHttpConnectionMgr.

Three main neqo functions responsible for driving neqo are process_input, process_output, and  next_event. They are called by:
- Http3Session::ProcessInput,
- Http3Session::ProcesOutput and,
- Http3Session::ProcessEvents.

**ProcessInput**
In this function we take data from the UDP socket and call NeqoHttp3Conn::ProcessInput that maps to Http3Client::process_input. The packets are read from the socket until the socket buffer is empty.

**ProcessEvents**
This function process all available neqo events. It returns earlier only in case of a fatal error.
It calls NeqoHttp3Conn::GetEvent which maps to Http3Client::next_event.
The events and their handling will be explained below.

**ProcessOutput**
The function is called when necko has performed some action on neqo, e.g. new HTTP transaction is added, certificate verification is done, etc., or when the timer expires. In both cases, necko wants to check if neqo has data to send or change its state. This function calls NeqoHttp3Conn::ProcessOutput that maps to Http3Client::process_output. NeqoHttp3Conn::ProcessOutput may return a packet that is sent on the socket or a callback timeout. In the Http3Session::ProcessOutput function, NeqoHttp3Conn::ProcessOutput is called repeatedly and packets are sent until a callback timer is returned or a fatal error happens.

**Http3Session::RecvData** performs the following steps:
- ProcessSlowConsumers - explained below.
- ProcessInput - process new packets.
- ProcessEvents - look if there are new events
- ProcessOutput - look if we have new packets to send after packets arrive(e.g. sending ack) or due to event processing (e.g. a stream has been canceled).

**Http3Session::SendData** performed the following steps:
- Process (HTTP and WebTransport) streams that have data to write.
- ProcessOutput - look if there are new packets to be sent after streams have supplied data to neqo.


**Http3Session::ProcessOutputAndEvents** performed the  following steps:
- ProcessOutput - after a timeout most probably neqo will have data to retransmit or it will send a ping
- ProcessEvents - look if the state of the connection has changed, i.e. the connection timed out


## HTTP and WebTransport Streams reading data

The following diagram shows how data are read from an HTTP stream. The diagram for a WebTransport stream will be added later.

```{mermaid}
flowchart TD
   A1[nsUDPSocket::OnSocketReady] --> |HttpConnectionUDP::OnPacketReceived| C[HttpConnectionUDP]
   A[HttpConnectionUDP::ResumeRecv calls] --> C
   B[HttpConnectionUDPForceIO] --> |HttpConnectionUDP::RecvData| C
   C -->|1. Http3Session::RecvData| D[Http3Session]
   D --> |2. Http3Stream::WriteSegments|E[Http3Stream]
   E --> |3. nsHttpTransaction::WriteSegmentsAgain| F[nsHttpTransaction]
   F --> |4. nsPipeOutputStream::WriteSegments| G["nsPipeOutputStream"]
   G --> |5. nsHttpTransaction::WritePiipeSegnemt| F
   F --> |6. Http3Stream::OnWriteSegment| E
   E --> |"7. Return response headers or call Http3Session::ReadResponseData"|D
   D --> |8. NeqoHttp3Conn::ReadResponseDataReadResponseData| H[NeqoHHttp3Conn]

```

When there is a new packet carrying a stream data arriving on a QUIC connection nsUDPSocket::OnSocketReady will be called which will call Http3Session::RecvData. Http3Session::RecvData and ProcessInput will read the new packet from the socket and give it to neqo for processing. In the next step, ProcessEvent will be called which will have a DataReadable event and Http3Stream::WriteSegments will be called.
Http3Stream::WriteSegments calls nsHttpTransaction::WriteSegmentsAgain repeatedly until all data are read from the QUIC stream or until the pipe cannot accept more data. The latter can happen when listeners of an HTTP transaction or WebTransport stream are slow and are not able to read all data available on an HTTP3/WebTransport stream fast enough.

When the pipe cannot accept more data nsHttpTransaction will call nsPipeOutputStream::AsyncWait and wait for the nsHttpTransaction::OnOutputStreamReady callback. When nsHttpTransaction::OnOutputStreamReady is called,  Http3Stream/Session::TransactionHasDataToRecv is is executed with the following actions:
- the corresponding stream to a list(mSlowConsumersReadyForRead) and
- nsHttpConnection::ResumeRecv is called (i.e. it forces the same code path as when a socket has data to receive so that errors can be properly handled as explained previously).

These streams will be processed in ProcessSlowConsumers which is called by Http3Session::RecvData.

## HTTP and WebTransport Streams writing data

The following diagram shows how data are sent from an HTTP stream. The diagram for a WebTransport stream will be added later.

```{mermaid}
flowchart TD
   A[HttpConnectionUDP::ResumeSend calls] --> C[HttpConnectionUDP]
   B[HttpConnectionUDPForceIO] --> |HttpConnectionUDP::SendData| C
   C -->|1. Http3Session::SendData| D[Http3Session]
   D --> |2. Http3Stream::ReadSegments|E[Http3Stream]
   E --> |3. nsHttpTransaction::ReadSegmentsAgain| F[nsHttpTransaction]
   F --> |4. nsPipeInputStream::ReadSegments| G["nsPipeInputStream(Request stream)"]
   G --> |5. nsHttpTransaction::ReadRequestSegment| F
   F --> |6. Http3Stream::OnReadSegment| E
   E --> |7. Http3Session::TryActivating/SendRequestBody|D
   D --> |8. NeqoHttp3Conn::Fetch/SendRequestBody| H[NeqoHHttp3Conn]
```

When a nsHttpTransaction has been newly added to a transaction or when nsHttpTransaction has more data to write Http3Session::StreamReadyToWrite is called (in the latter case through Http3Session::TransactionHasDataToWrite) which performs the following actions:
- add the corresponding stream to a list(mReadyForWrite) and
- call HttpConnectionUDP::ResumeSend

The Http3Session::SendData function iterates through mReadyForWrite and calls Http3Stream::ReadSegments for each stream.

## Neqo events

For **HeaderReady** and **DataReadable** the Http3Stream::WriteSegments function of the corresponding stream is called. The code path shown in the flowchart above will call the nssHttpTransaction served by the stream to take headers and data.

**DataWritable** means that a stream could not accept more data earlier and that flow control now allows sending more data. Http3Sesson will mark the stream as writable(by calling Http3Session::StreamReadyToWrite) to verify if a stream wants to write more data.

**Reset** and **StopSending** events will be propagated to the stream and the stream will be closed.

**RequestsCreatable** events are posted when a QUIC connection could not accept new streams due to the flow control in the past and the stream flow control is increased and the streams are creatable again. Http3Session::ProcessPendingProcessPending will trigger the activation of the queued streams.

**AuthenticationNeeded** and **EchFallbackAuthenticationNeeded** are posted when a certificate verification is needed.


**ZeroRttRejected** is posted when zero RTT data was rejected.

**ResumptionToken** is posted when a new resumption token is available.

**ConnectionConnected**, **GoawayReceived**, **ConnectionClosing** and **ConnectionClosed** expose change in the connection state. Difference between **ConnectionClosing** and **ConnectionClosed** that after **ConnectionClosed** the connection can be immediately closed and after **ConnectionClosing** we will keep the connection object for a short time until **ConnectionClosed** event is received. During this period we will retransmit the closing frame if they are lost.

### WebTransport events

**Negotiated** - WebTransport is negotiated only after the HTTP/3 settings frame has been received from the server. At that point **Negotiated** event is posted to inform the application.

The **Session** event is posted when a WebTransport session is successfully negotiated.

The **SessionClosed** event is posted when a connection is closed gracefully or abruptly.

The **NewStream** is posted when a new stream has been opened by the peer.
