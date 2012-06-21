/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "../../source/BitstreamBuilder.h"
#include "../../source/BitstreamParser.h"

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <tchar.h>
#include <windows.h>

WebRtc_UWord32 BitRateBPS(WebRtc_UWord16 x )
{
    return (x & 0x3fff) * WebRtc_UWord32(pow(10.0f,(2 + (x >> 14))));
}

WebRtc_UWord16 BitRateBPSInv(WebRtc_UWord32 x )
{
    // 16383 0x3fff
    //     1 638 300    exp 0
    //    16 383 000    exp 1
    //   163 830 000    exp 2
    // 1 638 300 000    exp 3
    const float exp = log10(float(x>>14)) - 2;
    if(exp < 0.0)
    {
        return WebRtc_UWord16(x /100);
    }else if(exp < 1.0)
    {
        return 0x4000 + WebRtc_UWord16(x /1000);
    }else if(exp < 2.0)
    {
        return 0x8000 + WebRtc_UWord16(x /10000);
    }else if(exp < 3.0)
    {
        return 0xC000 + WebRtc_UWord16(x /100000);
    } else
    {
        assert(false);
        return 0;
    }
}


int _tmain(int argc, _TCHAR* argv[])
{
    WebRtc_UWord8 dataBuffer[128];
    BitstreamBuilder builder(dataBuffer, sizeof(dataBuffer));

    // test 1 to 4 bits
    builder.Add1Bit(1);
    builder.Add1Bit(0);
    builder.Add1Bit(1);

    builder.Add2Bits(1);
    builder.Add2Bits(2);
    builder.Add2Bits(3);

    builder.Add3Bits(1);
    builder.Add3Bits(3);
    builder.Add3Bits(7);

    builder.Add4Bits(1);
    builder.Add4Bits(5);
    builder.Add4Bits(15);

    assert(4 == builder.Length());

    BitstreamParser parser(dataBuffer, sizeof(dataBuffer));

    assert(1 == parser.Get1Bit());
    assert(0 == parser.Get1Bit());
    assert(1 == parser.Get1Bit());

    assert(1 == parser.Get2Bits());
    assert(2 == parser.Get2Bits());
    assert(3 == parser.Get2Bits());

    assert(1 == parser.Get3Bits());
    assert(3 == parser.Get3Bits());
    assert(7 == parser.Get3Bits());

    assert(1 == parser.Get4Bits());
    assert(5 == parser.Get4Bits());
    assert(15 == parser.Get4Bits());

    printf("Test of 1 to 4 bits done\n");

    // test 5 to 7 bits
    builder.Add5Bits(1);
    builder.Add5Bits(15);
    builder.Add5Bits(30);

    builder.Add6Bits(1);
    builder.Add6Bits(30);
    builder.Add6Bits(60);

    builder.Add7Bits(1);
    builder.Add7Bits(60);
    builder.Add7Bits(120);

    assert(1 == parser.Get5Bits());
    assert(15 == parser.Get5Bits());
    assert(30 == parser.Get5Bits());

    assert(1 == parser.Get6Bits());
    assert(30 == parser.Get6Bits());
    assert(60 == parser.Get6Bits());

    assert(1 == parser.Get7Bits());
    assert(60 == parser.Get7Bits());
    assert(120 == parser.Get7Bits());

    printf("Test of 5 to 7 bits done\n");

    builder.Add8Bits(1);
    builder.Add1Bit(1);
    builder.Add8Bits(255);
    builder.Add1Bit(0);
    builder.Add8Bits(127);
    builder.Add1Bit(1);
    builder.Add8Bits(60);
    builder.Add1Bit(0);
    builder.Add8Bits(30);
    builder.Add1Bit(1);
    builder.Add8Bits(120);
    builder.Add1Bit(0);
    builder.Add8Bits(160);
    builder.Add1Bit(1);
    builder.Add8Bits(180);

    assert(1 == parser.Get8Bits());
    assert(1 == parser.Get1Bit());
    assert(255 == parser.Get8Bits());
    assert(0 == parser.Get1Bit());
    assert(127 == parser.Get8Bits());
    assert(1 == parser.Get1Bit());
    assert(60 == parser.Get8Bits());
    assert(0 == parser.Get1Bit());
    assert(30 == parser.Get8Bits());
    assert(1 == parser.Get1Bit());
    assert(120 == parser.Get8Bits());
    assert(0 == parser.Get1Bit());
    assert(160 == parser.Get8Bits());
    assert(1 == parser.Get1Bit());
    assert(180 == parser.Get8Bits());

    printf("Test of 8 bits done\n");

    builder.Add16Bits(1);
    builder.Add1Bit(1);
    builder.Add16Bits(255);
    builder.Add1Bit(0);
    builder.Add16Bits(12756);
    builder.Add1Bit(1);
    builder.Add16Bits(60);
    builder.Add1Bit(0);
    builder.Add16Bits(30);
    builder.Add1Bit(1);
    builder.Add16Bits(30120);
    builder.Add1Bit(0);
    builder.Add16Bits(160);
    builder.Add1Bit(1);
    builder.Add16Bits(180);

    assert(1 == parser.Get16Bits());
    assert(1 == parser.Get1Bit());
    assert(255 == parser.Get16Bits());
    assert(0 == parser.Get1Bit());
    assert(12756 == parser.Get16Bits());
    assert(1 == parser.Get1Bit());
    assert(60 == parser.Get16Bits());
    assert(0 == parser.Get1Bit());
    assert(30 == parser.Get16Bits());
    assert(1 == parser.Get1Bit());
    assert(30120 == parser.Get16Bits());
    assert(0 == parser.Get1Bit());
    assert(160 == parser.Get16Bits());
    assert(1 == parser.Get1Bit());
    assert(180 == parser.Get16Bits());

    printf("Test of 16 bits done\n");

    builder.Add24Bits(1);
    builder.Add1Bit(1);
    builder.Add24Bits(255);
    builder.Add1Bit(0);
    builder.Add24Bits(12756);
    builder.Add1Bit(1);
    builder.Add24Bits(60);
    builder.Add1Bit(0);
    builder.Add24Bits(303333);
    builder.Add1Bit(1);
    builder.Add24Bits(30120);
    builder.Add1Bit(0);
    builder.Add24Bits(160);
    builder.Add1Bit(1);
    builder.Add24Bits(8018018);

    assert(1 == parser.Get24Bits());
    assert(1 == parser.Get1Bit());
    assert(255 == parser.Get24Bits());
    assert(0 == parser.Get1Bit());
    assert(12756 == parser.Get24Bits());
    assert(1 == parser.Get1Bit());
    assert(60 == parser.Get24Bits());
    assert(0 == parser.Get1Bit());
    assert(303333 == parser.Get24Bits());
    assert(1 == parser.Get1Bit());
    assert(30120 == parser.Get24Bits());
    assert(0 == parser.Get1Bit());
    assert(160 == parser.Get24Bits());
    assert(1 == parser.Get1Bit());
    assert(8018018 == parser.Get24Bits());

    printf("Test of 24 bits done\n");

    builder.Add32Bits(1);
    builder.Add1Bit(1);
    builder.Add32Bits(255);
    builder.Add1Bit(0);
    builder.Add32Bits(12756);
    builder.Add1Bit(1);
    builder.Add32Bits(60);
    builder.Add1Bit(0);
    builder.Add32Bits(303333);
    builder.Add1Bit(1);
    builder.Add32Bits(3012000012);
    builder.Add1Bit(0);
    builder.Add32Bits(1601601601);
    builder.Add1Bit(1);
    builder.Add32Bits(8018018);

    assert(1 == parser.Get32Bits());
    assert(1 == parser.Get1Bit());
    assert(255 == parser.Get32Bits());
    assert(0 == parser.Get1Bit());
    assert(12756 == parser.Get32Bits());
    assert(1 == parser.Get1Bit());
    assert(60 == parser.Get32Bits());
    assert(0 == parser.Get1Bit());
    assert(303333 == parser.Get32Bits());
    assert(1 == parser.Get1Bit());
    assert(3012000012 == parser.Get32Bits());
    assert(0 == parser.Get1Bit());
    assert(1601601601 == parser.Get32Bits());
    assert(1 == parser.Get1Bit());
    assert(8018018 == parser.Get32Bits());

    printf("Test of 32 bits done\n");

    builder.AddUE(1);
    builder.AddUE(4);
    builder.AddUE(9809706);
    builder.AddUE(2);
    builder.AddUE(15);
    builder.AddUE(16998);

    assert( 106 == builder.Length());

    assert(1 == parser.GetUE());
    assert(4 == parser.GetUE());
    assert(9809706 == parser.GetUE());
    assert(2 == parser.GetUE());
    assert(15 == parser.GetUE());
    assert(16998 == parser.GetUE());

    printf("Test UE bits done\n");

    BitstreamBuilder builderScalabilityInfo(dataBuffer, sizeof(dataBuffer));
    BitstreamParser parserScalabilityInfo(dataBuffer, sizeof(dataBuffer));

    const WebRtc_UWord8 numberOfLayers = 4;
    const WebRtc_UWord8 layerId[numberOfLayers] = {0,1,2,3};
    const WebRtc_UWord8 priorityId[numberOfLayers] = {0,1,2,3};
    const WebRtc_UWord8 discardableId[numberOfLayers] = {0,1,1,1};

    const WebRtc_UWord8 dependencyId[numberOfLayers]= {0,1,1,1};
    const WebRtc_UWord8 qualityId[numberOfLayers]= {0,0,0,1};
    const WebRtc_UWord8 temporalId[numberOfLayers]= {0,0,1,1};

    const WebRtc_UWord16 avgBitrate[numberOfLayers]= {BitRateBPSInv(100000),
                                                    BitRateBPSInv(200000),
                                                    BitRateBPSInv(400000),
                                                    BitRateBPSInv(800000)};

    // todo which one is the sum?
    const WebRtc_UWord16 maxBitrateLayer[numberOfLayers]= {BitRateBPSInv(150000),
                                                         BitRateBPSInv(300000),
                                                         BitRateBPSInv(500000),
                                                         BitRateBPSInv(900000)};

    const WebRtc_UWord16 maxBitrateLayerRepresentation[numberOfLayers] = {BitRateBPSInv(150000),
                                                                        BitRateBPSInv(450000),
                                                                        BitRateBPSInv(950000),
                                                                        BitRateBPSInv(1850000)};

    assert( 16300 == BitRateBPS(BitRateBPSInv(16383)));
    assert( 163800 == BitRateBPS(BitRateBPSInv(163830)));
    assert( 1638300 == BitRateBPS(BitRateBPSInv(1638300)));
    assert( 1638000 == BitRateBPS(BitRateBPSInv(1638400)));

    assert( 18500 == BitRateBPS(BitRateBPSInv(18500)));
    assert( 185000 == BitRateBPS(BitRateBPSInv(185000)));
    assert( 1850000 == BitRateBPS(BitRateBPSInv(1850000)));
    assert( 18500000 == BitRateBPS(BitRateBPSInv(18500000)));
    assert( 185000000 == BitRateBPS(BitRateBPSInv(185000000)));

    const WebRtc_UWord16 maxBitrareCalcWindow[numberOfLayers] = {200, 200,200,200};// in 1/100 of second

    builderScalabilityInfo.Add1Bit(0);  // temporal_id_nesting_flag
    builderScalabilityInfo.Add1Bit(0);    // priority_layer_info_present_flag
    builderScalabilityInfo.Add1Bit(0);  // priority_id_setting_flag

    builderScalabilityInfo.AddUE(numberOfLayers-1);

    for(int i = 0; i<= numberOfLayers-1; i++)
    {
        builderScalabilityInfo.AddUE(layerId[i]);
        builderScalabilityInfo.Add6Bits(priorityId[i]);
        builderScalabilityInfo.Add1Bit(discardableId[i]);
        builderScalabilityInfo.Add3Bits(dependencyId[i]);
        builderScalabilityInfo.Add4Bits(qualityId[i]);
        builderScalabilityInfo.Add3Bits(temporalId[i]);

        builderScalabilityInfo.Add1Bit(0);
        builderScalabilityInfo.Add1Bit(0);
        builderScalabilityInfo.Add1Bit(0);
        builderScalabilityInfo.Add1Bit(0);

        builderScalabilityInfo.Add1Bit(1);    // bitrate_info_present_flag

        builderScalabilityInfo.Add1Bit(0);
        builderScalabilityInfo.Add1Bit(0);
        builderScalabilityInfo.Add1Bit(0);
        builderScalabilityInfo.Add1Bit(0);
        builderScalabilityInfo.Add1Bit(0);
        builderScalabilityInfo.Add1Bit(0);
        builderScalabilityInfo.Add1Bit(0);
        builderScalabilityInfo.Add1Bit(0);

        builderScalabilityInfo.Add16Bits(avgBitrate[i]);
        builderScalabilityInfo.Add16Bits(maxBitrateLayer[i]);
        builderScalabilityInfo.Add16Bits(maxBitrateLayerRepresentation[i]);
        builderScalabilityInfo.Add16Bits(maxBitrareCalcWindow[i]);

        builderScalabilityInfo.AddUE(0); // layer_dependency_info_src_layer_id_delta
        builderScalabilityInfo.AddUE(0); // parameter_sets_info_src_layer_id_delta
    }

    printf("Test builderScalabilityInfo done\n");

    // Scalability Info parser
    parserScalabilityInfo.Get1Bit(); // not used in futher parsing
    const WebRtc_UWord8 priority_layer_info_present = parserScalabilityInfo.Get1Bit();
    const WebRtc_UWord8 priority_id_setting_flag = parserScalabilityInfo.Get1Bit();

    WebRtc_UWord32 numberOfLayersMinusOne = parserScalabilityInfo.GetUE();
    for(WebRtc_UWord32 j = 0; j<= numberOfLayersMinusOne; j++)
    {
        parserScalabilityInfo.GetUE();
        parserScalabilityInfo.Get6Bits();
        parserScalabilityInfo.Get1Bit();
        parserScalabilityInfo.Get3Bits();
        parserScalabilityInfo.Get4Bits();
        parserScalabilityInfo.Get3Bits();

        const WebRtc_UWord8 sub_pic_layer_flag = parserScalabilityInfo.Get1Bit();
        const WebRtc_UWord8 sub_region_layer_flag = parserScalabilityInfo.Get1Bit();
        const WebRtc_UWord8 iroi_division_info_present_flag = parserScalabilityInfo.Get1Bit();
        const WebRtc_UWord8 profile_level_info_present_flag = parserScalabilityInfo.Get1Bit();
        const WebRtc_UWord8 bitrate_info_present_flag = parserScalabilityInfo.Get1Bit();
        const WebRtc_UWord8 frm_rate_info_present_flag = parserScalabilityInfo.Get1Bit();
        const WebRtc_UWord8 frm_size_info_present_flag = parserScalabilityInfo.Get1Bit();
        const WebRtc_UWord8 layer_dependency_info_present_flag = parserScalabilityInfo.Get1Bit();
        const WebRtc_UWord8 parameter_sets_info_present_flag = parserScalabilityInfo.Get1Bit();
        const WebRtc_UWord8 bitstream_restriction_info_present_flag = parserScalabilityInfo.Get1Bit();
        const WebRtc_UWord8 exact_inter_layer_pred_flag = parserScalabilityInfo.Get1Bit();  // not used in futher parsing

        if(sub_pic_layer_flag || iroi_division_info_present_flag)
        {
            parserScalabilityInfo.Get1Bit();
        }
        const WebRtc_UWord8 layer_conversion_flag = parserScalabilityInfo.Get1Bit();
        const WebRtc_UWord8 layer_output_flag = parserScalabilityInfo.Get1Bit();  // not used in futher parsing

        if(profile_level_info_present_flag)
        {
            parserScalabilityInfo.Get24Bits();
        }
        if(bitrate_info_present_flag)
        {
            // this is what we want
            assert(avgBitrate[j] == parserScalabilityInfo.Get16Bits());
            assert(maxBitrateLayer[j] == parserScalabilityInfo.Get16Bits());
            assert(maxBitrateLayerRepresentation[j] == parserScalabilityInfo.Get16Bits());
            assert(maxBitrareCalcWindow[j] == parserScalabilityInfo.Get16Bits());
        }else
        {
            assert(false);
        }
        if(frm_rate_info_present_flag)
        {
            parserScalabilityInfo.Get2Bits();
            parserScalabilityInfo.Get16Bits();
        }
        if(frm_size_info_present_flag || iroi_division_info_present_flag)
        {
            parserScalabilityInfo.GetUE();
            parserScalabilityInfo.GetUE();
        }
        if(sub_region_layer_flag)
        {
            parserScalabilityInfo.GetUE();
            if(parserScalabilityInfo.Get1Bit())
            {
                parserScalabilityInfo.Get16Bits();
                parserScalabilityInfo.Get16Bits();
                parserScalabilityInfo.Get16Bits();
                parserScalabilityInfo.Get16Bits();
            }
        }
        if(sub_pic_layer_flag)
        {
            parserScalabilityInfo.GetUE();
        }
        if(iroi_division_info_present_flag)
        {
            if(parserScalabilityInfo.Get1Bit())
            {
                parserScalabilityInfo.GetUE();
                parserScalabilityInfo.GetUE();
            }else
            {
                const WebRtc_UWord32 numRoisMinusOne = parserScalabilityInfo.GetUE();
                for(WebRtc_UWord32 k = 0; k <= numRoisMinusOne; k++)
                {
                    parserScalabilityInfo.GetUE();
                    parserScalabilityInfo.GetUE();
                    parserScalabilityInfo.GetUE();
                }
            }
        }
        if(layer_dependency_info_present_flag)
        {
            const WebRtc_UWord32 numDirectlyDependentLayers = parserScalabilityInfo.GetUE();
            for(WebRtc_UWord32 k = 0; k < numDirectlyDependentLayers; k++)
            {
                parserScalabilityInfo.GetUE();
            }
        } else
        {
            parserScalabilityInfo.GetUE();
        }
        if(parameter_sets_info_present_flag)
        {
            const WebRtc_UWord32 numSeqParameterSetMinusOne = parserScalabilityInfo.GetUE();
            for(WebRtc_UWord32 k = 0; k <= numSeqParameterSetMinusOne; k++)
            {
                parserScalabilityInfo.GetUE();
            }
            const WebRtc_UWord32 numSubsetSeqParameterSetMinusOne = parserScalabilityInfo.GetUE();
            for(WebRtc_UWord32 l = 0; l <= numSubsetSeqParameterSetMinusOne; l++)
            {
                parserScalabilityInfo.GetUE();
            }
            const WebRtc_UWord32 numPicParameterSetMinusOne = parserScalabilityInfo.GetUE();
            for(WebRtc_UWord32 m = 0; m <= numPicParameterSetMinusOne; m++)
            {
                parserScalabilityInfo.GetUE();
            }
        }else
        {
            parserScalabilityInfo.GetUE();
        }
        if(bitstream_restriction_info_present_flag)
        {
            parserScalabilityInfo.Get1Bit();
            parserScalabilityInfo.GetUE();
            parserScalabilityInfo.GetUE();
            parserScalabilityInfo.GetUE();
            parserScalabilityInfo.GetUE();
            parserScalabilityInfo.GetUE();
            parserScalabilityInfo.GetUE();
        }
        if(layer_conversion_flag)
        {
            parserScalabilityInfo.GetUE();
            for(WebRtc_UWord32 k = 0; k <2;k++)
            {
                if(parserScalabilityInfo.Get1Bit())
                {
                    parserScalabilityInfo.Get24Bits();
                    parserScalabilityInfo.Get16Bits();
                    parserScalabilityInfo.Get16Bits();
                }
            }
        }
    }
    if(priority_layer_info_present)
    {
        const WebRtc_UWord32 prNumDidMinusOne = parserScalabilityInfo.GetUE();
        for(WebRtc_UWord32 k = 0; k <= prNumDidMinusOne;k++)
        {
            parserScalabilityInfo.Get3Bits();
            const WebRtc_UWord32 prNumMinusOne = parserScalabilityInfo.GetUE();
            for(WebRtc_UWord32 l = 0; l <= prNumMinusOne; l++)
            {
                parserScalabilityInfo.GetUE();
                parserScalabilityInfo.Get24Bits();
                parserScalabilityInfo.Get16Bits();
                parserScalabilityInfo.Get16Bits();
            }
        }
    }
    if(priority_id_setting_flag)
    {
        WebRtc_UWord8 priorityIdSettingUri;
        WebRtc_UWord32 priorityIdSettingUriIdx = 0;
        do
        {
            priorityIdSettingUri = parserScalabilityInfo.Get8Bits();
        } while (priorityIdSettingUri != 0);
    }
    printf("Test parserScalabilityInfo done\n");

    printf("\nAPI test of parser for ScalabilityInfo done\n");

    ::Sleep(5000);
}
