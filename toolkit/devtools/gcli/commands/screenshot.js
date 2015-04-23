/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
const l10n = require("gcli/l10n");
const { Services } = require("resource://gre/modules/Services.jsm");

loader.lazyImporter(this, "Downloads", "resource://gre/modules/Downloads.jsm");
loader.lazyImporter(this, "LayoutHelpers", "resource://gre/modules/devtools/LayoutHelpers.jsm");
loader.lazyImporter(this, "Task", "resource://gre/modules/Task.jsm");
loader.lazyImporter(this, "OS", "resource://gre/modules/osfile.jsm");

const BRAND_SHORT_NAME = Cc["@mozilla.org/intl/stringbundle;1"]
                           .getService(Ci.nsIStringBundleService)
                           .createBundle("chrome://branding/locale/brand.properties")
                           .GetStringFromName("brandShortName");

// String used as an indication to generate default file name in the following
// format: "Screen Shot yyyy-mm-dd at HH.MM.SS.png"
const FILENAME_DEFAULT_VALUE = " ";

/*
 * There are 2 commands and 1 converter here. The 2 commands are nearly
 * identical except that one runs on the client and one in the server.
 *
 * The server command is hidden, and is designed to be called from the client
 * command when the --chrome flag is *not* used.
 */

/**
 * Both commands have the same initial filename parameter
 */
const filenameParam = {
  name: "filename",
  type: "string",
  defaultValue: FILENAME_DEFAULT_VALUE,
  description: l10n.lookup("screenshotFilenameDesc"),
  manual: l10n.lookup("screenshotFilenameManual")
};

/**
 * Both commands have the same set of standard optional parameters
 */
const standardParams = {
  group: l10n.lookup("screenshotGroupOptions"),
  params: [
    {
      name: "clipboard",
      type: "boolean",
      description: l10n.lookup("screenshotClipboardDesc"),
      manual: l10n.lookup("screenshotClipboardManual")
    },
    {
      name: "imgur",
      hidden: true, // Hidden because it fails with "Could not reach imgur API"
      type: "boolean",
      description: l10n.lookup("screenshotImgurDesc"),
      manual: l10n.lookup("screenshotImgurManual")
    },
    {
      name: "delay",
      type: { name: "number", min: 0 },
      defaultValue: 0,
      description: l10n.lookup("screenshotDelayDesc"),
      manual: l10n.lookup("screenshotDelayManual")
    },
    {
      name: "fullpage",
      type: "boolean",
      description: l10n.lookup("screenshotFullPageDesc"),
      manual: l10n.lookup("screenshotFullPageManual")
    },
    {
      name: "selector",
      type: "node",
      defaultValue: null,
      description: l10n.lookup("inspectNodeDesc"),
      manual: l10n.lookup("inspectNodeManual")
    }
  ]
};

exports.items = [
  {
    /**
     * Format an 'imageSummary' (as output by the screenshot command).
     * An 'imageSummary' is a simple JSON object that looks like this:
     *
     * {
     *   destinations: [ "..." ], // Required array of descriptions of the
     *                            // locations of the result image (the command
     *                            // can have multiple outputs)
     *   data: "...",             // Optional Base64 encoded image data
     *   width:1024, height:768,  // Dimensions of the image data, required
     *                            // if data != null
     *   filename: "...",         // If set, clicking the image will open the
     *                            // folder containing the given file
     *   href: "...",             // If set, clicking the image will open the
     *                            // link in a new tab
     * }
     */
    item: "converter",
    from: "imageSummary",
    to: "dom",
    exec: function(imageSummary, context) {
      const document = context.document;
      const root = document.createElement("div");

      // Add a line to the result for each destination
      imageSummary.destinations.forEach(destination => {
        const title = document.createElement("div");
        title.textContent = destination;
        root.appendChild(title);
      });

      // Add the thumbnail image
      if (imageSummary.data != null) {
        const image = context.document.createElement("div");
        const previewHeight = parseInt(256 * imageSummary.height / imageSummary.width);
        const style = "" +
            "width: 256px;" +
            "height: " + previewHeight + "px;" +
            "max-height: 256px;" +
            "background-image: url('" + imageSummary.data + "');" +
            "background-size: 256px " + previewHeight + "px;" +
            "margin: 4px;" +
            "display: block;";
        image.setAttribute("style", style);
        root.appendChild(image);
      }

      // Click handler
      if (imageSummary.filename) {
        root.style.cursor = "pointer";
        root.addEventListener("click", () => {
          if (imageSummary.filename) {
            const file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
            file.initWithPath(imageSummary.filename);
            file.reveal();
          }
        });
      }

      return root;
    }
  },
  {
    item: "command",
    runAt: "client",
    name: "screenshot",
    description: l10n.lookup("screenshotDesc"),
    manual: l10n.lookup("screenshotManual"),
    returnType: "imageSummary",
    buttonId: "command-button-screenshot",
    buttonClass: "command-button command-button-invertable",
    tooltipText: l10n.lookup("screenshotTooltip"),
    params: [
      filenameParam,
      standardParams,
      {
        group: l10n.lookup("screenshotAdvancedOptions"),
        params: [
          {
            name: "chrome",
            type: "boolean",
            description: l10n.lookupFormat("screenshotChromeDesc2", [BRAND_SHORT_NAME]),
            manual: l10n.lookupFormat("screenshotChromeManual2", [BRAND_SHORT_NAME])
          },
        ]
      },
    ],
    exec: function(args, context) {
      if (args.chrome && args.selector) {
        // Node screenshot with chrome option does not work as intended
        // Refer https://bugzilla.mozilla.org/show_bug.cgi?id=659268#c7
        // throwing for now.
        throw new Error(l10n.lookup("screenshotSelectorChromeConflict"));
      }

      if (!args.chrome) {
        // Re-execute the command on the server
        const command = context.typed.replace(/^screenshot/, "screenshot_server");
        return context.updateExec(command).then(output => {
          return output.error ? Promise.reject(output.data) : output.data;
        });
      }

      return processScreenshot(args, context.environment.chromeDocument);
    },
  },
  {
    item: "command",
    runAt: "server",
    name: "screenshot_server",
    hidden: true,
    returnType: "imageSummary",
    params: [ filenameParam, standardParams ],
    exec: function(args, context) {
      return processScreenshot(args, context.environment.document);
    },
  }
];

