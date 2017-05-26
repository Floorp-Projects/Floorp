#[macro_use]
extern crate language_tags;

use std::ascii::AsciiExt;
use std::default::Default;
use std::collections::BTreeMap;

use language_tags::{Error, LanguageTag, Result};

// All tests here may be completly nonsensical.

#[test]
fn test_lang_from_str() {
    let a: LanguageTag = "de".parse().unwrap();
    let mut b: LanguageTag = Default::default();
    b.language = Some("de".to_owned());
    assert_eq!(a, b);
}

#[test]
fn test_extlang_from_str() {
    let a: LanguageTag = "ar-afb".parse().unwrap();
    let mut b: LanguageTag = Default::default();
    b.language = Some("ar".to_owned());
    b.extlangs = vec!["afb".to_owned()];
    assert_eq!(a, b);
}

#[test]
fn test_script_from_str() {
    let a: LanguageTag = "ar-afb-Latn".parse().unwrap();
    let mut b: LanguageTag = Default::default();
    b.language = Some("ar".to_owned());
    b.extlangs = vec!["afb".to_owned()];
    b.script = Some("latn".to_owned());
    assert_eq!(a, b);
}

#[test]
fn test_region_from_str() {
    let mut a: LanguageTag = "ar-DE".parse().unwrap();
    let mut b: LanguageTag = Default::default();
    b.language = Some("ar".to_owned());
    b.region = Some("de".to_owned());
    assert_eq!(a, b);

    a = "ar-005".parse().unwrap();
    b = Default::default();
    b.language = Some("ar".to_owned());
    b.region = Some("005".to_owned());
    assert_eq!(a, b);

    a = "ar-005".parse().unwrap();
    b = Default::default();
    b.language = Some("ar".to_owned());
    b.region = Some("005".to_owned());
    assert_eq!(a, b);
}

#[test]
fn test_variant_from_str() {
    let a: LanguageTag = "sl-IT-nedis".parse().unwrap();
    let mut b: LanguageTag = Default::default();
    b.language = Some("sl".parse().unwrap());
    b.region = Some("it".parse().unwrap());
    b.variants = vec!["nedis".parse().unwrap()];
    assert_eq!(a, b);
}

#[test]
fn test_invalid_from_str() {
    assert_eq!("sl-07".parse::<LanguageTag>(), Err(Error::InvalidSubtag));
}

#[test]
fn test_strange_case_from_str() {
    // This is a perfectly valid language code
    let a: LanguageTag = "SL-AFB-lATN-005-nEdis".parse().unwrap();
    let b = LanguageTag {
        language: Some("sl".to_owned()),
        extlangs: vec!["afb".to_owned()],
        script: Some("Latn".to_owned()),
        region: Some("005".to_owned()),
        variants: vec!["nedis".to_owned()],
        extensions: BTreeMap::new(),
        privateuse: Vec::new(),
    };
    assert_eq!(a, b);
}

#[test]
fn test_fmt() {
    let a: LanguageTag = "ar-arb-Latn-DE-nedis-foobar".parse().unwrap();
    assert_eq!(format!("{}", a), "ar-arb-Latn-DE-nedis-foobar");
    let b: LanguageTag = "ar-arb-latn-de-nedis-foobar".parse().unwrap();
    assert_eq!(format!("{}", b), "ar-arb-Latn-DE-nedis-foobar");
    let b: LanguageTag = "AR-ARB-LATN-DE-NEDIS-FOOBAR".parse().unwrap();
    assert_eq!(format!("{}", b), "ar-arb-Latn-DE-nedis-foobar");
    let b: LanguageTag = "xx-z-foo-a-bar-F-spam-b-eggs".parse().unwrap();
    assert_eq!(format!("{}", b), "xx-a-bar-b-eggs-F-spam-z-foo");
}

#[test]
fn test_match() {
    let de_de: LanguageTag = "de-DE".parse().unwrap();
    let de: LanguageTag = "de".parse().unwrap();
    assert!(de.matches(&de_de));
    assert!(!de_de.matches(&de));
}

