// src/apps/startup/vite.config.ts
import path from "node:path";
import { defineConfig } from "file:///H:/noraneko/node_modules/.pnpm/vite@5.4.8_@types+node@22.7.5_lightningcss@1.27.0_terser@5.34.1/node_modules/vite/dist/node/index.js";

// src/apps/common/scripts/gen_jarmanifest.ts
async function generateJarManifest(bundle, options) {
  console.log("generate jar.mn");
  const viteManifest = bundle;
  const arr = [];
  for (const i of Object.values(viteManifest)) {
    arr.push(i["fileName"]);
  }
  console.log("generate end jar.mn");
  return `noraneko.jar:
% ${options.register_type} ${options.namespace} %nora-${options.prefix}/ contentaccessible=yes
 ${Array.from(
    new Set(arr)
  ).map((v) => `nora-${options.prefix}/${v} (${v})`).join("\n ")}`;
}

// src/apps/startup/vite.config.ts
var __vite_injected_original_dirname = "H:\\noraneko\\src\\apps\\startup";
var r = (subpath) => path.resolve(__vite_injected_original_dirname, subpath);
var vite_config_default = defineConfig({
  build: {
    outDir: r("_dist"),
    target: "firefox128",
    lib: {
      formats: ["es"],
      entry: [
        r("src/chrome_root.ts"),
        r("src/about-preferences.ts"),
        r("src/about-newtab.ts")
      ]
    },
    rollupOptions: {
      external(source, importer, isResolved) {
        if (source.startsWith("chrome://")) {
          return true;
        }
        return false;
      }
    }
  },
  plugins: [
    {
      name: "gen_jarmn",
      enforce: "post",
      async generateBundle(options, bundle, isWrite) {
        this.emitFile({
          type: "asset",
          fileName: "jar.mn",
          needsCodeReference: false,
          source: await generateJarManifest(bundle, {
            prefix: "startup",
            namespace: "noraneko-startup",
            register_type: "content"
          })
        });
        this.emitFile({
          type: "asset",
          fileName: "moz.build",
          needsCodeReference: false,
          source: `JAR_MANIFESTS += ["jar.mn"]`
        });
      }
    }
  ]
});
export {
  vite_config_default as default
};
//# sourceMappingURL=data:application/json;base64,ewogICJ2ZXJzaW9uIjogMywKICAic291cmNlcyI6IFsic3JjL2FwcHMvc3RhcnR1cC92aXRlLmNvbmZpZy50cyIsICJzcmMvYXBwcy9jb21tb24vc2NyaXB0cy9nZW5famFybWFuaWZlc3QudHMiXSwKICAic291cmNlc0NvbnRlbnQiOiBbImNvbnN0IF9fdml0ZV9pbmplY3RlZF9vcmlnaW5hbF9kaXJuYW1lID0gXCJIOlxcXFxub3JhbmVrb1xcXFxzcmNcXFxcYXBwc1xcXFxzdGFydHVwXCI7Y29uc3QgX192aXRlX2luamVjdGVkX29yaWdpbmFsX2ZpbGVuYW1lID0gXCJIOlxcXFxub3JhbmVrb1xcXFxzcmNcXFxcYXBwc1xcXFxzdGFydHVwXFxcXHZpdGUuY29uZmlnLnRzXCI7Y29uc3QgX192aXRlX2luamVjdGVkX29yaWdpbmFsX2ltcG9ydF9tZXRhX3VybCA9IFwiZmlsZTovLy9IOi9ub3JhbmVrby9zcmMvYXBwcy9zdGFydHVwL3ZpdGUuY29uZmlnLnRzXCI7aW1wb3J0IHBhdGggZnJvbSBcIm5vZGU6cGF0aFwiO1xuaW1wb3J0IHsgZGVmaW5lQ29uZmlnIH0gZnJvbSBcInZpdGVcIjtcblxuaW1wb3J0IHsgZ2VuZXJhdGVKYXJNYW5pZmVzdCB9IGZyb20gXCIuLi9jb21tb24vc2NyaXB0cy9nZW5famFybWFuaWZlc3RcIjtcblxuY29uc3QgciA9IChzdWJwYXRoOiBzdHJpbmcpOiBzdHJpbmcgPT5cbiAgcGF0aC5yZXNvbHZlKGltcG9ydC5tZXRhLmRpcm5hbWUsIHN1YnBhdGgpO1xuXG5leHBvcnQgZGVmYXVsdCBkZWZpbmVDb25maWcoe1xuICBidWlsZDoge1xuICAgIG91dERpcjogcihcIl9kaXN0XCIpLFxuICAgIHRhcmdldDogXCJmaXJlZm94MTI4XCIsXG4gICAgbGliOiB7XG4gICAgICBmb3JtYXRzOiBbXCJlc1wiXSxcbiAgICAgIGVudHJ5OiBbXG4gICAgICAgIHIoXCJzcmMvY2hyb21lX3Jvb3QudHNcIiksXG4gICAgICAgIHIoXCJzcmMvYWJvdXQtcHJlZmVyZW5jZXMudHNcIiksXG4gICAgICAgIHIoXCJzcmMvYWJvdXQtbmV3dGFiLnRzXCIpLFxuICAgICAgXSxcbiAgICB9LFxuICAgIHJvbGx1cE9wdGlvbnM6IHtcbiAgICAgIGV4dGVybmFsKHNvdXJjZSwgaW1wb3J0ZXIsIGlzUmVzb2x2ZWQpIHtcbiAgICAgICAgaWYgKHNvdXJjZS5zdGFydHNXaXRoKFwiY2hyb21lOi8vXCIpKSB7XG4gICAgICAgICAgcmV0dXJuIHRydWU7XG4gICAgICAgIH1cbiAgICAgICAgcmV0dXJuIGZhbHNlO1xuICAgICAgfSxcbiAgICB9LFxuICB9LFxuICBwbHVnaW5zOiBbXG4gICAge1xuICAgICAgbmFtZTogXCJnZW5famFybW5cIixcbiAgICAgIGVuZm9yY2U6IFwicG9zdFwiLFxuICAgICAgYXN5bmMgZ2VuZXJhdGVCdW5kbGUob3B0aW9ucywgYnVuZGxlLCBpc1dyaXRlKSB7XG4gICAgICAgIHRoaXMuZW1pdEZpbGUoe1xuICAgICAgICAgIHR5cGU6IFwiYXNzZXRcIixcbiAgICAgICAgICBmaWxlTmFtZTogXCJqYXIubW5cIixcbiAgICAgICAgICBuZWVkc0NvZGVSZWZlcmVuY2U6IGZhbHNlLFxuICAgICAgICAgIHNvdXJjZTogYXdhaXQgZ2VuZXJhdGVKYXJNYW5pZmVzdChidW5kbGUsIHtcbiAgICAgICAgICAgIHByZWZpeDogXCJzdGFydHVwXCIsXG4gICAgICAgICAgICBuYW1lc3BhY2U6IFwibm9yYW5la28tc3RhcnR1cFwiLFxuICAgICAgICAgICAgcmVnaXN0ZXJfdHlwZTogXCJjb250ZW50XCIsXG4gICAgICAgICAgfSksXG4gICAgICAgIH0pO1xuICAgICAgICB0aGlzLmVtaXRGaWxlKHtcbiAgICAgICAgICB0eXBlOiBcImFzc2V0XCIsXG4gICAgICAgICAgZmlsZU5hbWU6IFwibW96LmJ1aWxkXCIsXG4gICAgICAgICAgbmVlZHNDb2RlUmVmZXJlbmNlOiBmYWxzZSxcbiAgICAgICAgICBzb3VyY2U6IGBKQVJfTUFOSUZFU1RTICs9IFtcImphci5tblwiXWAsXG4gICAgICAgIH0pO1xuICAgICAgfSxcbiAgICB9LFxuICBdLFxufSk7XG4iLCAiY29uc3QgX192aXRlX2luamVjdGVkX29yaWdpbmFsX2Rpcm5hbWUgPSBcIkg6XFxcXG5vcmFuZWtvXFxcXHNyY1xcXFxhcHBzXFxcXGNvbW1vblxcXFxzY3JpcHRzXCI7Y29uc3QgX192aXRlX2luamVjdGVkX29yaWdpbmFsX2ZpbGVuYW1lID0gXCJIOlxcXFxub3JhbmVrb1xcXFxzcmNcXFxcYXBwc1xcXFxjb21tb25cXFxcc2NyaXB0c1xcXFxnZW5famFybWFuaWZlc3QudHNcIjtjb25zdCBfX3ZpdGVfaW5qZWN0ZWRfb3JpZ2luYWxfaW1wb3J0X21ldGFfdXJsID0gXCJmaWxlOi8vL0g6L25vcmFuZWtvL3NyYy9hcHBzL2NvbW1vbi9zY3JpcHRzL2dlbl9qYXJtYW5pZmVzdC50c1wiOy8qIFRoaXMgU291cmNlIENvZGUgRm9ybSBpcyBzdWJqZWN0IHRvIHRoZSB0ZXJtcyBvZiB0aGUgTW96aWxsYSBQdWJsaWNcbiAqIExpY2Vuc2UsIHYuIDIuMC4gSWYgYSBjb3B5IG9mIHRoZSBNUEwgd2FzIG5vdCBkaXN0cmlidXRlZCB3aXRoIHRoaXNcbiAqIGZpbGUsIFlvdSBjYW4gb2J0YWluIG9uZSBhdCBodHRwOi8vbW96aWxsYS5vcmcvTVBMLzIuMC8uICovXG5cbmltcG9ydCAqIGFzIGZzIGZyb20gXCJub2RlOmZzL3Byb21pc2VzXCI7XG5pbXBvcnQgZmcgZnJvbSBcImZhc3QtZ2xvYlwiO1xuXG5leHBvcnQgYXN5bmMgZnVuY3Rpb24gZ2VuZXJhdGVKYXJNYW5pZmVzdChcbiAgYnVuZGxlOiBvYmplY3QsXG4gIG9wdGlvbnM6IHtcbiAgICBwcmVmaXg6IHN0cmluZztcbiAgICBuYW1lc3BhY2U6IHN0cmluZztcbiAgICByZWdpc3Rlcl90eXBlOiBcImNvbnRlbnRcIiB8IFwic2tpblwiO1xuICB9LFxuKSB7XG4gIGNvbnNvbGUubG9nKFwiZ2VuZXJhdGUgamFyLm1uXCIpO1xuICBjb25zdCB2aXRlTWFuaWZlc3QgPSBidW5kbGU7XG5cbiAgY29uc3QgYXJyID0gW107XG4gIGZvciAoY29uc3QgaSBvZiBPYmplY3QudmFsdWVzKHZpdGVNYW5pZmVzdCkpIHtcbiAgICBhcnIucHVzaCgoaSBhcyB7IGZpbGVOYW1lOiBzdHJpbmcgfSlbXCJmaWxlTmFtZVwiXSk7XG4gIH1cbiAgY29uc29sZS5sb2coXCJnZW5lcmF0ZSBlbmQgamFyLm1uXCIpO1xuXG4gIHJldHVybiBgbm9yYW5la28uamFyOlxcbiUgJHtvcHRpb25zLnJlZ2lzdGVyX3R5cGV9ICR7b3B0aW9ucy5uYW1lc3BhY2V9ICVub3JhLSR7b3B0aW9ucy5wcmVmaXh9LyBjb250ZW50YWNjZXNzaWJsZT15ZXNcXG4gJHtBcnJheS5mcm9tKFxuICAgIG5ldyBTZXQoYXJyKSxcbiAgKVxuICAgIC5tYXAoKHYpID0+IGBub3JhLSR7b3B0aW9ucy5wcmVmaXh9LyR7dn0gKCR7dn0pYClcbiAgICAuam9pbihcIlxcbiBcIil9YDtcbn1cbiJdLAogICJtYXBwaW5ncyI6ICI7QUFBZ1IsT0FBTyxVQUFVO0FBQ2pTLFNBQVMsb0JBQW9COzs7QUNNN0IsZUFBc0Isb0JBQ3BCLFFBQ0EsU0FLQTtBQUNBLFVBQVEsSUFBSSxpQkFBaUI7QUFDN0IsUUFBTSxlQUFlO0FBRXJCLFFBQU0sTUFBTSxDQUFDO0FBQ2IsYUFBVyxLQUFLLE9BQU8sT0FBTyxZQUFZLEdBQUc7QUFDM0MsUUFBSSxLQUFNLEVBQTJCLFVBQVUsQ0FBQztBQUFBLEVBQ2xEO0FBQ0EsVUFBUSxJQUFJLHFCQUFxQjtBQUVqQyxTQUFPO0FBQUEsSUFBb0IsUUFBUSxhQUFhLElBQUksUUFBUSxTQUFTLFVBQVUsUUFBUSxNQUFNO0FBQUEsR0FBNkIsTUFBTTtBQUFBLElBQzlILElBQUksSUFBSSxHQUFHO0FBQUEsRUFDYixFQUNHLElBQUksQ0FBQyxNQUFNLFFBQVEsUUFBUSxNQUFNLElBQUksQ0FBQyxLQUFLLENBQUMsR0FBRyxFQUMvQyxLQUFLLEtBQUssQ0FBQztBQUNoQjs7O0FEN0JBLElBQU0sbUNBQW1DO0FBS3pDLElBQU0sSUFBSSxDQUFDLFlBQ1QsS0FBSyxRQUFRLGtDQUFxQixPQUFPO0FBRTNDLElBQU8sc0JBQVEsYUFBYTtBQUFBLEVBQzFCLE9BQU87QUFBQSxJQUNMLFFBQVEsRUFBRSxPQUFPO0FBQUEsSUFDakIsUUFBUTtBQUFBLElBQ1IsS0FBSztBQUFBLE1BQ0gsU0FBUyxDQUFDLElBQUk7QUFBQSxNQUNkLE9BQU87QUFBQSxRQUNMLEVBQUUsb0JBQW9CO0FBQUEsUUFDdEIsRUFBRSwwQkFBMEI7QUFBQSxRQUM1QixFQUFFLHFCQUFxQjtBQUFBLE1BQ3pCO0FBQUEsSUFDRjtBQUFBLElBQ0EsZUFBZTtBQUFBLE1BQ2IsU0FBUyxRQUFRLFVBQVUsWUFBWTtBQUNyQyxZQUFJLE9BQU8sV0FBVyxXQUFXLEdBQUc7QUFDbEMsaUJBQU87QUFBQSxRQUNUO0FBQ0EsZUFBTztBQUFBLE1BQ1Q7QUFBQSxJQUNGO0FBQUEsRUFDRjtBQUFBLEVBQ0EsU0FBUztBQUFBLElBQ1A7QUFBQSxNQUNFLE1BQU07QUFBQSxNQUNOLFNBQVM7QUFBQSxNQUNULE1BQU0sZUFBZSxTQUFTLFFBQVEsU0FBUztBQUM3QyxhQUFLLFNBQVM7QUFBQSxVQUNaLE1BQU07QUFBQSxVQUNOLFVBQVU7QUFBQSxVQUNWLG9CQUFvQjtBQUFBLFVBQ3BCLFFBQVEsTUFBTSxvQkFBb0IsUUFBUTtBQUFBLFlBQ3hDLFFBQVE7QUFBQSxZQUNSLFdBQVc7QUFBQSxZQUNYLGVBQWU7QUFBQSxVQUNqQixDQUFDO0FBQUEsUUFDSCxDQUFDO0FBQ0QsYUFBSyxTQUFTO0FBQUEsVUFDWixNQUFNO0FBQUEsVUFDTixVQUFVO0FBQUEsVUFDVixvQkFBb0I7QUFBQSxVQUNwQixRQUFRO0FBQUEsUUFDVixDQUFDO0FBQUEsTUFDSDtBQUFBLElBQ0Y7QUFBQSxFQUNGO0FBQ0YsQ0FBQzsiLAogICJuYW1lcyI6IFtdCn0K
