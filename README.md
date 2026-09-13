# Process Party
Process Party is a real-time system resource monitor that provides a
customizable alternative to Linux’s default task manager. The application
displays details about system resource usage, allows users to record system 
usage data as a CSV, and offers enhanced color customization within its interface.

## Features
-  Detailed real-time graphical information about CPU usage, memory usage, disk activity, network activity, and
process-level statistics.
-  Searchable process table with the ability to sort by metric, show or hide metrics, kill processes, or change
process priority.
-  Application dashboard with at-a-glance system information graphs.
-  History tab with configuration settings for process-by-process session recordings, session summaries, and CSV session data export.
-  Full color and font customization, including 18 preset themes, a color picker, and the ability to save user-generated themes.

## How to Install
1.  Open a terminal and clone the repository.
```
cd Documents
Git clone https://github.com/shiro-han/Process-Party
cd ~/Documents/Process-Party 
```
2.  Build the release version.
```
make -DCMAKE_BUILD_TYPE=Release -B build/Desktop-Release
cmake --build build/Desktop-Release
```
3.  Create the deploy folder.
```
mkdir -p ~/ProcessPartyDeploy/platforms
cp ~/Documents/Process-Party/build/Desktop-Release/
SystemMonitorDemo ~/ProcessPartyDeploy/
cp ~/Documents/Process-Party/capyy_2.png ~/ProcessPartyDeploy/ 
cp /usr/lib/aarch64-linux-gnu/libQt6Widgets.so.6 ~/ ProcessPartyDeploy/
cp /usr/lib/aarch64-linux-gnu/libQt6Gui.so.6 ~/ ProcessPartyDeploy/
cp /usr/lib/aarch64-linux-gnu/libQt6Core.so.6 ~/ ProcessPartyDeploy/
cp /usr/lib/aarch64-linux-gnu/libQt6DBus.so.6 ~/ ProcessPartyDeploy/
cp /usr/lib/aarch64-linux-gnu/qt6/plugins/platforms/libqxcb.so
~/ProcessPartyDeploy/platforms/ 
```
4.  Create the run script.
```
vi ~/ProcessPartyDeploy/run-process-party.sh
```
Paste the following into the window:
```
#!/bin/sh
DIR="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$DIR:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$DIR"
export QT_QPA_PLATFORM_PLUGIN_PATH="$DIR/platforms"
"$DIR/SystemMonitorDemo" "$@" 
```
Save and exit by hitting esc, then typing :wq and hitting enter.

5.  Make the script executable.
```
chmod +x ~/ProcessPartyDeploy/run-process-party.sh 
```

6.  Check the final folder structure by running:
```
tree -a ./ProcessPartyDeploy/
```
It should look like this:
```
 ProcessPartyDeploy/
 ├── SystemMonitorDemo
 ├── run-process-party.sh
 ├── libQt6Core.so.6
 ├── libQt6Gui.so.6
 ├── libQt6Widgets.so.6
 ├── libQt6DBus.so.6
 ├── platforms/
 │ └── libqxcb.so
 └── capyy_2.png
```
7.  Run the application with:
```
cd ~/ProcessPartyDeploy ./run-process-party.sh
````
## System Requirements
Process Party is a standalone Linux desktop application created for Ubuntu 22.04+ Linux
distributions. It requires a GUI environment (X11 or Wayland). The minimum hardware
required is a dual-core CPU with 4GB of RAM and minimal disk space, but a
quad-core CPU with 8GB of RAM is recommended. The software required includes:
- C++17 compatible compiler (g++ or clang)
- CMake build system
- Qt framework (Qt6 Core, Widgets, and GUI)
- Linux system libraries
-  Access to /proc filesystem

The system was developed and tested in both a
native Linux environment and a virtualized environment using UTM. Operating
systems tested included Ubuntu 22.04, 24.04, 25.10, and 26.04. Testing was
performed both through Qt Creator IDE and directly through terminal execution
of the compiled binary. 


## FAQ
### Why do I receive an error when trying to change process priority?

Increasing process
priority (lowering the nice value) requires root permissions. If you are
running as a normal user, this operation may fail. To run the application with
root permissions, exit the application and run the following command in terminal:
```
cd ~/ProcessPartyDeploy
sudo ./run-process-party.sh
```
Enter your password to give the application root permissions. The
     application should reopen with the ability to change priority.

</br>

### Why did my application freeze while using the color picker?

This is a known Qt
layering issue: when accidentally clicking on another area of the screen, the
color picker gets moved behind the open window and the application appears to
freeze. Switching focus back to the color picker from the taskbar resolves the
issue.

</br>

### What does this warning mean?
‘Warning: Ignoring WAYLAND_DISPLAY on Gnome. </br> Use QT_QPA_PLATFORM=wayland to run on Wayland anyway.’

This is a normal Qt
message and can be safely ignored. It simply indicates that the application is
running under X11 compatibility on a wayland desktop (the usual for many Ubuntu
machines). The X11 configuration provides the best compatibility for features
like the color picker.

</br>

### Why are some process names truncated?

Some process names
come from /proc/[pid]/stat, which limits names to ~15 characters. The system
attempts to use longer command-line names when available.


