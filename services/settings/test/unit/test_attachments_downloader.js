/* import-globals-from ../../../common/tests/unit/head_helpers.js */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { UptakeTelemetry } = ChromeUtils.import(
  "resource://services-common/uptake-telemetry.js"
);
const { Downloader } = ChromeUtils.import(
  "resource://services-settings/Attachments.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const RECORD = {
  id: "1f3a0802-648d-11ea-bd79-876a8b69c377",
  attachment: {
    hash: "f41ed47d0f43325c9f089d03415c972ce1d3f1ecab6e4d6260665baf3db3ccee",
    size: 1597,
    filename: "test_file.pem",
    location:
      "main-workspace/some-collection/65650a0f-7c22-4c10-9744-2d67e301f5f4.pem",
    mimetype: "application/x-pem-file",
  },
};

const RECORD_OF_DUMP = {
  id: "filename-of-dump.txt",
  attachment: {
    filename: "filename-of-dump.txt",
    hash: "4c46ef7e4f1951d210fe54c21e07c09bab265fd122580083ed1d6121547a8c6b",
    size: 25,
  },
  some_key: "some metadata",
};

let downloader;
let server;

function pathFromURL(url) {
  const uri = Services.io.newURI(url);
  const file = uri.QueryInterface(Ci.nsIFileURL).file;
  return file.path;
}

const PROFILE_URL =
  "file://" +
  OS.Path.split(OS.Constants.Path.localProfileDir).components.join("/");

function run_test() {
  server = new HttpServer();
  server.start(-1);
  registerCleanupFunction(() => server.stop(() => {}));

  server.registerDirectory(
    "/cdn/main-workspace/some-collection/",
    do_get_file("test_attachments_downloader")
  );

  server.registerPathHandler("/v1/", (request, response) => {
    response.write(
      JSON.stringify({
        capabilities: {
          attachments: {
            base_url: `http://localhost:${server.identity.primaryPort}/cdn/`,
          },
        },
      })
    );
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.setStatusLine(null, 200, "OK");
  });

  Services.prefs.setCharPref(
    "services.settings.server",
    `http://localhost:${server.identity.primaryPort}/v1`
  );

  run_next_test();
}

async function clear_state() {
  downloader = new Downloader("main", "some-collection");
  await downloader.delete(RECORD);
}

add_task(clear_state);

add_task(async function test_download_writes_file_in_profile() {
  const fileURL = await downloader.download(RECORD);
  const localFilePath = pathFromURL(fileURL);

  Assert.equal(
    fileURL,
    PROFILE_URL + "/settings/main/some-collection/test_file.pem"
  );
  Assert.ok(await OS.File.exists(localFilePath));
  const stat = await OS.File.stat(localFilePath);
  Assert.equal(stat.size, 1597);
});
add_task(clear_state);

add_task(async function test_download_as_bytes() {
  const bytes = await downloader.downloadAsBytes(RECORD);

  // See *.pem file in tests data.
  Assert.ok(bytes.byteLength > 1500, `Wrong bytes size: ${bytes.byteLength}`);
});
add_task(clear_state);

add_task(async function test_file_is_redownloaded_if_size_does_not_match() {
  const fileURL = await downloader.download(RECORD);
  const localFilePath = pathFromURL(fileURL);
  await OS.File.writeAtomic(localFilePath, "bad-content", {
    encoding: "utf-8",
  });
  let stat = await OS.File.stat(localFilePath);
  Assert.notEqual(stat.size, 1597);

  await downloader.download(RECORD);

  stat = await OS.File.stat(localFilePath);
  Assert.equal(stat.size, 1597);
});
add_task(clear_state);

add_task(async function test_file_is_redownloaded_if_corrupted() {
  const fileURL = await downloader.download(RECORD);
  const localFilePath = pathFromURL(fileURL);
  const byteArray = await OS.File.read(localFilePath);
  byteArray[0] = 42;
  await OS.File.writeAtomic(localFilePath, byteArray);
  let content = await OS.File.read(localFilePath, { encoding: "utf-8" });
  Assert.notEqual(content.slice(0, 5), "-----");

  await downloader.download(RECORD);

  content = await OS.File.read(localFilePath, { encoding: "utf-8" });
  Assert.equal(content.slice(0, 5), "-----");
});
add_task(clear_state);

add_task(async function test_download_is_retried_3_times_if_download_fails() {
  const record = {
    attachment: {
      ...RECORD.attachment,
      location: "404-error.pem",
    },
  };

  let called = 0;
  const _fetchAttachment = downloader._fetchAttachment;
  downloader._fetchAttachment = async url => {
    called++;
    return _fetchAttachment(url);
  };

  let error;
  try {
    await downloader.download(record);
  } catch (e) {
    error = e;
  }

  Assert.equal(called, 4); // 1 + 3 retries
  Assert.ok(error instanceof Downloader.DownloadError);
});
add_task(clear_state);

