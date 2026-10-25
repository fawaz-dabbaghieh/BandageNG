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

#include "graph/indexedgraphsession.h"

#include <QDialog>
#include <memory>
#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTabWidget;

// Query window for a gfaidx-indexed graph. One instance lives as long as its index,
// so the previous query is still filled in when it is opened again.
class IndexedGraphDialog : public QDialog {
    Q_OBJECT

  public:
    IndexedGraphDialog(QWidget *parent, std::shared_ptr<const graph::IndexedGraphSession> session);

    [[nodiscard]] bool loadFullGraphRequested() const { return m_loadFullGraph; }
    [[nodiscard]] const graph::IndexQuery &query() const { return m_query; }

  private slots:
    void runQueryClicked();
    void loadFullGraphClicked();
    void updateRegionRange();

  private:
    QWidget *createSubgraphTab();
    QWidget *createRegionTab();
    bool readSubgraphQuery(QString &error);
    bool readRegionQuery(QString &error);
    [[nodiscard]] const graph::RegionTarget *selectedTarget() const;

    std::shared_ptr<const graph::IndexedGraphSession> m_session;
    std::vector<graph::RegionTarget> m_targets;

    QTabWidget *m_tabs = nullptr;
    QLineEdit *m_seedsLineEdit = nullptr;
    QSpinBox *m_maxNodesSpinBox = nullptr;
    QCheckBox *m_subgraphCoordinatesCheckBox = nullptr;
    QComboBox *m_targetComboBox = nullptr;
    QLabel *m_targetRangeLabel = nullptr;
    QLineEdit *m_startLineEdit = nullptr;
    QLineEdit *m_endLineEdit = nullptr;
    QCheckBox *m_regionCoordinatesCheckBox = nullptr;
    QSpinBox *m_haplotypeGapSpinBox = nullptr;

    graph::IndexQuery m_query;
    bool m_loadFullGraph = false;
};