#[test]
fn test_match_all() {
    let de_de: LanguageTag = "de-DE".parse().unwrap();
    let crazy: LanguageTag = "GDJ-nHYa-bw-X-ke-rohH5GfS-LdJKsGVe".parse().unwrap();
    let wildcard: LanguageTag = Default::default();
    assert!(wildcard.matches(&de_de));
    assert!(wildcard.matches(&crazy));
}

#[test]
fn test_klingon() {
    let a: LanguageTag = "i-klingon".parse().unwrap();
    let mut b: LanguageTag = Default::default();
    b.language = Some("i-klingon".to_owned());
    assert_eq!(a, b);
}

#[test]
fn test_private_use() {
    let a: LanguageTag = "es-x-foobar-AT-007".parse().unwrap();
    let mut b: LanguageTag = Default::default();
    b.language = Some("es".to_owned());
    b.privateuse = vec!["foobar".to_owned(), "AT".to_owned(), "007".to_owned()];
    assert_eq!(a, b);
}

#[test]
fn test_unicode() {
    let x: Result<LanguageTag> = "zh-x-Üńìcødê".parse();
    assert!(x.is_err());
}

#[test]
fn test_format() {
    let x: LanguageTag = "HkgnmerM-x-e5-zf-VdDjcpz-1V6".parse().unwrap();
    assert!(format!("{}", x).eq_ignore_ascii_case("HkgnmerM-x-e5-zf-VdDjcpz-1V6"));
    let y: LanguageTag = "MgxQa-ywEp-8lcW-7bvT-h-dP1Md-0h7-0Z3ir".parse().unwrap();
    assert!(format!("{}", y).eq_ignore_ascii_case("MgxQa-ywEp-8lcW-7bvT-h-dP1Md-0h7-0Z3ir"));
}

#[test]
fn test_cmp() {
    assert_eq!(langtag!(dE;;AraB;lY), langtag!(DE;;aRaB;LY));
    assert!(langtag!(dE;;AraB;lY).matches(&langtag!(DE;;aRaB;LY)));
    let mut extensions = BTreeMap::new();
    extensions.insert(75 /* K */, vec!["foo".to_owned(), "bar".to_owned()]);
    extensions.insert(112 /* p */, vec!["spam".to_owned(), "eggs".to_owned()]);
    let langtag = LanguageTag {
        language: Some("it".to_owned()),
        extlangs: Vec::new(),
        script: None,
        region: None,
        variants: Vec::new(),
        extensions: extensions,
        privateuse: Vec::new(),
    };
    assert_eq!(langtag, langtag);
}

#[test]
fn test_macro() {
    let a1 = langtag!(it);
    let a2 = LanguageTag {
        language: Some("it".to_owned()),
        extlangs: Vec::new(),
        script: None,
        region: None,
        variants: Vec::new(),
        extensions: BTreeMap::new(),
        privateuse: Vec::new(),
    };
    assert_eq!(a1, a2);
    let b1 = langtag!(it;;;LY);
    let b2 = LanguageTag {
        language: Some("it".to_owned()),
        extlangs: Vec::new(),
        script: None,
        region: Some("LY".to_owned()),
        variants: Vec::new(),
        extensions: BTreeMap::new(),
        privateuse: Vec::new(),
    };
    assert_eq!(b1, b2);
    let c1 = langtag!(it;;Arab;LY);
    let c2 = LanguageTag {
        language: Some("it".to_owned()),
        extlangs: Vec::new(),
        script: Some("Arab".to_owned()),
        region: Some("LY".to_owned()),
        variants: Vec::new(),
        extensions: BTreeMap::new(),
        privateuse: Vec::new(),
    };
    assert_eq!(c1, c2);
}

#[test]
fn test_private_tag() {
    let mut tag: LanguageTag = Default::default();
    tag.privateuse = vec!["foo".to_owned(), "bar".to_owned()];
    assert_eq!(format!("{}", tag), "x-foo-bar");
}

#[test]
fn test_grandfathered_tag() {
    let tag_irregular: LanguageTag = "i-klingon".parse().unwrap();
    assert_eq!(tag_irregular.language.unwrap(), "i-klingon");
    let tag_regular: LanguageTag = "zh-hakka".parse().unwrap();
    assert_eq!(tag_regular.language.unwrap(), "zh-hakka");
}

