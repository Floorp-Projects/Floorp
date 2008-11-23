
const RED = "data:image/gif;base64,R0lGODdhAgACALMAAAAAAP///wAAAAAAAP8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACwAAAAAAgACAAAEA5BIEgA7";
const BLUE = "data:image/gif;base64,R0lGODdhAgACALMAAAAAAP///wAAAAAAAP8AAAAAAAAAAAAAAAAA/wAAAAAAAAAAAAAAAAAAAAAAAAAAACwAAAAAAgACAAAEAxBJFAA7";
const ORANGE = "data:image/gif;base64,R0lGODdhAgACALMAAAAAAP///wAAAP//AP8AAP+AAAD/AAAAAAAA//8A/wAAAAAAAAAAAAAAAAAAAAAAACwAAAAAAgACAAAEA7DIEgA7";

var gCheckCount = 0;

// test helpers
function XUL(s, id, attrs) {
  let e = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul", s);
  if (id)
    e.setAttribute("id", id);

  if (attrs) {
    for (var a in attrs)
      e.setAttribute(a, attrs[a]);
  }
  return e;
}

function e(id) {
  return document.getElementById(id);
}

function attr(id, a, v) {
  e(id).setAttribute(a, v);
}

function CleanDocument(x, y) {
  gCheckCount = 0;

  var root = document.getElementById("testroot");
  while (root.firstChild)
    root.removeChild(root.firstChild);

  if (x && y) {
    var tr = document.getElementById("testroot");
    tr.style.width = x + "px";
    tr.style.height = y + "px";
  }
}

function IMAGE(src, id, x, y, w, h) {
  return XUL("image", id, { style: "-moz-stack-sizing: ignore; opacity: 0.5", src: src, width: w, height: h, left: x, top: y });
}

function SetupFourBoxes() {
  var s = XUL("stack", "s");
  var a = IMAGE(RED,  "a",   0,   0, 100, 100);
  var b = IMAGE(BLUE, "b", 100,   0, 100, 100);
  var c = IMAGE(BLUE, "c",   0, 100, 100, 100);
  var d = IMAGE(RED,  "d", 100, 100, 100, 100);

  s.appendChild(a);
  s.appendChild(b);
  s.appendChild(c);
  s.appendChild(d);

  document.getElementById("testroot").appendChild(s);

  return s;
}

function SetupOneBoxAndViewport(x,y) {
  var s = XUL("stack", "s");

  var b = IMAGE(RED,  "b",  x-20,  y-20, 20, 20);

  s.appendChild(b);

  var vp = IMAGE(ORANGE, "vp", x, y, 50, 50);
  vp.setAttribute("viewport", "true");
  s.appendChild(vp);

  document.getElementById("testroot").appendChild(s);

  return s;
}

function SetupEightBoxes(x, y, sz, _wantStack) {
  var s = XUL("stack", "s");

  x = x || 0;
  y = y || 0;
  sz = sz || 20;

  // boxes are b1-b9 from the top left, going clockwise
  var b1 = IMAGE(RED,  "b1", x-sz, y-sz, sz, sz);
  var b2 = IMAGE(BLUE, "b2", x,    y-sz, sz, sz);
  var b3 = IMAGE(RED,  "b3", x+sz, y-sz, sz, sz);
  var b4 = IMAGE(BLUE, "b4", x+sz, y,    sz, sz);
  var b5 = IMAGE(RED,  "b5", x+sz, y+sz, sz, sz);
  var b6 = IMAGE(BLUE, "b6", x,    y+sz, sz, sz);
  var b7 = IMAGE(RED,  "b7", x-sz, y+sz, sz, sz);
  var b8 = IMAGE(BLUE, "b8", x-sz, y,    sz, sz);

  s.appendChild(b1);
  s.appendChild(b2);
  s.appendChild(b3);
  s.appendChild(b4);
  s.appendChild(b5);
  s.appendChild(b6);
  s.appendChild(b7);
  s.appendChild(b8);

  if (_wantStack)
    return s;

  document.getElementById("testroot").appendChild(s);

  return s;
}

function SetupNineBoxes(x, y, sz) {
  var s = SetupEightBoxes(x, y, sz, true);

  var b0 = IMAGE(ORANGE, "b0", x, y, sz, sz);

  s.appendChild(b0);

  document.getElementById("testroot").appendChild(s);

  return s;
}

function SetupEightBoxesAndViewport(x, y, sz) {
  var s = SetupEightBoxes(x, y, sz, true);

  var vp = IMAGE(ORANGE, "vp", x, y, 20, 20);
  vp.setAttribute("viewport", "true");
  s.appendChild(vp);

  document.getElementById("testroot").appendChild(s);

  return s;
}

