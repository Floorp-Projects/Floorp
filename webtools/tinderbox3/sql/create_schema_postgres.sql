--
-- Represents a tree--a set of machines
--
CREATE TABLE tbox_tree (
  tree_name VARCHAR(200) UNIQUE,
  password TEXT,
  -- The short names for particular fields that will show up on the main page
  field_short_names TEXT,
  -- Name the processors that will show the fields (comma separated name=value pairs)
  field_processors TEXT
);

--
-- A patch (associated with a tree)
--
CREATE TABLE tbox_patch (
  patch_id SERIAL,
  tree_name VARCHAR(200),
  patch_name VARCHAR(200),
  patch_ref TEXT,
  patch_ref_url TEXT,
  patch TEXT,
  -- obsolete: no Tinderboxes will pick up this patch
  obsolete BOOLEAN
);

--
-- A tinderbox machine
--
CREATE TABLE tbox_machine (
  machine_id SERIAL,
  tree_name VARCHAR(200),
  machine_name VARCHAR(200),
  description TEXT
);

--
-- A particular build on a machine
--
CREATE TABLE tbox_build (
  machine_id INTEGER,
  build_time TIMESTAMP,

  status_time TIMESTAMP,
  status VARCHAR(200),
  log TEXT
);

--
-- Fields (like Tp and friends) associated with a build
--
CREATE TABLE tbox_build_field (
  name VARCHAR(200),
  value VARCHAR(200)
);


--
-- Tells what patches were on a particular build
--
CREATE TABLE tbox_build_patch (
  machine_id INTEGER,
  build_time TIMESTAMP,
  patch_id INTEGER
);


-- TODO:
-- comments
-- build commands

