/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var CastingApps = {
  _castMenuId: -1,

  init: function ca_init() {
    if (!this.isEnabled()) {
      return;
    }

    // Search for devices continuously every 120 seconds
    SimpleServiceDiscovery.search(120 * 1000);

    this._castMenuId = NativeWindow.contextmenus.add(
      Strings.browser.GetStringFromName("contextmenu.castToScreen"),
      this.filterCast,
      this.openExternal.bind(this)
    );

    Services.obs.addObserver(this, "Casting:Play", false);
    Services.obs.addObserver(this, "Casting:Pause", false);
    Services.obs.addObserver(this, "Casting:Stop", false);
  },

  uninit: function ca_uninit() {
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

  makeURI: function makeURI(aURL, aOriginCharset, aBaseURI) {
    return Services.io.newURI(aURL, aOriginCharset, aBaseURI);
  },

  getVideo: function(aElement, aX, aY) {
    // Fast path: Is the given element a video element
    let video = this._getVideo(aElement);
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
          video = this._getVideo(element);
          if (video) {
            break;
          }
        }
      }
    } catch(e) {}

    // Could be null
    return video;
  },

  _getVideo: function(aElement) {
    // Given the hardware support for H264, let's only look for 'mp4' sources
    if (!aElement instanceof HTMLVideoElement) {
      return null;
    }

    function allowableExtension(aURI) {
      if (aURI && aURI instanceof Ci.nsIURL) {
        return (aURI.fileExtension == "mp4");
      }
      return false;
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
      if (allowableExtension(sourceURI)) {
        return { video: aElement, source: sourceURI.spec, poster: posterURL };
      }
    }

    // Next, look to see if there is a <source> child element that meets
    // our needs
    let sourceNodes = aElement.getElementsByTagName("source");
    for (let sourceNode of sourceNodes) {
      let sourceURI = this.makeURI(sourceNode.src, null, this.makeURI(sourceNode.baseURI));

      // Using the type attribute is our ideal way to guess the mime type. Otherwise,
      // fallback to using the file extension to guess the mime type
      if (sourceNode.type == "video/mp4" || allowableExtension(sourceURI)) {
        return { video: aElement, source: sourceURI.spec, poster: posterURL };
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

  prompt: function(aCallback) {
    let items = [];
    SimpleServiceDiscovery.services.forEach(function(aService) {
      let item = {
        label: aService.friendlyName,
        selected: false
      };
      items.push(item);
    });

    let prompt = new Prompt({
      title: Strings.browser.GetStringFromName("casting.prompt")
    }).setSingleChoiceItems(items).show(function(data) {
      let selected = data.button;
      let service = selected == -1 ? null : SimpleServiceDiscovery.services[selected];
      if (aCallback)
        aCallback(service);
    });
  },

  openExternal: function(aElement, aX, aY) {
    // Start a second screen media service
    let video = this.getVideo(aElement, aX, aY);
    if (!video) {
      return;
    }

    this.prompt(function(aService) {
      if (!aService)
        return;

      // Make sure we have a player app for the given service
      let app = SimpleServiceDiscovery.findAppForService(aService, "video-sharing");
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
        app.start("", function() {
          app.remoteMedia(function(aRemoteMedia) {
            this.session = {
              service: aService,
              app: app,
              remoteMedia: aRemoteMedia,
              data: {
                title: video.title,
                source: video.source,
                poster: video.poster
              }
            };
          }.bind(this), this);
        }.bind(this));
      }.bind(this));
    }.bind(this));
  },

  // RemoteMedia callback API methods
  onRemoteMediaStart: function(aRemoteMedia) {
    if (!this.session) {
      return;
    }

    aRemoteMedia.load(this.session.data);
    sendMessageToJava({ type: "Casting:Started", device: this.session.service.friendlyName });
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
      aRemoteMedia.shutdown();
      this.session.app.stop();
      delete this.session;
    }
  }
};

