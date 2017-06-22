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

#ifndef CONFIG_H
#define CONFIG_H

#include "ui_config.h"
#include <KCModule>
#include <KConfigGroup>


struct Config {
    constexpr static const char *showActions = "showAdditionalActions";
    constexpr static const char *showFileContentAction = "showFullFileContentAction";
    struct Group {
        constexpr static const char *Actions = "AdditionalActions";
    };
};


struct PassAction {
    QString name, icon, regex;

    // Using QDataStream doesn't work with KConfig (at least I was not able to get it work)
    // So I'm just implementing my own serialization function for this struct
    QString toString()
    {
        return QString("PassAction {\"%1\", \"%2\", \"%3\"}").arg(name).arg(icon).arg(regex);
    }

    static PassAction fromString(const QString &str)
    {
        QRegularExpression re("PassAction {\"(.+?)\", \"(.+?)\", \"(.+?)\"}");
        auto match = re.match(str);
        return PassAction {match.captured(1), match.captured(2), match.captured(3)};
    }
}; Q_DECLARE_METATYPE(PassAction)


class PassConfigForm : public QWidget, public Ui::PassConfigUi
{
    Q_OBJECT

public:
    explicit PassConfigForm(QWidget* parent);

    void addPassAction(const QString &, const QString &, const QString &, bool isNew = true);
    void clearPassActions();
    void clearInputs();

    QVector<PassAction> passActions();

signals:
    void passActionRemoved();
    void passActionAdded();
};


class PassConfig : public KCModule
{
    Q_OBJECT

public:
    explicit PassConfig(QWidget* parent = 0, const QVariantList& args = QVariantList());

public Q_SLOTS:
    void save();
    void load();
    void defaults();

private:
    PassConfigForm *ui;
};


#endif
