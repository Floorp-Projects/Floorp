use crate::{AllocId, Allocation, AllocatorOptions, DEFAULT_OPTIONS, Size, Rectangle, point2, size2};

const SHELF_SPLIT_THRESHOLD: u16 = 8;
const ITEM_SPLIT_THRESHOLD: u16 = 8;

#[repr(transparent)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "serialization", derive(Serialize, Deserialize))]
struct ShelfIndex(u16);

impl ShelfIndex {
    const NONE: Self = ShelfIndex(std::u16::MAX);

    fn index(self) -> usize { self.0 as usize }

    fn is_some(self) -> bool { self.0 != std::u16::MAX }

    fn is_none(self) -> bool { self.0 == std::u16::MAX }
}

#[repr(transparent)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "serialization", derive(Serialize, Deserialize))]
struct ItemIndex(u16);

impl ItemIndex {
    const NONE: Self = ItemIndex(std::u16::MAX);

    fn index(self) -> usize { self.0 as usize }

    fn is_some(self) -> bool { self.0 != std::u16::MAX }

    fn is_none(self) -> bool { self.0 == std::u16::MAX }
}

#[derive(Clone)]
#[cfg_attr(feature = "serialization", derive(Serialize, Deserialize))]
struct Shelf {
    x: u16,
    y: u16,
    height: u16,
    prev: ShelfIndex,
    next: ShelfIndex,
    first_item: ItemIndex,
    is_empty: bool,
}

#[derive(Clone)]
#[cfg_attr(feature = "serialization", derive(Serialize, Deserialize))]
struct Item {
    x: u16,
    width: u16,
    prev: ItemIndex,
    next: ItemIndex,
    shelf: ShelfIndex,
    allocated: bool,
    generation: u16,
}

// Note: if allocating is slow we can use the guillotiere trick of storing multiple lists of free
// rects (per shelf height) instead of iterating the shelves and items.

/// A shelf-packing dynamic texture atlas allocator tracking each allocation individually and with support
/// for coalescing empty shelves.
#[derive(Clone)]
#[cfg_attr(feature = "serialization", derive(Serialize, Deserialize))]
pub struct AtlasAllocator {
    shelves: Vec<Shelf>,
    items: Vec<Item>,
    alignment: Size,
    flip_xy: bool,
    size: Size,
    first_shelf: ShelfIndex,
    free_items: ItemIndex,
    free_shelves: ShelfIndex,
    shelf_width: u16,
    allocated_space: i32,
}

impl AtlasAllocator {
    /// Create an atlas allocator with provided options.
    pub fn with_options(size: Size, options: &AllocatorOptions) -> Self {
        let (shelf_alignment, width, height) = if options.vertical_shelves {
            (options.alignment.height, size.height, size.width)
        } else {
            (options.alignment.width, size.width, size.height)
        };
        let mut shelf_width = width / options.num_columns;
        shelf_width -= shelf_width % shelf_alignment;

        let mut atlas = AtlasAllocator {
            shelves: Vec::new(),
            items: Vec::new(),
            size: size2(width, height),
            alignment: options.alignment,
            flip_xy: options.vertical_shelves,
            first_shelf: ShelfIndex(0),
            free_items: ItemIndex::NONE,
            free_shelves: ShelfIndex::NONE,
            shelf_width: shelf_width as u16,
            allocated_space: 0,
        };

        atlas.init();

        atlas
    }

    /// Create an atlas allocator with default options.
    pub fn new(size: Size) -> Self {
        Self::with_options(size, &DEFAULT_OPTIONS)
    }

    pub fn clear(&mut self) {
        self.init();
    }

