// Copyright 2026 Fawaz Dabbaghie

// This file is part of Bandage-NG

// Bandage-NG is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Bandage-NG is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with Bandage.  If not, see <http://www.gnu.org/licenses/>.

#include "indexedgraphsession.h"

#include "graph/assemblygraph.h"

#include <gfaidx/gfaidx.hpp>

#include <QFileInfo>
#include <QTemporaryDir>

#include <stdexcept>

namespace graph {

    bool IndexedGraphSession::isIndexed(const QString &fullFileName) {
        return QFileInfo::exists(fullFileName + ".idx") &&
               QFileInfo::exists(fullFileName + ".ndx");
    }

    class IndexedGraphSession::Impl {
      public:
        explicit Impl(const QString &fullFileName)
                : index(fullFileName.toStdString()) {}

        gfaidx::IndexedGraph index;
    };

    IndexedGraphSession::IndexedGraphSession(const QString &fullFileName)
            : impl_(std::make_unique<Impl>(fullFileName)) {}

    IndexedGraphSession::~IndexedGraphSession() = default;
    IndexedGraphSession::IndexedGraphSession(IndexedGraphSession &&) noexcept = default;
    IndexedGraphSession &IndexedGraphSession::operator=(IndexedGraphSession &&) noexcept = default;

    std::uint64_t IndexedGraphSession::nodeCount() const {
        return impl_->index.node_count();
    }

    std::uint32_t IndexedGraphSession::communityCount() const {
        return impl_->index.community_count();
    }

    std::size_t IndexedGraphSession::pathCount() const {
        return impl_->index.paths().size();
    }

    llvm::Error IndexedGraphSession::loadSubgraphByNodes(AssemblyGraph &graph,
                                                          const std::vector<QString> &seedNodes,
                                                          unsigned maxNodes) {
        if (seedNodes.empty())
            return llvm::createStringError("no seed node(s) given for subgraph query");

        std::vector<std::string> seeds;
        seeds.reserve(seedNodes.size());
        for (const auto &seed : seedNodes)
            seeds.push_back(seed.toStdString());

        gfaidx::ExtractionOptions options;
        options.max_nodes = maxNodes;
        options.include_paths = true;

        QTemporaryDir workDir;
        if (!workDir.isValid())
            return llvm::createStringError("failed to create a temporary directory for the subgraph query");
        const QString subgraphPath = workDir.filePath("subgraph.gfa");

        try {
            const auto subgraph = impl_->index.get_subgraph(seeds, options);
            subgraph.write_gfa(subgraphPath.toStdString());
        } catch (const std::exception &e) {
            return llvm::createStringError(std::string("gfaidx subgraph query failed: ") + e.what());
        }

        if (!graph.loadGraphFromFile(subgraphPath))
            return llvm::createStringError("failed to load the subgraph produced by gfaidx");

        return llvm::Error::success();
    }

}  // namespace graph
