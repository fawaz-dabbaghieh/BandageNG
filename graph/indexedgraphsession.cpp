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

#include <gfaidx/gfaidx.hpp>

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryDir>

#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace graph {

    namespace {
        QString toQString(std::string_view text) {
            return QString::fromUtf8(text.data(), qsizetype(text.size()));
        }

        IndexedGraphStats readGraphStats(const QString &fullFileName, const gfaidx::IndexedGraph &index) {
            IndexedGraphStats stats;
            stats.nodes = index.node_count();
            if (index.capabilities().paths) {
                std::uint64_t paths = 0, walks = 0;
                for (const auto &path : index.paths())
                    ++(path.record_type == 'W' ? walks : paths);
                stats.paths = paths;
                stats.walks = walks;
            }

            // The "graph" section of the .params JSON, e.g. {"segments": 2782249, "links": 3801658, ...}
            QFile file(fullFileName + ".params");
            if (!file.open(QIODevice::ReadOnly))
                return stats;
            const QJsonObject graph = QJsonDocument::fromJson(file.readAll()).object().value("graph").toObject();
            auto count = [&graph](const char *key) -> std::optional<std::uint64_t> {
                const QJsonValue value = graph.value(QLatin1String(key));
                if (!value.isDouble() || value.toInteger(-1) < 0)
                    return std::nullopt;
                return std::uint64_t(value.toInteger());
            };

            // A summary left over from indexing a different graph to the same name is ignored.
            if (count("segments") != stats.nodes)
                return stats;
            stats.edges = count("links");
            stats.totalLength = count("total_sequence_bp");
            if (auto paths = count("paths"), walks = count("walks"); paths && walks) {
                stats.paths = paths;
                stats.walks = walks;
            }
            return stats;
        }
    }

    QString describeQuery(const IndexQuery &query) {
        if (const auto *subgraph = std::get_if<SubgraphQuery>(&query))
            return QString("subgraph around %1 (max %2 nodes)")
                    .arg(subgraph->seeds.join(", "))
                    .arg(subgraph->maxNodes);

        const auto &region = std::get<RegionQuery>(query);
        QString target = region.reference.isEmpty() ? region.sequence
                                                    : region.reference + " " + region.sequence;
        return QString("region %1:%2-%3").arg(target).arg(region.begin).arg(region.end);
    }

    bool IndexedGraphSession::isIndexed(const QString &fullFileName) {
        return QFileInfo::exists(fullFileName + ".idx") &&
               QFileInfo::exists(fullFileName + ".ndx");
    }

    class IndexedGraphSession::Impl {
      public:
        explicit Impl(const QString &fullFileName)
                : fileName(fullFileName), index(fullFileName.toStdString()),
                  stats(readGraphStats(fullFileName, index)) {}

        QString fileName;
        gfaidx::IndexedGraph index;
        IndexedGraphStats stats;
    };

    IndexedGraphSession::IndexedGraphSession(const QString &fullFileName)
            : impl_(std::make_unique<Impl>(fullFileName)) {}

    IndexedGraphSession::~IndexedGraphSession() = default;

    const QString &IndexedGraphSession::fileName() const {
        return impl_->fileName;
    }

    std::uint64_t IndexedGraphSession::nodeCount() const {
        return impl_->index.node_count();
    }

    std::uint32_t IndexedGraphSession::communityCount() const {
        return impl_->index.community_count();
    }

    std::size_t IndexedGraphSession::pathCount() const {
        return impl_->index.paths().size();
    }

    const IndexedGraphStats &IndexedGraphSession::graphStats() const {
        return impl_->stats;
    }

    bool IndexedGraphSession::hasPathIndex() const {
        return impl_->index.capabilities().paths;
    }

    bool IndexedGraphSession::hasNodeLengths() const {
        return impl_->index.capabilities().node_lengths;
    }

    bool IndexedGraphSession::nodeExists(const QString &nodeId) const {
        return impl_->index.node_exists(nodeId.toStdString());
    }

    std::vector<RegionTarget> IndexedGraphSession::regionTargets() const {
        std::vector<RegionTarget> targets;
        if (!hasPathIndex())
            return targets;

        // Same convention gfaidx uses for coordinate-bearing P names, e.g. "HG002#1#chr1:100-200"
        static const QRegularExpression coordinateSuffix("^(.+):(\\d+)-(\\d+)$");

        for (const auto &path : impl_->index.paths()) {
            RegionTarget target;
            if (path.record_type == 'W') {
                target.reference = toQString(path.sample);
                target.sequence = toQString(path.sequence_name);
                target.label = QString("W  %1#%2#%3").arg(target.reference).arg(path.haplotype).arg(target.sequence);
                if (path.sequence_start >= 0 && path.sequence_end > path.sequence_start) {
                    target.begin = std::uint64_t(path.sequence_start);
                    target.end = std::uint64_t(path.sequence_end);
                    target.label += QString(":%1-%2").arg(target.begin).arg(*target.end);
                }
            } else {
                const QString name = toQString(path.name);
                target.sequence = name;
                target.label = "P  " + name;
                if (auto match = coordinateSuffix.match(name); match.hasMatch()) {
                    const auto begin = match.captured(2).toULongLong(), end = match.captured(3).toULongLong();
                    if (begin < end) {
                        target.sequence = match.captured(1);
                        target.begin = begin;
                        target.end = end;
                    }
                }
            }
            if (!target.end)
                target.label += "  (length unknown)";
            targets.push_back(std::move(target));
        }
        return targets;
    }

    IndexQueryResult IndexedGraphSession::runQuery(const IndexQuery &query,
                                                   const std::function<void(const QString &)> &progress,
                                                   const std::function<bool()> &keepGoing) const {
        IndexQueryResult result;

        gfaidx::QueryCallbacks callbacks;
        callbacks.warning = [&](std::string_view message) {
            result.warnings << toQString(message);
            if (progress)
                progress("Warning: " + toQString(message));
        };
        if (progress)
            callbacks.progress = [&](std::string_view message) { progress(toQString(message)); };
        if (keepGoing)
            callbacks.keep_going = keepGoing;

        try {
            auto workDir = std::make_shared<QTemporaryDir>();
            if (!workDir->isValid()) {
                result.error = "Could not create a temporary directory for the query result.";
                return result;
            }
            result.gfaPath = workDir->filePath("query.gfa");
            std::ofstream out(result.gfaPath.toStdString(), std::ios::binary);

            // The result is streamed straight to the file. gfaidx's in-memory Graph (get_subgraph/get_region)
            // takes minutes and several GB for results with hundreds of thousands of path/walk records.
            size_t segmentCount = 0;
            const gfaidx::GfaLineVisitor writeLine = [&](std::string_view line) {
                if (!line.empty() && line[0] == 'S')
                    ++segmentCount;
                out.write(line.data(), static_cast<std::streamsize>(line.size()));
                out.put('\n');
                return bool(out);
            };

            if (const auto *subgraph = std::get_if<SubgraphQuery>(&query)) {
                std::vector<std::string> seeds;
                for (const auto &seed : subgraph->seeds)
                    seeds.push_back(seed.toStdString());

                gfaidx::ExtractionOptions options;
                options.max_nodes = subgraph->maxNodes;
                options.include_paths = true;
                options.include_coordinates = subgraph->withCoordinates;
                impl_->index.stream_subgraph(seeds, writeLine, options, callbacks);
            } else {
                const auto &region = std::get<RegionQuery>(query);
                gfaidx::RegionOptions options;
                options.mode = gfaidx::RegionMode::all_haplotypes;
                options.include_paths = true;
                options.include_coordinates = region.withCoordinates;
                options.haplotype_gap = region.haplotypeGap;
                impl_->index.stream_region(region.reference.toStdString(), region.sequence.toStdString(),
                                           region.begin, region.end, writeLine, options, callbacks);
            }

            out.close();
            if (!out) {
                result.error = "Could not write the query result to " + result.gfaPath + ".";
                result.gfaPath.clear();
                return result;
            }
            if (segmentCount == 0) {
                result.error = "The query returned an empty graph.";
                result.gfaPath.clear();
                return result;
            }
            result.workDir = std::move(workDir);
        } catch (const std::exception &e) {
            if (keepGoing && !keepGoing())
                result.cancelled = true;
            else
                result.error = QString::fromUtf8(e.what());
            result.gfaPath.clear();
        }

        return result;
    }

}  // namespace graph
