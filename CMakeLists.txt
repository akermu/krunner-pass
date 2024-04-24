cmake_minimum_required(VERSION 3.16.0)

project(RunnerPass)

find_package(ECM 5.12.0 REQUIRED NO_MODULE)
set (CMAKE_MODULE_PATH
     ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_MODULE_PATH}
)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

# CPack configuration
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Copies a password from the password store (https://www.passwordstore.org/) to the clipboard")
set(CPACK_PACKAGE_NAME krunner-pass)
set(CPACK_PACKAGE_VERSION "1.1.0")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "alex1701c")
set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
set(CPACK_GENERATOR "DEB;RPM")
set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/postinst")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/postinst")

INCLUDE(CPack)

if (QT_MAJOR_VERSION STREQUAL "6")
    set(QT_MIN_VERSION "6.4.0")
    set(KF_MIN_VERSION "5.240.0")
    set(KF_MAJOR_VERSION "6")
else()
  set(KF_MAJOR_VERSION "5")
endif()

find_package (Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION}
  REQUIRED CONFIG COMPONENTS
  Widgets
  Core
  Quick
  DBus
)
find_package (KF${KF_MAJOR_VERSION}
  REQUIRED COMPONENTS
  I18n
  Service
  Runner
  TextWidgets
  ConfigWidgets
  Notifications
  Auth
  KCMUtils
  GuiAddons
)

if(KF_MAJOR_VERSION STREQUAL 5)
  find_package(KF${KF_MAJOR_VERSION}
    REQUIRED COMPONENTS
    PlasmaQuick
    Plasma
  )
else()
  find_package(PlasmaQuick REQUIRED)
  find_package(Plasma)
endif()

set(krunner_pass_SRCS
    pass.cpp
)

set(kcm_krunner_pass_SRCS
    config.cpp
)

ki18n_wrap_ui(kcm_krunner_pass_SRCS config.ui)
add_library(kcm_krunner_pass MODULE ${kcm_krunner_pass_SRCS})
target_link_libraries(kcm_krunner_pass
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    KF${KF_MAJOR_VERSION}::CoreAddons
    KF${KF_MAJOR_VERSION}::ConfigCore
    KF${KF_MAJOR_VERSION}::ConfigWidgets
    KF${KF_MAJOR_VERSION}::I18n
    KF${KF_MAJOR_VERSION}::Runner
    KF${KF_MAJOR_VERSION}::KCMUtils
    )
kcmutils_generate_desktop_file(kcm_krunner_pass)

add_library(krunner_pass MODULE ${krunner_pass_SRCS})
target_link_libraries(krunner_pass KF${KF_MAJOR_VERSION}::Runner Qt${QT_MAJOR_VERSION}::Widgets
                      KF${KF_MAJOR_VERSION}::I18n
                      KF${KF_MAJOR_VERSION}::Service
                      KF${KF_MAJOR_VERSION}::ConfigWidgets
                      KF${KF_MAJOR_VERSION}::Notifications
                      KF${KF_MAJOR_VERSION}::KCMUtils
                      KF${KF_MAJOR_VERSION}::GuiAddons)
if(KF_MAJOR_VERSION STREQUAL 5)
  target_link_libraries(krunner_pass
    KF${KF_MAJOR_VERSION}::Plasma
  )
else()
   target_link_libraries(krunner_pass
     Plasma::Plasma
   )
endif()

add_dependencies(krunner_pass kcm_krunner_pass)

install(TARGETS kcm_krunner_pass DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/kf${KF_MAJOR_VERSION}/krunner/kcms)
install(TARGETS krunner_pass DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/kf${KF_MAJOR_VERSION}/krunner)
install(FILES krunner_pass.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
