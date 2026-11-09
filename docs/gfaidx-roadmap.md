# gfaidx integration: next steps

Working notes on where the indexed-graph-loading feature (see
`subgraph-ui.md` for what's implemented so far) goes next. Captured
2026-09-16 after the first working end-to-end pass (detect -> summary dialog
-> node-seeded subgraph load -> re-query) was confirmed working.

## 1. gfaidx: a metadata sidecar (done 2026-11-09)

gfaidx 2.3 writes `<file>.params`, a JSON summary with the segment, link, path,
walk and path-step counts and the total sequence length (plus the index
parameters and file sizes). `IndexedGraphSession::graphStats()` reads it.
Connected-component count is not in it.

## 2. Dual stats display (done 2026-11-09)

While a query result is loaded, the "Graph information" panel shows a "Complete
graph" block (from the index and `.params`) above a "Loaded subgraph" block.
The query window's summary shows the same numbers. See `subgraph-ui.md`. The
CLI `info` command still shows only nodes, communities and paths/walks; it could
use `graphStats()` too.

## 3. Region and path/walk queries (region queries done 2026-10-24)

Done: the query window (shown on load and from the sidebar's "Re-query..."
button) has a Subgraph tab (`get_subgraph`) and a Region tab (`get_region`, mode
all, haplotype gap, with-coordinates), running in the background with cancel.
See `subgraph-ui.md`.

Still open:
- `get_path_subgraph()` (a range of steps along a path/walk) is not wired up.
- gfaidx-side: plain P paths (no `:start-end` in the name) have no known length,
  so their end can't be range-checked before querying. The user plans to add a way
  to get path coordinates/lengths to gfaidx; BandageNG should use it in
  `IndexedGraphSession::regionTargets()` once it exists.
- gfaidx-side: `get_region` does not reject an interval that runs past the end of
  a W line/P path. E.g. `chr1:101-500` on a W line covering 100-106 returned a
  subgraph with `chr1#subpath_0_2 * *` instead of an error. BandageNG checks
  W lines and `:start-end` P names itself, so this only affects plain P paths.
- gfaidx-side: the in-memory `Graph` returned by `get_subgraph`/`get_region` is
  very slow for results with many path/walk records. On the PGGB chr21 graph a
  5,000-node `get_subgraph` has 525,712 P records (896 MB of GFA): the CLI
  writes it in 9 s, but `get_subgraph` was still running after 7 minutes at
  4.5 GiB. The two causes:
  - `Graph::add_path` (`src/api/graph.cpp`) scans every existing path for a
    duplicate id, so building the graph is quadratic in the number of paths.
  - `materialize()` (`src/api/indexed_graph.cpp`) buffers the whole result in
    an `ostringstream`, copies it with `.str()` into an `istringstream`, then
    parses it.

  BandageNG now uses `stream_subgraph`/`stream_region` and writes the lines
  straight to its temporary file, so it is not affected (28 s and 2.1 GiB in
  total for that query, including Bandage's own parse). Other API users still
  would be.

## 4. Windows support (gfaidx-side, not started)

gfaidx is now fetched automatically at configure time (see "Building" in
`subgraph-ui.md`), but it is forced off on Windows: the library uses
POSIX-only APIs (`mmap`/`sys/mman.h`, `unistd.h`, `fcntl.h`, `sysconf`) and
links `libm`, none of which exist with MSVC. Porting means a small mmap shim
(`CreateFileMapping`/`MapViewOfFile` on Windows), guarding the POSIX headers,
not linking `m` on `WIN32`, and a zlib that CMake can find on Windows. Once
that lands, drop the `"NOT WIN32"` condition on `BANDAGE_WITH_GFAIDX`.

## 5. Release binaries from the fork (deferred - more app development first)

Agreed 2026-10-24 to postpone. Notes for when it's picked up:
- The fork already has upstream's `.github/workflows/snapshots.yml`, which
  builds a Linux AppImage, a macOS universal DMG and a Windows ZIP and
  publishes them with the built-in `GITHUB_TOKEN`; it has no
  `asl/BandageNG` guard, so it runs on the fork. Pushes to `dev` update a
  rolling "continuous" pre-release; pushing a `v*` tag makes a real release.
- GitHub disables workflows on forks until Actions are enabled in the fork's
  Actions tab.
- `release.yml` / `auto-release.yml` (automatic monthly versioning) only run
  on `asl/BandageNG` and need a `RELEASE_TOKEN` secret - tag fork releases by
  hand instead, with a distinct version (e.g. `v2026.9.1-gfaidx.1`) so they
  can't be confused with upstream's.
- gfaidx is linked statically, so nothing extra needs bundling. It is MIT
  (Bandage-NG is GPL-3.0, compatible) - ship its licence notice.
- The macOS universal (x86_64 + arm64) build of gfaidx was verified locally;
  Linux is covered by gfaidx's own CI but not yet by this repo's workflows
  (the runner needs zlib development headers for gfaidx's
  `find_package(ZLIB)`).

## Rough sequencing

(1) and (2) are done. What is left of (3) and (4) is mostly gfaidx-side.
No firm commitment on order yet - revisit this doc when resuming.
