/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AV1_ENCODER_PUSTATS_H_
#define AV1_ENCODER_PUSTATS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "av1/encoder/ml.h"

#define NUM_FEATURES 20
#define NUM_HIDDEN_LAYERS 2
#define HIDDEN_LAYERS_0_NODES 10
#define HIDDEN_LAYERS_1_NODES 10
#define LOGITS_NODES 1

static const float
    av1_pustats_rate_hiddenlayer_0_kernel[NUM_FEATURES *
                                          HIDDEN_LAYERS_0_NODES] = {
      13.8498f,  19.6630f,   13.3036f,  5.2448f,   -18.0270f,  21.6671f,
      -0.2135f,  -0.0060f,   0.1211f,   -0.3549f,  -0.3550f,   0.0190f,
      0.0167f,   -0.1192f,   0.2003f,   8.6663f,   32.0264f,   9.9558f,
      9.0935f,   -110.4994f, 51.8056f,  64.8041f,  58.5392f,   53.0189f,
      -61.6300f, 4.7540f,    -0.0140f,  0.0185f,   -15.8050f,  0.0790f,
      0.0707f,   0.0784f,    0.0766f,   -0.3030f,  0.0392f,    49.3312f,
      63.3326f,  61.4025f,   54.2723f,  -62.2769f, -147.1736f, -84.9432f,
      -82.5422f, -70.4857f,  46.7622f,  -1.0285f,  -0.4809f,   0.0068f,
      1.0888f,   -0.0515f,   -0.0384f,  -0.0232f,  -0.0396f,   0.2429f,
      0.2040f,   -144.4016f, -88.0868f, -80.3134f, -70.6685f,  66.8528f,
      -53.8097f, -45.4011f,  -52.8680f, -58.7226f, 99.7830f,   2.3728f,
      0.0229f,   0.0002f,    -0.3288f,  -0.0563f,  -0.0550f,   -0.0552f,
      -0.0563f,  0.2214f,    0.0139f,   -60.8965f, -45.5251f,  -50.4188f,
      -51.5623f, 85.7369f,   77.3415f,  47.4930f,  53.8120f,   58.2311f,
      -45.9650f, -2.4938f,   0.1639f,   -0.5270f,  -75.4622f,  -0.0026f,
      0.0031f,   0.0047f,    0.0015f,   0.0092f,   0.0654f,    75.6402f,
      54.7447f,  54.8156f,   52.6834f,  -9.1246f,  -34.0108f,  -35.6423f,
      -34.2911f, -38.5444f,  72.1123f,  10.9750f,  -0.1595f,   0.1983f,
      22.5724f,  -0.0556f,   -0.0618f,  -0.0571f,  -0.0608f,   0.2439f,
      -0.0805f,  -32.5107f,  -28.9688f, -33.7284f, -48.1365f,  61.5297f,
      39.2492f,  -35.1928f,  -11.5000f, 7.7038f,   -94.2469f,  13.5586f,
      0.7541f,   0.0105f,    4.4041f,   0.1799f,   0.1339f,    0.1567f,
      -0.6668f,  -0.7384f,   0.2185f,   17.1700f,  -26.4601f,  -1.8970f,
      38.9635f,  -30.1916f,  31.8139f,  14.6157f,  10.0565f,   3.3340f,
      -40.6985f, -2.1186f,   0.0116f,   0.0962f,   0.7115f,    -1.4071f,
      -1.3701f,  -1.4728f,   -1.3404f,  -1.7286f,  5.5632f,    28.4998f,
      5.4087f,   16.2668f,   11.8693f,  -39.4153f, 106.3281f,  38.3075f,
      39.4933f,  47.3805f,   -15.0514f, -21.2421f, -0.2358f,   -0.0024f,
      0.3505f,   -0.0429f,   -0.0377f,  -0.0322f,  -0.0344f,   0.2020f,
      0.1417f,   99.6711f,   35.3896f,  43.1117f,  59.8879f,   -17.8250f,
      -16.6976f, 18.5100f,   6.3383f,   25.3020f,  -55.8824f,  25.1027f,
      -0.9926f,  -0.0738f,   -1.4892f,  0.0269f,   -0.0051f,   -5.8168f,
      -0.0579f,  -0.1500f,   0.7224f,   8.3066f,   -3.8805f,   -12.1482f,
      14.3492f,  -20.8118f,
    };