#[test]
fn test_eq() {
    let mut tag1: LanguageTag = Default::default();
    tag1.language = Some("zh".to_owned());
    let mut tag2: LanguageTag = Default::default();
    tag2.language = Some("zh".to_owned());
    tag2.script = Some("Latn".to_owned());
    assert!(tag1 != tag2);
}

#[test]
fn test_wellformed_tags() {
    // Source: http://www.langtag.net/test-suites/well-formed-tags.txt
    let tags = vec![
        "fr ",
        "fr-Latn",
        "fr-fra", // Extended tag
        "fr-Latn-FR",
        "fr-Latn-419",
        "fr-FR",
        "ax-TZ", // Not in the registry, but well-formed
        "fr-shadok", // Variant
        "fr-y-myext-myext2",
        "fra-Latn", // ISO 639 can be 3-letters
        "fra",
        "fra-FX",
        "i-klingon", // grandfathered with singleton
        "I-kLINgon", // tags are case-insensitive...
        "no-bok", // grandfathered without singleton
        "fr-Lat", // Extended",
        "mn-Cyrl-MN",
        "mN-cYrL-Mn",
        "fr-Latn-CA",
        "en-US",
        "fr-Latn-CA",
        "i-enochian", // Grand fathered
        "x-fr-CH ",
        "sr-Latn-CS",
        "es-419",
        "sl-nedis",
        "de-CH-1996",
        "de-Latg-1996",
        "sl-IT-nedis",
        "en-a-bbb-x-a-ccc",
        "de-a-value",
        "en-Latn-GB-boont-r-extended-sequence-x-private",
        "en-x-US",
        "az-Arab-x-AZE-derbend",
        "es-Latn-CO-x-private",
        "en-US-boont",
        "ab-x-abc-x-abc", // anything goes after x
        "ab-x-abc-a-a", // ditto",
        "i-default", // grandfathered",
        "i-klingon", // grandfathered",
        "abcd-Latn", // Language of 4 chars reserved for future use
        "AaBbCcDd-x-y-any-x", // Language of 5-8 chars, registered
        "en",
        "de-AT",
        "es-419",
        "de-CH-1901",
        "sr-Cyrl",
        "sr-Cyrl-CS",
        "sl-Latn-IT-rozaj",
        "en-US-x-twain",
        "zh-cmn",
        "zh-cmn-Hant",
        "zh-cmn-Hant-HK",
        "zh-gan",
        "zh-yue-Hant-HK",
        "xr-lxs-qut", // extlangS
        "xr-lqt-qu", // extlang + region
        "xr-p-lze", // Extension
    ];
    let failed: Vec<&str> = tags.iter()
                                .filter(|x| x.parse::<LanguageTag>().is_err())
                                .map(|&x| x)
                                .collect();
    println!("Number: { } Failed: {:?}", failed.len(), failed);
    assert!(failed.is_empty());
}

#[test]
fn test_broken_tags() {
    // http://www.langtag.net/test-suites/broken-tags.txt
    let tags = vec![
        "f",
        "f-Latn",
        "fr-Latn-F",
        "a-value",
        "en-a-bbb-a-ccc", // 'a' appears twice
        "tlh-a-b-foo",
        "i-notexist", // grandfathered but not registered: always invalid
        "abcdefghi-012345678",
        "ab-abc-abc-abc-abc",
        "ab-abcd-abc",
        "ab-ab-abc",
        "ab-123-abc",
        "a-Hant-ZH",
        "a1-Hant-ZH",
        "ab-abcde-abc",
        "ab-1abc-abc",
        "ab-ab-abcd",
        "ab-123-abcd",
        "ab-abcde-abcd",
        "ab-1abc-abcd",
        "ab-a-b",
        "ab-a-x",
        "ab--ab",
        "ab-abc-",
        "-ab-abc",
        "ab-c-abc-r-toto-c-abc  # 'c' appears twice ",
        "abcd-efg",
        "aabbccddE",
    ];
    let failed: Vec<(&str, Result<LanguageTag>)> = tags.iter()
                                                       .map(|x| (*x, x.parse::<LanguageTag>()))
                                                       .filter(|x| x.1.is_ok())
                                                       .collect();
    println!("Number: { } Failed: {:?}", failed.len(), failed);
    assert!(failed.is_empty());
}