    fn init(&mut self) {
        assert!(self.size.width > 0);
        assert!(self.size.height > 0);
        assert!(self.size.width <= std::u16::MAX as i32);
        assert!(self.size.height <= std::u16::MAX as i32);
        assert!(
            self.size.width.checked_mul(self.size.height).is_some(),
            "The area of the atlas must fit in a i32 value"
        );

        assert!(self.alignment.width > 0);
        assert!(self.alignment.height > 0);

        self.shelves.clear();
        self.items.clear();

        let num_columns = self.size.width as u16 / self.shelf_width;

        let mut prev = ShelfIndex::NONE;
        for i in 0..num_columns {
            let first_item = ItemIndex(self.items.len() as u16);
            let x = i * self.shelf_width;
            let current = ShelfIndex(i);
            let next = if i + 1 < num_columns { ShelfIndex(i + 1) } else { ShelfIndex::NONE };

            self.shelves.push(Shelf {
                x,
                y: 0,
                height: self.size.height as u16,
                prev,
                next,
                is_empty: true,
                first_item,
            });

            self.items.push(Item {
                x,
                width: self.shelf_width,
                prev: ItemIndex::NONE,
                next: ItemIndex::NONE,
                shelf: current,
                allocated: false,
                generation: 1,
            });

            prev = current;
        }

        self.first_shelf = ShelfIndex(0);
        self.free_items = ItemIndex::NONE;
        self.free_shelves = ShelfIndex::NONE;
        self.allocated_space = 0;
    }

    pub fn size(&self) -> Size {
        if self.flip_xy {
            size2(self.size.height, self.size.width)
        } else {
            self.size
        }
    }

    /// Allocate a rectangle in the atlas.
    pub fn allocate(&mut self, mut size: Size) -> Option<Allocation> {
        if size.is_empty()
            || size.width > std::u16::MAX as i32
            || size.height > std::u16::MAX as i32 {
            return None;
        }

        adjust_size(self.alignment.width, &mut size.width);
        adjust_size(self.alignment.height, &mut size.height);

        let (width, height) = convert_coordinates(self.flip_xy, size.width, size.height);
        let height = shelf_height(height);

        if width > self.shelf_width as i32 || height > self.size.height {
            return None;
        }

        let mut width = width as u16;
        let mut height = height as u16;

        let mut selected_shelf_height = std::u16::MAX;
        let mut selected_shelf = ShelfIndex::NONE;
        let mut selected_item = ItemIndex::NONE;
        let mut shelf_idx = self.first_shelf;
        while shelf_idx.is_some() {
            let shelf = &self.shelves[shelf_idx.index()];

            if shelf.height < height
                || shelf.height >= selected_shelf_height
                || (!shelf.is_empty && shelf.height > height + height / 2) {
                shelf_idx = shelf.next;
                continue;
            }

            let mut item_idx = shelf.first_item;
            while item_idx.is_some() {
                let item = &self.items[item_idx.index()];
                if !item.allocated && item.width >= width {
                    break;
                }

                item_idx = item.next;
            }

            if item_idx.is_some() {
                selected_shelf = shelf_idx;
                selected_shelf_height = shelf.height;
                selected_item = item_idx;

                if shelf.height == height {
                    // Perfect fit, stop searching.
                    break;
                }
            }

            shelf_idx = shelf.next;
        }

        if selected_shelf.is_none() {
            return None;
        }

        let shelf = self.shelves[selected_shelf.index()].clone();
        if shelf.is_empty {
            self.shelves[selected_shelf.index()].is_empty = false;
        }

        if shelf.is_empty && shelf.height > height + SHELF_SPLIT_THRESHOLD {
            // Split the empty shelf into one of the desired size and a new
            // empty one with a single empty item.

            let new_shelf_idx =  self.add_shelf(Shelf {
                x: shelf.x,
                y: shelf.y + height,
                height: shelf.height - height,
                prev: selected_shelf,
                next: shelf.next,
                first_item: ItemIndex::NONE,
                is_empty: true,
            });

            let new_item_idx = self.add_item(Item {
                x: shelf.x,
                width: self.shelf_width,
                prev: ItemIndex::NONE,
                next: ItemIndex::NONE,
                shelf: new_shelf_idx,
                allocated: false,
                generation: 1,
            });

            self.shelves[new_shelf_idx.index()].first_item = new_item_idx;

            let next = self.shelves[selected_shelf.index()].next;
            self.shelves[selected_shelf.index()].height = height;
            self.shelves[selected_shelf.index()].next = new_shelf_idx;

            if next.is_some() {
                self.shelves[next.index()].prev = new_shelf_idx;
            }
        } else {
            height = shelf.height;
        }

        let item = self.items[selected_item.index()].clone();

        if item.width - width > ITEM_SPLIT_THRESHOLD {

            let new_item_idx = self.add_item(Item {
                x: item.x + width,
                width: item.width - width,
                prev: selected_item,
                next: item.next,
                shelf: item.shelf,
                allocated: false,
                generation: 1,
            });

            self.items[selected_item.index()].width = width;
            self.items[selected_item.index()].next = new_item_idx;

            if item.next.is_some() {
                self.items[item.next.index()].prev = new_item_idx;
            }
        } else {
            width = item.width;
        }

        self.items[selected_item.index()].allocated = true;
        let generation = self.items[selected_item.index()].generation;

        let x0 = item.x;
        let y0 = shelf.y;
        let x1 = x0 + width;
        let y1 = y0 + height;

        let (x0, y0) = convert_coordinates(self.flip_xy, x0 as i32, y0 as i32);
        let (x1, y1) = convert_coordinates(self.flip_xy, x1 as i32, y1 as i32);

        self.check();

        let rectangle = Rectangle {
            min: point2(x0, y0),
            max: point2(x1, y1),
        };

        self.allocated_space += rectangle.area();

        Some(Allocation {
            id: AllocId::new(selected_item.0, generation),
            rectangle,
        })
    }

