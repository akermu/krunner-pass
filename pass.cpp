/******************************************************************************
 *  Copyright (C) 2016 by Dominik Schreiber <dev@dominikschreiber.de>         *
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
#include <KLocalizedString>
#include <knotification.h>

#include <QDirIterator>
#include <QProcess>
#include <QRegularExpression>

#include <stdlib.h>

#include "pass.h"

using namespace std;

Pass::Pass(QObject *parent, const QVariantList &args)
    : Plasma::AbstractRunner(parent, args)
{
    Q_UNUSED(args);
    
    // General runner configuration
    setObjectName(QString("Pass"));
    setSpeed(AbstractRunner::NormalSpeed);
    setPriority(HighestPriority);
    auto comment = i18n("Looks for a password matching :q:. Pressing ENTER copies the password to the clipboard.");
    setDefaultSyntax(Plasma::RunnerSyntax(QString(":q:"), comment));
}

Pass::~Pass() {}

void Pass::init() {
    this->baseDir = QDir(QDir::homePath() + "/.password-store");
    auto baseDir = getenv("PASSWORD_STORE_DIR");
    if (baseDir != nullptr) {
        this->baseDir = QDir(baseDir);
    }

    this->timeout = 45;
    auto timeout = getenv("PASSWORD_STORE_CLIP_TIME");
    if (timeout != nullptr) {
        QString str(timeout);
        bool ok;
        auto timeout = str.toInt(&ok);
        if (ok) {
            this->timeout = timeout;
        }
    }

    initPasswords();
}

void Pass::initPasswords() {
    passwords.clear();
    QDirIterator it(this->baseDir, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        auto fileInfo = it.fileInfo();
        if (fileInfo.isFile() && fileInfo.suffix() == "gpg") {
            QString password = this->baseDir.relativeFilePath(fileInfo.absoluteFilePath());
            // Remove suffix ".gpg"
            password.chop(4);
            passwords.append(password);
        }
    }
}

void Pass::match(Plasma::RunnerContext &context)
{
    if (!context.isValid()) return;

    auto input = context.query();

    QList<Plasma::QueryMatch> matches;

    Q_FOREACH (auto password, passwords) {
        QRegularExpression re(".*" + input + ".*");
        if (re.match(password).hasMatch()) {
            Plasma::QueryMatch match(this);
            if (input.length() == password.length()) {
                match.setType(Plasma::QueryMatch::ExactMatch);
            } else {
                match.setType(Plasma::QueryMatch::CompletionMatch);
            }
            match.setIcon(QIcon::fromTheme("object-locked"));
            match.setText(password);
            matches.append(match);
        }
    }
    context.addMatches(matches);
}

void Pass::run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &match)
{
    Q_UNUSED(context);

    auto ret = QProcess::execute(QString("pass -c ") + match.text());
    if (ret == 0) {
        QString msg = i18n("Password %1 copied to clipboard for %2 seconds", match.text(), timeout);
        KNotification::event("password-unlocked", "Pass", msg,
                             "object-unlocked", nullptr, KNotification::CloseOnTimeout,
                             "krunner_pass");
    }
}

K_EXPORT_PLASMA_RUNNER(pass, Pass)

#include "pass.moc"
