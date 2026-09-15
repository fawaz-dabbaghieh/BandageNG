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

## 3. Region-based (and path/walk-based) queries, wired into the left panel (BandageNG-only, not started)

`gfaidx::IndexedGraph::get_region()` (coordinate-based) and
`get_path_subgraph()` (path/walk-based) aren't wired up at all yet - only
node-seeded BFS (`get_subgraph`) is. Plan: instead of only the modal
`IndexedGraphDialog` at load time, surface query controls inline in the left
sidebar - likely reusing/extending the panel from the recent "Paths and Walks
displayed together" work, so a user can pick a path/walk or a region and load
it immediately, the same way node re-querying already works via the
"Re-query" controls. This also covers "wire this more seamlessly" - folding
querying into the existing sidebar rather than a popup dialog gating every
load.

## Rough sequencing

(1) is blocked on the user's own gfaidx work and can't start from the
BandageNG side yet. (2) and (3) are independent of each other and of (1) for
their node/path-count-only parts; (2) becomes fully useful once (1) lands.
No firm commitment on order yet - revisit this doc when resuming.
