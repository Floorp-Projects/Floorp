export type {};

interface GitHubAsset {
  name: string;
  browser_download_url: string;
}

interface GitHubRelease {
  name: string;
  assets: GitHubAsset[];
}

const releases: GitHubRelease[] = JSON.parse(
  await (
    await fetch(
      "https://api.github.com/repos/black7375/Firefox-UI-Fix/releases?per_page=1",
    )
  ).text(),
);

const release = releases[0];

console.log(release.name);

let lepton_original = "";
let lepton_proton = "";
let lepton_photon = "";
release.assets.forEach((v: GitHubAsset) => {
  if (v.name === "Lepton.zip") {
    lepton_original = v.browser_download_url;
  } else if (v.name === "Lepton-Proton-Style.zip") {
    lepton_proton = v.browser_download_url;
  } else if (v.name === "Lepton-Photon-Style.zip") {
    lepton_photon = v.browser_download_url;
  }
});

console.log(lepton_original);
console.log(lepton_proton);
console.log(lepton_photon);
