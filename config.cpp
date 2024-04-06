/******************************************************************************
 *  Copyright (C) 2017 by Lukas Fürmetz <fuermetz@mailbox.org>                *
 *                                                                            *
 *  This library is free software; you can redistribute it and/or modify      *
 *  it under the terms of the GNU General Public License as published         *
 *  by the Free Software Foundation; either version 3 of the License or (at   *
 *  your option) any later version.                                           *
 *                                                                            *
 *  This library is distributed in the hope that it will be useful,           *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
 *  Library General Public License for more details.                          *
 *                                                                            *
 *  You should have received a copy of the GNU General Public License         *
 *  along with this library; see the file LICENSE.                            *
 *  If not, see <http://www.gnu.org/licenses/>.                               *
 *****************************************************************************/

#include <KSharedConfig>
#include <KPluginFactory>
#include <krunner/abstractrunner.h>
#include <QToolButton>
#include <QtCore/QDir>

#include "kcmutils_version.h"
#include "config.h"

K_PLUGIN_FACTORY_WITH_JSON(PassConfigFactory, "pass.json", registerPlugin<PassConfig>();)

PassConfigForm::PassConfigForm(QWidget *parent)
        : QWidget(parent)
{
    setupUi(this);
    this->listSavedActions->setDragEnabled(true);
    this->listSavedActions->setDragDropMode(QAbstractItemView::InternalMove);
    this->listSavedActions->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(this->checkAdditionalActions, &QCheckBox::stateChanged, [this](int state) {
        Q_UNUSED(state)

        bool isChecked = this->checkAdditionalActions->isChecked();
        this->boxNewAction->setEnabled(isChecked);
        this->boxSavedActions->setEnabled(isChecked);
        this->checkShowFileContentAction->setEnabled(isChecked);
    });

    connect(this->buttonAddAction, &QPushButton::clicked, [this]() {
        this->addPassAction(this->lineName->text(), this->lineIcon->text(), this->lineRegEx->text());
    });

    // Disable add button if the necessary field are not filled out
    connect(this->lineIcon, &QLineEdit::textChanged, this, &PassConfigForm::validateAddButton);
    connect(this->lineName, &QLineEdit::textChanged, this, &PassConfigForm::validateAddButton);
    connect(this->lineRegEx, &QLineEdit::textChanged, this, &PassConfigForm::validateAddButton);
    validateAddButton();
}

void
PassConfigForm::addPassAction(const QString &name, const QString &icon, const QString &regex, bool isNew /* = true */)
{
    // Checks
    for (const auto &act: this->passActions())
        if (act.name == name)
            return;

    // Widgets
    auto *listWidget = new QWidget(this);
    auto *layoutAction = new QHBoxLayout(listWidget);
    auto *buttonRemoveAction = new QToolButton(listWidget);

    buttonRemoveAction->setIcon(QIcon::fromTheme("delete"));
    layoutAction->setMargin(0);
    layoutAction->addStretch();
    layoutAction->addWidget(buttonRemoveAction);
    listWidget->setLayout(layoutAction);

    // Item
    auto *item = new QListWidgetItem(name + (isNew ? "*" : ""), this->listSavedActions);
    item->setData(Qt::UserRole, QVariant::fromValue(PassAction{name, icon, regex}));
    this->listSavedActions->setItemWidget(item, listWidget);

    this->clearInputs();
    emit passActionAdded();

    // Remove
    connect(buttonRemoveAction, &QToolButton::clicked, [=](bool clicked) {
        Q_UNUSED(clicked)

        delete this->listSavedActions->takeItem(this->listSavedActions->row(item));
        delete listWidget;

        emit passActionRemoved();
    });
}

QVector<PassAction> PassConfigForm::passActions()
{
    QVector<PassAction> passActions;
    for (int i = 0; i < listSavedActions->count(); ++i) {
        QListWidgetItem *item = this->listSavedActions->item(i);
        passActions << item->data(Qt::UserRole).value<PassAction>();
    }
    return passActions;
}

