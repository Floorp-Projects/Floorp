content = addMemberListToObject( {
    'mp4-basic' : {     initDataType:   'cenc',
                        audio : {   type:   'audio/mp4;codecs="mp4a.40.2"',
                                    path:   '/encrypted-media/content/audio_aac-lc_128k_dashinit.mp4' },
                        video : {   type:   'video/mp4;codecs="avc1.4d401e"',
                                    path:   '/encrypted-media/content/video_512x288_h264-360k_enc_dashinit.mp4' },
                        keys :  [ { kid: [ 0xad, 0x13, 0xf9, 0xea, 0x2b, 0xe6, 0x98, 0xb8, 0x75, 0xf5, 0x04, 0xa8, 0xe3, 0xcc, 0xea, 0x64 ],
                                    key: [ 0xbe, 0x7d, 0xf8, 0xa3, 0x66, 0x7a, 0x6a, 0x8f, 0xd5, 0x64, 0xd0, 0xed, 0x81, 0x33, 0x9a, 0x95 ],
                                    initDataType: 'cenc',
                                    initData: 'AAAAcXBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAAFEIARIQrRP56ivmmLh19QSo48zqZBoIY2FzdGxhYnMiKGV5SmhjM05sZEVsa0lqb2laVzFsTFhSbGMzUXRjMmx1WjJ4bEluMD0yB2RlZmF1bHQAAAMacHNzaAAAAACaBPB5mEBChquS5lvgiF+VAAAC+voCAAABAAEA8AI8AFcAUgBNAEgARQBBAEQARQBSACAAeABtAGwAbgBzAD0AIgBoAHQAdABwADoALwAvAHMAYwBoAGUAbQBhAHMALgBtAGkAYwByAG8AcwBvAGYAdAAuAGMAbwBtAC8ARABSAE0ALwAyADAAMAA3AC8AMAAzAC8AUABsAGEAeQBSAGUAYQBkAHkASABlAGEAZABlAHIAIgAgAHYAZQByAHMAaQBvAG4APQAiADQALgAwAC4AMAAuADAAIgA+ADwARABBAFQAQQA+ADwAUABSAE8AVABFAEMAVABJAE4ARgBPAD4APABLAEUAWQBMAEUATgA+ADEANgA8AC8ASwBFAFkATABFAE4APgA8AEEATABHAEkARAA+AEEARQBTAEMAVABSADwALwBBAEwARwBJAEQAPgA8AC8AUABSAE8AVABFAEMAVABJAE4ARgBPAD4APABLAEkARAA+ADYAdgBrAFQAcgBlAFkAcgB1AEoAaAAxADkAUQBTAG8ANAA4AHoAcQBaAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AagBZAEYATgBmADAAeQBmADQAaQBzAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAHAAbABhAHkAcgBlAGEAZAB5AC4AZABpAHIAZQBjAHQAdABhAHAAcwAuAG4AZQB0AC8AcAByAC8AcwB2AGMALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA/AFAAbABhAHkAUgBpAGcAaAB0AD0AMQAmAGEAbQBwADsAVQBzAGUAUwBpAG0AcABsAGUATgBvAG4AUABlAHIAcwBpAHMAdABlAG4AdABMAGkAYwBlAG4AcwBlAD0AMQA8AC8ATABBAF8AVQBSAEwAPgA8AC8ARABBAFQAQQA+ADwALwBXAFIATQBIAEUAQQBEAEUAUgA+AA==' } ]
                    },

    'mp4-av-multikey' : {
                        initDataType:   'cenc',
                        associatedInitData: true,       // indicates that initData for one key causes other keys to be returned as well
                        audio:  {   type:   'audio/mp4;codecs="mp4a.40.2"',
                                    path:   '/encrypted-media/content/audio_aac-lc_128k_enc_dashinit.mp4' },
                        video : {   type:   'video/mp4;codecs="avc1.4d401e"',
                                    path:   '/encrypted-media/content/video_512x288_h264-360k_enc_dashinit.mp4' },
                        keys :  [ { kid: [ 0xad, 0x13, 0xf9, 0xea, 0x2b, 0xe6, 0x98, 0xb8, 0x75, 0xf5, 0x04, 0xa8, 0xe3, 0xcc, 0xea, 0x64 ],
                                    key: [ 0xbe, 0x7d, 0xf8, 0xa3, 0x66, 0x7a, 0x6a, 0x8f, 0xd5, 0x64, 0xd0, 0xed, 0x81, 0x33, 0x9a, 0x95 ],
                                    initDataType: 'cenc',
                                    initData: 'AAAAcXBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAAFEIARIQrRP56ivmmLh19QSo48zqZBoIY2FzdGxhYnMiKGV5SmhjM05sZEVsa0lqb2laVzFsTFhSbGMzUXRjMmx1WjJ4bEluMD0yB2RlZmF1bHQAAAMacHNzaAAAAACaBPB5mEBChquS5lvgiF+VAAAC+voCAAABAAEA8AI8AFcAUgBNAEgARQBBAEQARQBSACAAeABtAGwAbgBzAD0AIgBoAHQAdABwADoALwAvAHMAYwBoAGUAbQBhAHMALgBtAGkAYwByAG8AcwBvAGYAdAAuAGMAbwBtAC8ARABSAE0ALwAyADAAMAA3AC8AMAAzAC8AUABsAGEAeQBSAGUAYQBkAHkASABlAGEAZABlAHIAIgAgAHYAZQByAHMAaQBvAG4APQAiADQALgAwAC4AMAAuADAAIgA+ADwARABBAFQAQQA+ADwAUABSAE8AVABFAEMAVABJAE4ARgBPAD4APABLAEUAWQBMAEUATgA+ADEANgA8AC8ASwBFAFkATABFAE4APgA8AEEATABHAEkARAA+AEEARQBTAEMAVABSADwALwBBAEwARwBJAEQAPgA8AC8AUABSAE8AVABFAEMAVABJAE4ARgBPAD4APABLAEkARAA+ADYAdgBrAFQAcgBlAFkAcgB1AEoAaAAxADkAUQBTAG8ANAA4AHoAcQBaAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AagBZAEYATgBmADAAeQBmADQAaQBzAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAHAAbABhAHkAcgBlAGEAZAB5AC4AZABpAHIAZQBjAHQAdABhAHAAcwAuAG4AZQB0AC8AcAByAC8AcwB2AGMALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA/AFAAbABhAHkAUgBpAGcAaAB0AD0AMQAmAGEAbQBwADsAVQBzAGUAUwBpAG0AcABsAGUATgBvAG4AUABlAHIAcwBpAHMAdABlAG4AdABMAGkAYwBlAG4AcwBlAD0AMQA8AC8ATABBAF8AVQBSAEwAPgA8AC8ARABBAFQAQQA+ADwALwBXAFIATQBIAEUAQQBEAEUAUgA+AA==' },
                                  { kid: [ 0x55, 0x8e, 0xe5, 0x41, 0xb9, 0x0a, 0xb2, 0xf3, 0x95, 0x0d, 0x00, 0xad, 0xe3, 0x76, 0x0d, 0x45 ],
                                    key: [ 0x91, 0x03, 0x92, 0x63, 0x01, 0x6d, 0xa6, 0x35, 0x77, 0x0d, 0x57, 0xdb, 0x92, 0xf9, 0x8b, 0xd0 ],
                                    initDataType : 'cenc',
                                    initData: 'AAAAcXBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAAFEIARIQVY7lQbkKsvOVDQCt43YNRRoIY2FzdGxhYnMiKGV5SmhjM05sZEVsa0lqb2laVzFsTFhSbGMzUXRjMmx1WjJ4bEluMD0yB2RlZmF1bHQAAAMacHNzaAAAAACaBPB5mEBChquS5lvgiF+VAAAC+voCAAABAAEA8AI8AFcAUgBNAEgARQBBAEQARQBSACAAeABtAGwAbgBzAD0AIgBoAHQAdABwADoALwAvAHMAYwBoAGUAbQBhAHMALgBtAGkAYwByAG8AcwBvAGYAdAAuAGMAbwBtAC8ARABSAE0ALwAyADAAMAA3AC8AMAAzAC8AUABsAGEAeQBSAGUAYQBkAHkASABlAGEAZABlAHIAIgAgAHYAZQByAHMAaQBvAG4APQAiADQALgAwAC4AMAAuADAAIgA+ADwARABBAFQAQQA+ADwAUABSAE8AVABFAEMAVABJAE4ARgBPAD4APABLAEUAWQBMAEUATgA+ADEANgA8AC8ASwBFAFkATABFAE4APgA8AEEATABHAEkARAA+AEEARQBTAEMAVABSADwALwBBAEwARwBJAEQAPgA8AC8AUABSAE8AVABFAEMAVABJAE4ARgBPAD4APABLAEkARAA+AFEAZQBXAE8AVgBRAHEANQA4ADcASwBWAEQAUQBDAHQANAAzAFkATgBSAFEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AWQBpAE8ALwAxADYATABzADkANgBFAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAHAAbABhAHkAcgBlAGEAZAB5AC4AZABpAHIAZQBjAHQAdABhAHAAcwAuAG4AZQB0AC8AcAByAC8AcwB2AGMALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA/AFAAbABhAHkAUgBpAGcAaAB0AD0AMQAmAGEAbQBwADsAVQBzAGUAUwBpAG0AcABsAGUATgBvAG4AUABlAHIAcwBpAHMAdABlAG4AdABMAGkAYwBlAG4AcwBlAD0AMQA8AC8ATABBAF8AVQBSAEwAPgA8AC8ARABBAFQAQQA+ADwALwBXAFIATQBIAEUAQQBEAEUAUgA+AA==' } ]
                    },

    'mp4-multikey' :    {   initDataType: 'cenc',
                            audio:  {   type:   'audio/mp4;codecs="mp4a.40.2"',
                                        path:   '/encrypted-media/content/audio_aac-lc_128k_2keys_bis.mp4' },
                            video:  {   type:   'video/mp4;codecs="avc1.4d401e"',
                                        path:   '/encrypted-media/content/video_512x288_h264-360k_enc_2keys_bis.mp4' },
                            keys: [ {   kid:    [ 0x36, 0x32, 0xe8, 0x34, 0x58, 0xfc, 0x34, 0x65, 0xd0, 0x4b, 0xb3, 0x9c, 0x51, 0x26, 0xef, 0xd0 ],
                                        key:    [ 0x00, 0xf9, 0x5b, 0xde, 0x44, 0x4e, 0x0c, 0xce, 0x0e, 0x37, 0xaf, 0xbb, 0x08, 0xa9, 0xbb, 0xad ],
                                        initDataType: 'cenc',
                                        initData: 'AAAAiXBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAAGkIARIQNjLoNFj8NGXQS7OcUSbv0BoIY2FzdGxhYnMiQGV5SmhjM05sZEVsa0lqb2laVzFsTFhSbGMzUXRiWFZzZEdraUxDSjJZWEpwWVc1MFNXUWlPaUpyWlhreEluMD0yB2RlZmF1bHQAAAMacHNzaAAAAACaBPB5mEBChquS5lvgiF+VAAAC+voCAAABAAEA8AI8AFcAUgBNAEgARQBBAEQARQBSACAAeABtAGwAbgBzAD0AIgBoAHQAdABwADoALwAvAHMAYwBoAGUAbQBhAHMALgBtAGkAYwByAG8AcwBvAGYAdAAuAGMAbwBtAC8ARABSAE0ALwAyADAAMAA3AC8AMAAzAC8AUABsAGEAeQBSAGUAYQBkAHkASABlAGEAZABlAHIAIgAgAHYAZQByAHMAaQBvAG4APQAiADQALgAwAC4AMAAuADAAIgA+ADwARABBAFQAQQA+ADwAUABSAE8AVABFAEMAVABJAE4ARgBPAD4APABLAEUAWQBMAEUATgA+ADEANgA8AC8ASwBFAFkATABFAE4APgA8AEEATABHAEkARAA+AEEARQBTAEMAVABSADwALwBBAEwARwBJAEQAPgA8AC8AUABSAE8AVABFAEMAVABJAE4ARgBPAD4APABLAEkARAA+AE4ATwBnAHkATgB2AHgAWQBaAFQAVABRAFMANwBPAGMAVQBTAGIAdgAwAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AcQBGAGQAMgB5AHIARQB3AHMALwBBAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAHAAbABhAHkAcgBlAGEAZAB5AC4AZABpAHIAZQBjAHQAdABhAHAAcwAuAG4AZQB0AC8AcAByAC8AcwB2AGMALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA/AFAAbABhAHkAUgBpAGcAaAB0AD0AMQAmAGEAbQBwADsAVQBzAGUAUwBpAG0AcABsAGUATgBvAG4AUABlAHIAcwBpAHMAdABlAG4AdABMAGkAYwBlAG4AcwBlAD0AMQA8AC8ATABBAF8AVQBSAEwAPgA8AC8ARABBAFQAQQA+ADwALwBXAFIATQBIAEUAQQBEAEUAUgA+AA==' },
                                    {   kid:    [ 0xdd, 0xc3, 0x72, 0x2d, 0x85, 0x3e, 0xfe, 0xaa, 0x09, 0x4e, 0x0d, 0xdd, 0xc0, 0x9f, 0x66, 0x98 ],
                                        key:    [ 0x1b, 0x0b, 0x74, 0xeb, 0x6e, 0x0f, 0x28, 0xe7, 0x0f, 0xea, 0xb0, 0xa2, 0x7d, 0x85, 0x8d, 0xc7 ],
                                        initDataType: 'cenc',
                                        initData: 'AAAAiXBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAAGkIARIQ3cNyLYU+/qoJTg3dwJ9mmBoIY2FzdGxhYnMiQGV5SmhjM05sZEVsa0lqb2laVzFsTFhSbGMzUXRiWFZzZEdraUxDSjJZWEpwWVc1MFNXUWlPaUpyWlhreUluMD0yB2RlZmF1bHQAAAMacHNzaAAAAACaBPB5mEBChquS5lvgiF+VAAAC+voCAAABAAEA8AI8AFcAUgBNAEgARQBBAEQARQBSACAAeABtAGwAbgBzAD0AIgBoAHQAdABwADoALwAvAHMAYwBoAGUAbQBhAHMALgBtAGkAYwByAG8AcwBvAGYAdAAuAGMAbwBtAC8ARABSAE0ALwAyADAAMAA3AC8AMAAzAC8AUABsAGEAeQBSAGUAYQBkAHkASABlAGEAZABlAHIAIgAgAHYAZQByAHMAaQBvAG4APQAiADQALgAwAC4AMAAuADAAIgA+ADwARABBAFQAQQA+ADwAUABSAE8AVABFAEMAVABJAE4ARgBPAD4APABLAEUAWQBMAEUATgA+ADEANgA8AC8ASwBFAFkATABFAE4APgA8AEEATABHAEkARAA+AEEARQBTAEMAVABSADwALwBBAEwARwBJAEQAPgA8AC8AUABSAE8AVABFAEMAVABJAE4ARgBPAD4APABLAEkARAA+AEwAWABMAEQAMwBUADYARgBxAHYANABKAFQAZwAzAGQAdwBKADkAbQBtAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AZABFAFMAMAArAFgAMwBSAFoANgBzAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAHAAbABhAHkAcgBlAGEAZAB5AC4AZABpAHIAZQBjAHQAdABhAHAAcwAuAG4AZQB0AC8AcAByAC8AcwB2AGMALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA/AFAAbABhAHkAUgBpAGcAaAB0AD0AMQAmAGEAbQBwADsAVQBzAGUAUwBpAG0AcABsAGUATgBvAG4AUABlAHIAcwBpAHMAdABlAG4AdABMAGkAYwBlAG4AcwBlAD0AMQA8AC8ATABBAF8AVQBSAEwAPgA8AC8ARABBAFQAQQA+ADwALwBXAFIATQBIAEUAQQBEAEUAUgA+AA==' } ]
                            },

    'webm' :        {   audio : {   type:   'audio/webm; codecs="opus"' },
                        video : {   type:   'video/webm; codecs="vp8"',
                                    path:   '/encrypted-media/content/test-encrypted.webm' },
                        keys :  [ { kid:    [48,49,50,51,52,53,54,55,56,57,48,49,50,51,52,53],
                                    key:    [0xeb, 0xdd, 0x62, 0xf1, 0x68, 0x14, 0xd2, 0x7b,
                                             0x68, 0xef, 0x12, 0x2a, 0xfc, 0xe4, 0xae, 0x3c ] } ]
                    },
    'webm-multikey' :
                    {   audio : {   type:   'audio/webm; codecs="opus"' },
                        video : {   type:   'video/webm; codecs="vp8"',
                                    path:   '/encrypted-media/content/test-encrypted-different-av-keys.webm' },
                        keys :  [ { kid:    [48,49,50,51,52,53,54,55,56,57,48,49,50,51,52,53],
                                    key:    [   0x7A, 0x7A, 0x62, 0xF1, 0x68, 0x14, 0xD2, 0x7B,
                                                0x68, 0xEF, 0x12, 0x2A, 0xFC, 0xE4, 0xAE, 0x0A ] },
                                  { kid:    [49,50,51,52,53,54,55,56,57,48,49,50,51,52,53,54],
                                    key:    [   0x30, 0x30, 0x62, 0xF1, 0x68, 0x14, 0xD2, 0x7B,
                                                0x68, 0xEF, 0x12, 0x2A, 0xFC, 0xE4, 0xAE, 0x0A ] } ]
                    },
} );

