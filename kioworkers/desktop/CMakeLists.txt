add_definitions(-DTRANSLATION_DOMAIN=\"kio_desktop\")

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

set(kio_desktop_SRCS kio_desktop.cpp kio_desktop.h)
qt_add_dbus_interface( kio_desktop_SRCS ${KDED_DBUS_INTERFACE} kded_interface )

qt_generate_dbus_interface( desktopnotifier.h ${CMAKE_CURRENT_BINARY_DIR}/desktopnotifier.xml )
qt_add_dbus_interface( kio_desktop_SRCS ${CMAKE_CURRENT_BINARY_DIR}/desktopnotifier.xml desktopnotifier_interface )

include_directories(${CMAKE_CURRENT_BINARY_DIR})

kcoreaddons_add_plugin(kio_desktop SOURCES ${kio_desktop_SRCS} INSTALL_NAMESPACE "kf6/kio")
target_link_libraries(kio_desktop Qt::DBus Qt::Gui KF6::KIOCore KF6::I18n KF6::ConfigCore Qt::Network)
set_target_properties(kio_desktop PROPERTIES OUTPUT_NAME "desktop")

########### next target ###############

kcoreaddons_add_plugin(desktopnotifier SOURCES desktopnotifier.cpp INSTALL_NAMESPACE "kf6/kded")
target_link_libraries(desktopnotifier KF6::ConfigCore KF6::KIOCore KF6::DBusAddons)

########### install files ###############

install(FILES directory.desktop directory.trash DESTINATION ${KDE_INSTALL_DATADIR}/kio_desktop)
