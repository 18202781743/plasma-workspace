/*
    SPDX-FileCopyrightText: 2018 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <QGuiApplication>
#include <QSessionManager>

#include <KAboutData>
#include <KCrash>
#include <KDBusService>
#include <KWindowSystem>

#include "config-workspace.h"
#include "menuproxy.h"

using namespace Qt::StringLiterals;

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "xcb");

    QGuiApplication::setDesktopSettingsAware(false);

    QGuiApplication app(argc, argv);

    if (!KWindowSystem::isPlatformX11()) {
        qFatal("gmenudbusmenuproxy is only useful XCB. Aborting");
    }

    KAboutData about(QStringLiteral("gmenudbusmenuproxy"), QString(), QStringLiteral(WORKSPACE_VERSION_STRING));
    KAboutData::setApplicationData(about);

    KCrash::initialize();

    auto disableSessionManagement = [](QSessionManager &sm) {
        sm.setRestartHint(QSessionManager::RestartNever);
    };
    app.connect(&app, &QGuiApplication::commitDataRequest, disableSessionManagement);
    app.connect(&app, &QGuiApplication::saveStateRequest, disableSessionManagement);

    app.setQuitOnLastWindowClosed(false);

    KDBusService service(KDBusService::Unique);
    MenuProxy proxy;

    return app.exec();
}
