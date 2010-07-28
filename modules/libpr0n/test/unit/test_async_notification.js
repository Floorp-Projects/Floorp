/*
 * Test for asynchronous image load/decode notifications in the case that the image load works.
 */

// A simple 3x3 png; rows go red, green, blue. Stolen from the PNG encoder test.
var pngspec = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAMAAAADCAIAAADZSiLoAAAAEUlEQVQImWP4z8AAQTAamQkAhpcI+DeMzFcAAAAASUVORK5CYII=";
var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
var uri = ioService.newURI(pngspec, null, null);

load('async_load_tests.js');
