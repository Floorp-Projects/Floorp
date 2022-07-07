/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineModuleGetter(
  lazy,
  "RemoteSettings",
  "resource://services-settings/remote-settings.js"
);

const COLLECTION_NAME = "tracking-protection-lists";

// SafeBrowsing protocol parameters.
const SBRS_UPDATE_MINIMUM_DELAY = 21600; // Minimum delay before polling again in seconds

function UrlClassifierRemoteSettingsService() {}
UrlClassifierRemoteSettingsService.prototype = {
  classID: Components.ID("{1980624c-c50b-4b46-a91c-dfaba7792706}"),
  QueryInterface: ChromeUtils.generateQI([
    "nsIUrlClassifierRemoteSettingsService",
  ]),

  _initialized: false,

  // Entries that are retrieved from RemoteSettings.get(). keyed by the table name.
  _entries: {},

  async lazyInit() {
    if (this._initialized) {
      return;
    }

    let rs = lazy.RemoteSettings(COLLECTION_NAME);
    // Bug 1750191: Notify listmanager to trigger an update instead
    // of polling data periodically.
    rs.on("sync", async event => {
      let {
        data: { current },
      } = event;
      this._onUpdateEntries(current);
    });

    this._initialized = true;

    let entries;
    try {
      entries = await rs.get();
    } catch (e) {}

    this._onUpdateEntries(entries || []);
  },

  _onUpdateEntries(aEntries) {
    aEntries.map(entry => {
      this._entries[entry.Name] = entry;
    });
  },

  // Parse the update request. See UrlClassifierListManager.jsm makeUpdateRequest
  // for more details about how we build the update request.
  //
  // @param aRequest the request payload of the update request
  // @return array The array of requested tables. Each item in the array is composed
  //               with [table name, chunk numner]
  _parseRequest(aRequest) {
    let lines = aRequest.split("\n");
    let requests = [];
    for (let line of lines) {
      let fields = line.split(";");
      let chunkNum = fields[1]?.match(/(?<=a:).*/);
      requests.push([fields[0], chunkNum]);
    }
    return requests;
  },

  async _getLists(aRequest, aListener) {
    await this.lazyInit();

    let rs = lazy.RemoteSettings(COLLECTION_NAME);
    let payload = "n:" + SBRS_UPDATE_MINIMUM_DELAY + "\n";

    let requests = this._parseRequest(aRequest);
    for (let request of requests) {
      let [reqTableName, reqChunkNum] = request;
      let entry = this._entries[reqTableName];
      if (!entry?.attachment) {
        continue;
      }

      // If the request version is the same as what we have in Remote Settings,
      // we are up-to-date now.
      if (entry.Version == reqChunkNum) {
        continue;
      }

      let downloadError = false;
      try {
        // SafeBrowsing maintains its own files, so we can remove the downloaded
        // files after SafeBrowsing processes the data.
        let buffer = await rs.attachments.downloadAsBytes(entry);
        let bytes = new Uint8Array(buffer);
        let strData = String.fromCharCode.apply(String, bytes);

        // Construct the payload
        payload += "i:" + reqTableName + "\n";
        payload += strData;
      } catch (e) {
        downloadError = true;
      }

      if (downloadError) {
        try {
          aListener.onStartRequest(null);
          aListener.onStopRequest(null, Cr.NS_ERROR_FAILURE);
        } catch (e) {}
        return;
      }
    }

    // Send the update response over stream listener interface
    let stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
      Ci.nsIStringInputStream
    );
    stream.setData(payload, payload.length);

    aListener.onStartRequest(null);
    aListener.onDataAvailable(null, stream, 0, payload.length);
    aListener.onStopRequest(null, Cr.NS_OK);
  },

  fetchList(aPayload, aListener) {
    this._getLists(aPayload, aListener);
  },

  clear() {
    this._initialized = false;
    this._entries = {};
  },
};

const EXPORTED_SYMBOLS = [
  "UrlClassifierRemoteSettingsService",
  "SBRS_UPDATE_MINIMUM_DELAY",
];
