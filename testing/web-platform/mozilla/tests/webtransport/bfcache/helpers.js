export function runWebTransportBfcacheTest(params, description) {
  runBfcacheTest(
    {
      // due to the baseurl issue, this would try to load the scripts from
      // the main wpt test directory
      // scripts: ["/webtransport/resources/helpers.js"],
      openFunc: url =>
        window.open(
          url + `&prefix=${location.pathname}-${description}`,
          "_blank",
          "noopener"
        ),
      ...params,
    },
    description
  );
}
