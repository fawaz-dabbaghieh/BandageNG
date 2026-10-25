// Benchmark for the gfaidx integration (see docs/gfaidx-benchmark.md). Not part of ctest.
// Measures the time and peak memory to get a node neighborhood into Bandage, either by loading the
// complete graph and doing a budgeted BFS (what Bandage does without an index), or by querying a
// gfaidx index and loading only the result (what the indexed-graph path does). Run one mode and one
// seed per process, so the peak resident set size belongs to that run alone.

#include "graph/assemblygraph.h"
#include "graph/debruijnedge.h"
#include "graph/debruijnnode.h"
#include "graph/indexedgraphsession.h"
#include "graph/io.h"
#include "program/globals.h"
#include "program/memory.h"
#include "program/settings.h"

#include <QApplication>
#include <QSet>

#include <sys/resource.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <deque>

namespace {
    using Clock = std::chrono::steady_clock;

    double secondsSince(Clock::time_point start) {
        return std::chrono::duration<double>(Clock::now() - start).count();
    }

    double peakRssMiB() {
        rusage usage{};
        getrusage(RUSAGE_SELF, &usage);
#ifdef __APPLE__
        return usage.ru_maxrss / (1024.0 * 1024.0); // bytes
#else
        return usage.ru_maxrss / 1024.0; // KiB
#endif
    }

    bool buildGraph(AssemblyGraph &graph, const QString &path) {
        auto builder = io::AssemblyGraphBuilder::get(path);
        if (!builder) {
            std::fprintf(stderr, "unsupported graph format: %s\n", qPrintable(path));
            return false;
        }
        if (auto err = builder->build(graph)) {
            std::fprintf(stderr, "could not load %s: %s\n", qPrintable(path), llvm::toString(std::move(err)).c_str());
            return false;
        }
        graph.determineGraphInfo();
        return true;
    }

    // Segments are counted by name: Bandage counts a segment whose sequence is its own reverse
    // complement twice, which would make the two modes disagree on the neighborhood size.
    size_t segmentCount(const AssemblyGraph &graph) {
        QSet<QString> names;
        for (const DeBruijnNode *node : graph.m_deBruijnGraphNodes)
            names.insert(node->getNameWithoutSign());
        return names.size();
    }

    // Hop-count BFS ignoring strand, stopping at maxNodes segments, like gfaidx's get_subgraph.
    QSet<QString> neighborhood(DeBruijnNode *seed, size_t maxNodes) {
        QSet<QString> segments{seed->getNameWithoutSign()};
        std::deque<DeBruijnNode *> queue{seed};
        while (!queue.empty() && size_t(segments.size()) < maxNodes) {
            DeBruijnNode *node = queue.front();
            queue.pop_front();
            for (DeBruijnNode *strand : {node, node->getReverseComplement()}) {
                for (const DeBruijnEdge *edge : strand->edges()) {
                    for (DeBruijnNode *next : {edge->getStartingNode(), edge->getEndingNode()}) {
                        if (size_t(segments.size()) < maxNodes && !segments.contains(next->getNameWithoutSign())) {
                            segments.insert(next->getNameWithoutSign());
                            queue.push_back(next);
                        }
                    }
                }
            }
        }
        return segments;
    }

    struct Row {
        double loadSeconds = 0, extractSeconds = 0, parseSeconds = 0;
        size_t nodes = 0;
    };

    // Full load of the graph file, then the neighborhood from the in-memory graph.
    bool runFull(const QString &graphPath, const QString &seedName, unsigned maxNodes, Row &row) {
        AssemblyGraph graph;
        auto start = Clock::now();
        if (!buildGraph(graph, graphPath))
            return false;
        row.loadSeconds = secondsSince(start);

        start = Clock::now();
        auto [posNode, negNode] = graph.getNodes(seedName);
        DeBruijnNode *seed = posNode ? posNode : negNode;
        if (!seed) {
            std::fprintf(stderr, "node %s not found\n", qPrintable(seedName));
            return false;
        }
        row.nodes = neighborhood(seed, maxNodes).size();
        row.extractSeconds = secondsSince(start);
        return true;
    }

    // Open the index, query the neighborhood (written as a temporary GFA) and load only that.
    bool runIndexed(const QString &graphPath, const QString &seedName, unsigned maxNodes, Row &row) {
        auto start = Clock::now();
        const graph::IndexedGraphSession session(graphPath);
        row.loadSeconds = secondsSince(start);

        start = Clock::now();
        const auto result = session.runQuery(graph::SubgraphQuery{{seedName}, maxNodes, false});
        if (!result.error.isEmpty()) {
            std::fprintf(stderr, "query failed: %s\n", qPrintable(result.error));
            return false;
        }
        row.extractSeconds = secondsSince(start);

        start = Clock::now();
        AssemblyGraph graph;
        if (!buildGraph(graph, result.gfaPath))
            return false;
        row.parseSeconds = secondsSince(start);
        row.nodes = segmentCount(graph);
        return true;
    }
}

int main(int argc, char **argv) {
    if (argc == 2 && std::strcmp(argv[1], "--header") == 0) {
        std::printf("mode\tgraph\tseed\tmax_nodes\tload_s\textract_s\tparse_s\ttotal_s\tnodes\tinit_rss_mib\tpeak_rss_mib\n");
        return 0;
    }
    if (argc != 5 || (std::strcmp(argv[1], "full") != 0 && std::strcmp(argv[1], "indexed") != 0)) {
        std::fprintf(stderr, "usage: %s full|indexed <graph> <seed_node> <max_nodes>\n"
                             "       %s --header\n", argv[0], argv[0]);
        return 2;
    }
    const bool full = std::strcmp(argv[1], "full") == 0;
    const QString graphPath = QString::fromLocal8Bit(argv[2]);
    const QString seedName = QString::fromLocal8Bit(argv[3]);
    const unsigned maxNodes = std::strtoul(argv[4], nullptr, 10);

    QApplication app(argc, argv);
    g_memory.reset(new Memory());
    g_settings.reset(new Settings());
    const double initRss = peakRssMiB();

    Row row;
    try {
        if (!(full ? runFull(graphPath, seedName, maxNodes, row) : runIndexed(graphPath, seedName, maxNodes, row)))
            return 1;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }

    std::printf("%s\t%s\t%s\t%u\t%.3f\t%.3f\t%.3f\t%.3f\t%zu\t%.1f\t%.1f\n", argv[1], qPrintable(graphPath),
                qPrintable(seedName), maxNodes, row.loadSeconds, row.extractSeconds, row.parseSeconds,
                row.loadSeconds + row.extractSeconds + row.parseSeconds, row.nodes, initRss, peakRssMiB());
    return 0;
}
