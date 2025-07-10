# DDC Monitor & Inputs Utils (dmi-gtk)
A GTK Interface for controlling brightness, contrast and input selection through the DDC/CI protocol. It includes support for controlling multiple displays.
Credit to @ahshabbir https://github.com/ahshabbir/ddcbc-gtk for the existing work.

The app works by running and then detecting inout in the window. If none is detected within 4 seconds it will "hide" rather than close.
Relaunching the app with bring the focus back. 

This was chosen to mimic the monitor OSD and step away from Gnome and their refusal to make system tray apps a pain to use by requiring extensions or non standard app indicators (unless its their own widgets).


![dmi-gtk screenshot](https://raw.githubusercontent.com/initiateit/dmi-gtk/gtk4/dmi-gtk4-screenshot.png)


# Dependencies
- ddcutil library package (typically libddcutil or libddcutil-dev if not already installed with ddcutil)
- GTK 4.0 (if using GTK4 Fork)

# Setup

1. Firstly, ensure that all the dependancies are installed.

#### Arch based distros:
```
pacman -S --needed ddcutil gtk4
```

#### Ubuntu-based distros:
```
sudo apt install libgtk-4-dev gcc
sudo add-apt-repository ppa:rockowitz/ddcutil
sudo apt update
sudo apt install ddcutil libddcutil-dev
```
#### Fedora:
```
sudo dnf install ddcutil libddcutil libddcutil-devel gtk4-devel gcc
```

#### Others:
How long is a peice of string.

2. Ensure that your user has access to the i2c devices:
https://www.ddcutil.com/i2c_permissions/

3. Change into the dmi-gtk directory and execute build.sh to build this application:
```
cd dmi-gtk
./build.sh
```
The build script will check for various CPU capabilities and compile to the best available.

4. This should result in a dmi-gtk binary that you can execute to contol various display functions as you would with the OSD.:
```
./dmi-gtk
```

### To Do:
- ~~Fully retire GTK3 fork.~~
- Add UI improvements (icons, better spacings and layout fixes)
- Add ability to change R G B channels individually.
- ~~Make app run in background as ddcutil takes at least 4 seconds to init~~
- ~~Once in background, enable running the app again to bring into focus~~
- ~~Make frameless~~
- ~~Enable autohide after timeout~~
- ~~Add Color temperature support~~
- ~~Add Voulme control~~
- ~~Add more detailed information for monitors such as Display Number~~
- ~~Add status for current input that is active~~
- ~~Fix dark mode support~~

