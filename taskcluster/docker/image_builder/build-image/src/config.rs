// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

use anyhow::Result;
use serde::de::Error;
use serde::Deserialize;
use std::collections::HashMap;

fn default_image_name() -> String {
    "mozilla.org/taskgraph/default-image:latest".into()
}
fn default_zstd_level() -> i32 {
    3
}

fn from_json<'de, D, T>(deserializer: D) -> Result<T, D::Error>
where
    D: serde::de::Deserializer<'de>,
    T: serde::de::DeserializeOwned,
{
    let value: String = serde::Deserialize::deserialize(deserializer)?;
    serde_json::from_str(&value).map_err(|err| {
        D::Error::invalid_value(serde::de::Unexpected::Str(&value), &&*err.to_string())
    })
}

#[derive(Deserialize, Debug, PartialEq, Eq)]
pub struct Config {
    pub context_task_id: String,
    pub context_path: String,
    pub parent_task_id: Option<String>,
    #[serde(default = "default_image_name")]
    pub image_name: String,
    #[serde(default = "default_zstd_level")]
    pub docker_image_zstd_level: i32,
    #[serde(default)]
    pub debug: bool,
    #[serde(default, deserialize_with = "from_json")]
    pub docker_build_args: HashMap<String, String>,
}

impl Config {
    pub fn from_env() -> Result<Config> {
        Ok(envy::from_env()?)
    }
}

#[cfg(test)]
mod test {
    use anyhow::Result;

    #[test]
    fn test() -> Result<()> {
        let env: Vec<(String, String)> = vec![
            ("CONTEXT_TASK_ID".into(), "xGRRgzG6QlCCwsFsyuqm0Q".into()),
            (
                "CONTEXT_PATH".into(),
                "public/docker-contexts/image.tar.gz".into(),
            ),
        ];
        let config: super::Config = envy::from_iter(env.into_iter())?;
        assert_eq!(
            config,
            super::Config {
                context_task_id: "xGRRgzG6QlCCwsFsyuqm0Q".into(),
                context_path: "public/docker-contexts/image.tar.gz".into(),
                parent_task_id: None,
                image_name: "mozilla.org/taskgraph/default-image:latest".into(),
                docker_image_zstd_level: 3,
                debug: false,
                docker_build_args: Default::default()
            }
        );
        Ok(())
    }

    #[test]
    fn test_docker_build_args() -> Result<()> {
        let env: Vec<(String, String)> = vec![
            ("CONTEXT_TASK_ID".into(), "xGRRgzG6QlCCwsFsyuqm0Q".into()),
            (
                "CONTEXT_PATH".into(),
                "public/docker-contexts/image.tar.gz".into(),
            ),
            (
                "DOCKER_BUILD_ARGS".into(),
                serde_json::json! ({
                    "test": "Value",
                })
                .to_string(),
            ),
        ];
        let config: super::Config = envy::from_iter(env.into_iter())?;
        assert_eq!(
            config,
            super::Config {
                context_task_id: "xGRRgzG6QlCCwsFsyuqm0Q".into(),
                context_path: "public/docker-contexts/image.tar.gz".into(),
                parent_task_id: None,
                image_name: "mozilla.org/taskgraph/default-image:latest".into(),
                docker_image_zstd_level: 3,
                debug: false,
                docker_build_args: [("test".to_string(), "Value".to_string())]
                    .iter()
                    .cloned()
                    .collect(),
            }
        );
        Ok(())
    }
}