static const float av1_pustats_rate_hiddenlayer_0_bias[HIDDEN_LAYERS_0_NODES] =
    {
      17.6566f,  62.2217f, -107.2644f, -56.2255f, 68.2252f,
      -37.5662f, 9.587f,   18.5206f,   69.6873f,  4.3903f,
    };

static const float
    av1_pustats_rate_hiddenlayer_1_kernel[HIDDEN_LAYERS_0_NODES *
                                          HIDDEN_LAYERS_1_NODES] = {
      -0.0494f, 0.3505f,   -0.0461f, -1.3451f, 0.0198f,  -0.0746f, -0.2217f,
      -0.9525f, 0.0633f,   -0.0737f, -0.3568f, 1.8569f,  -0.0189f, -1.8269f,
      0.6281f,  -1.3266f,  -0.9202f, 2.8978f,  -0.6437f, -0.8709f, -1.5066f,
      -1.0582f, -1.9509f,  -0.0417f, -0.1315f, -0.3368f, 0.0014f,  -0.5734f,
      -1.4640f, -1.6042f,  3.3911f,  -1.6815f, -1.9026f, -4.8702f, -0.1012f,
      -1.4517f, -3.2156f,  0.8448f,  0.2331f,  -0.1593f, 2.6627f,  -0.8451f,
      -1.7382f, 0.9303f,   2.3003f,  -0.0659f, 0.5772f,  0.4253f,  0.2083f,
      0.3649f,  -0.9198f,  -0.2183f, -0.5381f, -1.0831f, 2.0359f,  0.0040f,
      -0.0871f, -0.1715f,  2.2453f,  0.5099f,  -0.5900f, -0.6313f, -1.3028f,
      -1.7257f, 1.4130f,   -0.7189f, -0.4336f, 1.9266f,  1.7495f,  -0.3321f,
      0.2827f,  0.4015f,   -0.5044f, -1.0420f, -0.1258f, -0.0342f, -0.1190f,
      -3.1263f, 0.7485f,   -0.3161f, -0.2224f, 2.5533f,  -0.2121f, -1.3389f,
      0.5556f,  -0.9407f,  -0.7456f, 1.4137f,  -0.0353f, -0.0521f, 2.4382f,
      0.1493f,  -11.5631f, -1.6178f, 3.5538f,  -3.6538f, -0.5972f, -3.0038f,
      -2.1640f, 0.5754f,
    };

static const float av1_pustats_rate_hiddenlayer_1_bias[HIDDEN_LAYERS_1_NODES] =
    {
      69.1995f, 41.7369f, -1.4885f, -35.785f, 26.1678f,
      58.4472f, 36.2223f, 66.327f,  50.8867f, 2.8306f,
    };

static const float
    av1_pustats_rate_logits_kernel[HIDDEN_LAYERS_1_NODES * LOGITS_NODES] = {
      1.811f,  0.9009f, 0.0694f, -0.9985f, -0.039f,
      0.2076f, 0.5643f, 0.5408f, 0.6071f,  0.277f,
    };

static const float av1_pustats_rate_logits_bias[LOGITS_NODES] = {
  39.5529f,
};

static const NN_CONFIG av1_pustats_rate_nnconfig = {
  NUM_FEATURES,                                      // num_inputs
  LOGITS_NODES,                                      // num_outputs
  NUM_HIDDEN_LAYERS,                                 // num_hidden_layers
  { HIDDEN_LAYERS_0_NODES, HIDDEN_LAYERS_1_NODES },  // num_hidden_nodes
  {
      av1_pustats_rate_hiddenlayer_0_kernel,
      av1_pustats_rate_hiddenlayer_1_kernel,
      av1_pustats_rate_logits_kernel,
  },
  {
      av1_pustats_rate_hiddenlayer_0_bias,
      av1_pustats_rate_hiddenlayer_1_bias,
      av1_pustats_rate_logits_bias,
  },
};

