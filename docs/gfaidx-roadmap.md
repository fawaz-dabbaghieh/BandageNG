# gfaidx integration: next steps

Working notes on where the indexed-graph-loading feature (see
`subgraph-ui.md` for what's implemented so far) goes next. Captured
2026-09-16 after the first working end-to-end pass (detect -> summary dialog
-> node-seeded subgraph load -> re-query) was confirmed working.

## 1. gfaidx: a metadata sidecar (blocked on gfaidx-side work, not started)

`gfaidx::IndexedGraph` currently only exposes `node_count()`, `community_count()`,
and `paths()` cheaply. There's no global edge count, no total sequence length,
no connected-component count - anything beyond those three requires either a
full parse or isn't tracked at index time at all.

Plan: `index_gfa` writes a new sidecar (JSON or similar - simple, human-readable,
easy for BandageNG or any other consumer to read without linking against
gfaidx's binary index-reading code) containing whole-graph metadata collected
once at index-build time:
- node count, edge count, path count, walk count
- total sequence length (and/or total node length)
- connected component count
- whatever else turns out to be cheap to collect during indexing

This is gfaidx-side work (index format decision) - the user is planning to
implement this in gfaidx itself before BandageNG's side is revisited.
BandageNG's job afterward is small: `IndexedGraphSession` reads the sidecar
(a new method, e.g. `edgeCount()`, `totalLength()`, `componentCount()`) and
the UI shows the real numbers instead of omitting edge count as it does now.

## 2. Dual stats display (BandageNG-only, not started)

Right now, loading a subgraph overwrites the graph-details panel with just
the subgraph's stats - there's no indication the user is looking at a partial
view. Once (1) above lands, show both concurrently, e.g. "3 / 1,250,000
nodes", so the user always knows they're looking at a subgraph of size X out
of an indexed graph of size Y. Needs:
- keeping the index summary around after a subgraph load (currently
  `IndexedGraphSession` is kept for re-querying, but its summary isn't
  displayed once `finishGraphLoad()` overwrites the details panel with the
  loaded AssemblyGraph's own stats)
- a UI treatment for "X / Y" pairs in `displayGraphDetails()`

Can be prototyped now with node/community/path counts even before edge count
exists from (1), then extended once edge count is available.

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

(1) is blocked on the user's own gfaidx work and can't start from the
BandageNG side yet. (2) and (3) are independent of each other and of (1) for
their node/path-count-only parts; (2) becomes fully useful once (1) lands.
No firm commitment on order yet - revisit this doc when resuming.
