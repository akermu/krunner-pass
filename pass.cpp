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

#include <QIcon>
#include <QAction>
#include <QDirIterator>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>
#include <QMessageBox>
#include <QClipboard>
#include <KSystemClipboard>
#include <QMimeData>
#include <QDebug>
#include <QApplication>

#include <cstdlib>

#include "pass.h"
#include "config.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <KRunner/Action>
#endif

using namespace std;

K_PLUGIN_CLASS_WITH_JSON(Pass, "pass.json")

Pass::Pass(QObject *parent, const KPluginMetaData &metaData, const QVariantList &args)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    : KRunner::AbstractRunner(metaData, parent)
#else
    : KRunner::AbstractRunner(parent, metaData)
#endif
{
    Q_UNUSED(args)

    // General runner configuration
    setObjectName(QStringLiteral("Pass"));
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    setPriority(HighestPriority);
#endif
}

Pass::~Pass() = default;

void Pass::reloadConfiguration()
{
    //clearActions(); deprecated, needed?
    orderedActions.clear();

    KConfigGroup cfg = config();
    cfg.config()->reparseConfiguration(); // Just to be sure
    this->showActions = cfg.readEntry(Config::showActions, false);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    uint32_t actionIdCounter = 0;
#endif

    if (showActions) {
        const auto configActions = cfg.group(Config::Group::Actions);
        const auto configActionsList = configActions.groupList();
        for (const auto &name: configActionsList) {
            auto group = configActions.group(name);
            // FIXME how to  fallback?
            auto passAction = PassAction::fromConfig(group);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            auto icon = QIcon::fromTheme(passAction.icon, QIcon::fromTheme("object-unlocked"));
            auto *act = new QAction(icon, passAction.name, this);
            act->setData(passAction.regex);
#else
            auto *act = new KRunner::Action(passAction.regex, QIcon::hasThemeIcon(passAction.icon) ?
                                            passAction.icon : QStringLiteral("object-unlocked"),  passAction.name);
#endif
            this->orderedActions << act;
        }

    } else {
        this->orderedActions.clear();
    }

    if (cfg.readEntry(Config::showFileContentAction, false)) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        auto *act = new QAction(QIcon::fromTheme("document-new"),
                                 i18n("Show password file contents"), this);
        act->setData(Config::showFileContentAction);
#else
        auto *act = new KRunner::Action(Config::showFileContentAction, "document-new", i18n("Show password file contents"));
#endif
        this->orderedActions << act;
    }

    addSyntax(KRunner::RunnerSyntax(QString(":q:"),
                                          i18n("Looks for a password matching :q:. Pressing ENTER copies the password to the clipboard.")));

    addSyntax(KRunner::RunnerSyntax(QString("pass :q:"),
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

void Pass::match(KRunner::RunnerContext &context)
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

    QList<KRunner::QueryMatch> matches;

    lock.lockForRead();
    for (const auto &password: qAsConst(passwords)) {
        if (password.contains(input, Qt::CaseInsensitive)) {
            KRunner::QueryMatch match(this);
            match.setCategoryRelevance(input.length() == password.length() ? KRunner::QueryMatch::CategoryRelevance::Highest :
                                       KRunner::QueryMatch::CategoryRelevance::Moderate);
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
    auto md = new QMimeData;
    auto kc = KSystemClipboard::instance();
    // https://phabricator.kde.org/D12539
    md->setText(msg);
    md->setData(QStringLiteral("x-kde-passwordManagerHint"), "secret");
    kc->setMimeData(md,QClipboard::Clipboard);
    QTimer::singleShot(timeout * 1000, kc, [kc]() {
        kc->clear(QClipboard::Clipboard);
    });
}

void Pass::run(const KRunner::RunnerContext &context, const KRunner::QueryMatch &match)
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
                    if (match.selectedAction()) {
                        const auto data =
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                            match.selectedAction()->data().toString();
#else
                        match.selectedAction().id();
#endif
                        if (data == Config::showFileContentAction) {
                            QMessageBox::information(nullptr, match.text(), output);
                        } else {
                            QRegularExpression re(data, QRegularExpression::MultilineOption);
                            const auto matchre = re.match(output);

                            if (matchre.hasMatch()) {
                                clip(matchre.captured(1));
                                this->showNotification(match.text(),
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                                                       match.selectedAction()->text());
#else
                                match.selectedAction().text());
#endif
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
                        const auto lines = string.split('\n', Qt::SkipEmptyParts);
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
QList<QAction *> Pass::actionsForMatch(const Plasma::QueryMatch &match)
{
    Q_UNUSED(match)

    return this->orderedActions;
}

#endif

void Pass::showNotification(const QString &text, const QString &actionName)
{
    const QString msgPrefix = actionName.isEmpty() ? "" : actionName + i18n(" of ");
    const QString msg = i18n("Password %1 copied to clipboard for %2 seconds", text, timeout);
    KNotification::event("password-unlocked", "Pass", msgPrefix + msg,
                         "object-unlocked",
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                         nullptr,
#endif
                         KNotification::CloseOnTimeout,
                         "krunner_pass");
}

#include "pass.moc"