static const float
    av1_pustats_dist_hiddenlayer_0_kernel[NUM_FEATURES *
                                          HIDDEN_LAYERS_0_NODES] = {
      -39.0787f,  -212.9998f, -174.2088f, -264.1454f, 292.7151f,  -60.8750f,
      -5.9915f,   0.0712f,    -60.2312f,  -0.2020f,   -0.2135f,   -0.1663f,
      -0.0711f,   0.2267f,    0.9152f,    -36.1294f,  -159.9320f, -222.9809f,
      -270.2556f, 300.7162f,  159.9224f,  -172.5735f, -7.6852f,   54.3985f,
      110.6721f,  19.2907f,   -15.1039f,  -0.0457f,   0.3289f,    0.4529f,
      -8.2222f,   1.3213f,    -0.8378f,   -0.2605f,   3.9600f,    17.3407f,
      113.1116f,  34.6326f,   11.6688f,   109.3541f,  240.8123f,  45.0615f,
      80.7443f,   39.2500f,   -21.0931f,  -27.1989f,  -0.4264f,   -0.1345f,
      1.6269f,    -0.0716f,   0.0989f,    -0.1382f,   0.0248f,    0.0913f,
      4.3903f,    244.1014f,  32.2567f,   58.6171f,   62.2273f,   -2.8647f,
      -227.5659f, 16.0031f,   -70.5256f,  23.8071f,   290.7356f,  13.6094f,
      -2.1842f,   0.0104f,    -2.8760f,   0.3708f,    0.8501f,    -3.2964f,
      -0.2088f,   -0.4474f,   1.2248f,    40.5180f,   -130.7891f, -188.1583f,
      -174.0906f, 205.9622f,  0.3425f,    0.2531f,    0.2822f,    0.0488f,
      0.1416f,    -0.0433f,   -0.1195f,   -0.0413f,   -0.0708f,   -0.0787f,
      -0.0889f,   -0.4022f,   -0.5055f,   -0.4715f,   0.2315f,    0.1021f,
      -0.3676f,   -0.3499f,   -0.0715f,   0.1913f,    205.7521f,  125.2265f,
      92.0640f,   77.5566f,   -164.4280f, -19.3715f,  -0.1346f,   -0.4060f,
      0.5042f,    -0.2395f,   -0.1329f,   -0.1397f,   0.2175f,    0.2895f,
      5.5019f,    198.9799f,  114.0018f,  94.9015f,   86.8434f,   -183.4237f,
      121.5626f,  94.8945f,   65.0803f,   93.6487f,   -346.5279f, -47.6168f,
      0.0633f,    0.0135f,    -0.0692f,   -0.1015f,   -0.1146f,   -0.1341f,
      -0.1175f,   0.4186f,    0.1505f,    130.7402f,  107.8443f,  62.8497f,
      65.3501f,   -312.7407f, 282.8321f,  98.1531f,   75.6648f,   25.8733f,
      -176.9298f, -37.2695f,  -0.3760f,   0.0017f,    0.1030f,    -0.1483f,
      0.0787f,    -0.0962f,   0.4109f,    -0.2292f,   9.1681f,    274.3607f,
      60.9538f,   75.9405f,   68.3776f,   -167.3098f, -335.1045f, -69.2583f,
      -76.3441f,  -16.5793f,  218.5244f,  28.2405f,   0.9169f,    -0.0026f,
      -0.8077f,   -1.5756f,   -0.0804f,   0.1404f,    1.2656f,    0.0272f,
      -0.2529f,   -340.8659f, -112.7778f, -58.3890f,  -4.1224f,   108.1709f,
      -180.7382f, -93.7114f,  -77.8686f,  -131.8134f, 353.3893f,  4.8233f,
      0.0205f,    0.0000f,    -1.1654f,   -0.0161f,   -0.0255f,   -0.0358f,
      -0.0412f,   0.1103f,    0.1041f,    -188.9934f, -110.1792f, -88.6301f,
      -93.7226f,  336.9746f,
    };

