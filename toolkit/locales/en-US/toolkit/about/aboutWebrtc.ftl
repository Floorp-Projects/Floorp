# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### Localization for about:webrtc, a troubleshooting and diagnostic page
### for WebRTC calls. See https://developer.mozilla.org/en-US/docs/Web/API/WebRTC_API.

# The text "WebRTC" is a proper noun and should not be translated.
about-webrtc-document-title = WebRTC Internals

# "about:webrtc" is a internal browser URL and should not be
# translated. This string is used as a title for a file save dialog box.
about-webrtc-save-page-dialog-title = save about:webrtc as

## These labels are for a disclosure which contains the information for closed PeerConnection sections

about-webrtc-closed-peerconnection-disclosure-show-msg = Show Closed PeerConnections
about-webrtc-closed-peerconnection-disclosure-hide-msg = Hide Closed PeerConnections

## AEC is an abbreviation for Acoustic Echo Cancellation.

about-webrtc-aec-logging-msg-label = AEC Logging
about-webrtc-aec-logging-off-state-label = Start AEC Logging
about-webrtc-aec-logging-on-state-label = Stop AEC Logging
about-webrtc-aec-logging-toggled-on-state-msg = AEC logging active (speak with the caller for a few minutes and then stop the capture)
about-webrtc-aec-logging-unavailable-sandbox = The environment variable MOZ_DISABLE_CONTENT_SANDBOX=1 is required to export AEC logs. Only set this variable if you understand the possible risks.
# Variables:
#  $path (String) - The path to which the aec log file is saved.
about-webrtc-aec-logging-toggled-off-state-msg = Captured log files can be found in: { $path }

##

# The autorefresh checkbox causes a stats section to autorefresh its content when checked
about-webrtc-auto-refresh-label = Auto Refresh

# Determines the default state of the Auto Refresh check boxes
about-webrtc-auto-refresh-default-label = Auto Refresh By Default

# A button which forces a refresh of displayed statistics
about-webrtc-force-refresh-button = Refresh


# "PeerConnection" is a proper noun associated with the WebRTC module. "ID" is
# an abbreviation for Identifier. This string should not normally be translated
# and is used as a data label.
about-webrtc-peerconnection-id-label = PeerConnection ID:

# The number of DataChannels that a PeerConnection has opened
about-webrtc-data-channels-opened-label = Data Channels Opened:

# The number of once open DataChannels that a PeerConnection has closed
about-webrtc-data-channels-closed-label = Data Channels Closed:

## "SDP" is an abbreviation for Session Description Protocol, an IETF standard.
## See http://wikipedia.org/wiki/Session_Description_Protocol

about-webrtc-sdp-heading = SDP
about-webrtc-local-sdp-heading = Local SDP
about-webrtc-local-sdp-heading-offer = Local SDP (Offer)
about-webrtc-local-sdp-heading-answer = Local SDP (Answer)
about-webrtc-remote-sdp-heading = Remote SDP
about-webrtc-remote-sdp-heading-offer = Remote SDP (Offer)
about-webrtc-remote-sdp-heading-answer = Remote SDP (Answer)
about-webrtc-sdp-history-heading = SDP History
about-webrtc-sdp-parsing-errors-heading = SDP Parsing Errors

##

# "RTP" is an abbreviation for the Real-time Transport Protocol, an IETF
# specification, and should not normally be translated. "Stats" is an
# abbreviation for Statistics.
about-webrtc-rtp-stats-heading = RTP Stats

## "ICE" is an abbreviation for Interactive Connectivity Establishment, which
## is an IETF protocol, and should not normally be translated.

about-webrtc-ice-state = ICE State
# "Stats" is an abbreviation for Statistics.
about-webrtc-ice-stats-heading = ICE Stats
about-webrtc-ice-restart-count-label = ICE restarts:
about-webrtc-ice-rollback-count-label = ICE rollbacks:
about-webrtc-ice-pair-bytes-sent = Bytes sent:
about-webrtc-ice-pair-bytes-received = Bytes received:
about-webrtc-ice-component-id = Component ID

## These adjectives are used to label a line of statistics collected for a peer
## connection. The data represents either the local or remote end of the
## connection.

about-webrtc-type-local = Local
about-webrtc-type-remote = Remote

##

# This adjective is used to label a table column. Cells in this column contain
# the localized javascript string representation of "true" or are left blank.
about-webrtc-nominated = Nominated

# This adjective is used to label a table column. Cells in this column contain
# the localized javascript string representation of "true" or are left blank.
# This represents an attribute of an ICE candidate.
about-webrtc-selected = Selected