    /// Deallocate a rectangle in the atlas.
    pub fn deallocate(&mut self, id: AllocId) {
        let item_idx = ItemIndex(id.index());

        //let item = self.items[item_idx.index()].clone();
        let Item { mut prev, mut next, mut width, allocated, shelf, generation, .. } = self.items[item_idx.index()];
        assert!(allocated);
        assert_eq!(generation, id.generation(), "Invalid AllocId");

        self.items[item_idx.index()].allocated = false;
        self.allocated_space -= width as i32 * self.shelves[shelf.index()].height as i32;

        if next.is_some() && !self.items[next.index()].allocated {
            // Merge the next item into this one.

            let next_next = self.items[next.index()].next;
            let next_width = self.items[next.index()].width;

            self.items[item_idx.index()].next = next_next;
            self.items[item_idx.index()].width += next_width;
            width = self.items[item_idx.index()].width;

            if next_next.is_some() {
                self.items[next_next.index()].prev = item_idx;
            }

            // Add next to the free list.
            self.remove_item(next);

            next = next_next
        }

        if prev.is_some() && !self.items[prev.index()].allocated {
            // Merge the item into the previous one.

            self.items[prev.index()].next = next;
            self.items[prev.index()].width += width;

            if next.is_some() {
                self.items[next.index()].prev = prev;
            }

            // Add item_idx to the free list.
            self.remove_item(item_idx);

            prev = self.items[prev.index()].prev;
        }

        if prev.is_none() && next.is_none() {
            let shelf_idx = shelf;
            // The shelf is now empty.
            self.shelves[shelf_idx.index()].is_empty = true;

            // Only attempt to merge shelves on the same column.
            let x = self.shelves[shelf_idx.index()].x;

            let next_shelf = self.shelves[shelf_idx.index()].next;
            if next_shelf.is_some()
                && self.shelves[next_shelf.index()].is_empty
                && self.shelves[next_shelf.index()].x == x {
                // Merge the next shelf into this one.

                let next_next = self.shelves[next_shelf.index()].next;
                let next_height = self.shelves[next_shelf.index()].height;

                self.shelves[shelf_idx.index()].next = next_next;
                self.shelves[shelf_idx.index()].height += next_height;

                if next_next.is_some() {
                    self.shelves[next_next.index()].prev = shelf_idx;
                }

                // Add next to the free list.
                self.remove_shelf(next_shelf);
            }

            let prev_shelf = self.shelves[shelf_idx.index()].prev;
            if prev_shelf.is_some()
                && self.shelves[prev_shelf.index()].is_empty
                && self.shelves[prev_shelf.index()].x == x {
                // Merge the shelf into the previous one.

                let next_shelf = self.shelves[shelf_idx.index()].next;
                self.shelves[prev_shelf.index()].next = next_shelf;
                self.shelves[prev_shelf.index()].height += self.shelves[shelf_idx.index()].height;

                self.shelves[prev_shelf.index()].next = self.shelves[shelf_idx.index()].next;
                if next_shelf.is_some() {
                    self.shelves[next_shelf.index()].prev = prev_shelf;
                }

                // Add the shelf to the free list.
                self.remove_shelf(shelf_idx);
            }
        }

        self.check();
    }

