/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  safelist: [
    {
      pattern: /./,
      variants: ['hover', 'focus', 'active', 'disabled', 'dark'],
    }
  ],
  plugins: [require("daisyui")],
  daisyui: {
    themes: ["lofi", "business"],
  },
}