about-webrtc-save-page-label = Save Page
about-webrtc-enable-logging-label = Enable WebRTC Log Preset
about-webrtc-stats-heading = Session Statistics
about-webrtc-stats-clear = Clear History
about-webrtc-log-heading = Connection Log
about-webrtc-log-clear = Clear Log
about-webrtc-log-section-show-msg = Show Log
    .title = Click to expand this section
about-webrtc-log-section-hide-msg = Hide Log
    .title = Click to collapse this section
about-webrtc-copy-report-button = Copy Report
about-webrtc-copy-report-history-button = Copy Report History

## These are used to display a header for a PeerConnection.
## Variables:
##  $browser-id (Number) - A numeric id identifying the browser tab for the PeerConnection.
##  $id (String) - A globally unique identifier for the PeerConnection.
##  $url (String) - The url of the site which opened the PeerConnection.
##  $now (Date) - The JavaScript timestamp at the time the report was generated.

about-webrtc-connection-open = [ { $browser-id } | { $id } ] { $url } { $now }
about-webrtc-connection-closed = [ { $browser-id } | { $id } ] { $url } (closed) { $now }

## These are used to indicate what direction media is flowing.
## Variables:
##  $codecs - a list of media codecs

about-webrtc-short-send-receive-direction = Send / Receive: { $codecs }
about-webrtc-short-send-direction = Send: { $codecs }
about-webrtc-short-receive-direction = Receive: { $codecs }

##

about-webrtc-local-candidate = Local Candidate
about-webrtc-remote-candidate = Remote Candidate
about-webrtc-raw-candidates-heading = All Raw Candidates
about-webrtc-raw-local-candidate = Raw Local Candidate
about-webrtc-raw-remote-candidate = Raw Remote Candidate
about-webrtc-raw-cand-section-show-msg = Show Raw Candidates
    .title = Click to expand this section
about-webrtc-raw-cand-section-hide-msg = Hide Raw Candidates
    .title = Click to collapse this section
about-webrtc-priority = Priority
about-webrtc-fold-default-show-msg = Show Details
    .title = Click to expand this section
about-webrtc-fold-default-hide-msg = Hide Details
    .title = Click to collapse this section
about-webrtc-dropped-frames-label = Dropped frames:
about-webrtc-discarded-packets-label = Discarded packets:
about-webrtc-decoder-label = Decoder
about-webrtc-encoder-label = Encoder
about-webrtc-show-tab-label = Show tab
about-webrtc-current-framerate-label = Framerate
about-webrtc-width-px = Width (px)
about-webrtc-height-px = Height (px)
about-webrtc-consecutive-frames = Consecutive Frames
about-webrtc-time-elapsed = Time Elapsed (s)
about-webrtc-estimated-framerate = Estimated Framerate
about-webrtc-rotation-degrees = Rotation (degrees)
about-webrtc-first-frame-timestamp = First Frame Reception Timestamp
about-webrtc-last-frame-timestamp = Last Frame Reception Timestamp

## SSRCs are identifiers that represent endpoints in an RTP stream

# This is an SSRC on the local side of the connection that is receiving RTP
about-webrtc-local-receive-ssrc = Local Receiving SSRC
# This is an SSRC on the remote side of the connection that is sending RTP
about-webrtc-remote-send-ssrc = Remote Sending SSRC

## These are displayed on the button that shows or hides the
## PeerConnection configuration disclosure

about-webrtc-pc-configuration-show-msg = Show Configuration
about-webrtc-pc-configuration-hide-msg = Hide Configuration

##

# An option whose value will not be displayed but instead noted as having been
# provided
about-webrtc-configuration-element-provided = Provided

# An option whose value will not be displayed but instead noted as having not
# been provided
about-webrtc-configuration-element-not-provided = Not Provided

# The options set by the user in about:config that could impact a WebRTC call
about-webrtc-custom-webrtc-configuration-heading = User Set WebRTC Preferences

# Section header for estimated bandwidths of WebRTC media flows
about-webrtc-bandwidth-stats-heading = Estimated Bandwidth

# The ID of the MediaStreamTrack
about-webrtc-track-identifier = Track Identifier

# The estimated bandwidth available for sending WebRTC media in bytes per second
about-webrtc-send-bandwidth-bytes-sec = Send Bandwidth (bytes/sec)

# The estimated bandwidth available for receiving WebRTC media in bytes per second
about-webrtc-receive-bandwidth-bytes-sec = Receive Bandwidth (bytes/sec)

