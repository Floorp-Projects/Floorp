import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import "@/globals.css";
import App from "@/App.tsx";
import { ThemeProvider } from "@/components/theme-provider.tsx";
import "@/lib/i18n/i18n.ts";

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <ThemeProvider>
      <App />
    </ThemeProvider>
  </StrictMode>,
);
