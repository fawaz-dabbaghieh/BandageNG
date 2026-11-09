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

#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
    QString formatNumber(std::uint64_t value) {
        return QLocale().toString(qulonglong(value));
    }

    bool parsePosition(const QLineEdit *lineEdit, std::uint64_t &value) {
        bool ok = false;
        value = QString(lineEdit->text()).remove(',').trimmed().toULongLong(&ok);
        return ok;
    }

    QLabel *descriptionLabel(const QString &text) {
        auto *label = new QLabel(text);
        label->setWordWrap(true);
        return label;
    }
}

IndexedGraphDialog::IndexedGraphDialog(QWidget *parent,
                                       std::shared_ptr<const graph::IndexedGraphSession> session)
        : QDialog(parent), m_session(std::move(session)), m_targets(m_session->regionTargets()) {
    setWindowTitle("Query indexed graph");

    auto *fileLabel = new QLabel("<b>" + QFileInfo(m_session->fileName()).fileName().toHtmlEscaped() + "</b>", this);

    // Size of the complete graph; edges and total length are only known from a gfaidx 2.3+ .params file.
    const auto &stats = m_session->graphStats();
    auto format = [](std::optional<std::uint64_t> count) {
        return count ? formatNumber(*count) : QString("unknown");
    };
    auto *statsForm = new QFormLayout;
    statsForm->addRow("Nodes:", new QLabel(format(stats.nodes), this));
    statsForm->addRow("Edges:", new QLabel(format(stats.edges), this));
    statsForm->addRow("Paths / Walks:",
                      new QLabel(stats.paths ? format(stats.paths) + " / " + format(stats.walks) : "unknown", this));
    statsForm->addRow("Total length:", new QLabel(format(stats.totalLength), this));
    statsForm->addRow("Communities:", new QLabel(formatNumber(m_session->communityCount()), this));

    QString summary = "Load part of it with a query, or load the complete graph.";
    if (!stats.edges || !stats.totalLength)
        summary.prepend("Edges and total length are read from the .params file that gfaidx 2.3 and later "
                        "write next to the index; re-index the graph to see them. ");
    auto *summaryLabel = descriptionLabel(summary);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createSubgraphTab(), "Subgraph");
    m_tabs->addTab(createRegionTab(), "Region");
    if (m_targets.empty()) {
        m_tabs->setTabEnabled(1, false);
        m_tabs->setTabToolTip(1, m_session->hasPathIndex()
                                 ? "This graph has no P or W lines to query a region of."
                                 : "This index has no path index (.pdx), so region queries are not available.");
    }

    auto *runQueryButton = new QPushButton("Run query", this);
    runQueryButton->setDefault(true);
    auto *loadFullGraphButton = new QPushButton("Load full graph", this);
    loadFullGraphButton->setToolTip("Load the complete graph without using the index.");
    auto *cancelButton = new QPushButton("Cancel", this);

    auto *buttonBox = new QDialogButtonBox(this);
    buttonBox->addButton(runQueryButton, QDialogButtonBox::AcceptRole);
    buttonBox->addButton(loadFullGraphButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(cancelButton, QDialogButtonBox::RejectRole);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(fileLabel);
    layout->addLayout(statsForm);
    layout->addWidget(summaryLabel);
    layout->addWidget(m_tabs);
    layout->addWidget(buttonBox);

    connect(runQueryButton, &QPushButton::clicked, this, &IndexedGraphDialog::runQueryClicked);
    connect(loadFullGraphButton, &QPushButton::clicked, this, &IndexedGraphDialog::loadFullGraphClicked);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

QWidget *IndexedGraphDialog::createSubgraphTab() {
    auto *tab = new QWidget;

    m_seedsLineEdit = new QLineEdit(tab);
    m_seedsLineEdit->setPlaceholderText("e.g. 12, or 12, 40, 57");

    m_maxNodesSpinBox = new QSpinBox(tab);
    m_maxNodesSpinBox->setRange(1, 10'000'000);
    m_maxNodesSpinBox->setValue(200);
    m_maxNodesSpinBox->setGroupSeparatorShown(true);

    m_subgraphCoordinatesCheckBox = new QCheckBox("With coordinates", tab);
    m_subgraphCoordinatesCheckBox->setToolTip("Keep coordinates on the P/W lines of the result.");
    m_subgraphCoordinatesCheckBox->setEnabled(m_session->hasPathIndex());

    auto *form = new QFormLayout;
    form->addRow("Start node(s):", m_seedsLineEdit);
    form->addRow("Max nodes:", m_maxNodesSpinBox);
    form->addRow("", m_subgraphCoordinatesCheckBox);

    auto *layout = new QVBoxLayout(tab);
    layout->addWidget(descriptionLabel("Loads the neighbourhood around one or more start nodes "
                                       "(breadth-first, up to the maximum number of nodes)."));
    layout->addLayout(form);
    layout->addStretch();
    return tab;
}

QWidget *IndexedGraphDialog::createRegionTab() {
    auto *tab = new QWidget;

    m_targetComboBox = new QComboBox(tab);
    m_targetComboBox->setEditable(true);
    m_targetComboBox->setInsertPolicy(QComboBox::NoInsert);
    m_targetComboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_targetComboBox->setMinimumContentsLength(40);
    for (const auto &target : m_targets)
        m_targetComboBox->addItem(target.label);
    m_targetComboBox->completer()->setFilterMode(Qt::MatchContains);
    m_targetComboBox->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    m_targetComboBox->completer()->setCompletionMode(QCompleter::PopupCompletion);

    m_targetRangeLabel = new QLabel(tab);
    m_targetRangeLabel->setWordWrap(true);

    auto *positionValidator = new QRegularExpressionValidator(QRegularExpression("[0-9,]*"), tab);
    m_startLineEdit = new QLineEdit(tab);
    m_startLineEdit->setValidator(positionValidator);
    m_endLineEdit = new QLineEdit(tab);
    m_endLineEdit->setValidator(positionValidator);

    m_haplotypeGapSpinBox = new QSpinBox(tab);
    m_haplotypeGapSpinBox->setRange(0, 1'000'000'000);
    m_haplotypeGapSpinBox->setValue(10'000);
    m_haplotypeGapSpinBox->setSuffix(" bp");
    m_haplotypeGapSpinBox->setGroupSeparatorShown(true);
    m_haplotypeGapSpinBox->setToolTip("Maximum number of unshared bases that may bridge matches when "
                                      "collecting the other haplotypes.");
    if (!m_session->hasNodeLengths()) {
        m_haplotypeGapSpinBox->setEnabled(false);
        m_haplotypeGapSpinBox->setToolTip("Needs the node length index (.lnx), which this index does not have.");
    }

    m_regionCoordinatesCheckBox = new QCheckBox("With coordinates", tab);
    m_regionCoordinatesCheckBox->setToolTip("Keep coordinates on the P/W lines of the result.");

    auto *form = new QFormLayout;
    form->addRow("Path or walk:", m_targetComboBox);
    form->addRow("", m_targetRangeLabel);
    form->addRow("Start:", m_startLineEdit);
    form->addRow("End:", m_endLineEdit);
    form->addRow("Haplotype gap:", m_haplotypeGapSpinBox);
    form->addRow("", m_regionCoordinatesCheckBox);

    auto *layout = new QVBoxLayout(tab);
    layout->addWidget(descriptionLabel("Loads an interval of the chosen path or walk together with every "
                                       "haplotype that passes through it. Positions are 0-based and the "
                                       "end is excluded."));
    layout->addLayout(form);
    layout->addStretch();

    connect(m_targetComboBox, &QComboBox::currentIndexChanged, this, &IndexedGraphDialog::updateRegionRange);
    updateRegionRange();
    return tab;
}

void IndexedGraphDialog::updateRegionRange() {
    const auto *target = selectedTarget();
    if (!target) {
        m_targetRangeLabel->clear();
        return;
    }

    m_startLineEdit->setPlaceholderText(formatNumber(target->begin));
    if (target->end) {
        m_targetRangeLabel->setText(QString("Valid range: %1 to %2")
                                            .arg(formatNumber(target->begin), formatNumber(*target->end)));
        m_endLineEdit->setPlaceholderText(formatNumber(*target->end));
    } else {
        m_targetRangeLabel->setText(QString("Valid range: from %1; the length of this path is not known, "
                                            "so the end cannot be checked before querying.")
                                            .arg(formatNumber(target->begin)));
        m_endLineEdit->setPlaceholderText("");
    }
}

const graph::RegionTarget *IndexedGraphDialog::selectedTarget() const {
    const int index = m_targetComboBox->findText(m_targetComboBox->currentText(), Qt::MatchExactly);
    if (index < 0 || index >= int(m_targets.size()))
        return nullptr;
    return &m_targets[index];
}

bool IndexedGraphDialog::readSubgraphQuery(QString &error) {
    QStringList seeds = m_seedsLineEdit->text().split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
    seeds.removeDuplicates();
    if (seeds.isEmpty()) {
        error = "Enter at least one start node.";
        return false;
    }

    QStringList missing;
    for (auto &seed : seeds) {
        if (m_session->nodeExists(seed))
            continue;
        // Bandage names nodes with a strand suffix; the index only knows the segment name.
        if ((seed.endsWith('+') || seed.endsWith('-')) && m_session->nodeExists(seed.chopped(1)))
            seed.chop(1);
        else
            missing << seed;
    }
    if (!missing.isEmpty()) {
        error = "These nodes are not in the index: " + missing.join(", ");
        return false;
    }

    const auto maxNodes = unsigned(m_maxNodesSpinBox->value());
    if (unsigned(seeds.size()) > maxNodes) {
        error = "There are more start nodes than the maximum number of nodes.";
        return false;
    }

    m_query = graph::SubgraphQuery{seeds, maxNodes, m_subgraphCoordinatesCheckBox->isChecked()};
    return true;
}

bool IndexedGraphDialog::readRegionQuery(QString &error) {
    const auto *target = selectedTarget();
    if (!target) {
        error = "Choose a path or walk from the list.";
        return false;
    }

    std::uint64_t start = 0, end = 0;
    if (!parsePosition(m_startLineEdit, start) || !parsePosition(m_endLineEdit, end)) {
        error = "Enter both a start and an end position.";
        return false;
    }
    if (start >= end) {
        error = "The start must be smaller than the end.";
        return false;
    }
    if (start < target->begin || (target->end && end > *target->end)) {
        error = target->end ? QString("The interval must lie within %1 to %2.")
                                      .arg(formatNumber(target->begin), formatNumber(*target->end))
                            : QString("The start must be at least %1.").arg(formatNumber(target->begin));
        return false;
    }

    graph::RegionQuery query{target->reference, target->sequence, start, end,
                             m_regionCoordinatesCheckBox->isChecked(), std::nullopt};
    if (m_haplotypeGapSpinBox->isEnabled())
        query.haplotypeGap = std::uint64_t(m_haplotypeGapSpinBox->value());
    m_query = query;
    return true;
}

void IndexedGraphDialog::runQueryClicked() {
    QString error;
    const bool valid = m_tabs->currentIndex() == 0 ? readSubgraphQuery(error) : readRegionQuery(error);
    if (!valid) {
        QMessageBox::warning(this, "Cannot run query", error);
        return;
    }
    m_loadFullGraph = false;
    accept();
}

void IndexedGraphDialog::loadFullGraphClicked() {
    m_loadFullGraph = true;
    accept();
}
