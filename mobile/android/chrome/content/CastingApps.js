// -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Define service targets. We should consider moving these to their respective
// JSM files, but we left them here to allow for better lazy JSM loading.
var rokuTarget = {
  target: "roku:ecp",
  factory: function(aService) {
    Cu.import("resource://gre/modules/RokuApp.jsm");
    return new RokuApp(aService);
  },
  types: ["video/mp4"],
  extensions: ["mp4"]
};

var fireflyTarget = {
  target: "urn:dial-multiscreen-org:service:dial:1",
  filters: {
    server: null,
    modelName: "Eureka Dongle"
  },
  factory: function(aService) {
    Cu.import("resource://gre/modules/FireflyApp.jsm");
    return new FireflyApp(aService);
  },
  types: ["video/mp4", "video/webm"],
  extensions: ["mp4", "webm"]
};

var mediaPlayerTarget = {
  target: "media:router",
  factory: function(aService) {
    Cu.import("resource://gre/modules/MediaPlayerApp.jsm");
    return new MediaPlayerApp(aService);
  },
  types: ["video/mp4", "video/webm", "application/x-mpegurl"],
  extensions: ["mp4", "webm", "m3u", "m3u8"]
};

var CastingApps = {
  _castMenuId: -1,

  init: function ca_init() {
    if (!this.isEnabled()) {
      return;
    }

    // Register targets
    SimpleServiceDiscovery.registerTarget(rokuTarget);
    SimpleServiceDiscovery.registerTarget(fireflyTarget);
    SimpleServiceDiscovery.registerTarget(mediaPlayerTarget);

    // Search for devices continuously every 120 seconds
    SimpleServiceDiscovery.search(120 * 1000);

    this._castMenuId = NativeWindow.contextmenus.add(
      Strings.browser.GetStringFromName("contextmenu.castToScreen"),
      this.filterCast,
      this.handleContextMenu.bind(this)
    );

    Services.obs.addObserver(this, "Casting:Play", false);
    Services.obs.addObserver(this, "Casting:Pause", false);
    Services.obs.addObserver(this, "Casting:Stop", false);

    BrowserApp.deck.addEventListener("TabSelect", this, true);
    BrowserApp.deck.addEventListener("pageshow", this, true);
    BrowserApp.deck.addEventListener("playing", this, true);
    BrowserApp.deck.addEventListener("ended", this, true);
  },

  uninit: function ca_uninit() {
    BrowserApp.deck.removeEventListener("TabSelect", this, true);
    BrowserApp.deck.removeEventListener("pageshow", this, true);
    BrowserApp.deck.removeEventListener("playing", this, true);
    BrowserApp.deck.removeEventListener("ended", this, true);

    Services.obs.removeObserver(this, "Casting:Play");
    Services.obs.removeObserver(this, "Casting:Pause");
    Services.obs.removeObserver(this, "Casting:Stop");

    NativeWindow.contextmenus.remove(this._castMenuId);
  },

  isEnabled: function isEnabled() {
    return Services.prefs.getBoolPref("browser.casting.enabled");
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

    if (!this.getVideo(video, 0, 0)) {
      return;
    }

    // Let the binding know casting is allowed
    this._sendEventToVideo(video, { allow: true });
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

  getVideo: function(aElement, aX, aY) {
    let extensions = SimpleServiceDiscovery.getSupportedExtensions();
    let types = SimpleServiceDiscovery.getSupportedMimeTypes();

    // Fast path: Is the given element a video element
    let video = this._getVideo(aElement, types, extensions);
    if (video) {
      return video;
    }

    // The context menu system will keep walking up the DOM giving us a chance
    // to find an element we match. When it hits <html> things can go BOOM.
    try {
      // Maybe this is an overlay, with the video element under it
      // Use the (x, y) location to guess at a <video> element
      let elements = aElement.ownerDocument.querySelectorAll("video");
      for (let element of elements) {
        // Look for a video element contained in the overlay bounds
        let rect = element.getBoundingClientRect();
        if (aY >= rect.top && aX >= rect.left && aY <= rect.bottom && aX <= rect.right) {
          video = this._getVideo(element, types, extensions);
          if (video) {
            break;
          }
        }
      }
    } catch(e) {}

    // Could be null
    return video;
  },

  _getVideo: function(aElement, aTypes, aExtensions) {
    if (!(aElement instanceof HTMLVideoElement)) {
      return null;
    }


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
        return { element: aElement, source: sourceURI.spec, poster: posterURL, sourceURI: sourceURI};
      }
    }

    // Next, look to see if there is a <source> child element that meets
    // our needs
    let sourceNodes = aElement.getElementsByTagName("source");
    for (let sourceNode of sourceNodes) {
      let sourceURI = this.makeURI(sourceNode.src, null, this.makeURI(sourceNode.baseURI));

      // Using the type attribute is our ideal way to guess the mime type. Otherwise,
      // fallback to using the file extension to guess the mime type
      if (this.allowableMimeType(sourceNode.type, aTypes) || this.allowableExtension(sourceURI, aExtensions)) {
        return { element: aElement, source: sourceURI.spec, poster: posterURL, sourceURI: sourceURI, type: sourceNode.type };
      }
    }

    return null;
  },

  filterCast: {
    matches: function(aElement, aX, aY) {
      if (SimpleServiceDiscovery.services.length == 0)
        return false;
      let video = CastingApps.getVideo(aElement, aX, aY);
      if (CastingApps.session) {
        return (video && CastingApps.session.data.source != video.source);
      }
      return (video != null);
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
      NativeWindow.pageactions.remove(this.pageAction.id);
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
      this.pageAction.id = NativeWindow.pageactions.add({
        title: Strings.browser.GetStringFromName("contextmenu.castToScreen"),
        icon: "drawable://casting_active",
        clickCallback: this.pageAction.click,
        important: true
      });
    } else if (aVideo.mozAllowCasting) {
      this.pageAction.id = NativeWindow.pageactions.add({
        title: Strings.browser.GetStringFromName("contextmenu.castToScreen"),
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

    let prompt = new Prompt({
      title: Strings.browser.GetStringFromName("casting.prompt")
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
    let video = this.getVideo(aElement, aX, aY);
    if (!video) {
      return;
    }

    function filterFunc(service) {
      return this.allowableExtension(video.sourceURI, service.extensions) || this.allowableMimeType(video.type, service.types);
    }

    this.prompt(function(aService) {
      if (!aService)
        return;

      // Make sure we have a player app for the given service
      let app = SimpleServiceDiscovery.findAppForService(aService);
      if (!app)
        return;

      video.title = aElement.ownerDocument.defaultView.top.document.title;
      if (video.element) {
        // If the video is currently playing on the device, pause it
        if (!video.element.paused) {
          video.element.pause();
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
                title: video.title,
                source: video.source,
                poster: video.poster
              },
              videoRef: Cu.getWeakReference(video.element)
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
    sendMessageToJava({ type: "Casting:Started", device: this.session.service.friendlyName });

    let video = this.session.videoRef.get();
    if (video) {
      this._sendEventToVideo(video, { active: true });
      this._updatePageAction(video);
    }
  },

  onRemoteMediaStop: function(aRemoteMedia) {
    sendMessageToJava({ type: "Casting:Stopped" });
  },

  onRemoteMediaStatus: function(aRemoteMedia) {
    if (!this.session) {
      return;
    }

    let status = aRemoteMedia.status;
    if (status == "completed") {
      this.closeExternal();
    }
  },

  allowableExtension: function(aURI, aExtensions) {
    if (aURI && aURI instanceof Ci.nsIURL) {
      for (let x in aExtensions) {
        if (aURI.fileExtension == aExtensions[x]) return true;
      }
    }
    return false;
  },

  allowableMimeType: function(aType, aTypes) {
    for (let x in aTypes) {
      if (aType == aTypes[x]) return true;
    }
    return false;
  }
};