/**
 * This function simply handles the --delay argument before calling
 * processScreenshotNow
 */
function processScreenshot(args, document) {
  if (args.delay > 0) {
    return new Promise((resolve, reject) => {
      document.defaultView.setTimeout(() => {
        processScreenshotNow(args, document).then(resolve, reject);
      }, args.delay * 1000);
    });
  }
  else {
    return processScreenshotNow(args, document);
  }
}

/**
 * There are several possible destinations for the screenshot, SKIP is used
 * in processScreenshotNow() whenever one of them is not used
 */
const SKIP = Promise.resolve();

/**
 * This is just like exec, except the 'delay' has been handled already so
 * this is where we do that actual work of process the screenshot
 */
function processScreenshotNow(args, document) {
  const reply = createScreenshotData(document, args);

  const loadContext = document.defaultView
                            .QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIWebNavigation)
                            .QueryInterface(Ci.nsILoadContext);

  const fileNeeded = args.filename != FILENAME_DEFAULT_VALUE ||
                      (!args.imgur && !args.clipboard);

  return Promise.all([
    args.clipboard ? saveToClipboard(loadContext, reply) : SKIP,
    args.imgur     ? uploadToImgur(reply)                : SKIP,
    fileNeeded     ? saveToFile(loadContext, reply)      : SKIP,
  ]).then(() => reply);
}

/**
 * This does the dirty work of creating a base64 string out of an
 * area of the browser window
 */
function createScreenshotData(document, args) {
  const window = document.defaultView;
  let left = 0;
  let top = 0;
  let width;
  let height;
  const currentX = window.scrollX;
  const currentY = window.scrollY;

  if (args.fullpage) {
    // Bug 961832: GCLI screenshot shows fixed position element in wrong
    // position if we don't scroll to top
    window.scrollTo(0,0);
    width = window.innerWidth + window.scrollMaxX;
    height = window.innerHeight + window.scrollMaxY;
  }
  else if (args.selector) {
    const lh = new LayoutHelpers(window);
    ({ top, left, width, height }) = lh.getRect(args.selector, window);
  }
  else {
    left = window.scrollX;
    top = window.scrollY;
    width = window.innerWidth;
    height = window.innerHeight;
  }

  const winUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);
  const scrollbarHeight = {};
  const scrollbarWidth = {};
  winUtils.getScrollbarSize(false, scrollbarWidth, scrollbarHeight);
  width -= scrollbarWidth.value;
  height -= scrollbarHeight.value;

  const canvas = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
  canvas.width = width;
  canvas.height = height;
  const ctx = canvas.getContext("2d");
  ctx.drawWindow(window, left, top, width, height, "#fff");
  const data = canvas.toDataURL("image/png", "");

  // See comment above on bug 961832
  if (args.fullpage) {
    window.scrollTo(currentX, currentY);
  }

  return {
    destinations: [],
    data: data,
    height: height,
    width: width,
    filename: getFilename(args.filename),
  };
}

/**
 * We may have a filename specified in args, or we might have to generate
 * one.
 */
