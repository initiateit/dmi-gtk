# DDC Monitor & Inputs Utils (dmi-gtk)
A GTK Interface for controlling brightness, contrast and input selection through the DDC/CI protocol. It includes support for controlling multiple displays.

![dmi-gtk screenshot](https://raw.githubusercontent.com/initiateit/dmi-gtk/master/dmi-screenshot.png)

# Dependencies
- ddcutil library package (typically libddcutil or libddcutil-dev if not already installed with ddcutil)
- GTK 3.0

# Setup

1. Firstly, ensure that all the dependancies are installed.

#### Arch Linux:
```
pacman -S --needed ddcutil gtk3
```

#### Ubuntu-based distros:
```
sudo apt install libgtk3-dev gcc
sudo add-apt-repository ppa:rockowitz/ddcutil # add ddcutil repo
sudo apt install ddcutil libddcutil-dev
```
#### Fedora:
```
sudo dnf install ddcutil libddcutil libddcutil-devel gtk3-devel gcc
```

2. Ensure that your user has access to the i2c devices:
https://www.ddcutil.com/i2c_permissions/


4. Change into the dmi-gtk directory and execute build.sh to build this application:
```
cd dmi-gtk
./build.sh
```

5. This should result in a dmi-gtk binary that you can execute to contol the brightness:
```
./dmi-gtk
```

#### To Do:
- Add Color temperature support
- Add Voulme control
- Add more detailed information for monitors such as Display Number
- Add status for current input that is active

