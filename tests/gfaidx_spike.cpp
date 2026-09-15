// Manual smoke test for the gfaidx integration (see docs/subgraph-ui.md).
// Not wired into ctest - run by hand to check that IndexedGraphSession
// (graph/indexedgraphsession.h) correctly detects an indexed graph, reports
// its summary, and loads a node-seeded subgraph query end to end.

#include "graph/assemblygraph.h"
#include "graph/indexedgraphsession.h"
#include "program/globals.h"
#include "program/memory.h"
#include "program/settings.h"

#include <QApplication>
#include <QTemporaryDir>

#include <cstdio>
#include <cstdlib>
#include <string>

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
                                  "\" --max_chunk_nodes 0 --min_chunk_nodes 0";
    if (std::system(indexCmd.c_str()) != 0) {
        std::fprintf(stderr, "index_gfa failed\n");
        return 1;
    }

    if (!graph::IndexedGraphSession::isIndexed(indexedGraphPath)) {
        std::fprintf(stderr, "IndexedGraphSession::isIndexed() failed to detect a freshly built index\n");
        return 1;
    }

    graph::IndexedGraphSession session(indexedGraphPath);
    std::printf("index summary: node_count=%llu community_count=%u path_count=%zu\n",
                static_cast<unsigned long long>(session.nodeCount()), session.communityCount(),
                session.pathCount());

    AssemblyGraph graph;
    if (auto err = session.loadSubgraphByNodes(graph, {seedNode}, 10)) {
        std::fprintf(stderr, "loadSubgraphByNodes failed: %s\n",
                     llvm::toString(std::move(err)).c_str());
        return 1;
    }

    std::printf("BandageNG loaded: nodes=%zu edges=%zu paths=%zu walks=%zu\n",
                graph.m_deBruijnGraphNodes.size(), graph.m_deBruijnGraphEdges.size(),
                graph.m_deBruijnGraphPaths.size(), graph.m_deBruijnGraphWalks.size());

    if (graph.m_deBruijnGraphNodes.empty()) {
        std::fprintf(stderr, "WARNING: loaded subgraph has no nodes\n");
        return 1;
    }

    std::printf("SPIKE OK\n");
    return 0;
}
