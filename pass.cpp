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
#include <KLocalizedString>
#include <KNotification>

#include <QAction>
#include <QDirIterator>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>
#include <QMessageBox>
#include <QClipboard>
#include <QDebug>
#include <QApplication>
#include <KGuiAddons/ksystemclipboard.h>
#include <QMimeData>

#include <cstdlib>

#include "pass.h"
#include "config.h"

using namespace std;


Pass::Pass(QObject *parent, const QVariantList &args)
    : Plasma::AbstractRunner(parent, args)
{
    // General runner configuration
    setObjectName(QStringLiteral("Pass"));
    setSpeed(AbstractRunner::NormalSpeed);
    setPriority(HighestPriority);
}

Pass::~Pass() = default;

void Pass::reloadConfiguration()
{
    clearActions();
    orderedActions.clear();

    KConfigGroup cfg = config();
    cfg.config()->reparseConfiguration(); // Just to be sure
    this->showActions = cfg.readEntry(Config::showActions, false);

    if (showActions) {
        const auto configActions = cfg.group(Config::Group::Actions);

        // Create actions for every additional field
        const auto configActionsList = configActions.groupList();
        for (const auto &name: configActionsList) {
            auto group = configActions.group(name);
            auto passAction = PassAction::fromConfig(group);

            auto icon = QIcon::fromTheme(passAction.icon, QIcon::fromTheme("object-unlocked"));
            QAction *act = addAction(passAction.name, icon, passAction.name);
            act->setData(passAction.regex);
            this->orderedActions << act;
        }

    } else {
        this->orderedActions.clear();
    }

    if (cfg.readEntry(Config::showFileContentAction, false)) {
        QAction *act = addAction(Config::showFileContentAction, QIcon::fromTheme("document-new"),
                                 i18n("Show password file contents"));
        act->setData(Config::showFileContentAction);
        this->orderedActions << act;
    }

    setDefaultSyntax(Plasma::RunnerSyntax(QString(":q:"),
                                          i18n("Looks for a password matching :q:. Pressing ENTER copies the password to the clipboard.")));

    addSyntax(Plasma::RunnerSyntax(QString("pass :q:"),
                                          i18n("Looks for a password matching :q:. This way you avoid results from other runners")));
}

void Pass::init()
{
    reloadConfiguration();

    this->baseDir = QDir(QDir::homePath() + "/.password-store");
    auto _baseDir = getenv("PASSWORD_STORE_DIR");
    if (_baseDir != nullptr) {
        this->baseDir = QDir(_baseDir);
    }

    this->timeout = 45;
    auto _timeout = getenv("PASSWORD_STORE_CLIP_TIME");
    if (_timeout != nullptr) {
        QString str(_timeout);
        bool ok;
        auto _timeoutParsed = str.toInt(&ok);
        if (ok) {
            this->timeout = _timeoutParsed;
        }
    }

    this->passOtpIdentifier = "totp::";
    auto _passOtpIdentifier = getenv("PASSWORD_STORE_OTP_IDENTIFIER");
    if (_passOtpIdentifier != nullptr) {
        this->passOtpIdentifier = _passOtpIdentifier;
    }

    initPasswords();

    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, &Pass::reinitPasswords);
}

void Pass::initPasswords()
{
    passwords.clear();

    watcher.addPath(this->baseDir.absolutePath());
    QDirIterator it(this->baseDir, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const auto fileInfo = it.fileInfo();
        if (fileInfo.isFile() && fileInfo.suffix() == QLatin1String("gpg")) {
            QString password = this->baseDir.relativeFilePath(fileInfo.absoluteFilePath());
            // Remove suffix ".gpg"
            password.chop(4);
            passwords.append(password);
        } else if (fileInfo.isDir() && it.fileName() != "." && it.fileName() != "..") {
            watcher.addPath(it.filePath());
        }
    }
}

void Pass::reinitPasswords(const QString &path)
{
    Q_UNUSED(path)

    lock.lockForWrite();
    initPasswords();
    lock.unlock();
}

