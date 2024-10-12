// import {
//   createTRPCProxyClient,
//   createWSClient,
//   httpLink,
//   splitLink,
//   wsLink,
// } from "@trpc/client";
// import type { AppRouter } from "@nora/middleware-settings/defines";

// const wsClient = createWSClient({
//   url: `ws://localhost:5192`,
// });
// export const trpc = createTRPCProxyClient<AppRouter>({
//   links: [
//     // call subscriptions through websockets and the rest over http
//     splitLink({
//       condition(op) {
//         return op.type === "subscription";
//       },
//       true: wsLink({
//         client: wsClient,
//       }),
//       false: httpLink({
//         url: `http://localhost:5192`,
//       }),
//     }),
//   ],
// });