add_task(async function test_download_is_retried_3_times_if_content_fails() {
  const record = {
    attachment: {
      ...RECORD.attachment,
      hash: "always-wrong",
    },
  };
  let called = 0;
  downloader._fetchAttachment = async () => {
    called++;
    return new ArrayBuffer();
  };

  let error;
  try {
    await downloader.download(record);
  } catch (e) {
    error = e;
  }

  Assert.equal(called, 4); // 1 + 3 retries
  Assert.ok(error instanceof Downloader.BadContentError);
});
add_task(clear_state);

add_task(async function test_delete_removes_local_file() {
  const fileURL = await downloader.download(RECORD);
  const localFilePath = pathFromURL(fileURL);
  Assert.ok(await OS.File.exists(localFilePath));

  downloader.delete(RECORD);

  Assert.ok(!(await OS.File.exists(localFilePath)));
  Assert.ok(
    !(await OS.File.exists(
      OS.Path.join(OS.Constants.Path.localProfileDir, ...downloader.folders)
    ))
  );
});
add_task(clear_state);

add_task(async function test_delete_all() {
  const client = RemoteSettings("some-collection");
  await client.db.create(RECORD);
  const fileURL = await downloader.download(RECORD);
  const localFilePath = pathFromURL(fileURL);
  Assert.ok(await OS.File.exists(localFilePath));

  await client.attachments.deleteAll();

  Assert.ok(!(await OS.File.exists(localFilePath)));
});
add_task(clear_state);

add_task(async function test_downloader_is_accessible_via_client() {
  const client = RemoteSettings("some-collection");

  const fileURL = await client.attachments.download(RECORD);

  Assert.equal(
    fileURL,
    [
      PROFILE_URL,
      "settings",
      client.bucketName,
      client.collectionName,
      RECORD.attachment.filename,
    ].join("/")
  );
});
add_task(clear_state);

add_task(async function test_downloader_reports_download_errors() {
  await withFakeChannel("nightly", async () => {
    const client = RemoteSettings("some-collection");

    const record = {
      attachment: {
        ...RECORD.attachment,
        location: "404-error.pem",
      },
    };

    try {
      await client.attachments.download(record, { retry: 0 });
    } catch (e) {}

    TelemetryTestUtils.assertEvents([
      [
        "uptake.remotecontent.result",
        "uptake",
        "remotesettings",
        UptakeTelemetry.STATUS.DOWNLOAD_ERROR,
        {
          source: client.identifier,
        },
      ],
    ]);
  });
});
add_task(clear_state);

add_task(async function test_downloader_reports_offline_error() {
  const backupOffline = Services.io.offline;
  Services.io.offline = true;

  await withFakeChannel("nightly", async () => {
    try {
      const client = RemoteSettings("some-collection");
      const record = {
        attachment: {
          ...RECORD.attachment,
          location: "will-try-and-fail.pem",
        },
      };
      try {
        await client.attachments.download(record, { retry: 0 });
      } catch (e) {}

      TelemetryTestUtils.assertEvents([
        [
          "uptake.remotecontent.result",
          "uptake",
          "remotesettings",
          UptakeTelemetry.STATUS.NETWORK_OFFLINE_ERROR,
          {
            source: client.identifier,
          },
        ],
      ]);
    } finally {
      Services.io.offline = backupOffline;
    }
  });
});
add_task(clear_state);

