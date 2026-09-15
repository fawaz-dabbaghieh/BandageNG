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

#include "indexedgraphdialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

IndexedGraphDialog::IndexedGraphDialog(QWidget *parent, const QString &summaryText)
        : QDialog(parent) {
    setWindowTitle("Indexed graph detected");

    auto *summaryLabel = new QLabel(summaryText, this);
    summaryLabel->setWordWrap(true);

    m_nodeLineEdit = new QLineEdit(this);
    m_nodeLineEdit->setPlaceholderText("Node ID");

    m_maxNodesSpinBox = new QSpinBox(this);
    m_maxNodesSpinBox->setRange(1, 1000000);
    m_maxNodesSpinBox->setValue(200);

    auto *form = new QFormLayout;
    form->addRow("Seed node ID:", m_nodeLineEdit);
    form->addRow("Max nodes:", m_maxNodesSpinBox);

    auto *loadSubgraphButton = new QPushButton("Load subgraph", this);
    loadSubgraphButton->setDefault(true);
    auto *loadFullGraphButton = new QPushButton("Load full graph", this);
    auto *cancelButton = new QPushButton("Cancel", this);

    auto *buttonBox = new QDialogButtonBox(this);
    buttonBox->addButton(loadSubgraphButton, QDialogButtonBox::AcceptRole);
    buttonBox->addButton(loadFullGraphButton, QDialogButtonBox::AcceptRole);
    buttonBox->addButton(cancelButton, QDialogButtonBox::RejectRole);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(summaryLabel);
    layout->addLayout(form);
    layout->addWidget(buttonBox);

    connect(loadSubgraphButton, &QPushButton::clicked, this, &IndexedGraphDialog::loadSubgraphClicked);
    connect(loadFullGraphButton, &QPushButton::clicked, this, &IndexedGraphDialog::loadFullGraphClicked);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void IndexedGraphDialog::loadSubgraphClicked() {
    if (m_nodeLineEdit->text().simplified().isEmpty()) {
        QMessageBox::warning(this, "No node ID given", "Please enter a node ID to query, "
                                                        "or click \"Load full graph\" instead.");
        return;
    }
    m_loadFullGraph = false;
    accept();
}

void IndexedGraphDialog::loadFullGraphClicked() {
    m_loadFullGraph = true;
    accept();
}

QString IndexedGraphDialog::seedNodeId() const {
    return m_nodeLineEdit->text().simplified();
}

unsigned IndexedGraphDialog::maxNodes() const {
    return static_cast<unsigned>(m_maxNodesSpinBox->value());
}
