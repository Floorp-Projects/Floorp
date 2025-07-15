const backgroundImages = import.meta.glob("../assets/background/*.avif", {
  eager: true,
  as: "url",
});

const floorpImages = import.meta.glob("../assets/floorp/*.png", {
  eager: true,
  as: "url",
});

const imageUrls = Object.values(backgroundImages);

export function getRandomBackgroundImage(): string {
  const randomIndex = Math.floor(Math.random() * imageUrls.length);
  return imageUrls[randomIndex];
}

export function getAllBackgroundImages(): string[] {
  return imageUrls;
}

export function getFloorpImages(): { name: string; url: string }[] {
  return Object.entries(floorpImages).map(([path, url]) => {
    const fileName = path.split("/").pop() || "";
    return {
      name: fileName,
      url: url as string,
    };
  });
}

export function getSelectedFloorpImage(
  imageName: string | null,
): string | null {
  if (!imageName) return null;

  const foundImage = Object.entries(floorpImages).find(([path]) => {
    return path.includes(imageName);
  });

  return foundImage ? foundImage[1] as string : null;
}
