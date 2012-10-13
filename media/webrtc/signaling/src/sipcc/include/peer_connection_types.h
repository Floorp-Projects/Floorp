/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _PEER_CONNECTION_TYPES_H_
#define _PEER_CONNECTION_TYPES_H_

#define MAX_TRACKS 8

enum StatusCode {
    PC_OK = 0,
    PC_INVALID_HINTS,
    PC_INVALID_OFFER,
    PC_INVALID_REMOTE_SDP,
    PC_INVALID_LOCAL_SDP,
    PC_NO_OBSERVER,
    PC_SDPCHANGED,
    PC_SETLOCALDESCERROR,
    PC_SETREMOTEDESCERROR,
    PC_INTERNAL_ERROR
};

typedef struct MediaTrack {
    unsigned int    media_stream_track_id;
    int             video;
} MediaTrack;

typedef struct MediaStreamTable {
    unsigned int    media_stream_id;
    unsigned int    num_tracks;
    MediaTrack      track[MAX_TRACKS];
} MediaStreamTable;


#endif /*_PEER_CONNECTION_TYPES_H_*/
