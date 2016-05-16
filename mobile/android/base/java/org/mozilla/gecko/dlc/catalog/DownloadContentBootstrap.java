/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc.catalog;

import android.support.v4.util.ArrayMap;

import org.mozilla.gecko.AppConstants;

import java.util.Arrays;
import java.util.List;

/* package-private */ class DownloadContentBootstrap {
    public static ArrayMap<String, DownloadContent> createInitialDownloadContentList() {
        if (!AppConstants.MOZ_ANDROID_EXCLUDE_FONTS) {
            // We are packaging fonts. There's nothing we want to download;
            return new ArrayMap<>();
        }

        List<DownloadContent> initialList = Arrays.asList(
                new DownloadContentBuilder()
                        .setId("c40929cf-7f4c-fa72-3dc9-12cadf56905d")
                        .setLocation("fennec/catalog/f63e5f92-793c-4574-a2d7-fbc50242b8cb.gz")
                        .setFilename("CharisSILCompact-B.ttf")
                        .setChecksum("699d958b492eda0cc2823535f8567d0393090e3842f6df3c36dbe7239cb80b6d")
                        .setDownloadChecksum("a9f9b34fed353169a88cc159b8f298cb285cce0b8b0f979c22a7d85de46f0532")
                        .setSize(1676072)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContentBuilder()
                        .setId("6d265876-85ed-0917-fdc8-baf583ca2cba")
                        .setLocation("fennec/catalog/19af6c88-09d9-4d6c-805e-cfebb8699a6c.gz")
                        .setFilename("CharisSILCompact-BI.ttf")
                        .setChecksum("82465e747b4f41471dbfd942842b2ee810749217d44b55dbc43623b89f9c7d9b")
                        .setDownloadChecksum("2be26671039a5e2e4d0360a948b4fa42048171133076a3bb6173d93d4b9cd55b")
                        .setSize(1667812)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContentBuilder()
                        .setId("8460dc6d-d129-fd1a-24b6-343dbf6531dd")
                        .setLocation("fennec/catalog/f35a384a-90ea-41c6-a957-bb1845de97eb.gz")
                        .setFilename("CharisSILCompact-I.ttf")
                        .setChecksum("ab3ed6f2a4d4c2095b78227bd33155d7ccd05a879c107a291912640d4d64f767")
                        .setDownloadChecksum("38a6469041c02624d43dfd41d2dd745e3e3211655e616188f65789a90952a1e9")
                        .setSize(1693988)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContentBuilder()
                        .setId("c906275c-3747-fe27-426f-6187526a6f06")
                        .setLocation("fennec/catalog/8c3bec92-d2df-4789-8c4a-0f523f026d96.gz")
                        .setFilename("CharisSILCompact-R.ttf")
                        .setChecksum("4ed509317f1bb441b185ea13bf1c9d19d1a0b396962efa3b5dc3190ad88f2067")
                        .setDownloadChecksum("7c2ec1f550c2005b75383b878f737266b5f0b1c82679dd886c8bbe30c82e340e")
                        .setSize(1727656)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContentBuilder()
                        .setId("ff5deecc-6ecc-d816-bb51-65face460119")
                        .setLocation("fennec/catalog/ea115d71-e2ac-4609-853e-c978780776b1.gz")
                        .setFilename("ClearSans-Bold.ttf")
                        .setChecksum("385d0a293c1714770e198f7c762ab32f7905a0ed9d2993f69d640bd7232b4b70")
                        .setDownloadChecksum("0d3c22bef90e7096f75b331bb7391de3aa43017e10d61041cd3085816db4919a")
                        .setSize(140136)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContentBuilder()
                        .setId("a173d1db-373b-ce42-1335-6b3285cfdebd")
                        .setLocation("fennec/catalog/0838e513-2d99-4e53-b58f-6b970f6548c6.gz")
                        .setFilename("ClearSans-BoldItalic.ttf")
                        .setChecksum("7bce66864e38eecd7c94b6657b9b38c35ebfacf8046bfb1247e08f07fe933198")
                        .setDownloadChecksum("de0903164dde1ad3768d0bd6dec949871d6ab7be08f573d9d70f38c138a22e37")
                        .setSize(156124)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContentBuilder()
                        .setId("e65c66df-0088-940d-ca5c-207c22118c0e")
                        .setLocation("fennec/catalog/7550fa42-0947-478c-a5f0-5ea1bbb6ba27.gz")
                        .setFilename("ClearSans-Italic.ttf")
                        .setChecksum("87c13c5fbae832e4f85c3bd46fcbc175978234f39be5fe51c4937be4cbff3b68")
                        .setDownloadChecksum("6e323db3115005dd0e96d2422db87a520f9ae426de28a342cd6cc87b55601d87")
                        .setSize(155672)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContentBuilder()
                        .setId("25610abb-5dc8-fd75-40e7-990507f010c4")
                        .setLocation("fennec/catalog/dd9bee7d-d784-476b-a3dd-69af8e516487.gz")
                        .setFilename("ClearSans-Light.ttf")
                        .setChecksum("e4885f6188e7a8587f5621c077c6c1f5e8d3739dffc8f4d055c2ba87568c750a")
                        .setDownloadChecksum("19d4f7c67176e9e254c61420da9c7363d9fe5e6b4bb9d61afa4b3b574280714f")
                        .setSize(145976)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContentBuilder()
                        .setId("ffe40339-a096-2262-c3f8-54af75c81fe6")
                        .setLocation("fennec/catalog/bc5ada8c-8cfc-443d-93d7-dc5f98138a07.gz")
                        .setFilename("ClearSans-Medium.ttf")
                        .setChecksum("5d0e0115f3a3ed4be3eda6d7eabb899bb9a361292802e763d53c72e00f629da1")
                        .setDownloadChecksum("edec86dab3ad2a97561cb41b584670262a48bed008c57bb587ee05ca47fb067f")
                        .setSize(148892)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContentBuilder()
                        .setId("139a94be-ac69-0264-c9cc-8f2d071fd29d")
                        .setLocation("fennec/catalog/0490c768-6178-49c2-af88-9f8769ff3167.gz")
                        .setFilename("ClearSans-MediumItalic.ttf")
                        .setChecksum("937dda88b26469306258527d38e42c95e27e7ebb9f05bd1d7c5d706a3c9541d7")
                        .setDownloadChecksum("34edbd1b325dbffe7791fba8dd2d19852eb3c2fe00cff517ea2161ddc424ee22")
                        .setSize(155228)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContentBuilder()
                        .setId("b887012a-01e1-7c94-fdcb-ca44d5b974a2")
                        .setLocation("fennec/catalog/78205bf8-c668-41b1-b68f-afd54f98713b.gz")
                        .setFilename("ClearSans-Regular.ttf")
                        .setChecksum("9b91bbdb95ffa6663da24fdaa8ee06060cd0a4d2dceaf1ffbdda00e04915ee5b")
                        .setDownloadChecksum("a72f1420b4da1ba9e6797adac34f08e72f94128a85e56542d5e6a8080af5f08a")
                        .setSize(142572)
                        .setKind("font")
                        .setType("asset-archive")
                        .build(),

                new DownloadContentBuilder()
                        .setId("c8703652-d317-0356-0bf8-95441a5b2c9b")
                        .setLocation("fennec/catalog/3570f44f-9440-4aa0-abd0-642eaf2a1aa0.gz")
                        .setFilename("ClearSans-Thin.ttf")
                        .setChecksum("07b0db85a3ad99afeb803f0f35631436a7b4c67ac66d0c7f77d26a47357c592a")
                        .setDownloadChecksum("d9f23fd8687d6743f5c281c33539fb16f163304795039959b8caf159e6d62822")
                        .setSize(147004)
                        .setKind("font")
                        .setType("asset-archive")
                        .build());

        ArrayMap<String, DownloadContent> content = new ArrayMap<>();
        for (DownloadContent currentContent : initialList) {
            content.put(currentContent.getId(), currentContent);
        }
        return content;
    }
}
