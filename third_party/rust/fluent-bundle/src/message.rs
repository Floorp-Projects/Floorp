use fluent_syntax::ast;

/// [`FluentAttribute`] is a component of a compound [`FluentMessage`].
///
/// It represents a key-value pair providing a translation of a component
/// of a user interface widget localized by the given message.
///
/// # Example
///
/// ```
/// use fluent_bundle::{FluentResource, FluentBundle};
///
/// let source = r#"
///
/// confirm-modal = Are you sure?
///     .confirm = Yes
///     .cancel = No
///     .tooltip = Closing the window will lose all unsaved data.
///
/// "#;
///
/// let resource = FluentResource::try_new(source.to_string())
///     .expect("Failed to parse the resource.");
///
/// let mut bundle = FluentBundle::default();
/// bundle.add_resource(resource)
///     .expect("Failed to add a resource.");
///
/// let msg = bundle.get_message("confirm-modal")
///     .expect("Failed to retrieve a message.");
///
/// let mut err = vec![];
///
/// let attributes = msg.attributes().map(|attr| {
///     bundle.format_pattern(attr.value(), None, &mut err)
/// }).collect::<Vec<_>>();
///
/// assert_eq!(attributes[0], "Yes");
/// assert_eq!(attributes[1], "No");
/// assert_eq!(attributes[2], "Closing the window will lose all unsaved data.");
/// ```
#[derive(Debug, PartialEq)]
pub struct FluentAttribute<'m> {
    node: &'m ast::Attribute<&'m str>,
}

impl<'m> FluentAttribute<'m> {
    /// Retrieves an id of an attribute.
    ///
    /// # Example
    ///
    /// ```
    /// # use fluent_bundle::{FluentResource, FluentBundle};
    /// # let source = r#"
    /// # confirm-modal =
    /// #     .confirm = Yes
    /// # "#;
    /// # let resource = FluentResource::try_new(source.to_string())
    /// #     .expect("Failed to parse the resource.");
    /// # let mut bundle = FluentBundle::default();
    /// # bundle.add_resource(resource)
    /// #     .expect("Failed to add a resource.");
    /// let msg = bundle.get_message("confirm-modal")
    ///     .expect("Failed to retrieve a message.");
    ///
    /// let attr1 = msg.attributes().next()
    ///     .expect("Failed to retrieve an attribute.");
    ///
    /// assert_eq!(attr1.id(), "confirm");
    /// ```
    pub fn id(&self) -> &'m str {
        &self.node.id.name
    }

    /// Retrieves an value of an attribute.
    ///
    /// # Example
    ///
    /// ```
    /// # use fluent_bundle::{FluentResource, FluentBundle};
    /// # let source = r#"
    /// # confirm-modal =
    /// #     .confirm = Yes
    /// # "#;
    /// # let resource = FluentResource::try_new(source.to_string())
    /// #     .expect("Failed to parse the resource.");
    /// # let mut bundle = FluentBundle::default();
    /// # bundle.add_resource(resource)
    /// #     .expect("Failed to add a resource.");
    /// let msg = bundle.get_message("confirm-modal")
    ///     .expect("Failed to retrieve a message.");
    ///
    /// let attr1 = msg.attributes().next()
    ///     .expect("Failed to retrieve an attribute.");
    ///
    /// let mut err = vec![];
    ///
    /// let value = attr1.value();
    /// assert_eq!(
    ///     bundle.format_pattern(value, None, &mut err),
    ///     "Yes"
    /// );
    /// ```
    pub fn value(&self) -> &'m ast::Pattern<&'m str> {
        &self.node.value
    }
}

impl<'m> From<&'m ast::Attribute<&'m str>> for FluentAttribute<'m> {
    fn from(attr: &'m ast::Attribute<&'m str>) -> Self {
        FluentAttribute { node: attr }
    }
}

