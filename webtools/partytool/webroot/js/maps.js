var map;
var marker;
var mouseloc;

function wheelZoom(event) {
  function out() {
    map.setCenter(mouseloc);
    map.zoomOut();
  }

  if (event.cancelable) event.preventDefault();
  {
    (event.detail || -event.wheelDelta) < 0 ? map.zoomIn(mouseloc, true) : out();
  }

  return false; 
}

function mapInit(aLat, aLng, aZoom, aState) {
  map = new GMap2(document.getElementById("map"));
  map.addControl((aState == "stationary") ? new GSmallZoomControl() : new GSmallMapControl());

  if (aLat) {
    map.setCenter(new GLatLng(aLat, aLng), aZoom);
    if (marker) {
      map.removeOverlay(marker);
    }
    marker = new GMarker(new GLatLng(aLat, aLng), (aState != "stationary") ? {draggable: true} : {draggable: false});
  }

  else {
    map.setCenter(new GLatLng(14.944785, -156.796875), 1);
    if (marker) {
      map.removeOverlay(marker);
    }
    marker = new GMarker(new GLatLng(14.944785, -156.796875), (aState != "stationary") ? {draggable: true} : {draggable: false});
  }

  map.addOverlay(marker);
  if (aState != "stationary") {
    GEvent.addListener(marker, "dragend", function() { onDragEnd(); });
    GEvent.addListener(map, "moveend", function(){ onMoveEnd(); });
  }
}

function search(event) {
  document.getElementById('map-load').setAttribute('style', '');
  
  if (event.cancelable) event.preventDefault();
  {
    var q = document.getElementById('location').value;
    var gcoder = new GClientGeocoder();
    gcoder.getLatLng(q, function (point) {
      if (!point) {
        suggest(q);
        document.getElementById('map-load').setAttribute('style', 'visibility: hidden');
      }
      else {
        map.setZoom(10);
        map.panTo(point);
        document.getElementById('map-load').setAttribute('style', 'visibility: hidden');
      }
    });
  }
  return false;
}

function geocode(aLoc) {
    var gcoder = new GClientGeocoder();
    gcoder.getLatLng(aLoc, function (point) {
      if (!point) {
        suggest(aLoc);
      }
      else {
        map.setZoom(10);
        map.panTo(point);
        map.removeOverlay(marker);
        marker = new GMarker(point, {draggable: true});
        GEvent.addListener(marker, "dragend", function() { onDragEnd(); });
        map.addOverlay(marker);
      }
    });
}

function suggest(loc) {
  GDownloadUrl("/js/suggest.php?s=" + loc, function(data, responseCode) {
    if (data != 0) {
      document.getElementById('locerrlink').innerHTML = data;
      document.getElementById('locerr').setAttribute('style', '');
    }
  });
}

function geocode_suggest() {
  var str = document.getElementById('locerrlink').innerHTML;
  document.getElementById('location').value = str;
  document.getElementById('locerr').setAttribute('style', 'display: none');
  geocode(str);
}

function onMoveEnd() {
  var point = map.getCenter();
  map.removeOverlay(marker);
  marker = new GMarker(point, {draggable: true});
  map.addOverlay(marker);
  GEvent.addListener(marker, "dragend", function() { onDragEnd(); });
  editForm();
}

function onDragEnd() {
  var point = marker.getPoint()
  map.panTo(point);
}

function initMashUp(lat, lng) {
  map = new GMap2(document.getElementById("map"));
  map.enableDoubleClickZoom();
  map.enableContinuousZoom();
  map.addControl(new GLargeMapControl());
  map.addControl(new GMapTypeControl());
  map.setCenter(new GLatLng(0, -5.25), 1);
  GEvent.addDomListener(document.getElementById("map"), "DOMMouseScroll", wheelZoom);
  GEvent.addDomListener(document.getElementById("map"), "mousewheel",     wheelZoom);
  GEvent.addListener(map, "mousemove", function(point) { mouseloc = point; });
  GEvent.addListener(map, "click", function(overlay, point) {
    if (overlay) {
      if (overlay.mid) {
        downloadMarker(overlay.mid, overlay);
      }
    }
  });

  if (lat && lng)
    map.setCenter(new GLatLng(lat, lng), 10);
  else
    map.setCenter(new GLatLng(0, -5.25), 1);

  addParties();
}

function editForm() {
  var ll = map.getCenter();
  document.getElementById('lat').value = ll.lat();
  document.getElementById('long').value = ll.lng();
  document.getElementById('zoom').value = map.getZoom();
}

function shide() {
  document.getElementById('locerr').setAttribute('style', 'display: none');
}

function downloadMarker(mid, overlay) {
  document.getElementById('map-load').setAttribute('style', '');
  GDownloadUrl("/parties/js/html/" + mid, function(data, responseCode) {
    if (data != "" && responseCode == 200) {
      document.getElementById('map-load').setAttribute('style', 'visibility: hidden');
      overlay.openInfoWindowHtml(data);
    }
  });
}

function addParty(aLat, aLng, aId) {
  var point = new GLatLng(aLat, aLng);
  var icon = new GIcon();
  icon.image = "/img/marker.png";
  icon.iconSize = new GSize(12, 20);
  icon.iconAnchor = new GPoint(6, 20);
  icon.infoWindowAnchor = new GPoint(5, 1);
  var mark = new GMarker(point, icon);
  mark.mid = aId;
  map.addOverlay(mark);
}