var RTCPeerConnection = null;
var getUserMedia = null;
var attachMediaStream = null;

if (navigator.mozGetUserMedia) {
  console.log("This appears to be Firefox");

  // The RTCPeerConnection object.
  RTCPeerConnection = mozRTCPeerConnection;

  // Get UserMedia (only difference is the prefix).
  // Code from Adam Barth.
  getUserMedia = navigator.mozGetUserMedia.bind(navigator);

  // Attach a media stream to an element.
  attachMediaStream = function(element, stream) {
    console.log("Attaching media stream");
    element.mozSrcObject = stream;
    element.play();
  };
} else if (navigator.webkitGetUserMedia) {
  console.log("This appears to be Chrome");

  // The RTCPeerConnection object.
  RTCPeerConnection = webkitRTCPeerConnection;
  
  // Get UserMedia (only difference is the prefix).
  // Code from Adam Barth.
  getUserMedia = navigator.webkitGetUserMedia.bind(navigator);

  // Attach a media stream to an element.
  attachMediaStream = function(element, stream) {
    element.src = webkitURL.createObjectURL(stream);
  };

  // The representation of tracks in a stream is changed in M26.
  // Unify them for earlier Chrome versions in the coexisting period.
  if (!webkitMediaStream.prototype.getVideoTracks) {
      webkitMediaStream.prototype.getVideoTracks = function() {
      return this.videoTracks;
    } 
  } 
  
  if (!webkitMediaStream.prototype.getAudioTracks) {
      webkitMediaStream.prototype.getAudioTracks = function() {
      return this.audioTracks;
    }
  } 
} else {
  console.log("Browser does not appear to be WebRTC-capable");
}