function addMemberListToObject( o )
{
    var items = [ ];
    for( var item in o )
    {
        if ( !o.hasOwnProperty( item ) ) continue;

        o[item].name = item;
        items.push( o[item] );
    }

    o._items = items;

    return o;
}

function getInitData( contentitem, initDataType )
{
    if (initDataType == 'webm') {
      return new Uint8Array( contentitem.keys[ 0 ].kid );       // WebM initData supports only a single key
    }

    if (initDataType == 'cenc') {

        var size = 36 + contentitem.keys.length * 16,
            kids = contentitem.keys.map( function( k ) { return k.kid; } );

        return new Uint8Array(Array.prototype.concat.call( [
            0x00, 0x00, size / 256, size % 256, // size
            0x70, 0x73, 0x73, 0x68, // 'pssh'
            0x01, // version = 1
            0x00, 0x00, 0x00, // flags
            0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02, // Common SystemID
            0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
            0x00, 0x00, 0x00, kids.length ], // key count ]
            Array.prototype.concat.apply( [], kids ),
          [ 0x00, 0x00, 0x00, 0x00 ]// datasize
        ));
    }
    if (initDataType == 'keyids') {

        return toUtf8( { kids: contentitem.keys.map( function( k ) { return base64urlEncode( new Uint8Array( k.kid ) ); } ) } );
    }
    throw 'initDataType ' + initDataType + ' not supported.';
}

