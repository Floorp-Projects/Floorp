import { defineConfig } from "vite";
import tsconfigPaths from "vite-tsconfig-paths"
import react from '@vitejs/plugin-react-swc'

export default defineConfig({
  server: {
    port: 5183,
    strictPort: true,
  },
  css:{
    postcss: import.meta.dirname,
  },
  build: {
    outDir:"_dist"
  },
  plugins: [
    tsconfigPaths(),
    react()
  ],
})