function Barrier(x, y, type, vpr) {
  if (x != undefined && y != undefined)
    throw "Bumper with both x and y given, that won't work";

  var spacer = XUL("spacer", null, { style: "-moz-stack-sizing: ignore;", barriertype: type, size: '10' });

  if (x != undefined)
    spacer.setAttribute("left", x);
  if (y != undefined)
    spacer.setAttribute("top", y);
  if (vpr)
    spacer.setAttribute("constraint", "vp-relative");

  document.getElementById("s").appendChild(spacer);
  return spacer;
}

function checkInnerBoundsInner(ws, x, y, w, h) {
  let vwib = ws._viewport.viewportInnerBounds;
  if (!((vwib.x != x ||
         vwib.y != y ||
         (w != undefined && vwib.width != w) ||
         (h != undefined && vwib.height != h))))
    return null;

  return [ vwib.x, vwib.y, vwib.width, vwib.height ];
}

function checkInnerBounds(ws, x, y, w, h) {
  gCheckCount++;

  var res = checkInnerBoundsInner(ws, x, y, w, h);
  if (!res)
    return null;

  var err;
  if (w == undefined || h == undefined) {
    err = "(" + gCheckCount + ") expected [" + x + "," + y + "] got [" + res[0] + "," + res[1] + "]";
  } else {
    err = "(" + gCheckCount + ") expected [" + x + "," + y + "," + w + "," + h + "] got [" + res[0] + "," + res[1] + "," + res[2] + "," + res[3] + "]";
  }

  throw "checkInnerBounds failed: " + err;
}

function checkRectInner(ws, id, x, y, w, h) {
  var e = document.getElementById(id);
  var bb = e.getBoundingClientRect();
  var wsb = ws._el.getBoundingClientRect();

  if (!((bb.left - wsb.left) != x ||
        (bb.top - wsb.left) != y ||
        (w != undefined && (bb.right - bb.left) != w) ||
        (h != undefined && (bb.bottom - bb.top) != h)))
    return null;

  return [(bb.left - wsb.left), (bb.top - wsb.left), (bb.right - bb.left), (bb.bottom - bb.top)];
}

function checkRect(ws, id, x, y, w, h) {
  gCheckCount++;

  var res = checkRectInner(ws, id, x, y, w, h);
  if (!res)
    return; // ok

  var err;
  if (w == undefined || h == undefined) {
    err = "(" + gCheckCount + ") expected [" + x + "," + y + "] got [" + res[0] + "," + res[1] + "]";
  } else {
    err = "(" + gCheckCount + ") expected [" + x + "," + y + "," + w + "," + h + "] got [" + res[0] + "," + res[1] + "," + res[2] + "," + res[3] + "]";
  }

  throw "checkRect failed: " + err;
}

//
// check that simple stuff works
//
function simple1() {
  CleanDocument();
  var s = SetupFourBoxes();

  var ws = new WidgetStack(s);

  checkRect(ws, "a", 0, 0);
  checkRect(ws, "b", 100, 0);
  checkRect(ws, "c", 0, 100);
  checkRect(ws, "d", 100, 100);

  ws.panBy(-10, -10);

  checkRect(ws, "a", -10, -10);
  checkRect(ws, "b",  90, -10);
  checkRect(ws, "c", -10,  90);
  checkRect(ws, "d",  90,  90);

  // should be the same as panBy(10,10)
  ws.dragStart(50, 50);
  ws.dragMove(0, 0);
  ws.dragMove(60, 60);
  ws.dragStop();

  checkRect(ws, "a", 0, 0);
  checkRect(ws, "b", 100, 0);
  checkRect(ws, "c", 0, 100);
  checkRect(ws, "d", 100, 100);

  return true;
}

// check that ignore-x, ignore-y, and frozen work
function simple2() {
  CleanDocument();
  var s = SetupFourBoxes();

  attr("b", "constraint", "ignore-x");
  attr("c", "constraint", "ignore-y");
  attr("d", "constraint", "frozen");

  var ws = new WidgetStack(s);

  ws.panBy(-20, -20);

  checkRect(ws, "a", -20, -20);
  checkRect(ws, "b", 100, -20);
  checkRect(ws, "c", -20, 100);
  checkRect(ws, "d", 100, 100);

  ws.panBy(20, 20);

  checkRect(ws, "a", 0, 0);
  checkRect(ws, "b", 100, 0);
  checkRect(ws, "c", 0, 100);
  checkRect(ws, "d", 100, 100);

  return true;
}