#[test]
fn test_random_good_tags() {
    // http://unicode.org/repos/cldr/trunk/tools/java/org/unicode/cldr/util/data/langtagTest.txt
    let tags = vec!["zszLDm-sCVS-es-x-gn762vG-83-S-mlL",
                    "IIJdFI-cfZv",
                    "kbAxSgJ-685",
                    "tbutP",
                    "hDL-595",
                    "dUf-iUjq-0hJ4P-5YkF-WD8fk",
                    "FZAABA-FH",
                    "xZ-lh-4QfM5z9J-1eG4-x-K-R6VPr2z",
                    "Fyi",
                    "SeI-DbaG",
                    "ch-xwFn",
                    "OeC-GPVI",
                    "JLzvUSi",
                    "Fxh-hLAs",
                    "pKHzCP-sgaO-554",
                    "eytqeW-hfgH-uQ",
                    "ydn-zeOP-PR",
                    "uoWmBM-yHCf-JE",
                    "xwYem",
                    "zie",
                    "Re-wjSv-Ey-i-XE-E-JjWTEB8-f-DLSH-NVzLH-AtnFGWoH-SIDE",
                    "Ri-063-c-u6v-ZfhkToTB-C-IFfmv-XT-j-rdyYFMhK-h-pY-D5-Oh6FqBhL-hcXt-v-WdpNx71-\
                     K-c74m4-eBTT7-JdH7Q1Z",
                    "ji",
                    "IM-487",
                    "EPZ-zwcB",
                    "GauwEcwo",
                    "kDEP",
                    "FwDYt-TNvo",
                    "ottqP-KLES-x-9-i9",
                    "fcflR-grQQ",
                    "TvFwdu-kYhs",
                    "WE-336",
                    "MgxQa-ywEp-8lcW-7bvT-h-dP1Md-0h7-0Z3ir-K-Srkm-kA-7LXM-Z-whb2MiO-2mNsvbLm-W3O\
                     -4r-U-KceIxHdI-gvMVgUBV-2uRUni-J0-7C8yTK2",
                    "Hyr-B-evMtVoB1-mtsVZf-vQMV-gM-I-rr-kvLzg-f-lAUK-Qb36Ne-Z-7eFzOD-mv6kKf-l-miZ\
                     7U3-k-XDGtNQG",
                    "ybrlCpzy",
                    "PTow-w-cAQ51-8Xd6E-cumicgt-WpkZv3NY-q-ORYPRy-v-A4jL4A-iNEqQZZ-sjKn-W-N1F-pzy\
                     c-xP5eWz-LmsCiCcZ",
                    "ih-DlPR-PE",
                    "Krf-362",
                    "WzaD",
                    "EPaOnB-gHHn",
                    "XYta",
                    "NZ-RgOO-tR",
                    "at-FE",
                    "Tpc-693",
                    "YFp",
                    "gRQrQULo",
                    "pVomZ-585",
                    "laSu-ZcAq-338",
                    "gCW",
                    "PydSwHRI-TYfF",
                    "zKmWDD",
                    "X-bCrL5RL",
                    "HK",
                    "YMKGcLY",
                    "GDJ-nHYa-bw-X-ke-rohH5GfS-LdJKsGVe",
                    "tfOxdau-yjge-489-a-oB-I8Csb-1ESaK1v-VFNz-N-FT-ZQyn-On2-I-hu-vaW3-jIQb-vg0U-h\
                     Ul-h-dO6KuJqB-U-tde2L-P3gHUY-vnl5c-RyO-H-gK1-zDPu-VF1oeh8W-kGzzvBbW-yuAJZ",
                    "LwDux",
                    "Zl-072",
                    "Ri-Ar",
                    "vocMSwo-cJnr-288",
                    "kUWq-gWfQ-794",
                    "YyzqKL-273",
                    "Xrw-ZHwH-841-9foT-ESSZF-6OqO-0knk-991U-9p3m-b-JhiV-0Kq7Y-h-cxphLb-cDlXUBOQ-X\
                     -4Ti-jty94yPp",
                    "en-GB-oed",
                    "LEuZl-so",
                    "HyvBvFi-cCAl-X-irMQA-Pzt-H",
                    "uDbsrAA-304",
                    "wTS",
                    "IWXS",
                    "XvDqNkSn-jRDR",
                    "gX-Ycbb-iLphEks-AQ1aJ5",
                    "FbSBz-VLcR-VL",
                    "JYoVQOP-Iytp",
                    "gDSoDGD-lq-v-7aFec-ag-k-Z4-0kgNxXC-7h",
                    "Bjvoayy-029",
                    "qSDJd",
                    "qpbQov",
                    "fYIll-516",
                    "GfgLyfWE-EHtB",
                    "Wc-ZMtk",
                    "cgh-VEYK",
                    "WRZs-AaFd-yQ",
                    "eSb-CpsZ-788",
                    "YVwFU",
                    "JSsHiQhr-MpjT-381",
                    "LuhtJIQi-JKYt",
                    "vVTvS-RHcP",
                    "SY",
                    "fSf-EgvQfI-ktWoG-8X5z-63PW",
                    "NOKcy",
                    "OjJb-550",
                    "KB",
                    "qzKBv-zDKk-589",
                    "Jr",
                    "Acw-GPXf-088",
                    "WAFSbos",
                    "HkgnmerM-x-e5-zf-VdDjcpz-1V6",
                    "UAfYflJU-uXDc-YV",
                    "x-CHsHx-VDcOUAur-FqagDTx-H-V0e74R",
                    "uZIAZ-Xmbh-pd"];
    let failed: Vec<(&str, Result<LanguageTag>)> = tags.iter()
                                                       .map(|x| (*x, x.parse::<LanguageTag>()))
                                                       .filter(|x| x.1.is_err())
                                                       .collect();
    println!("Number: { } Failed: {:?}", failed.len(), failed);
    assert!(failed.is_empty());
}

