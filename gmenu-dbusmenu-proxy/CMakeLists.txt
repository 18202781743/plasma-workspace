find_package(AppMenuGtkModule)
set_package_properties(AppMenuGtkModule PROPERTIES TYPE RUNTIME)

add_definitions(-DQT_NO_CAST_TO_ASCII
-DQT_NO_CAST_FROM_ASCII
-DQT_NO_CAST_FROM_BYTEARRAY)

find_package(XCB
    REQUIRED COMPONENTS
        XCB
)

set(GMENU_DBUSMENU_PROXY_SRCS
    main.cpp
    menuproxy.cpp menuproxy.h
    window.cpp
    menu.cpp
    actions.cpp actions.h
    gdbusmenutypes_p.cpp
    icons.cpp icons.h
    utils.cpp utils.h
    ../libdbusmenuqt/dbusmenutypes_p.cpp
)

qt_add_dbus_adaptor(GMENU_DBUSMENU_PROXY_SRCS ../libdbusmenuqt/com.canonical.dbusmenu.xml window.h Window)

set_source_files_properties(org.gtk.Actions.xml PROPERTIES
        NO_NAMESPACE true
        INCLUDE "gdbusmenutypes_p.h"
)
qt_add_dbus_interface(GMENU_DBUSMENU_PROXY_SRCS org.gtk.Actions.xml gtkactions_interface)

set_source_files_properties(org.gtk.Menus.xml PROPERTIES
        NO_NAMESPACE true
        INCLUDE "gdbusmenutypes_p.h"
)
qt_add_dbus_interface(GMENU_DBUSMENU_PROXY_SRCS org.gtk.Menus.xml gtkmenus_interface)

ecm_qt_declare_logging_category(GMENU_DBUSMENU_PROXY_SRCS HEADER debug.h
                                               IDENTIFIER DBUSMENUPROXY
                                               CATEGORY_NAME kde.dbusmenuproxy
                                               DEFAULT_SEVERITY Info
                                               DESCRIPTION "Gmenu dbusmenu proxy"
                                               EXPORT PLASMAWORKSPACE
                                           )

add_executable(gmenudbusmenuproxy ${GMENU_DBUSMENU_PROXY_SRCS})

set_package_properties(XCB PROPERTIES TYPE REQUIRED)

target_link_libraries(gmenudbusmenuproxy
    Qt::Core
    Qt::DBus
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::DBusAddons
    KF6::WindowSystem
    KF6::Crash
    XCB::XCB
)
target_link_libraries(gmenudbusmenuproxy Qt::GuiPrivate)

install(TARGETS gmenudbusmenuproxy ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES gmenudbusmenuproxy.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})

ecm_install_configured_files(INPUT plasma-gmenudbusmenuproxy.service.in @ONLY DESTINATION ${KDE_INSTALL_SYSTEMDUSERUNITDIR})
