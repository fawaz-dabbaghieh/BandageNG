# Indexed graph loading

When "Load graph" is pointed at a GFA file that has a [gfaidx](https://github.com/)
index next to it (sidecar `.idx`/`.ndx`/... files, same naming convention gfaidx's
CLI produces), Bandage-NG no longer parses the whole file up front. Instead it
opens the index (cheap - no full parse) and shows `IndexedGraphDialog` with the
index's summary counts (nodes, communities, paths/walks) plus two choices:

- **Load subgraph**: runs a node-seeded BFS query (`gfaidx::IndexedGraph::get_subgraph`)
  bounded by the given max-node count, and loads just that neighborhood.
- **Load full graph**: bypasses gfaidx entirely and loads the complete file the
  same way Bandage-NG always has.

A file with no index (or missing sidecars) is unaffected - `loadGraph()` goes
straight to `loadGraphFile()`, today's original behavior.

## Re-querying

Once a subgraph has been loaded from an index, the graph-scope panel's
"Re-query" controls (`subgraphNodeLineEdit` / `subgraphSizeSpinBox` /
`subgraphLoadButton`, wired to `MainWindow::requestSubgraph()`) become usable:
entering a different node ID and clicking **Re-query** runs another query
against the same still-open `IndexedGraphSession` (`m_indexedGraphSession`)
and replaces the currently loaded graph, without reopening the file. These
controls are only meaningful when a graph was loaded from an index in the
current session; otherwise they report that no index is open.

## Key files

- `graph/indexedgraphsession.h`/`.cpp` - wraps `gfaidx::IndexedGraph`, exposes
  index summary stats and `loadSubgraphByNodes()` (query -> temp GFA in an
  auto-deleted `QTemporaryDir` -> `AssemblyGraph::loadGraphFromFile()`, so
  subgraph loads reuse the ordinary GFA parser unchanged).
- `ui/dialogs/indexedgraphdialog.h`/`.cpp` - the initial-load choice dialog
  (built in C++, no `.ui` file).
- `ui/mainwindow.cpp`: `loadGraph()` (dispatch), `loadGraphFile()` (the
  original full-load path, factored out), `finishGraphLoad()` (shared
  post-load UI update, used by both the full-load and subgraph-load paths),
  `requestSubgraph()` (re-query).
- `command_line/info.cpp` - the CLI `info` subcommand also detects an index
  and prints its summary instead of doing a full load.

## Not done yet

- `load`/`image`/`layout` (CLI) still always do a full load; only `info` is
  index-aware so far.
- Region-based (`get_region`) and path/walk-based (`get_path_subgraph`)
  queries are not wired up - node-seeded BFS only.
- Subgraph queries run synchronously on the GUI thread (no progress dialog or
  cancellation) - acceptable so far because queries have been fast in testing,
  but worth revisiting for very large indexes.
