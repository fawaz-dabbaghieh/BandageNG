# Indexed graph loading

When "Load graph" is pointed at a GFA file that has a [gfaidx](https://github.com/fawaz-dabbaghieh/gfaidx)
index next to it (sidecar `.idx`/`.ndx`/... files, same naming convention gfaidx's
CLI produces), Bandage-NG does not parse the whole file. It opens the index (cheap,
no full parse) and shows the query window (`IndexedGraphDialog`) with the index's
summary (nodes, edges, paths/walks, total length, communities; see below for
where these come from) and two kinds of query:

- **Subgraph** (`gfaidx::IndexedGraph::stream_subgraph`): one or more start nodes
  (comma- or space-separated; a trailing `+`/`-` strand suffix is ignored), the
  maximum number of nodes, and a "with coordinates" toggle.
- **Region** (`stream_region`, always in all-haplotypes mode): pick a P or W line from
  a searchable list, give a 0-based, end-exclusive start and end, the haplotype gap
  (default 10,000 bp; needs the `.lnx` index) and a "with coordinates" toggle.
  - W lines are queried as (sample, sequence name) within the W line's start/end.
  - P lines are queried by name. A name ending in `:start-end` (e.g.
    `HG002#1#chr1:100-200`) is queried as the part before the suffix, within that range.
  - start must be smaller than end and both must lie inside the chosen line's range.
    Plain P paths have no known length, so only the start is checked; if the
    interval is past the end, gfaidx's own error is shown.

"Load full graph" (in the same window) bypasses gfaidx and loads the complete file
the way Bandage-NG always has. A file without an index is loaded as before.

Queries run in the background with a progress dialog (gfaidx's progress messages
appear in the status bar) and can be cancelled; the graph currently shown is kept
until a query succeeds. The result is loaded through the normal GFA parser and,
like any loaded graph, is only drawn when you click "Draw graph": the graph
details panel and the status bar show its size first, so a result too large to
lay out can be re-queried instead. If a query fails, the error is shown and the
query window reopens with the same values.

The query window's summary uses the same complete-graph numbers as the panel below.
While a query result is loaded, the "Graph information" panel shows two blocks:
"Complete graph" (the size of the whole indexed graph) and "Loaded subgraph" (the
query result). The complete graph's node count and path/walk split come from the
index itself. Edges and total length come from the `<file>.params` summary that
gfaidx 2.3 and later write next to the index. Older indexes show "unknown" for
those two, and a `.params` file whose segment count does not match the index is
ignored. After "Load full graph" the panel shows a single block, as for any other file.

## Re-querying

The graph-scope panel has an "Indexed graph: Re-query..." button, enabled only when
the current graph came from an index. It reopens the same query window (with the
previous query filled in) against the still-open index, so a new subgraph or region
replaces the current graph without reopening the file.

## Building

gfaidx is downloaded automatically at configure time (CMake `FetchContent`),
pinned to a release tarball (currently `v2.3.2`, checked by SHA-256), so a
plain clone + build includes it - no separate gfaidx checkout needed.

- **Developing gfaidx alongside Bandage-NG**: point the build at a local
  checkout instead of the pinned release with
  `cmake -DFETCHCONTENT_SOURCE_DIR_GFAIDX=/path/to/gfaidx ...`.
- **Updating the pinned version**: change the URL and `URL_HASH` in the
  `FetchContent_Declare(gfaidx ...)` block of the top-level `CMakeLists.txt`.
- **Turning it off**: `-DBANDAGE_WITH_GFAIDX=OFF` builds plain Bandage-NG
  (all integration code is behind `#ifdef BANDAGE_HAVE_GFAIDX`). It is always
  off on Windows, because gfaidx uses POSIX-only APIs (`mmap`, `unistd.h`).

## Key files

- `graph/indexedgraphsession.h`/`.cpp` - wraps `gfaidx::IndexedGraph`: summary
  stats (`graphStats()` adds the `.params` counts), the list of region targets (`regionTargets()`), and `runQuery()`, which
  runs a subgraph or region query off the GUI thread and writes the result as GFA
  into an auto-deleted temporary directory.
- `ui/dialogs/indexedgraphdialog.h`/`.cpp` - the query window (built in C++, no
  `.ui` file). One instance lives as long as its index, so it remembers the last query.
- `ui/mainwindow.cpp`: `loadGraph()` (dispatch), `openIndexedGraph()`,
  `showIndexQueryDialog()` / `runIndexQuery()` (query flow), `parseGraphFile()`
  (background GFA parsing shared by full loads and query results),
  `requeryIndexedGraph()` (the Re-query button), `displayGraphDetails()` (the
  complete-graph block).
- `command_line/info.cpp` - the CLI `info` subcommand also detects an index and
  prints its summary instead of doing a full load.
- `tests/gfaidx_spike.cpp` - manual smoke test (`GfaidxSpike` target, not in ctest).

## Not done yet

- `load`/`image`/`layout` (CLI) still always do a full load; only `info` is
  index-aware so far.
- Plain P paths can't be range-checked before querying (their length isn't known
  without a gfaidx-side addition).
- A region query can't span two records of the same fragmented contig, because
  start/end are checked against the chosen record (gfaidx itself would allow it).
- Path/walk step queries (`get_path_subgraph`) are not wired up.
- "Load full graph" on an indexed file loads a graph without paths or walks: the
  indexed `.gz` holds only H, S and L lines, and its P/W lines live in the `.pdx`.
