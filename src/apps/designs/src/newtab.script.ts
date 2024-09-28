const imgs = import.meta.glob("./newtab-background/*", {
  query: "?url",
  import: "default",
});
const style = document.createElement("style");
style.textContent = `
  body {
    background-image: url(chrome://noraneko/skin${await imgs[Object.keys(imgs)[Math.floor(Math.random() * 99)]]()})
  }
  `;
document.head.appendChild(style);
