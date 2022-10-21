/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

// This test verifies that
// nsIMIMEService.validateFileNameForSaving sanitizes filenames
// properly with different flags.

"use strict";

add_task(async function validate_filename_method() {
  let mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

  function checkFilename(filename, flags, mime = "image/png") {
    return mimeService.validateFileNameForSaving(filename, mime, flags);
  }

  Assert.equal(checkFilename("basicfile.png", 0), "basicfile.png");
  Assert.equal(checkFilename("  whitespace.png  ", 0), "whitespace.png");
  Assert.equal(
    checkFilename(" .whitespaceanddots.png...", 0),
    "whitespaceanddots.png"
  );
  Assert.equal(
    checkFilename("  \u00a0 \u00a0 extrawhitespace.png  \u00a0 \u00a0 ", 0),
    "extrawhitespace.png"
  );
  Assert.equal(
    checkFilename("  filename  with  whitespace.png  ", 0),
    "filename with whitespace.png"
  );
  Assert.equal(checkFilename("\\path.png", 0), "_path.png");
  Assert.equal(
    checkFilename("\\path*and/$?~file.png", 0),
    "_path and_$ ~file.png"
  );
  Assert.equal(
    checkFilename(" \u180e whit\u180ee.png \u180e", 0),
    "whit\u180ee.png"
  );
  Assert.equal(checkFilename("簡単簡単簡単", 0), "簡単簡単簡単.png");
  Assert.equal(checkFilename(" happy\u061c\u2069.png", 0), "happy__.png");
  Assert.equal(
    checkFilename("12345678".repeat(31) + "abcdefgh.png", 0),
    "12345678".repeat(31) + "abc.png"
  );
  Assert.equal(
    checkFilename("簡単".repeat(41) + ".png", 0),
    "簡単".repeat(41) + ".png"
  );
  Assert.equal(
    checkFilename("簡単".repeat(42) + ".png", 0),
    "簡単".repeat(41) + "簡.png"
  );
  Assert.equal(
    checkFilename("簡単".repeat(56) + ".png", 0),
    "簡単".repeat(40) + "簡.png"
  );
  Assert.equal(checkFilename("café.png", 0), "café.png");
  Assert.equal(
    checkFilename("café".repeat(50) + ".png", 0),
    "café".repeat(50) + ".png"
  );
  Assert.equal(
    checkFilename("café".repeat(51) + ".png", 0),
    "café".repeat(50) + ".png"
  );

  Assert.equal(
    checkFilename("\u{100001}\u{100002}.png", 0),
    "\u{100001}\u{100002}.png"
  );
  Assert.equal(
    checkFilename("\u{100001}\u{100002}".repeat(31) + ".png", 0),
    "\u{100001}\u{100002}".repeat(31) + ".png"
  );
  Assert.equal(
    checkFilename("\u{100001}\u{100002}".repeat(32) + ".png", 0),
    "\u{100001}\u{100002}".repeat(30) + "\u{100001}.png"
  );

  Assert.equal(
    checkFilename("noextensionfile".repeat(16), 0),
    "noextensionfile".repeat(16) + ".png"
  );
  Assert.equal(
    checkFilename("noextensionfile".repeat(17), 0),
    "noextensionfile".repeat(16) + "noextension.png"
  );
  Assert.equal(
    checkFilename("noextensionfile".repeat(16) + "noextensionfil.", 0),
    "noextensionfile".repeat(16) + "noextension.png"
  );

  Assert.equal(checkFilename("  first  .png  ", 0), "first .png");
  Assert.equal(
    checkFilename(
      "  second  .png  ",
      mimeService.VALIDATE_DONT_COLLAPSE_WHITESPACE
    ),
    "second  .png"
  );

  // For whatever reason, the Android mime handler accepts the .jpeg
  // extension for image/png, so skip this test there.
  if (AppConstants.platform != "android") {
    Assert.equal(checkFilename("thi/*rd.jpeg", 0), "thi_ rd.png");
  }

  Assert.equal(
    checkFilename("f*\\ourth  file.jpg", mimeService.VALIDATE_SANITIZE_ONLY),
    "f _ourth file.jpg"
  );
  Assert.equal(
    checkFilename(
      "f*\\ift  h.jpe*\\g",
      mimeService.VALIDATE_SANITIZE_ONLY |
        mimeService.VALIDATE_DONT_COLLAPSE_WHITESPACE
    ),
    "f _ift  h.jpe _g"
  );
  Assert.equal(checkFilename("sixth.j  pe/*g", 0), "sixth.png");

  let repeatStr = "12345678".repeat(31);
  Assert.equal(
    checkFilename(
      repeatStr + "seventh.png",
      mimeService.VALIDATE_DONT_TRUNCATE
    ),
    repeatStr + "seventh.png"
  );
  Assert.equal(
    checkFilename(repeatStr + "seventh.png", 0),
    repeatStr + "sev.png"
  );

  // no filename, so index is used by default.
  Assert.equal(checkFilename(".png", 0), "index.png");

  // sanitization only, so index is not added, but initial period is stripped.
  Assert.equal(
    checkFilename(".png", mimeService.VALIDATE_SANITIZE_ONLY),
    "png"
  );

  // correct .png extension is applied.
  Assert.equal(checkFilename(".butterpecan.icecream", 0), "butterpecan.png");

  // sanitization only, so extension is not modified, but initial period is stripped.
  Assert.equal(
    checkFilename(".butterpecan.icecream", mimeService.VALIDATE_SANITIZE_ONLY),
    "butterpecan.icecream"
  );

  let ext = ".fairlyLongExtension";
  Assert.equal(
    checkFilename(repeatStr + ext, mimeService.VALIDATE_SANITIZE_ONLY),
    repeatStr.substring(0, 255 - ext.length) + ext
  );

  ext = "lo%?ng/invalid? ch\\ars";
  Assert.equal(
    checkFilename(repeatStr + ext, mimeService.VALIDATE_SANITIZE_ONLY),
    repeatStr + "lo% ng_"
  );

  ext = ".long/invalid%? ch\\ars";
  Assert.equal(
    checkFilename(repeatStr + ext, mimeService.VALIDATE_SANITIZE_ONLY),
    repeatStr.substring(0, 234) + ".long_invalid% ch_ars"
  );

  Assert.equal(
    checkFilename("test_ﾃｽﾄ_T\x83E\\S\x83T.png", 0),
    "test_ﾃｽﾄ_T E_S T.png"
  );
  Assert.equal(
    checkFilename("test_ﾃｽﾄ_T\x83E\\S\x83T.pﾃ\x83ng", 0),
    "test_ﾃｽﾄ_T E_S T.png"
  );

  // Check we don't invalidate surrogate pairs when trimming.
  Assert.equal(checkFilename("test😀", 0, ""), "test😀");
  Assert.equal(checkFilename("test😀😀", 0, ""), "test😀😀");

  // Now check some media types
  Assert.equal(
    mimeService.validateFileNameForSaving("video.ogg", "video/ogg", 0),
    "video.ogg",
    "video.ogg"
  );
  Assert.equal(
    mimeService.validateFileNameForSaving("video.ogv", "video/ogg", 0),
    "video.ogv",
    "video.ogv"
  );
  Assert.equal(
    mimeService.validateFileNameForSaving("video.ogt", "video/ogg", 0),
    "video.ogv",
    "video.ogt"
  );

  Assert.equal(
    mimeService.validateFileNameForSaving("audio.mp3", "audio/mpeg", 0),
    "audio.mp3",
    "audio.mp3"
  );
  Assert.equal(
    mimeService.validateFileNameForSaving("audio.mpega", "audio/mpeg", 0),
    "audio.mpega",
    "audio.mpega"
  );
  Assert.equal(
    mimeService.validateFileNameForSaving("audio.mp2", "audio/mpeg", 0),
    "audio.mp2",
    "audio.mp2"
  );

  let expected = "audio.mp3";
  if (AppConstants.platform == "linux") {
    expected = "audio.mpga";
  } else if (AppConstants.platform == "android") {
    expected = "audio.mp4";
  }

  Assert.equal(
    mimeService.validateFileNameForSaving("audio.mp4", "audio/mpeg", 0),
    expected,
    "audio.mp4"
  );

  Assert.equal(
    mimeService.validateFileNameForSaving("sound.m4a", "audio/mp4", 0),
    "sound.m4a",
    "sound.m4a"
  );
  Assert.equal(
    mimeService.validateFileNameForSaving("sound.m4b", "audio/mp4", 0),
    AppConstants.platform == "android" ? "sound.m4a" : "sound.m4b",
    "sound.m4b"
  );
  Assert.equal(
    mimeService.validateFileNameForSaving("sound.m4c", "audio/mp4", 0),
    AppConstants.platform == "macosx" ? "sound.mp4" : "sound.m4a",
    "sound.mpc"
  );

  // This has a long filename with a 13 character extension. The end of the filename should be
  // cropped to fit into 255 bytes.
  Assert.equal(
    mimeService.validateFileNameForSaving(
      "라이브9.9만 시청컬처렐 다이제스티브 3박스 - 3박스 더 (뚱랑이 굿즈 증정) - 선물용 쇼핑백 2장컬처렐 다이제스티브 3박스 - 3박스 더 (뚱랑이 굿즈 증정) - 선물용 쇼핑백 2장24%102 000원 브랜드데이 앵콜 🎁 1.등 유산균 컬처렐 특가!",
      "text/unknown",
      mimeService.VALIDATE_SANITIZE_ONLY
    ),
    "라이브9.9만 시청컬처렐 다이제스티브 3박스 - 3박스 더 (뚱랑이 굿즈 증정) - 선물용 쇼핑백 2장컬처렐 다이제스티브 3박스 - 3박스 더 (뚱랑이 굿즈 .등 유산균 컬처렐 특가!",
    "very long filename with extension"
  );

  // This filename has a very long extension, almost the entire filename.
  Assert.equal(
    mimeService.validateFileNameForSaving(
      "라이브9.9만 시청컬처렐 다이제스티브 3박스 - 3박스 더 (뚱랑이 굿즈 증정) - 선물용 쇼핑백 2장컬처렐 다이제스티브 3박스 - 3박스 더 (뚱랑이 굿즈 증정) - 선물용 쇼핑백 2장24%102 000원 브랜드데이 앵콜 🎁 1등 유산균 컬처렐 특가!",
      "text/unknown",
      mimeService.VALIDATE_SANITIZE_ONLY
    ),
    "라이브9",
    "another very long filename with long extension"
  );

  // This filename is cropped at 254 bytes.
  Assert.equal(
    mimeService.validateFileNameForSaving(
      ".라이브99만 시청컬처렐 다이제스티브 3박스 - 3박스 더 (뚱랑이 굿즈 증정) - 선물용 쇼핑백 2장컬처렐 다이제스티브 3박스 - 3박스 더 (뚱랑이 굿즈 증정) - 선물용 쇼핑백 2장24%102 000원 브랜드데이 앵콜 🎁 1등 유산균 컬처렐 특가!",
      "text/unknown",
      mimeService.VALIDATE_SANITIZE_ONLY
    ),
    "라이브99만 시청컬처렐 다이제스티브 3박스 - 3박스 더 (뚱랑이 굿즈 증정) - 선물용 쇼핑백 2장컬처렐 다이제스티브 3박스 - 3박스 더 (뚱랑이 굿즈 증정) - 선물용 쇼핑백 2장24%102 000원 브랜드데",
    "very filename with extension only"
  );
});
