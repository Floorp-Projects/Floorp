import daisyui from "npm:daisyui";

export default {
  content: [
    "./core/**/*.{ts,tsx,js,jsx}",
    "./about/**/*.{ts,tsx,js,jsx}",
    "./**/*.{ts,tsx,js,jsx}",
  ],
  theme: {
    extend: {},
  },
  plugins: [daisyui],
  daisyui: {
    themes: ["light", "dark", "cupcake"],
  },
};
