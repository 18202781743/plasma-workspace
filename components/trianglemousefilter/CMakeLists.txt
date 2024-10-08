# SPDX-FileCopyrightText: 2022 Bharadwaj Raju <bharadwaj.raju777@protonmail.com>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

ecm_add_qml_module(trianglemousefilterplugin
    URI "org.kde.plasma.workspace.trianglemousefilter"
    VERSION 1.0
    GENERATE_PLUGIN_SOURCE
    DEPENDENCIES QtQuick
)

target_sources(trianglemousefilterplugin PRIVATE
    trianglemousefilter.cpp
)

ecm_qt_declare_logging_category(trianglemousefilterplugin HEADER debug.h
    IDENTIFIER TRIANGLE_MOUSE_FILTER
    CATEGORY_NAME kde.trianglemousefilter
    DEFAULT_SEVERITY Info
    DESCRIPTION "triangle mouse filter"
    EXPORT PLASMAWORKSPACE
)


target_link_libraries(trianglemousefilterplugin PRIVATE
    Qt::Core
    Qt::Quick
    Qt::Qml
)

ecm_finalize_qml_module(trianglemousefilterplugin)
