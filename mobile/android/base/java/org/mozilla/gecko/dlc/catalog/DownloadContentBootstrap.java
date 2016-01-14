/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc.catalog;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.dlc.catalog.DownloadContent;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/* package-private */ class DownloadContentBootstrap {
    public static List<DownloadContent> createInitialDownloadContentList() {
        if (!AppConstants.MOZ_ANDROID_EXCLUDE_FONTS) {
            // We are packaging fonts. There's nothing we want to download;
            return Collections.emptyList();
        }

        return Arrays.asList(
                new DownloadContent.Builder()
                        .setId("c40929cf-7f4c-fa72-3dc9-12cadf56905d")
                        .setLocation("fonts/ff7ecae7669a51d5fa6a5f8e703278ebda3a68f51bc49c4321bde4438020d639.gz")
                        .setFilename("CharisSILCompact-B.ttf")
                        .setChecksum("699d958b492eda0cc2823535f8567d0393090e3842f6df3c36dbe7239cb80b6d")
                        .setDownloadChecksum("ff7ecae7669a51d5fa6a5f8e703278ebda3a68f51bc49c4321bde4438020d639")
                        .setSize(1676072)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContent.Builder()
                        .setId("6d265876-85ed-0917-fdc8-baf583ca2cba")
                        .setLocation("fonts/dfb6d583edd27d5e6d91d479e6c8a5706275662c940c65b70911493bb279904a.gz")
                        .setFilename("CharisSILCompact-BI.ttf")
                        .setChecksum("82465e747b4f41471dbfd942842b2ee810749217d44b55dbc43623b89f9c7d9b")
                        .setDownloadChecksum("dfb6d583edd27d5e6d91d479e6c8a5706275662c940c65b70911493bb279904a")
                        .setSize(1667812)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContent.Builder()
                        .setId("8460dc6d-d129-fd1a-24b6-343dbf6531dd")
                        .setLocation("fonts/5a257ec3c5226e7be0be65e463f5b22eff108da853b9ff7bc47f1733b1ddacf2.gz")
                        .setFilename("CharisSILCompact-I.ttf")
                        .setChecksum("ab3ed6f2a4d4c2095b78227bd33155d7ccd05a879c107a291912640d4d64f767")
                        .setDownloadChecksum("5a257ec3c5226e7be0be65e463f5b22eff108da853b9ff7bc47f1733b1ddacf2")
                        .setSize(1693988)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContent.Builder()
                        .setId("c906275c-3747-fe27-426f-6187526a6f06")
                        .setLocation("fonts/cab284228b8dfe8ef46c3f1af70b5b6f9e92878f05e741ecc611e5e750a4a3b3.gz")
                        .setFilename("CharisSILCompact-R.ttf")
                        .setChecksum("4ed509317f1bb441b185ea13bf1c9d19d1a0b396962efa3b5dc3190ad88f2067")
                        .setDownloadChecksum("cab284228b8dfe8ef46c3f1af70b5b6f9e92878f05e741ecc611e5e750a4a3b3")
                        .setSize(1727656)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContent.Builder()
                        .setId("ff5deecc-6ecc-d816-bb51-65face460119")
                        .setLocation("fonts/d95168996dc932e6504cb5448fcb759e0ee6e66c5c8603293b046d28ab589cce.gz")
                        .setFilename("ClearSans-Bold.ttf")
                        .setChecksum("385d0a293c1714770e198f7c762ab32f7905a0ed9d2993f69d640bd7232b4b70")
                        .setDownloadChecksum("d95168996dc932e6504cb5448fcb759e0ee6e66c5c8603293b046d28ab589cce")
                        .setSize(140136)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContent.Builder()
                        .setId("a173d1db-373b-ce42-1335-6b3285cfdebd")
                        .setLocation("fonts/f5e18f4acc4ceaeca9e081b1be79cd6034e0dc7ad683fa240195fd6c838452e0.gz")
                        .setFilename("ClearSans-BoldItalic.ttf")
                        .setChecksum("7bce66864e38eecd7c94b6657b9b38c35ebfacf8046bfb1247e08f07fe933198")
                        .setDownloadChecksum("f5e18f4acc4ceaeca9e081b1be79cd6034e0dc7ad683fa240195fd6c838452e0")
                        .setSize(156124)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContent.Builder()
                        .setId("e65c66df-0088-940d-ca5c-207c22118c0e")
                        .setLocation("fonts/56d12114ac15d913d7d9876c698889cd25f26e14966a8bd7424aeb0f61ffaf87.gz")
                        .setFilename("ClearSans-Italic.ttf")
                        .setChecksum("87c13c5fbae832e4f85c3bd46fcbc175978234f39be5fe51c4937be4cbff3b68")
                        .setDownloadChecksum("56d12114ac15d913d7d9876c698889cd25f26e14966a8bd7424aeb0f61ffaf87")
                        .setSize(155672)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContent.Builder()
                        .setId("25610abb-5dc8-fd75-40e7-990507f010c4")
                        .setLocation("fonts/1fc716662866b9c01e32dda3fc9c54ca3e57de8c6ac523f46305d8ae6c0a9cf4.gz")
                        .setFilename("ClearSans-Light.ttf")
                        .setChecksum("e4885f6188e7a8587f5621c077c6c1f5e8d3739dffc8f4d055c2ba87568c750a")
                        .setDownloadChecksum("1fc716662866b9c01e32dda3fc9c54ca3e57de8c6ac523f46305d8ae6c0a9cf4")
                        .setSize(145976)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContent.Builder()
                        .setId("ffe40339-a096-2262-c3f8-54af75c81fe6")
                        .setLocation("fonts/a29184ec6621dbd3bc6ae1e30bba70c479d1001bca647ea4a205ecb64d5a00a0.gz")
                        .setFilename("ClearSans-Medium.ttf")
                        .setChecksum("5d0e0115f3a3ed4be3eda6d7eabb899bb9a361292802e763d53c72e00f629da1")
                        .setDownloadChecksum("a29184ec6621dbd3bc6ae1e30bba70c479d1001bca647ea4a205ecb64d5a00a0")
                        .setSize(148892)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContent.Builder()
                        .setId("139a94be-ac69-0264-c9cc-8f2d071fd29d")
                        .setLocation("fonts/a381a3d4060e993af440a7b72fed29fa3a488536cc451d7c435d5fae1256318b.gz")
                        .setFilename("ClearSans-MediumItalic.ttf")
                        .setChecksum("937dda88b26469306258527d38e42c95e27e7ebb9f05bd1d7c5d706a3c9541d7")
                        .setDownloadChecksum("a381a3d4060e993af440a7b72fed29fa3a488536cc451d7c435d5fae1256318b")
                        .setSize(155228)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContent.Builder()
                        .setId("b887012a-01e1-7c94-fdcb-ca44d5b974a2")
                        .setLocation("fonts/87dec7f0331e19b293fc510f2764b9bd1b94595ac279cf9414f8d03c5bf34dca.gz")
                        .setFilename("ClearSans-Regular.ttf")
                        .setChecksum("9b91bbdb95ffa6663da24fdaa8ee06060cd0a4d2dceaf1ffbdda00e04915ee5b")
                        .setDownloadChecksum("87dec7f0331e19b293fc510f2764b9bd1b94595ac279cf9414f8d03c5bf34dca")
                        .setSize(142572)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContent.Builder()
                        .setId("c8703652-d317-0356-0bf8-95441a5b2c9b")
                        .setLocation("fonts/64300b48b2867e5642212690f0ff9ea3988f47790311c444a81d25213b4102aa.gz")
                        .setFilename("ClearSans-Thin.ttf")
                        .setChecksum("07b0db85a3ad99afeb803f0f35631436a7b4c67ac66d0c7f77d26a47357c592a")
                        .setDownloadChecksum("64300b48b2867e5642212690f0ff9ea3988f47790311c444a81d25213b4102aa")
                        .setSize(147004)
                        .setKind("font")
                        .setType("asset-archive")
                        .build()
        );
    }
}
