
Debian
====================
This directory contains files used to package matxd/matx-qt
for Debian-based Linux systems. If you compile matxd/matx-qt yourself, there are some useful files here.

## matx: URI support ##


matx-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install matx-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your matx-qt binary to `/usr/bin`
and the `../../share/pixmaps/matx128.png` to `/usr/share/pixmaps`

matx-qt.protocol (KDE)