/// [`FluentMessage`] is a basic translation unit of the Fluent system.
///
/// The instance of a message is returned from the
/// [`FluentBundle::get_message`](crate::bundle::FluentBundle::get_message)
/// method, for the lifetime of the [`FluentBundle`](crate::bundle::FluentBundle) instance.
///
/// # Example
///
/// ```
/// use fluent_bundle::{FluentResource, FluentBundle};
///
/// let source = r#"
///
/// hello-world = Hello World!
///
/// "#;
///
/// let resource = FluentResource::try_new(source.to_string())
///     .expect("Failed to parse the resource.");
///
/// let mut bundle = FluentBundle::default();
/// bundle.add_resource(resource)
///     .expect("Failed to add a resource.");
///
/// let msg = bundle.get_message("hello-world")
///     .expect("Failed to retrieve a message.");
///
/// assert!(msg.value().is_some());
/// ```
///
/// That value can be then passed to
/// [`FluentBundle::format_pattern`](crate::bundle::FluentBundle::format_pattern) to be formatted
/// within the context of a given [`FluentBundle`](crate::bundle::FluentBundle) instance.
///
/// # Compound Message
///
/// A message may contain a `value`, but it can also contain a list of [`FluentAttribute`] elements.
///
/// If a message contains attributes, it is called a "compound" message.
///
/// In such case, the message contains a list of key-value attributes that represent
/// different translation values associated with a single translation unit.
///
/// This is useful for scenarios where a [`FluentMessage`] is associated with a
/// complex User Interface widget which has multiple attributes that need to be translated.
/// ```text
/// confirm-modal = Are you sure?
///     .confirm = Yes
///     .cancel = No
///     .tooltip = Closing the window will lose all unsaved data.
/// ```
#[derive(Debug, PartialEq)]
pub struct FluentMessage<'m> {
    node: &'m ast::Message<&'m str>,
}

impl<'m> FluentMessage<'m> {
    /// Retrieves an option of a [`ast::Pattern`](fluent_syntax::ast::Pattern).
    ///
    /// # Example
    ///
    /// ```
    /// # use fluent_bundle::{FluentResource, FluentBundle};
    /// # let source = r#"
    /// # hello-world = Hello World!
    /// # "#;
    /// # let resource = FluentResource::try_new(source.to_string())
    /// #     .expect("Failed to parse the resource.");
    /// # let mut bundle = FluentBundle::default();
    /// # bundle.add_resource(resource)
    /// #     .expect("Failed to add a resource.");
    /// let msg = bundle.get_message("hello-world")
    ///     .expect("Failed to retrieve a message.");
    ///
    /// if let Some(value) = msg.value() {
    ///     let mut err = vec![];
    ///     assert_eq!(
    ///         bundle.format_pattern(value, None, &mut err),
    ///         "Hello World!"
    ///     );
    /// #   assert_eq!(err.len(), 0);
    /// }
    /// ```
    pub fn value(&self) -> Option<&'m ast::Pattern<&'m str>> {
        self.node.value.as_ref()
    }

    /// An iterator over [`FluentAttribute`] elements.
    ///
    /// # Example
    ///
    /// ```
    /// # use fluent_bundle::{FluentResource, FluentBundle};
    /// # let source = r#"
    /// # hello-world =
    /// #     .label = This is a label
    /// #     .accesskey = C
    /// # "#;
    /// # let resource = FluentResource::try_new(source.to_string())
    /// #     .expect("Failed to parse the resource.");
    /// # let mut bundle = FluentBundle::default();
    /// # bundle.add_resource(resource)
    /// #     .expect("Failed to add a resource.");
    /// let msg = bundle.get_message("hello-world")
    ///     .expect("Failed to retrieve a message.");
    ///
    /// let mut err = vec![];
    ///
    /// for attr in msg.attributes() {
    ///     let _ = bundle.format_pattern(attr.value(), None, &mut err);
    /// }
    /// # assert_eq!(err.len(), 0);
    /// ```
    pub fn attributes(&self) -> impl Iterator<Item = FluentAttribute<'m>> {
        self.node.attributes.iter().map(Into::into)
    }

    /// Retrieve a single [`FluentAttribute`] element.
    ///
    /// # Example
    ///
    /// ```
    /// # use fluent_bundle::{FluentResource, FluentBundle};
    /// # let source = r#"
    /// # hello-world =
    /// #     .label = This is a label
    /// #     .accesskey = C
    /// # "#;
    /// # let resource = FluentResource::try_new(source.to_string())
    /// #     .expect("Failed to parse the resource.");
    /// # let mut bundle = FluentBundle::default();
    /// # bundle.add_resource(resource)
    /// #     .expect("Failed to add a resource.");
    /// let msg = bundle.get_message("hello-world")
    ///     .expect("Failed to retrieve a message.");
    ///
    /// let mut err = vec![];
    ///
    /// if let Some(attr) = msg.get_attribute("label") {
    ///     assert_eq!(
    ///         bundle.format_pattern(attr.value(), None, &mut err),
    ///         "This is a label"
    ///     );
    /// }
    /// # assert_eq!(err.len(), 0);
    /// ```
    pub fn get_attribute(&self, key: &str) -> Option<FluentAttribute<'m>> {
        self.node
            .attributes
            .iter()
            .find(|attr| attr.id.name == key)
            .map(Into::into)
    }
}

impl<'m> From<&'m ast::Message<&'m str>> for FluentMessage<'m> {
    fn from(msg: &'m ast::Message<&'m str>) -> Self {
        FluentMessage { node: msg }
    }
}