#[test]
fn test_random_bad_tags() {
    // http://unicode.org/repos/cldr/trunk/tools/java/org/unicode/cldr/util/data/langtagTest.txt
    let tags = vec!["EdY-z_H791Xx6_m_kj",
                    "qWt85_8S0-L_rbBDq0gl_m_O_zsAx_nRS",
                    "VzyL2",
                    "T_VFJq-L-0JWuH_u2_VW-hK-kbE",
                    "u-t",
                    "Q-f_ZVJXyc-doj_k-i",
                    "JWB7gNa_K-5GB-25t_W-s-ZbGVwDu1-H3E",
                    "b-2T-Qob_L-C9v_2CZxK86",
                    "fQTpX_0_4Vg_L3L_g7VtALh2",
                    "S-Z-E_J",
                    "f6wsq-02_i-F",
                    "9_GcUPq_G",
                    "QjsIy_9-0-7_Dv2yPV09_D-JXWXM",
                    "D_se-f-k",
                    "ON47Wv1_2_W",
                    "f-z-R_s-ha",
                    "N3APeiw_195_Bx2-mM-pf-Z-Ip5lXWa-5r",
                    "IRjxU-E_6kS_D_b1b_H",
                    "NB-3-5-AyW_FQ-9hB-TrRJg3JV_3C",
                    "yF-3a_V_FoJQAHeL_Z-Mc-u",
                    "n_w_bbunOG_1-s-tJMT5je",
                    "Q-AEWE_X",
                    "57b1O_k_R6MU_sb",
                    "hK_65J_i-o_SI-Y",
                    "wB4B7u_5I2_I_NZPI",
                    "J24Nb_q_d-zE",
                    "v6-dHjJmvPS_IEb-x_A-O-i",
                    "8_8_dl-ZgBr84u-P-E",
                    "nIn-xD7EVhe_C",
                    "5_N-6P_x7Of_Lo_6_YX_R",
                    "0_46Oo0sZ-YNwiU8Wr_d-M-pg1OriV",
                    "laiY-5",
                    "K-8Mdd-j_ila0sSpo_aO8_J",
                    "wNATtSL-Cp4_gPa_fD41_9z",
                    "H_FGz5V8_n6rrcoz0_1O6d-kH-7-N",
                    "wDOrnHU-odqJ_vWl",
                    "gP_qO-I-jH",
                    "h",
                    "dJ0hX-o_csBykEhU-F",
                    "L-Vf7_BV_eRJ5goSF_Kp",
                    "y-oF-chnavU-H",
                    "9FkG-8Q-8_v",
                    "W_l_NDQqI-O_SFSAOVq",
                    "kDG3fzXw",
                    "t-nsSp-7-t-mUK2",
                    "Yw-F",
                    "1-S_3_l",
                    "u-v_brn-Y",
                    "4_ft_3ZPZC5lA_D",
                    "n_dR-QodsqJnh_e",
                    "Hwvt-bSwZwj_KL-hxg0m-3_hUG",
                    "mQHzvcV-UL-o2O_1KhUJQo_G2_uryk3-a",
                    "b-UTn33HF",
                    "r-Ep-jY-aFM_N_H",
                    "K-k-krEZ0gwD_k_ua-9dm3Oy-s_v",
                    "XS_oS-p",
                    "EIx_h-zf5",
                    "p_z-0_i-omQCo3B",
                    "1_q0N_jo_9",
                    "0Ai-6-S",
                    "L-LZEp_HtW",
                    "Zj-A4JD_2A5Aj7_b-m3",
                    "x",
                    "p-qPuXQpp_d-jeKifB-c-7_G-X",
                    "X94cvJ_A",
                    "F2D25R_qk_W-w_Okf_kx",
                    "rc-f",
                    "D",
                    "gD_WrDfxmF-wu-E-U4t",
                    "Z_BN9O4_D9-D_0E_KnCwZF-84b-19",
                    "T-8_g-u-0_E",
                    "lXTtys9j_X_A_m-vtNiNMw_X_b-C6Nr",
                    "V_Ps-4Y-S",
                    "X5wGEA",
                    "mIbHFf_ALu4_Jo1Z1",
                    "ET-TacYx_c",
                    "Z-Lm5cAP_ri88-d_q_fi8-x",
                    "rTi2ah-4j_j_4AlxTs6m_8-g9zqncIf-N5",
                    "FBaLB85_u-0NxhAy-ZU_9c",
                    "x_j_l-5_aV95_s_tY_jp4",
                    "PL768_D-m7jNWjfD-Nl_7qvb_bs_8_Vg",
                    "9-yOc-gbh",
                    "6DYxZ_SL-S_Ye",
                    "ZCa-U-muib-6-d-f_oEh_O",
                    "Qt-S-o8340F_f_aGax-c-jbV0gfK_p",
                    "WE_SzOI_OGuoBDk-gDp",
                    "cs-Y_9",
                    "m1_uj",
                    "Y-ob_PT",
                    "li-B",
                    "f-2-7-9m_f8den_J_T_d",
                    "p-Os0dua-H_o-u",
                    "L",
                    "rby-w"];
    let failed: Vec<(&str, Result<LanguageTag>)> = tags.iter()
                                                       .map(|x| (*x, x.parse::<LanguageTag>()))
                                                       .filter(|x| x.1.is_ok())
                                                       .collect();
    println!("Number: { } Failed: {:?}", failed.len(), failed);
    assert!(failed.is_empty());
}

#[test]
fn test_canonicalize() {
    let langtag = langtag!(en;;;BU);
    assert_eq!(langtag.canonicalize().to_string(), "en-MM");
    let langtag = LanguageTag {
        language: Some("sgn-BE-FR".to_owned()),
        extlangs: vec![],
        script: None,
        region: None,
        variants: vec![],
        extensions: BTreeMap::new(),
        privateuse: vec![],
    };
    assert_eq!(langtag.canonicalize().to_string(), "sfb");
}
