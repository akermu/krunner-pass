project(RunnerPass)

find_package(ECM 5.12.0 REQUIRED NO_MODULE)
set (CMAKE_MODULE_PATH
     ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_MODULE_PATH}
)

find_package (Qt5 ${QT_MIN_VERSION} REQUIRED CONFIG COMPONENTS Widgets Core Quick)
find_package (KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS I18n Service Runner TextWidgets PlasmaQuick Notifications)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

set(krunner_pass_SRCS
    pass.cpp
)

add_library(krunner_pass MODULE ${krunner_pass_SRCS})
target_link_libraries(krunner_pass KF5::Runner Qt5::Widgets
                      KF5::I18n
                      KF5::Service
                      KF5::Plasma
                      KF5::Notifications)
                      
install(TARGETS krunner_pass DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES plasma-runner-pass.desktop DESTINATION ${SERVICES_INSTALL_DIR})
install(FILES krunner_pass.notifyrc DESTINATION ${KNOTIFYRC_INSTALL_DIR})
