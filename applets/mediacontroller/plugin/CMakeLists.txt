# SPDX-FileCopyrightText: 2022 Bharadwaj Raju <bharadwaj.raju777@protonmail.com>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

set(mediacontrollerplugin_SRCS
    mediacontrollerplugin.cpp mediacontrollerplugin.h
    globalconfig.cpp globalconfig.h
)

add_library(mediacontrollerplugin SHARED ${mediacontrollerplugin_SRCS})
target_link_libraries(mediacontrollerplugin
    PRIVATE
        Qt::Core
        Qt::Qml
        KF6::ConfigCore
)

install(TARGETS mediacontrollerplugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/plasma/private/mediacontroller)

install(FILES qmldir DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/plasma/private/mediacontroller)
