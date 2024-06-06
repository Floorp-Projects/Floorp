// See get_content in priority-dependent-content.py


function convertSizeToUrgency(size) {
    return Math.round(size / 10 - 1);
}

function convertRectToUrgency(rect) {
    if (rect.width != rect.height) {
        throw "not a square";
    }
    return convertSizeToUrgency(rect.width);
}

function getElementUrgencyFromSize(id) {
    let rect = document.getElementById(id).getBoundingClientRect();
    return convertRectToUrgency(rect);
}

function getScriptUrgency(id) {
    return self.scriptUrgency[id];
}

function getStyleUrgency(id) {
    return getElementUrgencyFromSize(`urgency_square_${id}`)
}

function getResourceUrgencyFromRawText(content) {
    const regexpUrgency = /id=\w+,u=([0-7])/;
    let matches = regexpUrgency.exec(content);
    if (!matches) {
        throw "cannot extract urgency from content"
    }
    return parseInt(matches[1]);
}