void Pass::match(Plasma::RunnerContext &context)
{
    if (!context.isValid()) {
        return;
    }

    auto input = context.query();
    // If we use the prefix we want to remove it
    if (input.contains(queryPrefix)) {
        input = input.remove(QLatin1String("pass")).simplified();
    } else if (input.count() < 3 && !context.singleRunnerQueryMode()) {
        return;
    }

    QList<Plasma::QueryMatch> matches;

    lock.lockForRead();
    for (const auto &password: qAsConst(passwords)) {
        if (password.contains(input, Qt::CaseInsensitive)) {
            Plasma::QueryMatch match(this);
            match.setType(input.length() == password.length() ?
                          Plasma::QueryMatch::ExactMatch : Plasma::QueryMatch::CompletionMatch);
            match.setIcon(QIcon::fromTheme("object-locked"));
            match.setText(password);
            matches.append(match);
        }
    }
    lock.unlock();

    context.addMatches(matches);
}

void Pass::clip(const QString &msg)
{
    auto* mimeData = new QMimeData();
    mimeData->setText(msg);
    KSystemClipboard::instance()->setMimeData(mimeData, QClipboard::Clipboard);
    QTimer::singleShot(timeout * 1000, nullptr, []() {
        auto* mimeData = new QMimeData();
        mimeData->setText("");
        KSystemClipboard::instance()->setMimeData(mimeData, QClipboard::Clipboard);
    });
}

void Pass::run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &match)
{
    Q_UNUSED(context);
    const auto regexp = QRegularExpression("^" + QRegularExpression::escape(this->passOtpIdentifier) + ".*");
    const auto isOtp = !match.text().split('/').filter(regexp).isEmpty();

    auto *pass = new QProcess();
    QStringList args;
    if (isOtp) {
        args << "otp";
    }
    args << "show" << match.text();
    pass->start("pass", args);

    connect(pass, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            [=](int exitCode, QProcess::ExitStatus exitStatus) {
                Q_UNUSED(exitStatus)

                if (exitCode == 0) {
                    const auto output = pass->readAllStandardOutput();
                    if (match.selectedAction() != nullptr) {
                        const auto data = match.selectedAction()->data().toString();
                        if (data == Config::showFileContentAction) {
                            QMessageBox::information(nullptr, match.text(), output);
                        } else {
                            QRegularExpression re(data, QRegularExpression::MultilineOption);
                            const auto matchre = re.match(output);

                            if (matchre.hasMatch()) {
                                clip(matchre.captured(1));
                                this->showNotification(match.text(), match.selectedAction()->text());
                            } else {
                                // Show some information to understand what went wrong.
                                qInfo() << "Regexp: " << data;
                                qInfo() << "Is regexp valid? " << re.isValid();
                                qInfo() << "The file: " << match.text();
                                // qInfo() << "Content: " << output;
                            }
                        }
                    } else {
                        const auto string = QString::fromUtf8(output.data());
                        const auto lines = string.split('\n', QString::SkipEmptyParts);
                        if (!lines.isEmpty()) {
                            clip(lines[0]);
                            this->showNotification(match.text());
                        }
                    }
                }

                pass->close();
                pass->deleteLater();
            });
}

QList<QAction *> Pass::actionsForMatch(const Plasma::QueryMatch &match)
{
    Q_UNUSED(match)

    return this->orderedActions;
}

void Pass::showNotification(const QString &text, const QString &actionName)
{
    const QString msgPrefix = actionName.isEmpty() ? "" : actionName + i18n(" of ");
    const QString msg = i18n("Password %1 copied to clipboard for %2 seconds", text, timeout);
    KNotification::event("password-unlocked", "Pass", msgPrefix + msg,
                         "object-unlocked", nullptr, KNotification::CloseOnTimeout,
                         "krunner_pass");
}

K_EXPORT_PLASMA_RUNNER(pass, Pass)

#include "pass.moc"
