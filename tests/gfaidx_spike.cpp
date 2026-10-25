// Manual smoke test for the gfaidx integration (see docs/subgraph-ui.md). Not part of ctest.
// Indexes a GFA with the gfaidx CLI, then exercises IndexedGraphSession: summary, a subgraph
// query, a region query, error reporting and cancellation, loading each result into Bandage.

#include "graph/assemblygraph.h"
#include "graph/indexedgraphsession.h"
#include "graph/io.h"
#include "program/globals.h"
#include "program/memory.h"
#include "program/settings.h"

#include <QApplication>
#include <QTemporaryDir>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
    bool loadResult(const char *label, const graph::IndexQueryResult &result) {
        if (!result.error.isEmpty() || result.cancelled) {
            std::fprintf(stderr, "%s: query failed: %s\n", label, qPrintable(result.error));
            return false;
        }
        AssemblyGraph graph;
        auto builder = io::AssemblyGraphBuilder::get(result.gfaPath);
        if (auto err = builder->build(graph)) {
            std::fprintf(stderr, "%s: Bandage could not load the query result: %s\n", label,
                         llvm::toString(std::move(err)).c_str());
            return false;
        }
        std::printf("%s: loaded nodes=%zu edges=%zu paths=%zu walks=%zu\n", label,
                    graph.m_deBruijnGraphNodes.size(), graph.m_deBruijnGraphEdges.size(),
                    graph.m_deBruijnGraphPaths.size(), graph.m_deBruijnGraphWalks.size());
        return !graph.m_deBruijnGraphNodes.empty();
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s <input.gfa> <seed_node_id>\n", argv[0]);
        return 2;
    }
    const QString seedNode = QString::fromStdString(argv[2]);

    QApplication app(argc, argv);
    g_memory.reset(new Memory());
    g_settings.reset(new Settings());

    QTemporaryDir workdir;
    if (!workdir.isValid()) {
        std::fprintf(stderr, "failed to create temp dir\n");
        return 1;
    }
    const QString indexedGraphPath = workdir.filePath("graph.gfa.gz");

    const std::string indexCmd = "\"" GFAIDX_CLI_PATH "\" index_gfa \"" + std::string(argv[1]) +
                                  "\" \"" + indexedGraphPath.toStdString() + "\" --tmp_dir \"" +
                                  workdir.path().toStdString() +
                                  "\" --max_chunk_nodes 0 --min_chunk_nodes 0 > /dev/null 2>&1";
    if (std::system(indexCmd.c_str()) != 0) {
        std::fprintf(stderr, "index_gfa failed\n");
        return 1;
    }

    if (!graph::IndexedGraphSession::isIndexed(indexedGraphPath)) {
        std::fprintf(stderr, "IndexedGraphSession::isIndexed() did not detect the index\n");
        return 1;
    }

    const graph::IndexedGraphSession session(indexedGraphPath);
    std::printf("index summary: node_count=%llu community_count=%u path_count=%zu\n",
                static_cast<unsigned long long>(session.nodeCount()), session.communityCount(),
                session.pathCount());

    if (!loadResult("subgraph", session.runQuery(graph::SubgraphQuery{{seedNode}, 10, true})))
        return 1;

    const auto targets = session.regionTargets();
    std::printf("region targets: %zu\n", targets.size());
    for (const auto &target : targets) {
        std::printf("  %s\n", qPrintable(target.label));
        if (!target.end)
            continue;
        const graph::RegionQuery region{target.reference, target.sequence, target.begin, *target.end, true, 10'000};
        if (!loadResult(qPrintable("region " + graph::describeQuery(region)), session.runQuery(region)))
            return 1;
        break;
    }

    const auto missing = session.runQuery(graph::RegionQuery{"", "no_such_path", 0, 10, false, std::nullopt});
    if (missing.error.isEmpty()) {
        std::fprintf(stderr, "expected an error for a region on a missing path\n");
        return 1;
    }
    std::printf("missing path error is relayed: %s\n", qPrintable(missing.error));

    const auto cancelled = session.runQuery(graph::SubgraphQuery{{seedNode}, 10, false}, {}, [] { return false; });
    if (!cancelled.cancelled || !cancelled.error.isEmpty()) {
        std::fprintf(stderr, "expected the query to report cancellation\n");
        return 1;
    }
    std::printf("cancellation is reported\n");

    std::printf("SPIKE OK\n");
    return 0;
}