void PassConfigForm::clearPassActions()
{
    for (int i = 0; i < listSavedActions->count(); ++i) {
        QListWidgetItem *item = this->listSavedActions->item(i);
        delete this->listSavedActions->itemWidget(item);
    }

    this->listSavedActions->clear();
}

void PassConfigForm::clearInputs()
{
    this->lineIcon->clear();
    this->lineName->clear();
    this->lineRegEx->clear();
}

void PassConfigForm::validateAddButton()
{
    this->buttonAddAction->setDisabled(this->lineIcon->text().isEmpty() ||
            this->lineName->text().isEmpty() ||
            this->lineRegEx->text().isEmpty());
}

PassConfig::PassConfig(QWidget *parent, const QVariantList &args)
        :
        KCModule(parent, args)
{
    this->ui = new PassConfigForm(this);
    QGridLayout *layout = new QGridLayout(this);
    layout->addWidget(ui, 0, 0);
#if KCMUTILS_VERSION >= QT_VERSION_CHECK(5, 64, 0)
    const auto changedSlotPointer = &PassConfig::markAsChanged;
#else
    const auto changedSlotPointer = static_cast<void (PassConfig::*)()>(&PassConfig::changed);
#endif
    connect(this->ui, &PassConfigForm::passActionAdded, this, changedSlotPointer);
    connect(this->ui, &PassConfigForm::passActionRemoved, this, changedSlotPointer);
    connect(this->ui->checkAdditionalActions, &QCheckBox::stateChanged, this, changedSlotPointer);
    connect(this->ui->checkShowFileContentAction, &QCheckBox::stateChanged, this, changedSlotPointer);
    connect(this->ui->listSavedActions, &QListWidget::itemSelectionChanged, this, changedSlotPointer);
}

void PassConfig::load()
{
    KCModule::load();

    KSharedConfig::Ptr cfg = KSharedConfig::openConfig(QStringLiteral("krunnerrc"));
    KConfigGroup passCfg = cfg->group("Runners").group("Pass");

    bool showActions = passCfg.readEntry(Config::showActions, false);
    bool showFileContentAction = passCfg.readEntry(Config::showFileContentAction, false);

    this->ui->checkAdditionalActions->setChecked(showActions);
    this->ui->checkShowFileContentAction->setChecked(showFileContentAction);

    // Load saved actions
    this->ui->clearPassActions();

    const auto actionGroup = passCfg.group(Config::Group::Actions);
    for (const auto &name: actionGroup.groupList()) {
        auto group = actionGroup.group(name);
        auto passAction = PassAction::fromConfig(group);
        this->ui->addPassAction(passAction.name, passAction.icon, passAction.regex, false);
    }
}

void PassConfig::save()
{
    KCModule::save();

    KSharedConfig::Ptr cfg = KSharedConfig::openConfig(QStringLiteral("krunnerrc"));
    KConfigGroup passCfg = cfg->group("Runners").group("Pass");

    auto showActions = this->ui->checkAdditionalActions->isChecked();
    auto showFileContentAction = this->ui->checkShowFileContentAction->isChecked();

    passCfg.writeEntry(Config::showActions, showActions);
    passCfg.writeEntry(Config::showFileContentAction, showFileContentAction);

    passCfg.deleteGroup(Config::Group::Actions);

    if (showActions) {
        int i = 0;
        for (PassAction &act: this->ui->passActions()) {
            auto group = passCfg.group(Config::Group::Actions).group(QString::number(i++));
            act.writeToConfig(group);
        }
    }
}

void PassConfig::defaults()
{
    KCModule::defaults();

    ui->checkAdditionalActions->setChecked(false);
    ui->checkShowFileContentAction->setChecked(false);
    ui->clearPassActions();
    ui->clearInputs();

#if KCMUTILS_VERSION >= QT_VERSION_CHECK(5, 64, 0)
    markAsChanged();
#else
    emit changed(true);
#endif
}


#include "config.moc"
