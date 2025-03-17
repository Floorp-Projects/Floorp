const backgroundImages = import.meta.glob("../assets/background/*.avif", {
  eager: true,
  as: "url",
});

const imageUrls = Object.values(backgroundImages);

export function getRandomBackgroundImage(): string {
  const randomIndex = Math.floor(Math.random() * imageUrls.length);
  return imageUrls[randomIndex];
}

export function getBackgroundImageCount(): number {
  return imageUrls.length;
}
