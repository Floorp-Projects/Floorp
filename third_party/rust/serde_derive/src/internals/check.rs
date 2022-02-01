use internals::ast::{Container, Data, Field, Style};
use internals::attr::{Identifier, TagType};
use internals::{ungroup, Ctxt, Derive};
use syn::{Member, Type};

/// Cross-cutting checks that require looking at more than a single attrs
/// object. Simpler checks should happen when parsing and building the attrs.
pub fn check(cx: &Ctxt, cont: &mut Container, derive: Derive) {
    check_getter(cx, cont);
    check_flatten(cx, cont);
    check_identifier(cx, cont);
    check_variant_skip_attrs(cx, cont);
    check_internal_tag_field_name_conflict(cx, cont);
    check_adjacent_tag_conflict(cx, cont);
    check_transparent(cx, cont, derive);
    check_from_and_try_from(cx, cont);
}

/// Getters are only allowed inside structs (not enums) with the `remote`
/// attribute.
fn check_getter(cx: &Ctxt, cont: &Container) {
    match cont.data {
        Data::Enum(_) => {
            if cont.data.has_getter() {
                cx.error_spanned_by(
                    cont.original,
                    "#[serde(getter = \"...\")] is not allowed in an enum",
                );
            }
        }
        Data::Struct(_, _) => {
            if cont.data.has_getter() && cont.attrs.remote().is_none() {
                cx.error_spanned_by(
                    cont.original,
                    "#[serde(getter = \"...\")] can only be used in structs that have #[serde(remote = \"...\")]",
                );
            }
        }
    }
}

/// Flattening has some restrictions we can test.
fn check_flatten(cx: &Ctxt, cont: &Container) {
    match &cont.data {
        Data::Enum(variants) => {
            for variant in variants {
                for field in &variant.fields {
                    check_flatten_field(cx, variant.style, field);
                }
            }
        }
        Data::Struct(style, fields) => {
            for field in fields {
                check_flatten_field(cx, *style, field);
            }
        }
    }
}

fn check_flatten_field(cx: &Ctxt, style: Style, field: &Field) {
    if !field.attrs.flatten() {
        return;
    }
    match style {
        Style::Tuple => {
            cx.error_spanned_by(
                field.original,
                "#[serde(flatten)] cannot be used on tuple structs",
            );
        }
        Style::Newtype => {
            cx.error_spanned_by(
                field.original,
                "#[serde(flatten)] cannot be used on newtype structs",
            );
        }
        _ => {}
    }
}

/// The `other` attribute must be used at most once and it must be the last
/// variant of an enum.
///
/// Inside a `variant_identifier` all variants must be unit variants. Inside a
/// `field_identifier` all but possibly one variant must be unit variants. The
/// last variant may be a newtype variant which is an implicit "other" case.
fn check_identifier(cx: &Ctxt, cont: &Container) {
    let variants = match &cont.data {
        Data::Enum(variants) => variants,
        Data::Struct(_, _) => {
            return;
        }
    };

    for (i, variant) in variants.iter().enumerate() {
        match (
            variant.style,
            cont.attrs.identifier(),
            variant.attrs.other(),
            cont.attrs.tag(),
        ) {
            // The `other` attribute may not be used in a variant_identifier.
            (_, Identifier::Variant, true, _) => {
                cx.error_spanned_by(
                    variant.original,
                    "#[serde(other)] may not be used on a variant identifier",
                );
            }

            // Variant with `other` attribute cannot appear in untagged enum
            (_, Identifier::No, true, &TagType::None) => {
                cx.error_spanned_by(
                    variant.original,
                    "#[serde(other)] cannot appear on untagged enum",
                );
            }

            // Variant with `other` attribute must be the last one.
            (Style::Unit, Identifier::Field, true, _) | (Style::Unit, Identifier::No, true, _) => {
                if i < variants.len() - 1 {
                    cx.error_spanned_by(
                        variant.original,
                        "#[serde(other)] must be on the last variant",
                    );
                }
            }

            // Variant with `other` attribute must be a unit variant.
            (_, Identifier::Field, true, _) | (_, Identifier::No, true, _) => {
                cx.error_spanned_by(
                    variant.original,
                    "#[serde(other)] must be on a unit variant",
                );
            }

            // Any sort of variant is allowed if this is not an identifier.
            (_, Identifier::No, false, _) => {}

            // Unit variant without `other` attribute is always fine.
            (Style::Unit, _, false, _) => {}

            // The last field is allowed to be a newtype catch-all.
            (Style::Newtype, Identifier::Field, false, _) => {
                if i < variants.len() - 1 {
                    cx.error_spanned_by(
                        variant.original,
                        format!("`{}` must be the last variant", variant.ident),
                    );
                }
            }

            (_, Identifier::Field, false, _) => {
                cx.error_spanned_by(
                    variant.original,
                    "#[serde(field_identifier)] may only contain unit variants",
                );
            }

            (_, Identifier::Variant, false, _) => {
                cx.error_spanned_by(
                    variant.original,
                    "#[serde(variant_identifier)] may only contain unit variants",
                );
            }
        }
    }
}

