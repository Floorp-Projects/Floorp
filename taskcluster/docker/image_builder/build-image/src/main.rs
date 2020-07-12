// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#![forbid(unsafe_code)]

use std::collections::HashMap;
use std::path::Path;
use std::process::Command;

use anyhow::{ensure, Context, Result};
use fs_extra::dir::{move_dir, CopyOptions};
use serde::Deserialize;

mod config;
mod taskcluster;

use config::Config;

fn log_step(msg: &str) {
    println!("[build-image] {}", msg);
}

fn read_image_digest(path: &str) -> Result<String> {
    let output = Command::new("/kaniko/skopeo")
        .arg("inspect")
        .arg(format!("docker-archive:{}", path))
        .stdout(std::process::Stdio::piped())
        .spawn()?
        .wait_with_output()?;
    ensure!(output.status.success(), "Could not inspect parent image.");

    #[derive(Deserialize, Debug)]
    #[serde(rename_all = "PascalCase")]
    struct ImageInfo {
        #[serde(skip_serializing_if = "Option::is_none")]
        name: Option<String>,
        #[serde(skip_serializing_if = "Option::is_none")]
        tag: Option<String>,
        digest: String,
        // ...
    }

    let image_info: ImageInfo = serde_json::from_slice(&output.stdout)
        .with_context(|| format!("Could parse image info from {:?}", path))?;
    Ok(image_info.digest)
}

fn download_parent_image(
    cluster: &taskcluster::TaskCluster,
    task_id: &str,
    dest: &str,
) -> Result<String> {
    zstd::stream::copy_decode(
        cluster.stream_artifact(&task_id, "public/image.tar.zst")?,
        std::fs::File::create(dest)?,
    )
    .context("Could not download parent image.")?;

    read_image_digest(dest)
}

fn build_image(
    context_path: &str,
    dest: &str,
    debug: bool,
    build_args: HashMap<String, String>,
) -> Result<()> {
    let mut command = Command::new("/kaniko/executor");
    command
        .stderr(std::process::Stdio::inherit())
        .args(&["--context", &format!("tar://{}", context_path)])
        .args(&["--destination", "image"])
        .args(&["--dockerfile", "Dockerfile"])
        .arg("--no-push")
        .args(&["--cache-dir", "/workspace/cache"])
        .arg("--single-snapshot")
        // FIXME: Generating reproducible layers currently causes OOM.
        // .arg("--reproducible")
        .arg("--whitelist-var-run=false")
        .args(&["--tarPath", dest]);
    if debug {
        command.args(&["-v", "debug"]);
    }
    for (key, value) in build_args {
        command.args(&["--build-arg", &format!("{}={}", key, value)]);
    }
    let status = command.status()?;
    ensure!(status.success(), "Could not build image.");
    Ok(())
}

fn repack_image(source: &str, dest: &str, image_name: &str) -> Result<()> {
    let status = Command::new("/kaniko/skopeo")
        .arg("copy")
        .arg(format!("docker-archive:{}", source))
        .arg(format!("docker-archive:{}:{}", dest, image_name))
        .stderr(std::process::Stdio::inherit())
        .status()?;
    ensure!(status.success(), "Could repack image.");
    Ok(())
}

fn main() -> Result<()> {
    // Kaniko expects everything to be in /kaniko, so if not running from there, move
    // everything there.
    if let Some(path) = std::env::current_exe()?.parent() {
        if path != Path::new("/kaniko") {
            let mut options = CopyOptions::new();
            options.copy_inside = true;
            move_dir(path, "/kaniko", &options)?;
        }
    }

    let config = Config::from_env().context("Could not parse environment variables.")?;

    let cluster = taskcluster::TaskCluster::from_env()?;

    let mut build_args = config.docker_build_args;

    build_args.insert("TASKCLUSTER_ROOT_URL".into(), cluster.root_url());

    log_step("Downloading context.");

    std::io::copy(
        &mut cluster.stream_artifact(&config.context_task_id, &config.context_path)?,
        &mut std::fs::File::create("/workspace/context.tar.gz")?,
    )
    .context("Could not download image context.")?;

    if let Some(parent_task_id) = config.parent_task_id {
        log_step("Downloading image.");
        let digest = download_parent_image(&cluster, &parent_task_id, "/workspace/parent.tar")?;

        log_step(&format!("Parent image digest {}", &digest));
        std::fs::create_dir_all("/workspace/cache")?;
        std::fs::rename(
            "/workspace/parent.tar",
            format!("/workspace/cache/{}", digest),
        )?;

        build_args.insert(
            "DOCKER_IMAGE_PARENT".into(),
            format!("parent:latest@{}", digest),
        );
    }

    log_step("Building image.");
    build_image(
        "/workspace/context.tar.gz",
        "/workspace/image-pre.tar",
        config.debug,
        build_args,
    )?;
    log_step("Repacking image.");
    repack_image(
        "/workspace/image-pre.tar",
        "/workspace/image.tar",
        &config.image_name,
    )?;

    log_step("Compressing image.");
    compress_file(
        "/workspace/image.tar",
        "/workspace/image.tar.zst",
        config.docker_image_zstd_level,
    )?;

    Ok(())
}

fn compress_file(
    source: impl AsRef<std::path::Path>,
    dest: impl AsRef<std::path::Path>,
    zstd_level: i32,
) -> Result<()> {
    Ok(zstd::stream::copy_encode(
        std::fs::File::open(source)?,
        std::fs::File::create(dest)?,
        zstd_level,
    )?)
}
