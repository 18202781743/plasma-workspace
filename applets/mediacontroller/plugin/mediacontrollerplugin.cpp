/*
    SPDX-FileCopyrightText: 2022 Bharadwaj Raju <bharadwaj.raju777@protonmail.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "mediacontrollerplugin.h"
#include "globalconfig.h"

#include <QQmlEngine>

void MediaControllerPlugin::registerTypes(const char *uri)
{
    qmlRegisterType<GlobalConfig>(uri, 1, 0, "GlobalConfig");
}

#include "moc_mediacontrollerplugin.cpp"