/// Skip-(de)serializing attributes are not allowed on variants marked
/// (de)serialize_with.
fn check_variant_skip_attrs(cx: &Ctxt, cont: &Container) {
    let variants = match &cont.data {
        Data::Enum(variants) => variants,
        Data::Struct(_, _) => {
            return;
        }
    };

    for variant in variants.iter() {
        if variant.attrs.serialize_with().is_some() {
            if variant.attrs.skip_serializing() {
                cx.error_spanned_by(
                    variant.original,
                    format!(
                        "variant `{}` cannot have both #[serde(serialize_with)] and #[serde(skip_serializing)]",
                        variant.ident
                    ),
                );
            }

            for field in &variant.fields {
                let member = member_message(&field.member);

                if field.attrs.skip_serializing() {
                    cx.error_spanned_by(
                        variant.original,
                        format!(
                            "variant `{}` cannot have both #[serde(serialize_with)] and a field {} marked with #[serde(skip_serializing)]",
                            variant.ident, member
                        ),
                    );
                }

                if field.attrs.skip_serializing_if().is_some() {
                    cx.error_spanned_by(
                        variant.original,
                        format!(
                            "variant `{}` cannot have both #[serde(serialize_with)] and a field {} marked with #[serde(skip_serializing_if)]",
                            variant.ident, member
                        ),
                    );
                }
            }
        }

        if variant.attrs.deserialize_with().is_some() {
            if variant.attrs.skip_deserializing() {
                cx.error_spanned_by(
                    variant.original,
                    format!(
                        "variant `{}` cannot have both #[serde(deserialize_with)] and #[serde(skip_deserializing)]",
                        variant.ident
                    ),
                );
            }

            for field in &variant.fields {
                if field.attrs.skip_deserializing() {
                    let member = member_message(&field.member);

                    cx.error_spanned_by(
                        variant.original,
                        format!(
                            "variant `{}` cannot have both #[serde(deserialize_with)] and a field {} marked with #[serde(skip_deserializing)]",
                            variant.ident, member
                        ),
                    );
                }
            }
        }
    }
}

/// The tag of an internally-tagged struct variant must not be
/// the same as either one of its fields, as this would result in
/// duplicate keys in the serialized output and/or ambiguity in
/// the to-be-deserialized input.
fn check_internal_tag_field_name_conflict(cx: &Ctxt, cont: &Container) {
    let variants = match &cont.data {
        Data::Enum(variants) => variants,
        Data::Struct(_, _) => return,
    };

    let tag = match cont.attrs.tag() {
        TagType::Internal { tag } => tag.as_str(),
        TagType::External | TagType::Adjacent { .. } | TagType::None => return,
    };

    let diagnose_conflict = || {
        cx.error_spanned_by(
            cont.original,
            format!("variant field name `{}` conflicts with internal tag", tag),
        );
    };

    for variant in variants {
        match variant.style {
            Style::Struct => {
                for field in &variant.fields {
                    let check_ser = !field.attrs.skip_serializing();
                    let check_de = !field.attrs.skip_deserializing();
                    let name = field.attrs.name();
                    let ser_name = name.serialize_name();

                    if check_ser && ser_name == tag {
                        diagnose_conflict();
                        return;
                    }

                    for de_name in field.attrs.aliases() {
                        if check_de && de_name == tag {
                            diagnose_conflict();
                            return;
                        }
                    }
                }
            }
            Style::Unit | Style::Newtype | Style::Tuple => {}
        }
    }
}

