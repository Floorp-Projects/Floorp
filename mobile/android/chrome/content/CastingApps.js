// -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "PageActions",
                                  "resource://gre/modules/PageActions.jsm");

// Define service devices. We should consider moving these to their respective
// JSM files, but we left them here to allow for better lazy JSM loading.
var rokuDevice = {
  id: "roku:ecp",
  target: "roku:ecp",
  factory: function(aService) {
    Cu.import("resource://gre/modules/RokuApp.jsm");
    return new RokuApp(aService);
  },
  types: ["video/mp4"],
  extensions: ["mp4"]
};

var mediaPlayerDevice = {
  id: "media:router",
  target: "media:router",
  factory: function(aService) {
    Cu.import("resource://gre/modules/MediaPlayerApp.jsm");
    return new MediaPlayerApp(aService);
  },
  types: ["video/mp4", "video/webm", "application/x-mpegurl"],
  extensions: ["mp4", "webm", "m3u", "m3u8"],
  init: function() {
    Services.obs.addObserver(this, "MediaPlayer:Added", false);
    Services.obs.addObserver(this, "MediaPlayer:Changed", false);
    Services.obs.addObserver(this, "MediaPlayer:Removed", false);
  },
  observe: function(subject, topic, data) {
    if (topic === "MediaPlayer:Added") {
      let service = this.toService(JSON.parse(data));
      SimpleServiceDiscovery.addService(service);
    } else if (topic === "MediaPlayer:Changed") {
      let service = this.toService(JSON.parse(data));
      SimpleServiceDiscovery.updateService(service);
    } else if (topic === "MediaPlayer:Removed") {
      SimpleServiceDiscovery.removeService(data);
    }
  },
  toService: function(display) {
    // Convert the native data into something matching what is created in _processService()
    return {
      location: display.location,
      target: "media:router",
      friendlyName: display.friendlyName,
      uuid: display.uuid,
      manufacturer: display.manufacturer,
      modelName: display.modelName,
      mirror: display.mirror
    };
  }
};

