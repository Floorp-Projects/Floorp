export default function CustomHmr() {
  return {
    name: "custom-hmr",
    enforce: "post",
    // biome-ignore lint/suspicious/noExplicitAny: <explanation>
    handleHotUpdate({ file, server }: { file: string; server: any }) {
      if (file.endsWith(".json")) {
        console.log("reloading json file...");

        server.ws.send({ type: "full-reload", path: "*" });
      }
    },
  };
}
