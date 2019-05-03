project(RunnerPass)

find_package(ECM 5.12.0 REQUIRED NO_MODULE)
set (CMAKE_MODULE_PATH
     ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_MODULE_PATH}
)

find_package (Qt5 ${QT_MIN_VERSION} REQUIRED CONFIG COMPONENTS Widgets Core DBus Quick)
find_package (KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS Auth I18n Service Runner TextWidgets ConfigWidgets PlasmaQuick Notifications)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

set(krunner_pass_SRCS
    pass.cpp
)

set(kcm_krunner_pass_SRCS
    config.cpp
)

ki18n_wrap_ui(kcm_krunner_pass_SRCS config.ui)
add_library(kcm_krunner_pass MODULE ${kcm_krunner_pass_SRCS})
target_link_libraries(kcm_krunner_pass
    Qt5::Core
    Qt5::Gui
    KF5::CoreAddons
    KF5::ConfigCore
    KF5::I18n
    KF5::ConfigWidgets
    KF5::Runner
    )

add_library(krunner_pass MODULE ${krunner_pass_SRCS})
target_link_libraries(krunner_pass KF5::Runner Qt5::Widgets
                      KF5::I18n
                      KF5::Service
                      KF5::Plasma
                      KF5::ConfigWidgets
                      KF5::Notifications)

add_dependencies(krunner_pass kcm_krunner_pass)

install(TARGETS krunner_pass kcm_krunner_pass DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES plasma-runner-pass.desktop plasma-runner-pass_config.desktop DESTINATION ${SERVICES_INSTALL_DIR})
install(FILES krunner_pass.notifyrc DESTINATION ${KNOTIFYRC_INSTALL_DIR})
