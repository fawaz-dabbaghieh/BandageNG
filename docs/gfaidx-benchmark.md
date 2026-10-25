# Benchmarking indexed-graph loading

The question: how much time and memory does it take to get a neighborhood of N nodes around a node
into Bandage-NG, with and without a gfaidx index?

- **Full** (what Bandage-NG does without an index): parse the complete GFA into an `AssemblyGraph`,
  then take the neighborhood from the in-memory graph.
- **Indexed** (what Bandage-NG does with an index): open the index, run `stream_subgraph` (written to a
  temporary GFA, as the GUI does), then parse only that GFA into an `AssemblyGraph`.

Both modes end with a graph of the same size, ready for "Draw graph". Layout and drawing are not
measured, because they cost the same in both modes.

## Tool

`tests/gfaidx_bench.cpp` builds the `GfaidxBench` target (not built by default, only with gfaidx):

```sh
make GfaidxBench    # in the build directory
GfaidxBench full    <graph.gfa[.gz]>    <seed_node> <max_nodes>
GfaidxBench indexed <indexed.gfa.gz>    <seed_node> <max_nodes>
GfaidxBench --header
```

It uses the same code as the GUI: `io::AssemblyGraphBuilder` plus `determineGraphInfo()` for parsing,
and `graph::IndexedGraphSession::runQuery()` for the query. In full mode the neighborhood is a hop-count
BFS that ignores strand and stops at `max_nodes` segments, matching gfaidx's `get_subgraph`. Bandage's
"Around nodes" scope uses a distance in steps instead, so it cannot be set to a node budget.

Each call prints one TSV row:

| column | full | indexed |
|---|---|---|
| `load_s` | parse the complete graph | open the index |
| `extract_s` | BFS on the in-memory graph | gfaidx query + write temporary GFA |
| `parse_s` | 0 | parse the subgraph GFA |
| `total_s` | sum: time until the neighborhood is in Bandage | same |
| `nodes` | segments in the neighborhood | same |
| `init_rss_mib` | peak RSS after start-up (Qt, settings), before any graph work | same |
| `peak_rss_mib` | peak resident set size of the whole run (`getrusage`) | same |

Run one mode and one seed per process, so each peak RSS belongs to that run alone.
`tests/gfaidx_bench.sh` does this for a list of seeds, alternating the two modes:

```sh
tests/gfaidx_bench.sh <build>/tests/GfaidxBench graph.gfa.gz graph.indexed.gfa.gz seeds.txt 5000 3 > results.tsv
```

Pass `-` as the full graph to run only the indexed mode, e.g. when the full graph does not fit in memory.

## Protocol

- **Seeds**: draw 10 random segment IDs with a fixed random seed, so the same seeds can be reused.
  This uses reservoir sampling over the S lines:
  ```sh
  gzip -dc graph.gfa.gz | LC_ALL=C awk -v k=10 'BEGIN{srand(42)} $1=="S"{n++; if(n<=k) r[n]=$2; else {j=int(rand()*n)+1; if(j<=k) r[j]=$2}} END{for(i=1;i<=k;i++) print r[i]}' > seeds.txt
  ```
- **Repeats**: 3 runs per seed and mode. Report the median and the range over all runs.
- **Neighborhood size**: 5,000 nodes as the main number. Optionally sweep 500 / 5,000 / 50,000:
  the indexed cost grows with the neighborhood, while the full cost stays the cost of loading the whole graph.
- **Machine**: keep the machine otherwise idle, and record CPU, RAM, OS, the Bandage-NG commit and the gfaidx version.
- **Page cache**: by default runs are warm, because the OS caches the files after the first run.
  For cold-cache numbers, clear the cache before each run:
  - macOS: `sudo purge`
  - Linux: `sync; echo 3 | sudo tee /proc/sys/vm/drop_caches`
- **One-time costs**: also report the index build (`gfaidx index_gfa` under `/usr/bin/time -l` on
  macOS, or `/usr/bin/time -v` on Linux) and the index size on disk. A reader will ask what the speed-up costs up front.

Summary per mode (median and range):

```sh
python3 - results.tsv <<'EOF'
import csv, statistics, sys
rows = list(csv.DictReader(open(sys.argv[1]), delimiter="\t"))
for mode in ("full", "indexed"):
    sel = [r for r in rows if r["mode"] == mode]
    if not sel:
        continue
    for col in ("total_s", "peak_rss_mib", "nodes"):
        v = [float(r[col]) for r in sel]
        print(f"{mode:8} {col:13} median={statistics.median(v):10.3f}  min={min(v):10.3f}  max={max(v):10.3f}  n={len(v)}")
EOF
```

## Caveats

- The index files are memory-mapped, and the pages gfaidx touches count toward peak RSS. That is
  intended: it is memory the process really uses.
- Bandage-NG counts a segment whose sequence is its own reverse complement (e.g. `GC`) twice in its
  node count. The benchmark counts unique segment names in both modes, so the two modes agree.
- The benchmark has no GUI. The GUI adds a constant overhead, roughly the same in both modes.
