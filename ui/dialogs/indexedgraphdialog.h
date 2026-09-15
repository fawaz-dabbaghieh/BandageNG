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

#include <QDialog>
#include <QString>

class QLineEdit;
class QSpinBox;

// Shown when a file chosen via "Load graph" turns out to be a gfaidx-indexed
// GFA: lets the user either load just the neighborhood of a seed node, or
// fall back to loading the complete graph as if it weren't indexed.
class IndexedGraphDialog : public QDialog {
    Q_OBJECT

  public:
    IndexedGraphDialog(QWidget *parent, const QString &summaryText);

    // Valid only when accepted with loadFullGraphRequested() == false.
    [[nodiscard]] QString seedNodeId() const;
    [[nodiscard]] unsigned maxNodes() const;
    [[nodiscard]] bool loadFullGraphRequested() const { return m_loadFullGraph; }

  private slots:
    void loadSubgraphClicked();
    void loadFullGraphClicked();

  private:
    QLineEdit *m_nodeLineEdit;
    QSpinBox *m_maxNodesSpinBox;
    bool m_loadFullGraph = false;
};
