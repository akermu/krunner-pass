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

#ifndef PASS_H
#define PASS_H

#include <KRunner/AbstractRunner>
#include <QDir>
#include <QReadWriteLock>
#include <QFileSystemWatcher>

class Pass : public Plasma::AbstractRunner
{
    Q_OBJECT

public:
    Pass(QObject *parent, const QVariantList &args);
    ~Pass();

    void match(Plasma::RunnerContext &) override;
    void run(const Plasma::RunnerContext &, const Plasma::QueryMatch &) override;
    QList<QAction *> actionsForMatch(const Plasma::QueryMatch &) override;
    void reloadConfiguration() override;
    
    
public slots:
    void reinitPasswords(const QString &path);

protected:
    void init() override;
    void initPasswords();
    void showNotification(const QString &, const QString & = "");

private:
    QDir baseDir;
    QString passOtpIdentifier;
    int timeout;
    QReadWriteLock lock;
    QList<QString> passwords;
    QFileSystemWatcher watcher;
    
    bool showActions;
    QList<QAction *> orderedActions;
    
};

#endif
