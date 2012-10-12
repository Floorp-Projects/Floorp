/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CSFMEDIATERMINATION_H_
#define CSFMEDIATERMINATION_H_

// definitions shared by audio and video

typedef enum
{
    CodecRequestType_DECODEONLY,
    CodecRequestType_ENCODEONLY,
    CodecRequestType_FULLDUPLEX,
    CodecRequestType_IGNORE,

} CodecRequestType;

typedef enum
{
    EncryptionAlgorithm_NONE,
    EncryptionAlgorithm_AES_128_COUNTER

} EncryptionAlgorithm;

#if __cplusplus

namespace CSF
{
    class MediaTermination
    {
    public:
        virtual int  getCodecList( CodecRequestType requestType ) = 0;

        virtual int  rxAlloc    ( int groupId, int streamId, int requestedPort ) = 0;
        virtual int  rxOpen     ( int groupId, int streamId, int requestedPort, int listenIp, bool isMulticast ) = 0;
        virtual int  rxStart    ( int groupId, int streamId, int payloadType, int packPeriod, int localPort, int rfc2833PayloadType,
                                  EncryptionAlgorithm algorithm, unsigned char* key, int keyLen, unsigned char* salt, int saltLen, int mode, int party ) = 0;
        virtual void rxClose    ( int groupId, int streamId ) = 0;
        virtual void rxRelease  ( int groupId, int streamId, int port ) = 0;

        virtual int  txStart    ( int groupId, int streamId, int payloadType, int packPeriod, bool vad, short tos,
                                  char* remoteIpAddr, int remotePort, int rfc2833PayloadType, EncryptionAlgorithm algorithm,
                                  unsigned char* key, int keyLen, unsigned char* salt, int saltLen, int mode, int party ) = 0;
        virtual void txClose    ( int groupId, int streamId ) = 0;

        virtual void setLocalIP    ( const char* addr ) = 0;
        virtual void setMediaPorts ( int startPort, int endPort ) = 0;
        virtual void setDSCPValue ( int value ) = 0;
    };
} // namespace

#endif // __cplusplus

#endif /* CSFMEDIATERMINATION_H_ */