    pub fn is_empty(&self) -> bool {
        let mut shelf_idx = self.first_shelf;

        while shelf_idx.is_some() {
            let shelf = &self.shelves[shelf_idx.index()];
            if !shelf.is_empty {
                return false;
            }

            shelf_idx = shelf.next;
        }

        true
    }

    /// Amount of occupied space in the atlas.
    pub fn allocated_space(&self) -> i32 {
        self.allocated_space
    }

    /// How much space is available for future allocations.
    pub fn free_space(&self) -> i32 {
        self.size.area() - self.allocated_space
    }

    pub fn iter(&self) -> Iter {
        Iter {
            atlas: self,
            idx: 0,
        }
    }

    fn remove_item(&mut self, idx: ItemIndex) {
        self.items[idx.index()].next = self.free_items;
        self.free_items = idx;
    }

    fn remove_shelf(&mut self, idx: ShelfIndex) {
        // Remove the shelf's item.
        self.remove_item(self.shelves[idx.index()].first_item);

        self.shelves[idx.index()].next = self.free_shelves;
        self.free_shelves = idx;
    }

    fn add_item(&mut self, mut item: Item) -> ItemIndex {
        if self.free_items.is_some() {
            let idx = self.free_items;
            item.generation = self.items[idx.index()].generation.wrapping_add(1);
            self.free_items = self.items[idx.index()].next;
            self.items[idx.index()] = item;

            return idx;
        }

        let idx = ItemIndex(self.items.len() as u16);
        self.items.push(item);

        idx
    }

    fn add_shelf(&mut self, shelf: Shelf) -> ShelfIndex {
        if self.free_shelves.is_some() {
            let idx = self.free_shelves;
            self.free_shelves = self.shelves[idx.index()].next;
            self.shelves[idx.index()] = shelf;

            return idx;
        }

        let idx = ShelfIndex(self.shelves.len() as u16);
        self.shelves.push(shelf);

        idx
    }

    #[cfg(not(feature = "checks"))]
    fn check(&self) {}

    #[cfg(feature = "checks")]
    fn check(&self) {
        let mut prev_empty = false;
        let mut accum_h = 0;
        let mut shelf_idx = self.first_shelf;
        let mut shelf_x = 0;
        while shelf_idx.is_some() {
            let shelf = &self.shelves[shelf_idx.index()];
            let new_column = shelf_x != shelf.x;
            if new_column {
                assert_eq!(accum_h as i32, self.size.height);
                accum_h = 0;
            }
            shelf_x = shelf.x;
            accum_h += shelf.height;
            if prev_empty && !new_column {
                assert!(!shelf.is_empty);
            }
            if shelf.is_empty {
                assert!(!self.items[shelf.first_item.index()].allocated);
                assert!(self.items[shelf.first_item.index()].next.is_none());
            }
            prev_empty = shelf.is_empty;

            let mut accum_w = 0;
            let mut prev_allocated = true;
            let mut item_idx = shelf.first_item;
            let mut prev_item_idx = ItemIndex::NONE;
            while item_idx.is_some() {
                let item = &self.items[item_idx.index()];
                accum_w += item.width;

                assert_eq!(item.prev, prev_item_idx);

                if !prev_allocated {
                    assert!(item.allocated, "item {:?} should be allocated", item_idx.0);
                }
                prev_allocated = item.allocated;

                prev_item_idx = item_idx;
                item_idx = item.next;
            }

            assert_eq!(accum_w, self.shelf_width);

            shelf_idx = shelf.next;
        }
    }

