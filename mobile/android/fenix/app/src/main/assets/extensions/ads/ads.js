function collect_urls(urls) {
    let anchors = document.getElementsByTagName("a");
    for (let anchor of anchors) {
          if (!anchor.href) {
            continue;
          }
          urls.push(anchor.href);
    }
}
let urls = [];
collect_urls(urls)

let message = {
    'url': document.location.href,
    'urls': urls
}
browser.runtime.sendNativeMessage("MozacBrowserAds", message);