var CastingApps = {
  _castMenuId: -1,
  mirrorStartMenuId: -1,
  mirrorStopMenuId: -1,

  init: function ca_init() {
    if (!this.isCastingEnabled()) {
      return;
    }

    // Register targets
    SimpleServiceDiscovery.registerDevice(rokuDevice);

    // MediaPlayerDevice will notify us any time the native device list changes.
    mediaPlayerDevice.init();
    SimpleServiceDiscovery.registerDevice(mediaPlayerDevice);

    // Search for devices continuously every 120 seconds
    SimpleServiceDiscovery.search(120 * 1000);

    this._castMenuId = NativeWindow.contextmenus.add(
      Strings.browser.GetStringFromName("contextmenu.sendToDevice"),
      this.filterCast,
      this.handleContextMenu.bind(this)
    );

    Services.obs.addObserver(this, "Casting:Play", false);
    Services.obs.addObserver(this, "Casting:Pause", false);
    Services.obs.addObserver(this, "Casting:Stop", false);
    Services.obs.addObserver(this, "Casting:Mirror", false);
    Services.obs.addObserver(this, "ssdp-service-found", false);
    Services.obs.addObserver(this, "ssdp-service-lost", false);

    BrowserApp.deck.addEventListener("TabSelect", this, true);
    BrowserApp.deck.addEventListener("pageshow", this, true);
    BrowserApp.deck.addEventListener("playing", this, true);
    BrowserApp.deck.addEventListener("ended", this, true);
  },

  _mirrorStarted: function(stopMirrorCallback) {
    this.stopMirrorCallback = stopMirrorCallback;
    NativeWindow.menu.update(this.mirrorStartMenuId, { visible: false });
    NativeWindow.menu.update(this.mirrorStopMenuId, { visible: true });
  },

  serviceAdded: function(aService) {
    if (this.isMirroringEnabled() && aService.mirror && this.mirrorStartMenuId == -1) {
      this.mirrorStartMenuId = NativeWindow.menu.add({
        name: Strings.browser.GetStringFromName("casting.mirrorTab"),
        callback: function() {
          let callbackFunc = function(aService) {
            let app = SimpleServiceDiscovery.findAppForService(aService);
            if (app) {
              app.mirror(function() {}, window, BrowserApp.selectedTab.getViewport(), this._mirrorStarted.bind(this), window.BrowserApp.selectedBrowser.contentWindow);
            }
          }.bind(this);

          this.prompt(callbackFunc, aService => aService.mirror);
        }.bind(this),
        parent: NativeWindow.menu.toolsMenuID
      });

      this.mirrorStopMenuId = NativeWindow.menu.add({
        name: Strings.browser.GetStringFromName("casting.mirrorTabStop"),
        callback: function() {
          if (this.tabMirror) {
            this.tabMirror.stop();
            this.tabMirror = null;
          } else if (this.stopMirrorCallback) {
            this.stopMirrorCallback();
            this.stopMirrorCallback = null;
          }
          NativeWindow.menu.update(this.mirrorStartMenuId, { visible: true });
          NativeWindow.menu.update(this.mirrorStopMenuId, { visible: false });
        }.bind(this),
      });
    }
    if (this.mirrorStartMenuId != -1) {
      NativeWindow.menu.update(this.mirrorStopMenuId, { visible: false });
    }
  },

  serviceLost: function(aService) {
    if (aService.mirror && this.mirrorStartMenuId != -1) {
      let haveMirror = false;
      SimpleServiceDiscovery.services.forEach(function(service) {
        if (service.mirror) {
          haveMirror = true;
        }
      });
      if (!haveMirror) {
        NativeWindow.menu.remove(this.mirrorStartMenuId);
        this.mirrorStartMenuId = -1;
      }
    }
  },

  isCastingEnabled: function isCastingEnabled() {
    return Services.prefs.getBoolPref("browser.casting.enabled");
  },

  isMirroringEnabled: function isMirroringEnabled() {
    return Services.prefs.getBoolPref("browser.mirroring.enabled");
  },

  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
      case "Casting:Play":
        if (this.session && this.session.remoteMedia.status == "paused") {
          this.session.remoteMedia.play();
        }
        break;
      case "Casting:Pause":
        if (this.session && this.session.remoteMedia.status == "started") {
          this.session.remoteMedia.pause();
        }
        break;
      case "Casting:Stop":
        if (this.session) {
          this.closeExternal();
        }
        break;
      case "Casting:Mirror":
        {
          Cu.import("resource://gre/modules/TabMirror.jsm");
          this.tabMirror = new TabMirror(aData, window);
          NativeWindow.menu.update(this.mirrorStartMenuId, { visible: false });
          NativeWindow.menu.update(this.mirrorStopMenuId, { visible: true });
        }
        break;
      case "ssdp-service-found":
        {
          this.serviceAdded(SimpleServiceDiscovery.findServiceForID(aData));
          break;
        }
      case "ssdp-service-lost":
        {
          this.serviceLost(SimpleServiceDiscovery.findServiceForID(aData));
          break;
        }
    }
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "TabSelect": {
        let tab = BrowserApp.getTabForBrowser(aEvent.target);
        this._updatePageActionForTab(tab, aEvent);
        break;
      }
      case "pageshow": {
        let tab = BrowserApp.getTabForWindow(aEvent.originalTarget.defaultView);
        this._updatePageActionForTab(tab, aEvent);
        break;
      }
      case "playing":
      case "ended": {
        let video = aEvent.target;
        if (video instanceof HTMLVideoElement) {
          // If playing, send the <video>, but if ended we send nothing to shutdown the pageaction
          this._updatePageActionForVideo(aEvent.type === "playing" ? video : null);
        }
        break;
      }
    }
  },

  _sendEventToVideo: function _sendEventToVideo(aElement, aData) {
    let event = aElement.ownerDocument.createEvent("CustomEvent");
    event.initCustomEvent("media-videoCasting", false, true, JSON.stringify(aData));
    aElement.dispatchEvent(event);
  },

  handleVideoBindingAttached: function handleVideoBindingAttached(aTab, aEvent) {
    // Let's figure out if we have everything needed to cast a video. The binding
    // defaults to |false| so we only need to send an event if |true|.
    let video = aEvent.target;
    if (!(video instanceof HTMLVideoElement)) {
      return;
    }

    if (SimpleServiceDiscovery.services.length == 0) {
      return;
    }

    this.getVideo(video, 0, 0, (aBundle) => {
      // Let the binding know casting is allowed
      if (aBundle) {
        this._sendEventToVideo(aBundle.element, { allow: true });
      }
    });
  },

  handleVideoBindingCast: function handleVideoBindingCast(aTab, aEvent) {
    // The binding wants to start a casting session
    let video = aEvent.target;
    if (!(video instanceof HTMLVideoElement)) {
      return;
    }

    // Close an existing session first. closeExternal has checks for an exsting
    // session and handles remote and video binding shutdown.
    this.closeExternal();

    // Start the new session
    UITelemetry.addEvent("cast.1", "button", null);
    this.openExternal(video, 0, 0);
  },

  makeURI: function makeURI(aURL, aOriginCharset, aBaseURI) {
    return Services.io.newURI(aURL, aOriginCharset, aBaseURI);
  },

  allowableExtension: function(aURI, aExtensions) {
    return (aURI instanceof Ci.nsIURL) && aExtensions.indexOf(aURI.fileExtension) != -1;
  },

  allowableMimeType: function(aType, aTypes) {
    return aTypes.indexOf(aType) != -1;
  },

  // This method will look at the aElement (or try to find a video at aX, aY) that has
  // a castable source. If found, aCallback will be called with a JSON meta bundle. If
  // no castable source was found, aCallback is called with null.
  getVideo: function(aElement, aX, aY, aCallback) {
    let extensions = SimpleServiceDiscovery.getSupportedExtensions();
    let types = SimpleServiceDiscovery.getSupportedMimeTypes();

    // Fast path: Is the given element a video element?
    if (aElement instanceof HTMLVideoElement) {
      // If we found a video element, no need to look further, even if no
      // castable video source is found.
      this._getVideo(aElement, types, extensions, aCallback);
      return;
    }

    // Maybe this is an overlay, with the video element under it.
    // Use the (x, y) location to guess at a <video> element.

    // The context menu system will keep walking up the DOM giving us a chance
    // to find an element we match. When it hits <html> things can go BOOM.
    try {
      let elements = aElement.ownerDocument.querySelectorAll("video");
      for (let element of elements) {
        // Look for a video element contained in the overlay bounds
        let rect = element.getBoundingClientRect();
        if (aY >= rect.top && aX >= rect.left && aY <= rect.bottom && aX <= rect.right) {
          // Once we find a <video> under the overlay, we check it and exit.
          this._getVideo(element, types, extensions, aCallback);
          return;
        }
      }
    } catch(e) {}
  },

  _getContentTypeForURI: function(aURI, aElement, aCallback) {
    let channel = Services.io.newChannelFromURI2(aURI,
                                                 aElement,
                                                 null, // aLoadingPrincipal
                                                 null, // aTriggeringPrincipal
                                                 Ci.nsILoadInfo.SEC_NORMAL,
                                                 Ci.nsIContentPolicy.TYPE_OTHER);

    let listener = {
      onStartRequest: function(request, context) {
        switch (channel.responseStatus) {
          case 301:
          case 302:
          case 303:
            request.cancel(0);
            let location = channel.getResponseHeader("Location");
            CastingApps._getContentTypeForURI(CastingApps.makeURI(location), aElement, aCallback);
            break;
          default:
            aCallback(channel.contentType);
            request.cancel(0);
            break;
        }
      },
      onStopRequest: function(request, context, statusCode)  {},
      onDataAvailable: function(request, context, stream, offset, count) {}
    };
    channel.asyncOpen(listener, null)
  },

  // Because this method uses a callback, make sure we return ASAP if we know
  // we have a castable video source.
  _getVideo: function(aElement, aTypes, aExtensions, aCallback) {
    // Keep a list of URIs we need for an async mimetype check
    let asyncURIs = [];

    // Grab the poster attribute from the <video>
    let posterURL = aElement.poster;

    // First, look to see if the <video> has a src attribute
    let sourceURL = aElement.src;

    // If empty, try the currentSrc
    if (!sourceURL) {
      sourceURL = aElement.currentSrc;
    }

    if (sourceURL) {
      // Use the file extension to guess the mime type
      let sourceURI = this.makeURI(sourceURL, null, this.makeURI(aElement.baseURI));
      if (this.allowableExtension(sourceURI, aExtensions)) {
        aCallback({ element: aElement, source: sourceURI.spec, poster: posterURL, sourceURI: sourceURI});
        return;
      }

      if (aElement.type) {
        // Fast sync check
        if (this.allowableMimeType(aElement.type, aTypes)) {
          aCallback({ element: aElement, source: sourceURI.spec, poster: posterURL, sourceURI: sourceURI, type: aElement.type });
          return;
        }
      }

      // Delay the async check until we sync scan all possible URIs
      asyncURIs.push(sourceURI);
    }

    // Next, look to see if there is a <source> child element that meets
    // our needs
    let sourceNodes = aElement.getElementsByTagName("source");
    for (let sourceNode of sourceNodes) {
      let sourceURI = this.makeURI(sourceNode.src, null, this.makeURI(sourceNode.baseURI));

      // Using the type attribute is our ideal way to guess the mime type. Otherwise,
      // fallback to using the file extension to guess the mime type
      if (this.allowableExtension(sourceURI, aExtensions)) {
        aCallback({ element: aElement, source: sourceURI.spec, poster: posterURL, sourceURI: sourceURI, type: sourceNode.type });
        return;
      }

      if (sourceNode.type) {
        // Fast sync check
        if (this.allowableMimeType(sourceNode.type, aTypes)) {
          aCallback({ element: aElement, source: sourceURI.spec, poster: posterURL, sourceURI: sourceURI, type: sourceNode.type });
          return;
        }
      }

      // Delay the async check until we sync scan all possible URIs
      asyncURIs.push(sourceURI);
    }

    // If we didn't find a good URI directly, let's look using async methods
    // As soon as we find a good sourceURL, avoid firing the callback any further
    aCallback.fired = false;
    for (let sourceURI of asyncURIs) {
      // Do an async fetch to figure out the mimetype of the source video
      this._getContentTypeForURI(sourceURI, aElement, (aType) => {
        if (!aCallback.fired && this.allowableMimeType(aType, aTypes)) {
          aCallback.fired = true;
          aCallback({ element: aElement, source: sourceURI.spec, poster: posterURL, sourceURI: sourceURI, type: aType });
        }
      });
    }

    // If we didn't find any castable source, let's send back a signal
    if (!aCallback.fired) {
      aCallback(null);
    }
  },

  // This code depends on handleVideoBindingAttached setting mozAllowCasting
  // so we can quickly figure out if the video is castable
  isVideoCastable: function(aElement, aX, aY) {
    // Use the flag set when the <video> binding was created as the check
    if (aElement instanceof HTMLVideoElement) {
      return aElement.mozAllowCasting;
    }

    // This is called by the context menu system and the system will keep
    // walking up the DOM giving us a chance to find an element we match.
    // When it hits <html> things can go BOOM.
    try {
      // Maybe this is an overlay, with the video element under it
      // Use the (x, y) location to guess at a <video> element
      let elements = aElement.ownerDocument.querySelectorAll("video");
      for (let element of elements) {
        // Look for a video element contained in the overlay bounds
        let rect = element.getBoundingClientRect();
        if (aY >= rect.top && aX >= rect.left && aY <= rect.bottom && aX <= rect.right) {
          // Use the flag set when the <video> binding was created as the check
          return element.mozAllowCasting;
        }
      }
    } catch(e) {}

    return false;
  },

  filterCast: {
    matches: function(aElement, aX, aY) {
      // This behavior matches the pageaction: As long as a video is castable,
      // we can cast it, even if it's already being cast to a device.
      if (SimpleServiceDiscovery.services.length == 0)
        return false;
      return CastingApps.isVideoCastable(aElement, aX, aY);
    }
  },

  pageAction: {
    click: function() {
      // Since this is a pageaction, we use the selected browser
      let browser = BrowserApp.selectedBrowser;
      if (!browser) {
        return;
      }

      // Look for a castable <video> that is playing, and start casting it
      let videos = browser.contentDocument.querySelectorAll("video");
      for (let video of videos) {
        if (!video.paused && video.mozAllowCasting) {
          UITelemetry.addEvent("cast.1", "pageaction", null);
          CastingApps.openExternal(video, 0, 0);
          return;
        }
      }
    }
  },

  _findCastableVideo: function _findCastableVideo(aBrowser) {
      if (!aBrowser) {
        return null;
      }

      // Scan for a <video> being actively cast. Also look for a castable <video>
      // on the page.
      let castableVideo = null;
      let videos = aBrowser.contentDocument.querySelectorAll("video");
      for (let video of videos) {
        if (video.mozIsCasting) {
          // This <video> is cast-active. Break out of loop.
          return video;
        }

        if (!video.paused && video.mozAllowCasting) {
          // This <video> is cast-ready. Keep looking so cast-active could be found.
          castableVideo = video;
        }
      }

      // Could be null
      return castableVideo;
  },

  _updatePageActionForTab: function _updatePageActionForTab(aTab, aEvent) {
    // We only care about events on the selected tab
    if (aTab != BrowserApp.selectedTab) {
      return;
    }

    // Update the page action, scanning for a castable <video>
    this._updatePageAction();
  },

  _updatePageActionForVideo: function _updatePageActionForVideo(aVideo) {
    this._updatePageAction(aVideo);
  },

  _updatePageAction: function _updatePageAction(aVideo) {
    // Remove any exising pageaction first, in case state changes or we don't have
    // a castable video
    if (this.pageAction.id) {
      PageActions.remove(this.pageAction.id);
      delete this.pageAction.id;
    }

    if (!aVideo) {
      aVideo = this._findCastableVideo(BrowserApp.selectedBrowser);
      if (!aVideo) {
        return;
      }
    }

    // We only show pageactions if the <video> is from the selected tab
    if (BrowserApp.selectedTab != BrowserApp.getTabForWindow(aVideo.ownerDocument.defaultView.top)) {
      return;
    }

    // We check for two state here:
    // 1. The video is actively being cast
    // 2. The video is allowed to be cast and is currently playing
    // Both states have the same action: Show the cast page action
    if (aVideo.mozIsCasting) {
      this.pageAction.id = PageActions.add({
        title: Strings.browser.GetStringFromName("contextmenu.sendToDevice"),
        icon: "drawable://casting_active",
        clickCallback: this.pageAction.click,
        important: true
      });
    } else if (aVideo.mozAllowCasting) {
      this.pageAction.id = PageActions.add({
        title: Strings.browser.GetStringFromName("contextmenu.sendToDevice"),
        icon: "drawable://casting",
        clickCallback: this.pageAction.click,
        important: true
      });
    }
  },

  prompt: function(aCallback, aFilterFunc) {
    let items = [];
    let filteredServices = [];
    SimpleServiceDiscovery.services.forEach(function(aService) {
      let item = {
        label: aService.friendlyName,
        selected: false
      };
      if (!aFilterFunc || aFilterFunc(aService)) {
        filteredServices.push(aService);
        items.push(item);
      }
    });

    if (items.length == 0) {
      return;
    }

    let prompt = new Prompt({
      title: Strings.browser.GetStringFromName("casting.sendToDevice")
    }).setSingleChoiceItems(items).show(function(data) {
      let selected = data.button;
      let service = selected == -1 ? null : filteredServices[selected];
      if (aCallback)
        aCallback(service);
    });
  },

  handleContextMenu: function(aElement, aX, aY) {
    UITelemetry.addEvent("action.1", "contextmenu", null, "web_cast");
    UITelemetry.addEvent("cast.1", "contextmenu", null);
    this.openExternal(aElement, aX, aY);
  },

  openExternal: function(aElement, aX, aY) {
    // Start a second screen media service
    this.getVideo(aElement, aX, aY, this._openExternal.bind(this));
  },

  _openExternal: function(aVideo) {
    if (!aVideo) {
      return;
    }

    function filterFunc(aService) {
      return this.allowableExtension(aVideo.sourceURI, aService.extensions) || this.allowableMimeType(aVideo.type, aService.types);
    }

    this.prompt(function(aService) {
      if (!aService)
        return;

      // Make sure we have a player app for the given service
      let app = SimpleServiceDiscovery.findAppForService(aService);
      if (!app)
        return;

      if (aVideo.element) {
        aVideo.title = aVideo.element.ownerDocument.defaultView.top.document.title;

        // If the video is currently playing on the device, pause it
        if (!aVideo.element.paused) {
          aVideo.element.pause();
        }
      }

      app.stop(function() {
        app.start(function(aStarted) {
          if (!aStarted) {
            dump("CastingApps: Unable to start app");
            return;
          }

          app.remoteMedia(function(aRemoteMedia) {
            if (!aRemoteMedia) {
              dump("CastingApps: Failed to create remotemedia");
              return;
            }

            this.session = {
              service: aService,
              app: app,
              remoteMedia: aRemoteMedia,
              data: {
                title: aVideo.title,
                source: aVideo.source,
                poster: aVideo.poster
              },
              videoRef: Cu.getWeakReference(aVideo.element)
            };
          }.bind(this), this);
        }.bind(this));
      }.bind(this));
    }.bind(this), filterFunc.bind(this));
  },

  closeExternal: function() {
    if (!this.session) {
      return;
    }

    this.session.remoteMedia.shutdown();
    this._shutdown();
  },

  _shutdown: function() {
    if (!this.session) {
      return;
    }

    this.session.app.stop();
    let video = this.session.videoRef.get();
    if (video) {
      this._sendEventToVideo(video, { active: false });
      this._updatePageAction();
    }

    delete this.session;
  },

  // RemoteMedia callback API methods
  onRemoteMediaStart: function(aRemoteMedia) {
    if (!this.session) {
      return;
    }

    aRemoteMedia.load(this.session.data);
    Messaging.sendRequest({ type: "Casting:Started", device: this.session.service.friendlyName });

    let video = this.session.videoRef.get();
    if (video) {
      this._sendEventToVideo(video, { active: true });
      this._updatePageAction(video);
    }
  },

  onRemoteMediaStop: function(aRemoteMedia) {
    Messaging.sendRequest({ type: "Casting:Stopped" });
    this._shutdown();
  },

  onRemoteMediaStatus: function(aRemoteMedia) {
    if (!this.session) {
      return;
    }

    let status = aRemoteMedia.status;
    if (status == "completed") {
      this.closeExternal();
    }
  }
};