static const float av1_pustats_dist_hiddenlayer_0_bias[HIDDEN_LAYERS_0_NODES] =
    { -175.6918f, 43.4519f,  154.196f, -81.1015f,  -0.0758f,
      136.5695f,  110.8713f, 142.029f, -153.0901f, -145.2688f };

static const float
    av1_pustats_dist_hiddenlayer_1_kernel[HIDDEN_LAYERS_0_NODES *
                                          HIDDEN_LAYERS_1_NODES] = {
      -0.1727f, -0.2859f,  -0.3757f, -0.4260f,  -0.5441f, -0.0666f, -0.3792f,
      -0.1335f, -0.1521f,  -0.0821f, -3.1590f,  0.2711f,  0.5889f,  0.0878f,
      0.4693f,  0.7773f,   -9.2989f, 0.0414f,   0.4485f,  22.8958f, -3.7024f,
      -2.4672f, -43.2908f, 0.0956f,  0.4431f,   2.3429f,  1.7183f,  0.3985f,
      -0.2275f, -3.1583f,  -0.3485f, 0.3280f,   0.3763f,  0.2069f,  0.4231f,
      0.7366f,  -6.9527f,  0.0713f,  0.1359f,   16.6500f, -1.7655f, -0.1651f,
      0.1280f,  -0.2678f,  -0.2120f, 1.6243f,   1.8773f,  -0.7543f, -0.3292f,
      -0.7627f, -0.2001f,  -0.1125f, -0.8100f,  -0.1866f, 0.0567f,  -0.4002f,
      3.2429f,  0.6427f,   -0.3759f, -11.6518f, -2.2893f, 0.7708f,  -1.8637f,
      1.7148f,  0.3124f,   -0.7129f, -0.4927f,  0.1964f,  -0.2570f, -25.0783f,
      2.5061f,  0.1457f,   -1.1239f, 0.0570f,   -0.2526f, -0.0669f, 0.6791f,
      1.1531f,  -0.7246f,  -0.3180f, -0.0015f,  -0.0061f, -0.1626f, -0.0181f,
      0.1271f,  -0.0140f,  -0.6027f, 0.0736f,   -0.0157f, 1.2420f,  -6.4055f,
      0.2128f,  -0.0386f,  0.3446f,  0.1840f,   -0.7208f, -1.6979f, -0.0442f,
      0.3230f,  -1.9745f,
    };

static const float av1_pustats_dist_hiddenlayer_1_bias[HIDDEN_LAYERS_1_NODES] =
    { 0.f,      70.3414f, 9.6036f,   -118.1096f, 49.2507f,
      95.1849f, 81.8015f, 167.0967f, -337.7945f, 169.8344f };

static const float
    av1_pustats_dist_logits_kernel[HIDDEN_LAYERS_1_NODES * LOGITS_NODES] = {
      -0.3627f, 1.2272f,  0.2201f, -1.7406f, -0.6885f,
      0.8487f,  -0.2761f, 0.7731f, -5.2096f, -0.7351f,
    };

static const float av1_pustats_dist_logits_bias[LOGITS_NODES] = {
  48.2331f,
};

static const NN_CONFIG av1_pustats_dist_nnconfig = {
  NUM_FEATURES,                                      // num_inputs
  LOGITS_NODES,                                      // num_outputs
  NUM_HIDDEN_LAYERS,                                 // num_hidden_layers
  { HIDDEN_LAYERS_0_NODES, HIDDEN_LAYERS_1_NODES },  // num_hidden_nodes
  {
      av1_pustats_dist_hiddenlayer_0_kernel,
      av1_pustats_dist_hiddenlayer_1_kernel,
      av1_pustats_dist_logits_kernel,
  },
  {
      av1_pustats_dist_hiddenlayer_0_bias,
      av1_pustats_dist_hiddenlayer_1_bias,
      av1_pustats_dist_logits_bias,
  },
};

#undef NUM_FEATURES
#undef NUM_HIDDEN_LAYERS
#undef HIDDEN_LAYERS_0_NODES
#undef HIDDEN_LAYERS_1_NODES
#undef LOGITS_NODES

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_PUSTATS_H_
