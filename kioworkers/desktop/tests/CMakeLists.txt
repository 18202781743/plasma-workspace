add_executable(testdesktop kio_desktop_test.cpp)
target_link_libraries(testdesktop KF6::KIOWidgets KF6::Solid Qt::Test Qt::DBus)

ecm_mark_as_test(testdesktop)
add_test(NAME testdesktop COMMAND testdesktop)