    /// Turn a valid AllocId into an index that can be used as a key for external storage.
    ///
    /// The allocator internally stores all items in a single vector. In addition allocations
    /// stay at the same index in the vector until they are deallocated. As a result the index
    /// of an item can be used as a key for external storage using vectors. Note that:
    ///  - The provided ID must correspond to an item that is currently allocated in the atlas.
    ///  - After an item is deallocated, its index may be reused by a future allocation, so
    ///    the returned index should only be considered valid during the lifetime of the its
    ///    associated item.
    ///  - indices are expected to be "reasonable" with respect to the number of allocated items,
    ///    in other words it is never larger than the maximum number of allocated items in the
    ///    atlas (making it a good fit for indexing within a sparsely populated vector).
    pub fn get_index(&self, id: AllocId) -> u32 {
        let index = id.index();
        debug_assert_eq!(self.items[index as usize].generation, id.generation());

        index as u32
    }

    /// Dump a visual representation of the atlas in SVG format.
    pub fn dump_svg(&self, output: &mut dyn std::io::Write) -> std::io::Result<()> {
        use svg_fmt::*;

        writeln!(
            output,
            "{}",
            BeginSvg {
                w: self.size.width as f32,
                h: self.size.height as f32
            }
        )?;

        self.dump_into_svg(None, output)?;

        writeln!(output, "{}", EndSvg)
    }

    /// Dump a visual representation of the atlas in SVG, omitting the beginning and end of the
    /// SVG document, so that it can be included in a larger document.
    ///
    /// If a rectangle is provided, translate and scale the output to fit it.
    pub fn dump_into_svg(&self, rect: Option<&Rectangle>, output: &mut dyn std::io::Write) -> std::io::Result<()> {
        use svg_fmt::*;

        let (sx, sy, tx, ty) = if let Some(rect) = rect {
            (
                rect.size().width as f32 / self.size.width as f32,
                rect.size().height as f32 / self.size.height as f32,
                rect.min.x as f32,
                rect.min.y as f32,
            )
        } else {
            (1.0, 1.0, 0.0, 0.0)
        };

        writeln!(
            output,
            r#"    {}"#,
            rectangle(tx, ty, self.size.width as f32 * sx, self.size.height as f32 * sy)
                .fill(rgb(40, 40, 40))
                .stroke(Stroke::Color(black(), 1.0))
        )?;

        let mut shelf_idx = self.first_shelf;
        while shelf_idx.is_some() {
            let shelf = &self.shelves[shelf_idx.index()];

            let y = shelf.y as f32 * sy;
            let h = shelf.height as f32 * sy;

            let mut item_idx = shelf.first_item;
            while item_idx.is_some() {
                let item = &self.items[item_idx.index()];

                let x = item.x as f32 * sx;
                let w = item.width as f32 * sx;

                let color = if item.allocated {
                    rgb(70, 70, 180)
                } else {
                    rgb(50, 50, 50)
                };

                let (x, y) = if self.flip_xy { (y, x) } else { (x, y) };
                let (w, h) = if self.flip_xy { (h, w) } else { (w, h) };

                writeln!(
                    output,
                    r#"    {}"#,
                    rectangle(x + tx, y + ty, w, h).fill(color).stroke(Stroke::Color(black(), 1.0))
                )?;

                item_idx = item.next;
            }

            shelf_idx = shelf.next;
        }

        Ok(())
    }

}


fn adjust_size(alignment: i32, size: &mut i32) {
    let rem = *size % alignment;
    if rem > 0 {
        *size += alignment - rem;
    }
}

fn convert_coordinates(flip_xy: bool, x: i32, y: i32) -> (i32, i32) {
    if flip_xy {
        (y, x)
    } else {
        (x, y)
    }
}

fn shelf_height(mut size: i32) -> i32 {
    let alignment = match size {
        0 ..= 31 => 8,
        32 ..= 127 => 16,
        128 ..= 511 => 32,
        _ => 64,
    };

    let rem = size % alignment;
    if rem > 0 {
        size += alignment - rem;
    }

    size
}

/// Iterator over the allocations of an atlas.
pub struct Iter<'l> {
    atlas: &'l AtlasAllocator,
    idx: usize,
}