function getSingleKeyInitData( kid, initDataType )
{
    if (initDataType == 'webm') {
      return new Uint8Array( kid );
    }

    if (initDataType == 'cenc') {

        var size = 52;

        return new Uint8Array(Array.prototype.concat.call( [
            0x00, 0x00, size / 256, size % 256, // size
            0x70, 0x73, 0x73, 0x68, // 'pssh'
            0x01, // version = 1
            0x00, 0x00, 0x00, // flags
            0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02, // Common SystemID
            0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
            0x00, 0x00, 0x00, 0x01 ], // key count ]
            kid,
          [ 0x00, 0x00, 0x00, 0x00 ]// datasize
        ));
    }
    if (initDataType == 'keyids') {

        return toUtf8( { kids: [ base64urlEncode( new Uint8Array( kid ) ) ] } );
    }
    throw 'initDataType ' + initDataType + ' not supported.';
}

function getMultikeyInitDatas( contentitem, initDataType )
{
    return contentitem.keys.map( function( k ) { return getSingleKeyInitData( k.kid, initDataType ); } );
}

function getProprietaryInitDatas( contentitem )
{
    return { initDataType: contentitem.initDataType,
             initDatas : contentitem.keys.filter( function( k ) { return k.initData; } )
                                    .map( function( k ) { return k.initData; } ) };
}

