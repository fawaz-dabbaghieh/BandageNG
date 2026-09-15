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

#pragma once

#include "llvm/Support/Error.h"

#include <QString>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class AssemblyGraph;

namespace graph {

    // Wraps a gfaidx-indexed GFA bundle, giving cheap access to summary
    // statistics and bounded subgraph queries without a full parse of the
    // underlying graph. Only compiled when gfaidx is available
    // (BANDAGE_HAVE_GFAIDX); callers must guard use of this class the same way.
    class IndexedGraphSession {
      public:
        // True if fullFileName has gfaidx's sidecar index files next to it,
        // using gfaidx's own naming convention (<file>.idx / <file>.ndx).
        static bool isIndexed(const QString &fullFileName);

        explicit IndexedGraphSession(const QString &fullFileName);
        ~IndexedGraphSession();

        IndexedGraphSession(IndexedGraphSession &&) noexcept;
        IndexedGraphSession &operator=(IndexedGraphSession &&) noexcept;
        IndexedGraphSession(const IndexedGraphSession &) = delete;
        IndexedGraphSession &operator=(const IndexedGraphSession &) = delete;

        // Whole-index summary, available without running any query.
        [[nodiscard]] std::uint64_t nodeCount() const;
        [[nodiscard]] std::uint32_t communityCount() const;
        [[nodiscard]] std::size_t pathCount() const;

        // Runs a BFS subgraph query seeded at seedNodes (bounded by maxNodes),
        // writes the result out as a temporary GFA file, and loads that into
        // graph through the ordinary GFA loading path - so this gets the same
        // node/edge/path/tag handling as a normal load, for free.
        llvm::Error loadSubgraphByNodes(AssemblyGraph &graph,
                                        const std::vector<QString> &seedNodes,
                                        unsigned maxNodes);

      private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

}  // namespace graph