impl<'l> Iterator for Iter<'l> {
    type Item = Allocation;

    fn next(&mut self) -> Option<Allocation> {
        if self.idx >= self.atlas.items.len() {
            return None;
        }

        while !self.atlas.items[self.idx].allocated {
            self.idx += 1;
            if self.idx >= self.atlas.items.len() {
                return None;
            }
        }

        let item = &self.atlas.items[self.idx];
        let shelf = &self.atlas.shelves[item.shelf.index()];

        let mut alloc = Allocation {
            rectangle: Rectangle {
                min: point2(
                    item.x as i32,
                    shelf.y as i32,
                ),
                max: point2(
                    (item.x + item.width) as i32,
                    (shelf.y + shelf.height) as i32,
                ),
            },
            id: AllocId::new(self.idx as u16, item.generation),
        };

        if self.atlas.flip_xy {
            std::mem::swap(&mut alloc.rectangle.min.x, &mut alloc.rectangle.min.y);
            std::mem::swap(&mut alloc.rectangle.max.x, &mut alloc.rectangle.max.y);
        }

        self.idx += 1;

        Some(alloc)
    }
}

impl<'l> std::iter::IntoIterator for &'l AtlasAllocator {
    type Item = Allocation;
    type IntoIter = Iter<'l>;
    fn into_iter(self) -> Iter<'l> {
        self.iter()
    }
}

#[test]
fn test_simple() {
    let mut atlas = AtlasAllocator::with_options(
        size2(2048, 2048),
        &AllocatorOptions {
            alignment: size2(4, 8),
            vertical_shelves: false,
            num_columns: 2,
        },
    );

    assert!(atlas.is_empty());
    assert_eq!(atlas.allocated_space(), 0);

    let a1 = atlas.allocate(size2(20, 30)).unwrap();
    let a2 = atlas.allocate(size2(30, 40)).unwrap();
    let a3 = atlas.allocate(size2(20, 30)).unwrap();

    assert!(a1.id != a2.id);
    assert!(a1.id != a3.id);
    assert!(!atlas.is_empty());

    //atlas.dump_svg(&mut std::fs::File::create("tmp.svg").expect("!!")).unwrap();

    atlas.deallocate(a1.id);
    atlas.deallocate(a2.id);
    atlas.deallocate(a3.id);

    assert!(atlas.is_empty());
    assert_eq!(atlas.allocated_space(), 0);
}

#[test]
fn test_options() {
    let alignment = size2(8, 16);

    let mut atlas = AtlasAllocator::with_options(
        size2(2000, 1000),
        &AllocatorOptions {
            alignment,
            vertical_shelves: true,
            num_columns: 1,
        },
    );
    assert!(atlas.is_empty());
    assert_eq!(atlas.allocated_space(), 0);

    let a1 = atlas.allocate(size2(20, 30)).unwrap();
    let a2 = atlas.allocate(size2(30, 40)).unwrap();
    let a3 = atlas.allocate(size2(20, 30)).unwrap();

    assert!(a1.id != a2.id);
    assert!(a1.id != a3.id);
    assert!(!atlas.is_empty());

    for id in &atlas {
        assert!(id == a1 || id == a2 || id == a3);
    }

    assert_eq!(a1.rectangle.min.x % alignment.width, 0);
    assert_eq!(a1.rectangle.min.y % alignment.height, 0);
    assert_eq!(a2.rectangle.min.x % alignment.width, 0);
    assert_eq!(a2.rectangle.min.y % alignment.height, 0);
    assert_eq!(a3.rectangle.min.x % alignment.width, 0);
    assert_eq!(a3.rectangle.min.y % alignment.height, 0);

    assert!(a1.rectangle.size().width >= 20);
    assert!(a1.rectangle.size().height >= 30);
    assert!(a2.rectangle.size().width >= 30);
    assert!(a2.rectangle.size().height >= 40);
    assert!(a3.rectangle.size().width >= 20);
    assert!(a3.rectangle.size().height >= 30);


    //atlas.dump_svg(&mut std::fs::File::create("tmp.svg").expect("!!")).unwrap();

    atlas.deallocate(a1.id);
    atlas.deallocate(a2.id);
    atlas.deallocate(a3.id);

    assert!(atlas.is_empty());
    assert_eq!(atlas.allocated_space(), 0);
}

