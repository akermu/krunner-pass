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

#include "pass_config.h"

K_PLUGIN_FACTORY(PassConfigFactory, registerPlugin<PassConfig>("kcm_krunner_pass");)



PassConfigForm::PassConfigForm(QWidget *parent) : QWidget(parent)
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
}

void PassConfigForm::addPassAction(const QString &name, const QString &icon, const QString &regex, bool isNew /* = true */)
{
    // Checks
    for (auto act: this->passActions())
        if (act.name == name)
            return;

    if (name.isEmpty() || icon.isEmpty() || regex.isEmpty())
        return;

    // Widgets
    auto *listWidget = new QWidget(this);
    auto *layoutAction = new QHBoxLayout(listWidget);
    auto *buttonRemoveAction = new QToolButton(listWidget);

    buttonRemoveAction->setIcon(QIcon::fromTheme("remove"));
    layoutAction->setMargin(0);
    layoutAction->addStretch();
    layoutAction->addWidget(buttonRemoveAction);
    listWidget->setLayout(layoutAction);

    // Item
    auto *item = new QListWidgetItem(name + (isNew ? "*":""), this->listSavedActions);
    item->setData(Qt::UserRole, QVariant::fromValue(PassAction {name, icon, regex}));
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
    for(int i = 0; i < this->listSavedActions->count(); ++i) {
        QListWidgetItem* item = this->listSavedActions->item(i);
        passActions << item->data(Qt::UserRole).value<PassAction>();
    }
    return passActions;
}

void PassConfigForm::clearPassActions()
{
    for(int i = 0; i < this->listSavedActions->count(); ++i) {
        QListWidgetItem* item = this->listSavedActions->item(i);
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


PassConfig::PassConfig(QWidget *parent, const QVariantList &args) :
        KCModule(parent, args)
{
    this->ui = new PassConfigForm(this);
    QGridLayout* layout = new QGridLayout(this);
    layout->addWidget(ui, 0, 0);

    load();

    connect(this->ui,SIGNAL(passActionAdded()),this,SLOT(changed()));
    connect(this->ui,SIGNAL(passActionRemoved()),this,SLOT(changed()));
    connect(this->ui->checkAdditionalActions,SIGNAL(stateChanged(int)),this,SLOT(changed()));
    connect(this->ui->checkShowFileContentAction,SIGNAL(stateChanged(int)),this,SLOT(changed()));
    connect(this->ui->listSavedActions,SIGNAL(itemSelectionChanged()), this, SLOT(changed()));
}

void PassConfig::load()
{
    KCModule::load();

    KSharedConfig::Ptr cfg = KSharedConfig::openConfig(QStringLiteral("krunnerrc"));
    KConfigGroup passCfg = cfg->group("Runners");
    passCfg = KConfigGroup(&passCfg, "Pass");


    bool showActions = passCfg.readEntry(Config::showActions, false);
    bool showFileContentAction = passCfg.readEntry(Config::showFileContentAction, false);

    this->ui->checkAdditionalActions->setChecked(showActions);
    this->ui->checkShowFileContentAction->setChecked(showFileContentAction);

    // Load saved actions
    this->ui->clearPassActions();

    auto configActions = passCfg.group(Config::Group::Actions);
    for (int i = 0; i < configActions.keyList().count(); i++) {
        QString passStr = configActions.readEntry(QString::number(i));
        auto passAction = PassAction::fromString(passStr);

        this->ui->addPassAction(passAction.name, passAction.icon, passAction.regex, false);
    }

    emit changed(false);
}

void PassConfig::save()
{
    KCModule::save();

    KSharedConfig::Ptr cfg = KSharedConfig::openConfig(QStringLiteral("krunnerrc"));
    KConfigGroup passCfg = cfg->group("Runners");
    passCfg = KConfigGroup(&passCfg, "Pass");


    auto showActions = this->ui->checkAdditionalActions->isChecked();
    auto showFileContentAction =  this->ui->checkShowFileContentAction->isChecked();

    passCfg.writeEntry(Config::showActions, showActions);
    passCfg.writeEntry(Config::showFileContentAction, showFileContentAction);


    passCfg.deleteGroup(Config::Group::Actions);

    if (showActions) {
        int i = 0;
        for (PassAction act: this->ui->passActions())
            passCfg.group(Config::Group::Actions).writeEntry(QString::number(i++), act.toString());
    }

    emit changed(false);
}

void PassConfig::defaults()
{
    KCModule::defaults();

    ui->checkAdditionalActions->setChecked(false);
    ui->checkShowFileContentAction->setChecked(false);
    ui->clearPassActions();
    ui->clearInputs();

    emit changed(true);
}


#include "pass_config.moc"
