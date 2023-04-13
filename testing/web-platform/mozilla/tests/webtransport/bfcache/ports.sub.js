// The file including this must also include /common/get-host-info.sub.js to
// pick up the necessary constants.

const HOST = get_host_info().ORIGINAL_HOST;
const PORT = '{{ports[webtransport-h3][0]}}';
const BASE = `https://${HOST}:${PORT}`;

// Create URL for WebTransport session.
export function webtransport_url(handler) {
  return `${BASE}/webtransport/handlers/${handler}`;
}

