Krunner pass
============

Integrates [krunner](https://userbase.kde.org/Plasma/Krunner) with [pass](https://www.passwordstore.org).

## Use with [pass-otp](https://github.com/tadfisher/pass-otp)

To use with pass-otp, use the identifier "totp::" as a prefix in the filename or file path of the otp password file.

Alternatively, set $PASSWORD_STORE_OTP_IDENTIFIER to overwrite the identifier string. This must be set in `.xprofile`
or similar file, before the initalization of krunner.

Build and Installation
======================

You can use the provided `install.sh` script to build and install the plugin.

For archlinux users there is a PKGBUILD in the [AUR](https://aur.archlinux.org/packages/krunner-pass).

Alternatively if you just want to build the plugin:

```
$ mkdir -p build
$ cd build
$ cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_INSTALL_PREFIX=`kf5-config --prefix` -DQT_PLUGIN_INSTALL_DIR=`kf5-config --qt-plugins`
$ make
```

For debian (>=9) you will need the following build dependencies:
```
apt-get install build-essential cmake extra-cmake-modules gettext \
  qtdeclarative5-dev \
  libkf5i18n-dev \
  libkf5service-dev \
  libkf5runner-dev \
  libkf5textwidgets-dev \
  libkf5notifications-dev \
  libkf5kcmutils-dev

mkdir -p build
cd build
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_INSTALL_PREFIX=`kf5-config --prefix` -DKDE_INSTALL_QTPLUGINDIR=`kf5-config --qt-plugins`
make
```

For Fedora (>=23) you will need the following build dependencies:
```
dnf install @development-tools cmake extra-cmake-modules gettext \
   qt5-qtdeclarative-devel \
   kf5-ki18n-devel \
   kf5-kservice-devel \
   kf5-krunner-devel \
   kf5-ktextwidgets-devel \
   kf5-knotifications-devel \
   kf5-kconfigwidgets-devel \

mkdir -p build
cd build
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_INSTALL_PREFIX=`kf5-config --prefix` -DKDE_INSTALL_QTPLUGINDIR=`kf5-config --qt-plugins`
make
```