function simple3() {
  CleanDocument(50,50);

  var s = SetupNineBoxes(0, 0, 50);
  for (var i = 1; i <= 8; i++) {
    attr("b"+i, "constraint", "vp-relative");
  }

  Barrier(0, undefined, "vertical");
  Barrier(25, undefined, "vertical");
  Barrier(50, undefined, "vertical");
  Barrier(undefined, 50, "horizontal");

  var ws = new WidgetStack(s, 50, 50);

  ws.panBy(-15, 0);

  checkRect(ws, "b0", -5, 0);

  // test that dragging does the same thing
  ws.dragStart(0, 0);
  ws.dragMove(5, 0);
  ws.dragMove(10, 0);
  ws.dragMove(15, 0);
  ws.dragStop();

  checkRect(ws, "b0", 0, 0);

  // because there's a 10-px bumper, this pan should have no effect
  ws.panBy(-5, 0);
  checkRect(ws, "b0", 0, 0);

  // now we pan beyond the right barrier by 5px
  ws.panBy(-15, 0);
  checkRect(ws, "b0", -5, 0);

  // and then we go back.  We should just need 5 to get back to 0.
  ws.panBy(5, 0);
  checkRect(ws, "b0", 0, 0);

  // check that we hit the middle barrier correctly
  ws.panBy(-30, 0);
  checkRect(ws, "b0", -20, 0);

  // this should hit the middle barrier
  ws.panBy(-10, 0);
  checkRect(ws, "b0", -20, 0);

  // and then go past it
  ws.panBy(-20, 0);
  checkRect(ws, "b0", -30, 0);

  // reset
  ws.panBy(40, 0);

  // now let's do a simpler test of the horizontal barriers; there's only one at 50

  ws.panBy(0, -5);
  checkRect(ws, "b0", 0, 0);

  ws.panBy(0, 5);
  checkRect(ws, "b0", 0, 0);

  ws.panBy(0, -20);
  checkRect(ws, "b0", 0, -10);

  ws.panBy(0, 10);
  checkRect(ws, "b0", 0, 0);

  return true;
}

// now check some viewport stuff
function vp1() {
  CleanDocument(50, 50);

  var s = SetupOneBoxAndViewport(0, 0);
  attr("b", "constraint", "vp-relative");

  var ws = new WidgetStack(s, 50, 50);

  // explicitly use this form of svb
  ws.setViewportBounds({top: 0, left: 0, right: 200, bottom: 200});

  checkRect(ws, "b", -20, -20);

  ws.panBy(20, 20);
  checkRect(ws, "b", 0, 0);
  checkRect(ws, "vp", 20, 20);

  ws.panBy(50, 50);
  checkRect(ws, "b", 0, 0);
  checkRect(ws, "vp", 20, 20);

  ws.panBy(-20, -20);
  ws.panBy(50, 50);
  checkRect(ws, "b", 0, 0);
  checkRect(ws, "vp", 20, 20);

  ws.panBy(-200, -200);

  checkRect(ws, "vp", 0, 0);
  checkInnerBounds(ws, 150, 150, 50, 50);

  ws.panBy(500, 500);

  checkRect(ws, "vp", 20, 20);
  checkInnerBounds(ws, 0, 0, 50, 50);

  return true;
}

function vp2() {
  CleanDocument(20, 20);

  var s = SetupEightBoxesAndViewport(0, 0);
  for (var i = 1; i <= 8; i++) {
    attr("b"+i, "constraint", "vp-relative");
  }

  var ws = new WidgetStack(s, 20, 20);

  // b5 is the bottom-right; the initial setup has a 20x20 viewport in the middle
  checkRect(ws, "b5", 20, 20);

  // explicitly use this form of svb
  ws.setViewportBounds(0, 0, 200, 200);

  // after resizing the viewport bounds, the rect should get pushed out
  checkRect(ws, "b5", 200, 200);

  ws.panBy(-500, -500);

  checkRect(ws, "b5", 0, 0);
  checkInnerBounds(ws, 180, 180, 20, 20);

  return true;
}

