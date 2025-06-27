# dmi-gtk
A GTK Interface for controlling brightness through the DDC/CI protocol. It includes support for controlling the brightness of multiple displays.

![dmi-gtk screenshot](https://raw.githubusercontent.com/ahshabbir/dmi-gtk/master/dmi-screenshot.png)

# Dependencies
- ddcutil library package (typically libddcutil or libddcutil-dev if not already installed with ddcutil)
- GTK 3.0

# Setup

1. Firstly, ensure that all the dependancies are installed.

#### Arch Linux:
```
pacman -S --needed ddcutil gtk3
```

#### Ubuntu-based distros (credit @aidengilmartin):
```
sudo apt install libgtk3-dev gcc
sudo add-apt-repository ppa:rockowitz/ddcutil # add ddcutil repo
sudo apt install ddcutil libddcutil-dev
```
#### Fedora:
```
sudo dnf install ddcutil libddcutil libddcutil-devel gtk3-devel gcc
```
TODO: Add other distributions to this! [#3](../../issues/3)

2. Ensure that your user has access to the i2c devices:
https://www.ddcutil.com/i2c_permissions/

3. Clone this repo along with its submodules:
```
git clone --recurse-submodules https://github.com/ahshabbir/dmi-gtk.git
```

4. Change into the dmi-gtk directory and execute build.sh to build this application:
```
cd dmi-gtk
./build.sh
```

5. This should result in a dmi-gtk binary that you can execute to contol the brightness:
```
./dmi-gtk
```

To install this binary for all users execute 'install.sh' as root:
```
sudo ./install.sh
```

You should now be able to find dmi-gtk or DDC Brightness Control in your application menu under utilities.

# Known Issues/Todos

1. Unaligned seperator between each monitor. [#1](../../issues/1)
2. Find/create a logo. [#2](../../issues/2)
3. Add directions for installing deps on other distros. [#3](../../issues/3)

# Credits
rockowitz - for ddcutil c api:
https://github.com/rockowitz/ddcutil
