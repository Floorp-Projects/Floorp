use crate::source_location_accessor::SourceLocationAccessor;
use crate::type_id::{NodeTypeId, NodeTypeIdAccessor};
use crate::SourceLocation;
use std::collections::HashMap;

#[derive(Debug, PartialEq, Eq, Clone, Copy, Hash)]
pub struct Key {
    type_id: NodeTypeId,
    loc: SourceLocation,
}

impl Key {
    pub fn new<NodeT>(node: &NodeT) -> Self
    where
        NodeT: SourceLocationAccessor + NodeTypeIdAccessor,
    {
        Self {
            type_id: node.get_type_id(),
            loc: node.get_loc(),
        }
    }
}

impl NodeTypeIdAccessor for Key {
    fn get_type_id(&self) -> NodeTypeId {
        self.type_id
    }
}

impl SourceLocationAccessor for Key {
    fn set_loc(&mut self, start: SourceLocation, end: SourceLocation) {
        self.loc.start = start.start;
        self.loc.end = end.end;
    }

    fn get_loc(&self) -> SourceLocation {
        self.loc
    }
}

#[derive(Debug)]
struct Item<ValueT> {
    key: Key,
    value: ValueT,
}

impl<ValueT> Item<ValueT> {
    fn new(key: Key, value: ValueT) -> Self {
        Self { key, value }
    }
}

type ItemIndex = usize;

#[derive(Debug)]
pub struct AssociatedDataItemIndex {
    index: ItemIndex,
}
impl AssociatedDataItemIndex {
    fn new(index: ItemIndex) -> Self {
        Self { index }
    }
}

// A map from AST node to `ValueT`, to associate extra data to AST node.
#[derive(Debug)]
pub struct AssociatedData<ValueT> {
    items: Vec<Item<ValueT>>,
    map: HashMap<Key, ItemIndex>,
}

impl<ValueT> AssociatedData<ValueT> {
    pub fn new() -> Self {
        Self {
            items: Vec::new(),
            map: HashMap::new(),
        }
    }

    // Insert an item for `node`.
    // Returns the index of the inserted item.
    pub fn insert<NodeT>(&mut self, node: &NodeT, value: ValueT) -> AssociatedDataItemIndex
    where
        NodeT: SourceLocationAccessor + NodeTypeIdAccessor,
    {
        let index = self.items.len();
        let key = Key::new(node);
        self.items.push(Item::new(key.clone(), value));
        self.map.insert(key, index);

        AssociatedDataItemIndex::new(index)
    }

    // Get the immutable reference of the item for the index.
    // The index is the return value of `insert` or `to_index`.
    pub fn get_by_index(&self, index: AssociatedDataItemIndex) -> &ValueT {
        // NOTE: This can panic if `index` is created by another instance of
        // `AssociatedData`.
        &self.items[index.index].value
    }

    // Get the mutable reference of the item for the index.
    // The index is the return value of `insert` or `to_index`.
    pub fn get_mut_by_index(&mut self, index: AssociatedDataItemIndex) -> &mut ValueT {
        // NOTE: This can panic if `index` is created by another instance of
        // `AssociatedData`.
        &mut self.items[index.index].value
    }

    // Get the immutable reference of the item for `node`.
    // `None` if there's no item inserted for `node`.
    pub fn get<NodeT>(&self, node: &NodeT) -> Option<&ValueT>
    where
        NodeT: SourceLocationAccessor + NodeTypeIdAccessor,
    {
        self.to_index(node)
            .and_then(|index| Some(self.get_by_index(index)))
    }

    // Get the mutable reference of the item for `node`.
    // `None` if there's no item inserted for `node`.
    pub fn get_mut<NodeT>(&mut self, node: &NodeT) -> Option<&mut ValueT>
    where
        NodeT: SourceLocationAccessor + NodeTypeIdAccessor,
    {
        self.to_index(node)
            .and_then(move |index| Some(self.get_mut_by_index(index)))
    }

    // Returns the index for the item for `node`.
    // `None` if there's no item inserted for `node`.
    pub fn to_index<NodeT>(&self, node: &NodeT) -> Option<AssociatedDataItemIndex>
    where
        NodeT: SourceLocationAccessor + NodeTypeIdAccessor,
    {
        let key = Key::new(node);
        match self.map.get(&key) {
            Some(index) => Some(AssociatedDataItemIndex::new(*index)),
            None => None,
        }
    }
}
