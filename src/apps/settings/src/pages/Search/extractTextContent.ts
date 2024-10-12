import ReactDOMServer from "react-dom/server";
import type React from "react";

export function extractTextContent(component: React.ReactElement) {
  const html = ReactDOMServer.renderToStaticMarkup(component);
  const div = document?.createElement("div");
  if (div) {
    div.innerHTML = html;
    return div.textContent;
  }
  return "";
}
