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
#include <QTimer>

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

    this->passOtpIdentifier = "totp::";
    auto passOtpIdentifier = getenv("PASSWORD_STORE_OTP_IDENTIFIER");
    if (passOtpIdentifier != nullptr) {
        this->passOtpIdentifier = passOtpIdentifier;
    }

    initPasswords();

    connect(&watcher, SIGNAL(directoryChanged(QString)), this, SLOT(reinitPasswords(QString)));
}

void Pass::initPasswords() {
    passwords.clear();

    watcher.addPath(this->baseDir.absolutePath());
    QDirIterator it(this->baseDir, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        auto fileInfo = it.fileInfo();
        if (fileInfo.isFile() && fileInfo.suffix() == "gpg") {
            QString password = this->baseDir.relativeFilePath(fileInfo.absoluteFilePath());
            // Remove suffix ".gpg"
            password.chop(4);
            passwords.append(password);
        } else if (fileInfo.isDir() && it.fileName() != "." && it.fileName() != "..") {
            watcher.addPath(it.filePath());
        }
    }
}

void Pass::reinitPasswords(const QString &path) {
    Q_UNUSED(path);

    lock.lockForWrite();
    initPasswords();
    lock.unlock();
}

void Pass::match(Plasma::RunnerContext &context)
{
    if (!context.isValid()) return;

    auto input = context.query();

    QList<Plasma::QueryMatch> matches;

    lock.lockForRead();
    Q_FOREACH (auto password, passwords) {
        QRegularExpression re(".*" + input + ".*", QRegularExpression::CaseInsensitiveOption);
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
    lock.unlock();

    context.addMatches(matches);
}

void Pass::run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &match)
{
    Q_UNUSED(context);

    auto regexp = QRegularExpression("^" + QRegularExpression::escape(this->passOtpIdentifier) + ".*");
    auto isOtp = match.text().split('/').filter(regexp).size() > 0;

    auto ret = isOtp ?
            QProcess::execute(QString("pass"), QStringList() << "otp" << "-c" << match.text()) :
            QProcess::execute(QString("pass"), QStringList() << "-c" << match.text());
    if (ret == 0) {
        QString msg = i18n("Password %1 copied to clipboard for %2 seconds", match.text(), timeout);
        auto notification = KNotification::event("password-unlocked", "Pass", msg,
                                                 "object-unlocked", nullptr, KNotification::CloseOnTimeout,
                                                 "krunner_pass");
        QTimer::singleShot(timeout * 1000, notification, SLOT(quit));
    }
}

K_EXPORT_PLASMA_RUNNER(pass, Pass)

#include "pass.moc"