/// In the case of adjacently-tagged enums, the type and the
/// contents tag must differ, for the same reason.
fn check_adjacent_tag_conflict(cx: &Ctxt, cont: &Container) {
    let (type_tag, content_tag) = match cont.attrs.tag() {
        TagType::Adjacent { tag, content } => (tag, content),
        TagType::Internal { .. } | TagType::External | TagType::None => return,
    };

    if type_tag == content_tag {
        cx.error_spanned_by(
            cont.original,
            format!(
                "enum tags `{}` for type and content conflict with each other",
                type_tag
            ),
        );
    }
}

/// Enums and unit structs cannot be transparent.
fn check_transparent(cx: &Ctxt, cont: &mut Container, derive: Derive) {
    if !cont.attrs.transparent() {
        return;
    }

    if cont.attrs.type_from().is_some() {
        cx.error_spanned_by(
            cont.original,
            "#[serde(transparent)] is not allowed with #[serde(from = \"...\")]",
        );
    }

    if cont.attrs.type_try_from().is_some() {
        cx.error_spanned_by(
            cont.original,
            "#[serde(transparent)] is not allowed with #[serde(try_from = \"...\")]",
        );
    }

    if cont.attrs.type_into().is_some() {
        cx.error_spanned_by(
            cont.original,
            "#[serde(transparent)] is not allowed with #[serde(into = \"...\")]",
        );
    }

    let fields = match &mut cont.data {
        Data::Enum(_) => {
            cx.error_spanned_by(
                cont.original,
                "#[serde(transparent)] is not allowed on an enum",
            );
            return;
        }
        Data::Struct(Style::Unit, _) => {
            cx.error_spanned_by(
                cont.original,
                "#[serde(transparent)] is not allowed on a unit struct",
            );
            return;
        }
        Data::Struct(_, fields) => fields,
    };

    let mut transparent_field = None;

    for field in fields {
        if allow_transparent(field, derive) {
            if transparent_field.is_some() {
                cx.error_spanned_by(
                    cont.original,
                    "#[serde(transparent)] requires struct to have at most one transparent field",
                );
                return;
            }
            transparent_field = Some(field);
        }
    }

    match transparent_field {
        Some(transparent_field) => transparent_field.attrs.mark_transparent(),
        None => match derive {
            Derive::Serialize => {
                cx.error_spanned_by(
                    cont.original,
                    "#[serde(transparent)] requires at least one field that is not skipped",
                );
            }
            Derive::Deserialize => {
                cx.error_spanned_by(
                    cont.original,
                    "#[serde(transparent)] requires at least one field that is neither skipped nor has a default",
                );
            }
        },
    }
}

fn member_message(member: &Member) -> String {
    match member {
        Member::Named(ident) => format!("`{}`", ident),
        Member::Unnamed(i) => format!("#{}", i.index),
    }
}

fn allow_transparent(field: &Field, derive: Derive) -> bool {
    if let Type::Path(ty) = ungroup(field.ty) {
        if let Some(seg) = ty.path.segments.last() {
            if seg.ident == "PhantomData" {
                return false;
            }
        }
    }

    match derive {
        Derive::Serialize => !field.attrs.skip_serializing(),
        Derive::Deserialize => !field.attrs.skip_deserializing() && field.attrs.default().is_none(),
    }
}

fn check_from_and_try_from(cx: &Ctxt, cont: &mut Container) {
    if cont.attrs.type_from().is_some() && cont.attrs.type_try_from().is_some() {
        cx.error_spanned_by(
            cont.original,
            "#[serde(from = \"...\")] and #[serde(try_from = \"...\")] conflict with each other",
        );
    }
}
