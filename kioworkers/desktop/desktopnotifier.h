/*
    SPDX-FileCopyrightText: 2008, 2009 Fredrik Höglund <fredrik@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#pragma once

#include <QDBusAbstractAdaptor>
#include <QUrl>
#include <kdedmodule.h>

class QFileSystemWatcher;

class DesktopNotifier : public KDEDModule
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.DesktopNotifier")

public:
    DesktopNotifier(QObject *parent, const QList<QVariant> &);

public Q_SLOTS:
    Q_SCRIPTABLE Q_NOREPLY void watchDir(const QString &path);

private Q_SLOTS:
    void dirty(const QString &path);

private:
    void checkDesktopLocation();

    QFileSystemWatcher *watcher;
    QUrl m_desktopLocation;
};
