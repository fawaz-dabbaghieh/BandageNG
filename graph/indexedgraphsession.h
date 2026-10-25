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

#include <QString>
#include <QStringList>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

class QTemporaryDir;

namespace graph {

    // A P or W record that a region query can be anchored on.
    struct RegionTarget {
        QString label;
        QString reference;  // W sample; empty for P records
        QString sequence;   // W sequence name, or the P name without a trailing ":start-end"
        std::uint64_t begin = 0;
        std::optional<std::uint64_t> end;  // unknown for P records without coordinates in their name
    };

    struct SubgraphQuery {
        QStringList seeds;
        unsigned maxNodes = 200;
        bool withCoordinates = false;
    };

    struct RegionQuery {
        QString reference;
        QString sequence;
        std::uint64_t begin = 0;
        std::uint64_t end = 0;
        bool withCoordinates = false;
        std::optional<std::uint64_t> haplotypeGap;
    };

    using IndexQuery = std::variant<SubgraphQuery, RegionQuery>;

    QString describeQuery(const IndexQuery &query);

    struct IndexQueryResult {
        std::shared_ptr<QTemporaryDir> workDir;  // owns gfaPath; deleted with the last copy
        QString gfaPath;
        QString error;
        QStringList warnings;
        bool cancelled = false;
    };

    // An open gfaidx index: summary statistics plus node and region queries, without
    // parsing the whole graph. Only compiled when gfaidx is available (BANDAGE_HAVE_GFAIDX).
    class IndexedGraphSession {
      public:
        // True if fullFileName has gfaidx's sidecar index files (<file>.idx / <file>.ndx) next to it.
        static bool isIndexed(const QString &fullFileName);

        // Throws std::exception if the index cannot be opened.
        explicit IndexedGraphSession(const QString &fullFileName);
        ~IndexedGraphSession();

        IndexedGraphSession(const IndexedGraphSession &) = delete;
        IndexedGraphSession &operator=(const IndexedGraphSession &) = delete;

        [[nodiscard]] const QString &fileName() const;
        [[nodiscard]] std::uint64_t nodeCount() const;
        [[nodiscard]] std::uint32_t communityCount() const;
        [[nodiscard]] std::size_t pathCount() const;
        [[nodiscard]] bool hasPathIndex() const;
        [[nodiscard]] bool hasNodeLengths() const;
        [[nodiscard]] bool nodeExists(const QString &nodeId) const;
        [[nodiscard]] std::vector<RegionTarget> regionTargets() const;

        // Runs the query and writes the extracted subgraph as GFA. Safe to call off the GUI thread;
        // returning false from keepGoing cancels the query.
        [[nodiscard]] IndexQueryResult runQuery(const IndexQuery &query,
                                                const std::function<void(const QString &)> &progress = {},
                                                const std::function<bool()> &keepGoing = {}) const;

      private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

}  // namespace graph
