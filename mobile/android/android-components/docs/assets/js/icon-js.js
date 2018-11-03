let icons = {};
let needed = 0;
let got = 0;

/**
 * @param html {String} HTML representing a single element
 * @return {Element}
 */
function htmlToElement(html) {
    let template = document.createElement('template');
    html = html.trim(); // Never return a text node of whitespace as the result
    template.innerHTML = html;
    //firstChild may be a comment so we must use firstElementChild to avoid picking it
    return template.content.firstElementChild;
}

/**
 * Converts an Android Vector Drawable to a standard SVG by striping off or renaming some elements
 *
 * @param s {String} String of an Android Vector Drawable
 * @returns {String} The same string but in the standard SVG representation
 */
function androidSVGtoNormalSVG(s) {
    s = s.replace(/<\?xml version="1\.0" encoding="utf-8"\?>/g, '');
    s = s.replace(/<vector xmlns:android="http:\/\/schemas.android.com\/apk\/res\/android"/g, '<svg xmlns="http://www.w3.org/2000/svg"');
    s = s.replace(/<\/vector>/g, '</svg>');
    s = s.replace(/android:(height|width)="(\d+)dp"/g, '');
    s = s.replace(/android:viewportHeight="(\d+\.?\d+)"/g, 'height="$1"');
    s = s.replace(/android:viewportWidth="(\d+\.?\d+)"/g, 'width="$1"');
    s = s.replace(/android:fillColor=/g, 'fill=');
    s = s.replace(/android:pathData=/g, 'd=');
    //s = s.replace(/android:/g, '');
    return s;
}

function addToTable(name, svg) {
    let table = document.querySelector("#preview_table > tbody");
    let row = htmlToElement("<tr></tr>");
    row.appendChild(htmlToElement("<td>" + name + "</td>"));
    let td = htmlToElement("<td></td>");
    td.appendChild(svg);
    row.appendChild(td);
    table.appendChild(row);
}

function addSingleItemToTable(str) {
    let table = document.querySelector("#preview_table > tbody");
    table.append(htmlToElement("<tr><td colspan='2'>" + str + "</td></tr>"))
}

function getFile(iconName, downloadUrl) {
    let request = new XMLHttpRequest();
    request.open('GET', downloadUrl, true);
    request.onreadystatechange = function () {
        if (request.readyState === 4 && request.status === 200) {
            got++;
            let androidXmlText = request.responseText;
            androidXmlText = androidSVGtoNormalSVG(androidXmlText);
            icons[iconName] = androidXmlText;
        } else if (request.readyState === 4) {
            //Request completed with an error
            got++;
            icons[iconName] = "<span>Error during download</span>";
        }
        if (got == needed) {
            document.querySelector("#preview_table > tbody").innerHTML = "";
            for (let el in icons) {
                addToTable(el, htmlToElement(icons[el]));
            }
        }
    };
    request.send(null);
}

// This function recovers all icons inside the drawable folder via github API
(function getIcons() {
    let request = new XMLHttpRequest();
    request.open("GET", "https://api.github.com/repos/mozilla-mobile/android-components/contents/components/ui/icons/src/main/res/drawable", true);
    //Explicit request of the V3 version of the API
    request.setRequestHeader("Accept", "application/vnd.github.v3+json");
    request.onreadystatechange = function () {
        if (request.readyState === XMLHttpRequest.DONE && request.status === 200) {
            let response = JSON.parse(request.response);
            if (response.message) {
                //Something went wrong
                console.error(response.message);
                addSingleItemToTable("Error: " + response.message);
                return;
            }
            needed = response.length;
            addSingleItemToTable("Loading");
            for (let i = 0; i < response.length; i++) {
                let iconName = response[i]['name'].substr(0, response[i]['name'].length - 4);
                icons[iconName] = null;
                getFile(iconName, response[i]['download_url']);
            }
        }
    };
    request.send(null);
})();