add_task(async function test_download_cached() {
  const client = RemoteSettings("main", "some-collection");
  const attachmentId = "dummy filename";
  const badRecord = {
    attachment: {
      ...RECORD.attachment,
      hash: "non-matching hash",
      location: "non-existing-location-should-fail.bin",
    },
  };
  async function downloadWithCache(record, options) {
    options = { ...options, useCache: true };
    return client.attachments.download(record, options);
  }
  function checkInfo(downloadResult, expectedSource, msg) {
    Assert.deepEqual(
      downloadResult.record,
      RECORD,
      `${msg} : expected identical record`
    );
    // Simple check: assume that content is identical if the size matches.
    Assert.equal(
      downloadResult.buffer.byteLength,
      RECORD.attachment.size,
      `${msg} : expected buffer`
    );
    Assert.equal(
      downloadResult._source,
      expectedSource,
      `${msg} : expected source of the result`
    );
  }

  await Assert.rejects(
    downloadWithCache(null, { attachmentId }),
    /DownloadError: Could not download dummy filename/,
    "Download without record or cache should fail."
  );

  // Populate cache.
  const info1 = await downloadWithCache(RECORD, { attachmentId });
  checkInfo(info1, "remote_match", "first time download");

  await Assert.rejects(
    downloadWithCache(null, { attachmentId }),
    /DownloadError: Could not download dummy filename/,
    "Download without record still fails even if there is a cache."
  );

  await Assert.rejects(
    downloadWithCache(badRecord, { attachmentId }),
    /DownloadError: Could not download .*non-existing-location-should-fail.bin/,
    "Download with non-matching record still fails even if there is a cache."
  );

  // Download from cache.
  const info2 = await downloadWithCache(RECORD, { attachmentId });
  checkInfo(info2, "cache_match", "download matching record from cache");

  const info3 = await downloadWithCache(RECORD, {
    attachmentId,
    fallbackToCache: true,
  });
  checkInfo(info3, "cache_match", "fallbackToCache accepts matching record");

  const info4 = await downloadWithCache(null, {
    attachmentId,
    fallbackToCache: true,
  });
  checkInfo(info4, "cache_fallback", "fallbackToCache accepts null record");

  const info5 = await downloadWithCache(badRecord, {
    attachmentId,
    fallbackToCache: true,
  });
  checkInfo(info5, "cache_fallback", "fallbackToCache ignores bad record");

  // Bye bye cache.
  await client.attachments.deleteCached(attachmentId);
  await Assert.rejects(
    downloadWithCache(null, { attachmentId, fallbackToCache: true }),
    /DownloadError: Could not download dummy filename/,
    "Download without cache should fail again."
  );
  await Assert.rejects(
    downloadWithCache(badRecord, { attachmentId, fallbackToCache: true }),
    /DownloadError: Could not download .*non-existing-location-should-fail.bin/,
    "Download should fail to fall back to a download of a non-existing record"
  );
});
add_task(clear_state);

add_task(async function test_download_from_dump() {
  const bucketNamePref = "services.testing.custom-bucket-name-in-this-test";
  Services.prefs.setCharPref(bucketNamePref, "dump-bucket");
  const client = RemoteSettings("dump-collection", { bucketNamePref });

  // Temporarily replace the resource:-URL with another resource:-URL.
  const orig_RESOURCE_BASE_URL = Downloader._RESOURCE_BASE_URL;
  Downloader._RESOURCE_BASE_URL = "resource://rs-downloader-test";
  const resProto = Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);
  resProto.setSubstitution(
    "rs-downloader-test",
    Services.io.newFileURI(do_get_file("test_attachments_downloader"))
  );

  function checkInfo(result, expectedSource) {
    Assert.equal(
      new TextDecoder().decode(new Uint8Array(result.buffer)),
      "This would be a RS dump.\n",
      "expected content from dump"
    );
    Assert.deepEqual(result.record, RECORD_OF_DUMP, "expected record for dump");
    Assert.equal(result._source, expectedSource, "expected source of dump");
  }

  // If record matches, should happen before network request.
  const dump1 = await client.attachments.download(RECORD_OF_DUMP, {
    // Note: attachmentId not set, so should fall back to record.id.
    useCache: true,
    fallbackToDump: true,
  });
  checkInfo(dump1, "dump_match");

  // If no record given, should try network first, but then fall back to dump.
  const dump2 = await client.attachments.download(null, {
    attachmentId: RECORD_OF_DUMP.id,
    useCache: true,
    fallbackToDump: true,
  });
  checkInfo(dump2, "dump_fallback");

  await Assert.rejects(
    client.attachments.download(null, {
      attachmentId: "filename-without-meta.txt",
      useCache: true,
      fallbackToDump: true,
    }),
    /Could not download resource:\/\/rs-downloader-test\/settings\/dump-bucket\/dump-collection\/filename-without-meta\.txt/,
    "Cannot download dump that lacks a .meta.json file"
  );

  await Assert.rejects(
    client.attachments.download(null, {
      attachmentId: "filename-without-content.txt",
      useCache: true,
      fallbackToDump: true,
    }),
    /Could not download resource:\/\/rs-downloader-test\/settings\/dump-bucket\/dump-collection\/filename-without-content\.txt/,
    "Cannot download dump that is missing, despite the existing .meta.json"
  );

  // Restore, just in case.
  Downloader._RESOURCE_BASE_URL = orig_RESOURCE_BASE_URL;
  resProto.setSubstitution("rs-downloader-test", null);
});
// Not really needed because the last test doesn't modify the main collection,
// but added for consistency with other tests tasks around here.
add_task(clear_state);
