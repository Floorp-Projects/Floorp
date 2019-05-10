/* import-globals-from ../../../common/tests/unit/head_helpers.js */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { RemoteSettings } = ChromeUtils.import("resource://services-settings/remote-settings.js");
const { Downloader } = ChromeUtils.import("resource://services-settings/Attachments.jsm");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const RECORD = {
  attachment: {
    hash: "f41ed47d0f43325c9f089d03415c972ce1d3f1ecab6e4d6260665baf3db3ccee",
    size: 1597,
    filename: "test_file.pem",
    location: "main-workspace/some-collection/65650a0f-7c22-4c10-9744-2d67e301f5f4.pem",
    mimetype: "application/x-pem-file",
  },
};

let downloader;
let server;

function pathFromURL(url) {
  const uri = Services.io.newURI(url);
  const file = uri.QueryInterface(Ci.nsIFileURL).file;
  return file.path;
}

const PROFILE_URL = "file://" + OS.Path.split(OS.Constants.Path.localProfileDir).components.join("/");

function run_test() {
  server = new HttpServer();
  server.start(-1);
  registerCleanupFunction(() => server.stop(() => {}));

  server.registerDirectory("/cdn/main-workspace/some-collection/", do_get_file("test_attachments_downloader"));

  server.registerPathHandler("/v1/", (request, response) => {
    response.write(JSON.stringify({
      capabilities: {
        attachments: {
          base_url: `http://localhost:${server.identity.primaryPort}/cdn/`,
        },
      },
    }));
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.setStatusLine(null, 200, "OK");
  });

  Services.prefs.setCharPref("services.settings.server",
    `http://localhost:${server.identity.primaryPort}/v1`);

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

  Assert.equal(fileURL, PROFILE_URL + "/settings/main/some-collection/test_file.pem");
  Assert.ok(await OS.File.exists(localFilePath));
  const stat = await OS.File.stat(localFilePath);
  Assert.equal(stat.size, 1597);
});
add_task(clear_state);

add_task(async function test_file_is_redownloaded_if_size_does_not_match() {
  const fileURL = await downloader.download(RECORD);
  const localFilePath = pathFromURL(fileURL);
  await OS.File.writeAtomic(localFilePath, "bad-content", { encoding: "utf-8" });
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
  downloader._fetchAttachment = (url, destination) => {
    called++;
    return _fetchAttachment(url, destination);
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
  downloader._fetchAttachment = () => called++;

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

  Assert.ok(!await OS.File.exists(localFilePath));
  Assert.ok(!await OS.File.exists(downloader.baseFolder));
});
add_task(clear_state);

add_task(async function test_downloader_is_accessible_via_client() {
  const client = RemoteSettings("some-collection");

  const fileURL = await client.attachments.download(RECORD);

  Assert.equal(fileURL, [
    PROFILE_URL,
    "settings",
    client.bucketName,
    client.collectionName,
    RECORD.attachment.filename,
  ].join("/"));
});
add_task(clear_state);