function getFilename(defaultName) {
  // Create a name for the file if not present
  if (defaultName != FILENAME_DEFAULT_VALUE) {
    return defaultName;
  }

  const date = new Date();
  let dateString = date.getFullYear() + "-" + (date.getMonth() + 1) +
                  "-" + date.getDate();
  dateString = dateString.split("-").map(function(part) {
    if (part.length == 1) {
      part = "0" + part;
    }
    return part;
  }).join("-");

  const timeString = date.toTimeString().replace(/:/g, ".").split(" ")[0];
  return l10n.lookupFormat("screenshotGeneratedFilename",
                           [ dateString, timeString ]) + ".png";
}

/**
 * Save the image data to the clipboard. This returns a promise, so it can
 * be treated exactly like imgur / file processing, but it's really sync
 * for now.
 */
function saveToClipboard(loadContext, reply) {
  try {
    const io = Cc["@mozilla.org/network/io-service;1"]
                  .getService(Ci.nsIIOService);
    const channel = io.newChannel2(reply.data, null, null,
                                   null,      // aLoadingNode
                                   Services.scriptSecurityManager.getSystemPrincipal(),
                                   null,      // aTriggeringPrincipal
                                   Ci.nsILoadInfo.SEC_NORMAL,
                                   Ci.nsIContentPolicy.TYPE_IMAGE);
    const input = channel.open();
    const imgTools = Cc["@mozilla.org/image/tools;1"]
                        .getService(Ci.imgITools);

    const container = {};
    imgTools.decodeImageData(input, channel.contentType, container);

    const wrapped = Cc["@mozilla.org/supports-interface-pointer;1"]
                      .createInstance(Ci.nsISupportsInterfacePointer);
    wrapped.data = container.value;

    const trans = Cc["@mozilla.org/widget/transferable;1"]
                    .createInstance(Ci.nsITransferable);
    trans.init(loadContext);
    trans.addDataFlavor(channel.contentType);
    trans.setTransferData(channel.contentType, wrapped, -1);

    const clip = Cc["@mozilla.org/widget/clipboard;1"]
                    .getService(Ci.nsIClipboard);
    clip.setData(trans, null, Ci.nsIClipboard.kGlobalClipboard);

    reply.destinations.push(l10n.lookup("screenshotCopied"));
  }
  catch (ex) {
    console.error(ex);
    reply.destinations.push(l10n.lookup("screenshotErrorCopying"));
  }

  return Promise.resolve();
}

/**
 * Upload screenshot data to Imgur, returning a promise of a URL (as a string)
 */
function uploadToImgur(reply) {
  return new Promise((resolve, reject) => {
    const xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                  .createInstance(Ci.nsIXMLHttpRequest);
    const fd = Cc["@mozilla.org/files/formdata;1"]
                  .createInstance(Ci.nsIDOMFormData);
    fd.append("image", reply.data.split(",")[1]);
    fd.append("type", "base64");
    fd.append("title", reply.filename);

    const postURL = Services.prefs.getCharPref("devtools.gcli.imgurUploadURL");
    const clientID = "Client-ID " + Services.prefs.getCharPref("devtools.gcli.imgurClientID");

    xhr.open("POST", postURL);
    xhr.setRequestHeader("Authorization", clientID);
    xhr.send(fd);
    xhr.responseType = "json";

    xhr.onreadystatechange = function() {
      if (xhr.readyState == 4) {
        if (xhr.status == 200) {
          reply.href = xhr.response.data.link;
          reply.destinations.push(l10n.lookupFormat("screenshotImgurError",
                                                    [ reply.href ]));
        }
        else {
          reply.destinations.push(l10n.lookup("screenshotImgurError"));
        }

        resolve();
      }
    }
  });
}

/**
 * Save the screenshot data to disk, returning a promise which
 * is resolved on completion
 */
function saveToFile(loadContext, reply) {
  return Task.spawn(function*() {
    try {
      let filename = reply.filename;
      // Check there is a .png extension to filename
      if (!filename.match(/.png$/i)) {
        filename += ".png";
      }

      // If the filename is relative, tack it onto the download directory
      if (!filename.match(/[\\\/]/)) {
        const preferredDir = yield Downloads.getPreferredDownloadsDirectory();
        filename = OS.Path.join(preferredDir, filename);
        reply.filename = filename;
      }

      const file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
      file.initWithPath(filename);

      const ioService = Cc["@mozilla.org/network/io-service;1"]
                        .getService(Ci.nsIIOService);

      const Persist = Ci.nsIWebBrowserPersist;
      const persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
                      .createInstance(Persist);
      persist.persistFlags = Persist.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
                             Persist.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;

      // TODO: UTF8? For an image?
      const source = ioService.newURI(reply.data, "UTF8", null);
      persist.saveURI(source, null, null, 0, null, null, file, loadContext);

      reply.destinations.push(l10n.lookup("screenshotSavedToFile") + " \"" + filename + "\"");
    }
    catch (ex) {
      console.error(ex);
      reply.destinations.push(l10n.lookup("screenshotErrorSavingToFile") + " " + filename);
    }
  });
}
