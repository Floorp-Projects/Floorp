--
-- Represents a tree--a set of machines
--
CREATE TABLE tbox_tree (
  tree_name VARCHAR(200) UNIQUE,
  -- The short names for particular fields that will show up on the main page
  field_short_names TEXT,
  -- Name the processors that will show the fields (comma separated name=value pairs)
  field_processors TEXT,
  -- Statuses (open|closed|gated, for example)
  statuses TEXT,
  -- Min / max row size (in minutes)
  min_row_size INTEGER,
  max_row_size INTEGER,
  -- Default size of Tinderbox (in minutes)
  default_tinderbox_view INTEGER,
  -- Whether or not to make new Tinderboxen visible immediately
  new_machines_visible BOOLEAN,
  -- People who can edit this tree
  editors TEXT,

  -- Sheriff info

  -- Header
  header TEXT,
  -- Special message explaining reason for delay / whatever
  special_message TEXT,
  -- Footer
  footer TEXT,
  -- Sheriff
  sheriff TEXT,
  -- Build Engineer
  build_engineer TEXT,
  -- Status of the tree
  status TEXT,
  -- People who can sheriff this tree
  sheriffs TEXT,

  -- Build Engineer Info

  -- Set to 'off' to simply turn off cvs checkouts wherever they happen to be;
  -- set to a particular cvs date to tell all tboxes to check out for a
  -- particular date; set to blank to have cvs checkout constantly, normally
  cvs_co_date TEXT
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
  -- in_use: if true, Tinderboxes will pick up this patch
  in_use BOOLEAN
);

--
-- A tinderbox machine
--
CREATE TABLE tbox_machine (
  machine_id SERIAL,
  tree_name VARCHAR(200),

  -- Fields identifying the machine for display purposes
  machine_name VARCHAR(200),
  os VARCHAR(200),
  os_version VARCHAR(200),
  compiler VARCHAR(200),
  clobber BOOLEAN,

  -- A set of commands to give to the machine (kick is the only one right now)
  -- (will be cleared as soon as the command is given; obedience is assumed)
  commands TEXT,
  -- Whether or not this machine is visible
  visible BOOLEAN
);

--
-- Default values of fields which will be sent to the machine
--
CREATE TABLE tbox_initial_machine_config (
  tree_name VARCHAR(200),
  name TEXT,
  value TEXT
);

--
-- General fields which will be sent to the machine for configuration
-- (mozconfig, tests, cvsroot, clobber)
--
CREATE TABLE tbox_machine_config (
  machine_id INTEGER,
  name TEXT,
  value TEXT
);

--
-- A particular build on a machine
--
CREATE TABLE tbox_build (
  machine_id INTEGER,
  -- Time build started
  build_time TIMESTAMP (0),

  -- Time status was sent
  status_time TIMESTAMP (0),
  -- Current status of the build
  -- 0-99=incomplete (various phases in progress), 100-199=success,
  -- 200-299=failure, (201=kicked)
  -- 300-399=did not report (300+incomplete status from before)
  status INTEGER,
  -- Filename of the logfile
  log TEXT
);

--
-- Fields (like Tp and friends) associated with a build
--
CREATE TABLE tbox_build_field (
  machine_id INTEGER,
  build_time TIMESTAMP (0),
  name VARCHAR(200),
  value VARCHAR(200)
);

--
-- Build comments
--
CREATE TABLE tbox_build_comment (
  machine_id INTEGER,
  build_time TIMESTAMP (0),
  login VARCHAR(200),
  build_comment TEXT,
  comment_time TIMESTAMP (0)
);

--
-- Sessions
--
CREATE TABLE tbox_session (
  login VARCHAR(200),
  session_id VARCHAR(100),
  activity_time TIMESTAMP (0)
);

--
-- A bonsai info column
--
CREATE TABLE tbox_bonsai (
  bonsai_id SERIAL,
  tree_name VARCHAR(200),

  -- Parameters to bonsai
  display_name TEXT,
  bonsai_url TEXT,
  module TEXT,
  branch TEXT,
  directory TEXT,
  cvsroot TEXT,

  -- End and start date for bonsai
  start_cache TIMESTAMP(0),
  end_cache TIMESTAMP(0)
);

--
-- Cache of bonsai info info
--
CREATE TABLE tbox_bonsai_cache (
  bonsai_id INTEGER,
  checkin_date TIMESTAMP (0),
  who TEXT,
  files TEXT,
  revisions TEXT,
  size_plus INTEGER,
  size_minus INTEGER,
  description TEXT
);