#[test]
fn vertical() {
    let mut atlas = AtlasAllocator::with_options(size2(128, 256), &AllocatorOptions {
        num_columns: 2,
        vertical_shelves: true,
        ..DEFAULT_OPTIONS
    });

    assert_eq!(atlas.size(), size2(128, 256));

    let a = atlas.allocate(size2(32, 16)).unwrap();
    let b = atlas.allocate(size2(16, 32)).unwrap();

    assert!(a.rectangle.size().width >= 32);
    assert!(a.rectangle.size().height >= 16);

    assert!(b.rectangle.size().width >= 16);
    assert!(b.rectangle.size().height >= 32);

    let c = atlas.allocate(size2(128, 128)).unwrap();

    for _ in &atlas {}

    atlas.deallocate(a.id);
    atlas.deallocate(b.id);
    atlas.deallocate(c.id);

    for _ in &atlas {}

    assert!(atlas.is_empty());
    assert_eq!(atlas.allocated_space(), 0);
}


#[test]
fn clear() {
    let mut atlas = AtlasAllocator::new(size2(2048, 2048));

    // Run a workload a few hundred times to make sure clearing properly resets everything.
    for _ in 0..500 {
        atlas.clear();
        assert_eq!(atlas.allocated_space(), 0);

        atlas.allocate(size2(8, 2)).unwrap();
        atlas.allocate(size2(2, 8)).unwrap();
        atlas.allocate(size2(16, 512)).unwrap();
        atlas.allocate(size2(34, 34)).unwrap();
        atlas.allocate(size2(34, 34)).unwrap();
        atlas.allocate(size2(34, 34)).unwrap();
        atlas.allocate(size2(34, 34)).unwrap();
        atlas.allocate(size2(2, 8)).unwrap();
        atlas.allocate(size2(2, 8)).unwrap();
        atlas.allocate(size2(8, 2)).unwrap();
        atlas.allocate(size2(2, 8)).unwrap();
        atlas.allocate(size2(8, 2)).unwrap();
        atlas.allocate(size2(8, 8)).unwrap();
        atlas.allocate(size2(8, 8)).unwrap();
        atlas.allocate(size2(8, 8)).unwrap();
        atlas.allocate(size2(8, 8)).unwrap();
        atlas.allocate(size2(82, 80)).unwrap();
        atlas.allocate(size2(56, 56)).unwrap();
        atlas.allocate(size2(64, 66)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();
        atlas.allocate(size2(40, 40)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();
        atlas.allocate(size2(256, 52)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();
        atlas.allocate(size2(256, 52)).unwrap();
        atlas.allocate(size2(256, 52)).unwrap();
        atlas.allocate(size2(256, 52)).unwrap();
        atlas.allocate(size2(256, 52)).unwrap();
        atlas.allocate(size2(256, 52)).unwrap();
        atlas.allocate(size2(256, 52)).unwrap();
        atlas.allocate(size2(155, 52)).unwrap();
        atlas.allocate(size2(256, 52)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();
        atlas.allocate(size2(24, 24)).unwrap();
        atlas.allocate(size2(64, 64)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();
        atlas.allocate(size2(84, 84)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();
        atlas.allocate(size2(8, 2)).unwrap();
        atlas.allocate(size2(34, 34)).unwrap();
        atlas.allocate(size2(34, 34)).unwrap();
        atlas.allocate(size2(192, 192)).unwrap();
        atlas.allocate(size2(192, 192)).unwrap();
        atlas.allocate(size2(52, 52)).unwrap();
        atlas.allocate(size2(144, 144)).unwrap();
        atlas.allocate(size2(192, 192)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();
        atlas.allocate(size2(144, 144)).unwrap();
        atlas.allocate(size2(24, 24)).unwrap();
        atlas.allocate(size2(192, 192)).unwrap();
        atlas.allocate(size2(192, 192)).unwrap();
        atlas.allocate(size2(432, 243)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();
        atlas.allocate(size2(8, 2)).unwrap();
        atlas.allocate(size2(2, 8)).unwrap();
        atlas.allocate(size2(9, 9)).unwrap();
        atlas.allocate(size2(14, 14)).unwrap();
        atlas.allocate(size2(14, 14)).unwrap();
        atlas.allocate(size2(14, 14)).unwrap();
        atlas.allocate(size2(14, 14)).unwrap();
        atlas.allocate(size2(8, 8)).unwrap();
        atlas.allocate(size2(27, 27)).unwrap();
        atlas.allocate(size2(27, 27)).unwrap();
        atlas.allocate(size2(27, 27)).unwrap();
        atlas.allocate(size2(27, 27)).unwrap();
        atlas.allocate(size2(11, 12)).unwrap();
        atlas.allocate(size2(29, 28)).unwrap();
        atlas.allocate(size2(32, 32)).unwrap();

        for _ in &atlas {}
    }
}

#[test]
fn fuzz_01() {
    let s = 65472;

    let mut atlas = AtlasAllocator::new(size2(s, 64));
    let alloc = atlas.allocate(size2(s, 64)).unwrap();
    assert_eq!(alloc.rectangle.size().width, s);
    assert_eq!(alloc.rectangle.size().height, 64);

    let mut atlas = AtlasAllocator::new(size2(64, s));
    let alloc = atlas.allocate(size2(64, s)).unwrap();
    assert_eq!(alloc.rectangle.size().width, 64);
    assert_eq!(alloc.rectangle.size().height, s);

    let mut atlas = AtlasAllocator::new(size2(s, 64));
    let alloc = atlas.allocate(size2(s - 1, 64)).unwrap();
    assert_eq!(alloc.rectangle.size().width, s);
    assert_eq!(alloc.rectangle.size().height, 64);

    let mut atlas = AtlasAllocator::new(size2(64, s));
    let alloc = atlas.allocate(size2(64, s - 1)).unwrap();
    assert_eq!(alloc.rectangle.size().width, 64);
    assert_eq!(alloc.rectangle.size().height, s);

    // Because of potential alignment we won't necessarily
    // succeed at allocation something this big
    let s = std::u16::MAX as i32;

    let mut atlas = AtlasAllocator::new(size2(s, 64));
    if let Some(alloc) = atlas.allocate(size2(s, 64)) {
        assert_eq!(alloc.rectangle.size().width, s);
        assert_eq!(alloc.rectangle.size().height, 64);
    }

    let mut atlas = AtlasAllocator::new(size2(64, s));
    if let Some(alloc) = atlas.allocate(size2(64, s)) {
        assert_eq!(alloc.rectangle.size().width, 64);
        assert_eq!(alloc.rectangle.size().height, s);
    }
}


#[test]
fn fuzz_02() {
    let mut atlas = AtlasAllocator::new(size2(1000, 1000));

    assert!(atlas.allocate(size2(255, 65599)).is_none());
}

#[test]
fn fuzz_03() {
    let mut atlas = AtlasAllocator::new(size2(1000, 1000));

    let sizes = &[
        size2(999, 128),
        size2(168492810, 10),
        size2(45, 96),
        size2(-16711926, 0),
    ];

    let mut allocations = Vec::new();
    let mut allocated_space = 0;

    for size in sizes {
        if let Some(alloc) = atlas.allocate(*size) {
            allocations.push(alloc);
            allocated_space += alloc.rectangle.area();
            assert_eq!(allocated_space, atlas.allocated_space());
        }
    }

    for alloc in &allocations {
        atlas.deallocate(alloc.id);

        allocated_space -= alloc.rectangle.area();
        assert_eq!(allocated_space, atlas.allocated_space());
    }

    assert_eq!(atlas.allocated_space(), 0);
}

#[test]
fn fuzz_04() {
    let mut atlas = AtlasAllocator::new(size2(1000, 1000));

    assert!(atlas.allocate(size2(2560, 2147483647)).is_none());
}
