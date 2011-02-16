// The including script sets this for us
//var NUM_STEPS;

var plugin;
var left = 1, top = 1, width = 199, height = 199;
function movePluginTo(x, y, w, h) {
    left = x; top = y;  width = w; height = h;
    plugin.width = w;
    plugin.height = h;
    plugin.style.left = left + "px";
    plugin.style.top = top + "px";
}
function deltaInBounds(dx,dy, dw,dh) {
    var l = dx + left;
    var r = l + width + dw;
    var t = dy + top;
    var b = t + height + dh;
    return (0 <= l && l <= 20 &&
            0 <= t && t <= 20 &&
            200 <= r && r <= 220 &&
            200 <= b && b <= 220);
}

var initialFrame;
function start() {
    window.removeEventListener("MozReftestInvalidate", start, false);

    window.addEventListener("MozAfterPaint", step, false);
    window.addEventListener("MozPaintWaitFinished", step, false);

    initialFrame = window.mozPaintCount;
    plugin = document.getElementById("plugin");

    movePluginTo(0,0, 200,200);
}

var steps = 0;
var which = "move"; // or "grow"
var dx = 1, dy = 1, dw = 1, dh = 1;
function step() {
    if (++steps >= NUM_STEPS) {
        window.removeEventListener("MozAfterPaint", step, false);
        window.removeEventListener("MozPaintWaitFinished", step, false);
        return finish();
    }

    var didSomething = false;
    if (which == "grow") {
        if (deltaInBounds(0,0, dw,dh)) {
            movePluginTo(left,top, width+dw, height+dh);
            didSomething = true;
        } else {
            dw = -dw;  dh = -dh;
        }
    } else {
        // "move"
        if (deltaInBounds(dx,dy, 0,0)) {
            movePluginTo(left+dx,top+dy, width, height);
            didSomething = true;
        } else {
            dx = -dx;  dy = -dy;
        }
    }
    which = (which == "grow") ? "move" : "grow";

    if (!didSomething) {
        step();
    }
}

function finish() {
    document.documentElement.removeAttribute("class");
}

window.addEventListener("MozReftestInvalidate", start, false);
