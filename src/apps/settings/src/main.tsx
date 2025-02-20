import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client';
import "@/globals.css"
import App from './App.tsx'
import { HashRouter } from 'react-router-dom'
import { ThemeProvider } from "@/components/theme-provider.tsx"
import "@/i18n.ts"

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <ThemeProvider defaultTheme="dark" storageKey="vite-ui-theme">
      <HashRouter>
        <App />
      </HashRouter>
    </ThemeProvider>
  </StrictMode>,
)
