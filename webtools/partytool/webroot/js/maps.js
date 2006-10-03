var map;
var marker;

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

function geocode(aLoc) {
    var gcoder = new GClientGeocoder();
    document.getElementById('geocoded').value = 1;
    gcoder.getLatLng(aLoc, function (point) {
      if (!point) {
        suggest(aLoc);
      }
      else {
        map.setZoom(10);
        map.panTo(point);
        map.removeOverlay(marker);
        marker = new GMarker(point, {draggable: true});
        map.addOverlay(marker);
      }
    });
}

function suggest(loc) {
  GDownloadUrl("/cake/js/suggest.php?s=" + loc, function(data, responseCode) {
    var xml = GXml.parse(data);
    var sug = xml.documentElement.getElementsByTagName("string");
    if (sug[0].getAttribute('value') !== 0) {
      document.getElementById('locerrlink').innerHTML = sug[0].getAttribute('value');
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

function initMashUp() {
  map = new GMap2(document.getElementById("map"));
  map.addControl(new GLargeMapControl());
  map.addControl(new GMapTypeControl());
  map.setCenter(new GLatLng(37.4419, -122.1419), 2);
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

function addParty(aLat, aLng, aTxt) {
  var point = new GLatLng(aLat, aLng);
  var mark = new GMarker(point);
  GEvent.addListener(mark, "click", function() {
    mark.openInfoWindowHtml(aTxt);
  });
  map.addOverlay(mark);
}