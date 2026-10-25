#!/usr/bin/env bash
# Runs GfaidxBench (see docs/gfaidx-benchmark.md) for every seed node in a file, one process per run,
# alternating the indexed and the full-load mode. Prints one TSV row per run.
#
# usage: gfaidx_bench.sh <GfaidxBench> <full.gfa[.gz]|-> <indexed.gfa.gz> <seeds.txt> <max_nodes> [repeats]
#   Pass "-" as the full graph to skip the full-load runs (e.g. when the graph does not fit in memory).
set -euo pipefail

if [[ $# -lt 5 ]]; then
    sed -n '5,6p' "$0" | sed 's/^# *//'
    exit 2
fi
bench=$1 full=$2 indexed=$3 seeds=$4 max_nodes=$5 repeats=${6:-3}
export QT_QPA_PLATFORM=offscreen

"$bench" --header
for ((rep = 1; rep <= repeats; rep++)); do
    while read -r seed; do
        [[ -z $seed ]] && continue
        "$bench" indexed "$indexed" "$seed" "$max_nodes" || echo "indexed run failed for seed $seed" >&2
        if [[ $full != "-" ]]; then
            "$bench" full "$full" "$seed" "$max_nodes" || echo "full run failed for seed $seed" >&2
        fi
    done < "$seeds"
done