function vp3() {
  CleanDocument(20, 20);

  var s = SetupEightBoxesAndViewport(0, 0);
  for (var i = 1; i <= 8; i++) {
    attr("b"+i, "constraint", "vp-relative");
  }

  attr("b2", "constraint", "vp-relative,ignore-x");
  attr("b4", "constraint", "vp-relative,ignore-y");

  var ws = new WidgetStack(s, 20, 20);

  // b2 is the top-middle
  checkRect(ws, "b2", 0, -20);
  // b4 is the right-middle
  checkRect(ws, "b4", 20, 0);

  ws.setViewportBounds(200, 200);

  checkRect(ws, "b2", 0, -20);
  checkRect(ws, "b4", 200, 0);

  // x pans shouldn't affect ignore-x widgets
  ws.panBy(-20, 0);
  checkRect(ws, "b2", 0, -20);
  ws.panBy( 20, 0);

  // y pans shouldn't affect ignore-y widgets
  ws.panBy(0, -20);
  checkRect(ws, "b4", 200, 0);
  ws.panBy(0,  20);

  return true;
}

// test whether the right things happen when the viewport size changes
function vp4() {
  CleanDocument(20, 20);

  var s = SetupEightBoxesAndViewport(0, 0);
  for (var i = 1; i <= 8; i++) {
    attr("b"+i, "constraint", "vp-relative");
  }

  attr("b2", "constraint", "vp-relative,ignore-x");
  attr("b4", "constraint", "vp-relative,ignore-y");

  var ws = new WidgetStack(s, 20, 20);

  ws.setViewportBounds(200, 200);

  // after resizing the viewport bounds, the rect should get pushed out
  checkRect(ws, "b4", 200, 0);
  checkRect(ws, "b5", 200, 200);

  checkInnerBounds(ws, 0, 0, 20, 20);

  ws.panBy(-50, -50);

  checkInnerBounds(ws, 50, 50, 20, 20);

  // the viewport is now going to grow
  ws.setViewportBounds(400, 400);

  // ... and the inner bounds should remain the same
  checkInnerBounds(ws, 50, 50, 20, 20);

  // ... but b4 and b5 should be pushed out
  checkRect(ws, "b4", 400-50, 0);
  checkRect(ws, "b5", 400-50, 400-50);

  // now move to the far corner
  ws.panBy(-500, -500);

  // now shrink again
  ws.setViewportBounds(100, 100);

  return true;
}

function vp5() {
  CleanDocument(20, 20);

  var s = SetupEightBoxesAndViewport(0, 0);
  for (var i = 1; i <= 8; i++) {
    attr("b"+i, "constraint", "vp-relative");
  }

  Barrier(0, undefined, "vertical", true);
  Barrier(20, undefined, "vertical", true);
  Barrier(undefined, 0, "horizontal", true);
  Barrier(undefined, 100, "horizontal", true);

  var ws = new WidgetStack(s, 20, 20);

  ws.setViewportBounds(200, 200);

  ws.panBy(-20, 0);
  checkRect(ws, "b1", -30, -20);

  ws.panBy(10, 0);
  checkRect(ws, "b1", -20, -20);

  ws.panBy(-5, 0);
  checkRect(ws, "b1", -20, -20);


  // check the horizontal -- we should hit the first bumper,
  // but the other one should always be forever out of range of the vp
  ws.panBy(0, -110);
  checkRect(ws, "b1", -20, -120);

  ws.panBy(0, -300);
  checkRect(ws, "b1", -20, -220);

  return true;
}



function vp6() {
  CleanDocument(20, 20);

  var s = SetupEightBoxesAndViewport(0, 0);
  for (var i = 1; i <= 8; i++) {
    attr("b"+i, "constraint", "vp-relative");
  }

  var ws = new WidgetStack(s, 20, 20);

  ws.setViewportBounds(200, 200);
  checkInnerBounds(ws, 0, 0, 20, 20);

  ws.panTo(-75, -75);
  checkInnerBounds(ws, 75, 75, 20, 20);

  // scale up
  ws.setViewportBounds(500, 500);
  checkInnerBounds(ws, 75, 75, 20, 20);

  ws.panTo(-300, -300);
  checkInnerBounds(ws, 300, 300, 20, 20);

  // scale down
  ws.setViewportBounds(200, 200);
  checkInnerBounds(ws, 75, 75, 20, 20);// XXX?

  ws.panTo(-75, -75);
  checkInnerBounds(ws, 75, 75, 20, 20);

  return true;
}



function run(s) {
  var r = false;
  try {
    r = window[s].apply(window);
    if (r) {
      logbase("PASSED: " + s);
    } else {
      logbase("FAILED: " + s + " (no exception?)");
    }
  } catch (e) {
    logbase("FAILED: " + s + ": " + e);
  }

}

function runTests() {
  run("simple1");
  run("simple2");
  run("simple3");

  run("vp1");
  run("vp2");
  run("vp3");
  run("vp4");
  run("vp5");
  run("vp6");
}

function handleLoad() {
  gWsDoLog = true;
  gWsLogDiv = document.getElementById("logdiv");

  runTests();
}
