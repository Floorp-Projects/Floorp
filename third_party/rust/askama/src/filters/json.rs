use crate::error::{Error, Result};
use askama_escape::JsonEscapeBuffer;
use serde::Serialize;
use serde_json::to_writer_pretty;

/// Serialize to JSON (requires `json` feature)
///
/// The generated string does not contain ampersands `&`, chevrons `< >`, or apostrophes `'`.
/// To use it in a `<script>` you can combine it with the safe filter:
///
/// ``` html
/// <script>
/// var data = {{data|json|safe}};
/// </script>
/// ```
///
/// To use it in HTML attributes, you can either use it in quotation marks `"{{data|json}}"` as is,
/// or in apostrophes with the (optional) safe filter `'{{data|json|safe}}'`.
/// In HTML texts the output of e.g. `<pre>{{data|json|safe}}</pre>` is safe, too.
pub fn json<S: Serialize>(s: S) -> Result<String> {
    let mut writer = JsonEscapeBuffer::new();
    to_writer_pretty(&mut writer, &s).map_err(Error::from)?;
    Ok(writer.finish())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_json() {
        assert_eq!(json(true).unwrap(), "true");
        assert_eq!(json("foo").unwrap(), r#""foo""#);
        assert_eq!(json(true).unwrap(), "true");
        assert_eq!(json("foo").unwrap(), r#""foo""#);
        assert_eq!(
            json(vec!["foo", "bar"]).unwrap(),
            r#"[
  "foo",
  "bar"
]"#
        );
    }
}