# Maximum number of bytes per second that will be padding zeros at the ends of packets
about-webrtc-max-padding-bytes-sec = Maximum Padding (bytes/sec)

# The amount of time inserted between packets to keep them spaced out
about-webrtc-pacer-delay-ms = Pacer Delay ms

# The amount of time it takes for a packet to travel from the local machine to the remote machine,
# and then have a packet return
about-webrtc-round-trip-time-ms = RTT ms

# This is a section heading for video frame statistics for a MediaStreamTrack.
# see https://developer.mozilla.org/en-US/docs/Web/API/MediaStreamTrack.
# Variables:
#   $track-identifier (String) - The unique identifier for the MediaStreamTrack.
about-webrtc-frame-stats-heading = Video Frame Statistics - MediaStreamTrack ID: { $track-identifier }

# This path is used for saving the about:webrtc page so it can be attached to
# bug reports.
# Variables:
#  $path (String) - The path to which the file is saved.
about-webrtc-save-page-complete-msg = Page saved to: { $path }

# This is the total number of frames encoded or decoded over an RTP stream.
# Variables:
#  $frames (Number) - The number of frames encoded or decoded.
about-webrtc-frames =
  { $frames ->
      [one] { $frames } frame
     *[other] { $frames } frames
  }

# This is the number of audio channels encoded or decoded over an RTP stream.
# Variables:
#  $channels (Number) - The number of channels encoded or decoded.
about-webrtc-channels =
  { $channels ->
      [one] { $channels } channel
     *[other] { $channels } channels
  }

# This is the total number of packets received on the PeerConnection.
# Variables:
#  $packets (Number) - The number of packets received.
about-webrtc-received-label =
  { $packets ->
      [one] Received { $packets } packet
     *[other] Received { $packets } packets
  }

# This is the total number of packets lost by the PeerConnection.
# Variables:
#  $packets (Number) - The number of packets lost.
about-webrtc-lost-label =
  { $packets ->
      [one] Lost { $packets } packet
     *[other] Lost { $packets } packets
  }

# This is the total number of packets sent by the PeerConnection.
# Variables:
#  $packets (Number) - The number of packets sent.
about-webrtc-sent-label =
  { $packets ->
      [one] Sent { $packets } packet
     *[other] Sent { $packets } packets
  }

# Jitter is the variance in the arrival time of packets.
# See: https://w3c.github.io/webrtc-stats/#dom-rtcreceivedrtpstreamstats-jitter
# Variables:
#   $jitter (Number) - The jitter.
about-webrtc-jitter-label = Jitter { $jitter }

# ICE candidates arriving after the remote answer arrives are considered trickled
# (an attribute of an ICE candidate). These are highlighted in the ICE stats
# table with light blue background.
about-webrtc-trickle-caption-msg = Trickled candidates (arriving after answer) are highlighted in blue

## "SDP" is an abbreviation for Session Description Protocol, an IETF standard.
## See http://wikipedia.org/wiki/Session_Description_Protocol

# This is used as a header for local SDP.
# Variables:
#  $timestamp (Number) - The Unix Epoch time at which the SDP was set.
about-webrtc-sdp-set-at-timestamp-local = Set Local SDP at timestamp { NUMBER($timestamp, useGrouping: "false") }

# This is used as a header for remote SDP.
# Variables:
#  $timestamp (Number) - The Unix Epoch time at which the SDP was set.
about-webrtc-sdp-set-at-timestamp-remote = Set Remote SDP at timestamp { NUMBER($timestamp, useGrouping: "false") }

# This is used as a header for an SDP section contained in two columns allowing for side-by-side comparisons.
# Variables:
#  $timestamp (Number) - The Unix Epoch time at which the SDP was set.
#  $relative-timestamp (Number) - The timestamp relative to the timestamp of the earliest received SDP.
about-webrtc-sdp-set-timestamp = Timestamp { NUMBER($timestamp, useGrouping: "false") } (+ { $relative-timestamp } ms)

## These are displayed on the button that shows or hides the SDP information disclosure

about-webrtc-show-msg-sdp = Show SDP
about-webrtc-hide-msg-sdp = Hide SDP

## These are displayed on the button that shows or hides the Media Context information disclosure.
## The Media Context is the set of preferences and detected capabilities that informs
## the negotiated CODEC settings.

about-webrtc-media-context-show-msg = Show Media Context
about-webrtc-media-context-hide-msg = Hide Media Context
about-webrtc-media-context-heading = Media Context

##
