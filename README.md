Krunner pass
============

Integrates [krunner](https://userbase.kde.org/Plasma/Krunner) with [pass](https://www.passwordstore.org).

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