// Returns a promise that resolves to true or false depending on whether the content is supported with the key system and one of the initDataTypes
function isContentSupportedForInitDataTypes( keysystem, intiDataTypes, contentitem )
{
    var configuration = {   initDataTypes : intiDataTypes,
                            audioCapabilities: [ { contentType: contentitem.audio.type } ],
                            videoCapabilities: [ { contentType: contentitem.video.type } ]
                        };
    return navigator.requestMediaKeySystemAccess( keysystem, [ configuration ] )
    .then(  function( access ) {
                return { content: contentitem, supported: true, initDataTypes: access.getConfiguration().initDataTyes };
            },
            function() {
                return { content: contentitem, supported: false };
            } );
}

// Returns a promise that resolves to { content:, supported:, initDataTypes: } object
function isContentSupported( keysystem, contentitem )
{
    return isContentSupportedForInitDataTypes( keysystem, [ 'cenc', 'webm', 'keyids' ], contentitem );
}

// Returns a Promise resolving to an array of supported content for the key system
function getSupportedContent( keysystem )
{
    return Promise.all( content._items.map( isContentSupported.bind( null, keysystem ) ) ).
    then( function( results )
    {
        return results.filter( function( r ) { return r.supported; } ).map( function( r ) { return r.content; } );
    } );
}

// Returns a Promise resolving to an array of { content:, initDataType: } pairs for the key system
function getSupportedContentAndInitDataTypes( keysystem )
{
    return Promise.all( content._items.map( isContentSupported.bind( null, keysystem ) ) ).
    then( function( results )
    {
        return results.filter( function( r ) { return r.supported; } );
    } );
}

// gets a configuration object for provided piece of content
function getSimpleConfigurationForContent( contentitem )
{
    return {    initDataTypes: [ 'keyids', 'webm', 'cenc' ],
                audioCapabilities: [ { contentType: contentitem.audio.type } ],
                videoCapabilities: [ { contentType: contentitem.video.type } ] };
}