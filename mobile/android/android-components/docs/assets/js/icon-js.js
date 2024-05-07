/**
 * @param html {String} HTML representing a single element
 * @return {Element}
 */
function htmlToElement(html) {
  let template = document.createElement("template");
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
  s = s.replace(/<\?xml version="1\.0" encoding="utf-8"\?>/g, "");
  s = s.replace(
    /<vector xmlns:android="http:\/\/schemas.android.com\/apk\/res\/android"/g,
    '<svg xmlns="http://www.w3.org/2000/svg"'
  );
  s = s.replace(/<\/vector>/g, "</svg>");
  s = s.replace(/android:(height|width)="(\d+)dp"/g, "");
  s = s.replace(/android:viewportHeight="(\d+\.?\d+)"/g, 'height="$1"');
  s = s.replace(/android:viewportWidth="(\d+\.?\d+)"/g, 'width="$1"');
  s = s.replace(/android:fillColor=/g, "fill=");
  s = s.replace(/android:pathData=/g, "d=");
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
  table.append(htmlToElement("<tr><td colspan='2'>" + str + "</td></tr>"));
}

function getFile(iconName, downloadUrl) {
  return new Promise((resolve, reject) => {
    let request = new XMLHttpRequest();
    request.open("GET", downloadUrl, true);
    request.onreadystatechange = function () {
      if (request.readyState === 4 && request.status === 200) {
        let androidXmlText = request.responseText;
        androidXmlText = androidSVGtoNormalSVG(androidXmlText);
        resolve([iconName, androidXmlText]);
      } else if (request.readyState === 4) {
        //Request completed with an error
        resolve([iconName, "<span>Error during download</span>"]);
      }
    };
    request.send(null);
  });
}

// This function recovers all icons inside the drawable folder via github API
(function getIcons() {
  let request = new XMLHttpRequest();
  request.open(
    "GET",
    "https://api.github.com/repos/mozilla-mobile/android-components/contents/components/ui/icons/src/main/res/drawable",
    true
  );
  //Explicit request of the V3 version of the API
  request.setRequestHeader("Accept", "application/vnd.github.v3+json");
  request.onreadystatechange = function () {
    if (request.readyState === XMLHttpRequest.DONE && request.status === 200) {
      let response = JSON.parse(request.response);
      if (response.message) {
        //Something went wrong
        addSingleItemToTable("Error: " + response.message);
        return;
      }
      addSingleItemToTable("Loading");
      let promises = [];
      for (let i = 0; i < response.length; i++) {
        let iconName = response[i]["name"].substr(
          0,
          response[i]["name"].length - 4
        );
        promises.push(getFile(iconName, response[i]["download_url"]));
      }
      Promise.all(promises).then(values => {
        document.querySelector("#preview_table > tbody").innerHTML = "";
        for (let i = 0; i < values.length; i++) {
          let name = values[i][0],
            svg = values[i][1];
          addToTable(name, htmlToElement(svg));
        }
      });
    }
  };
  request.send(null);
})();
