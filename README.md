# tmsi
Libusb driver for TMSI porti

The developed driver is based on the usage of the libusb-1.0.
Using this lib it is possible to run the tmsi/porti device under
linux 64bit (14.04) or even a armhf architecture like the beaglebone black.

The example implementation in main.cpp shows how the class and driver should be used.
There will be a lot of text printed during the usage of the libusb which can be reduced my changing the class implementation and reducing the verbosity level of the libusb.

## Installation
To use the drive the libusb-1.0 is needed. Furthermore the fxload tool is needed to flash the ftid chip with fuss.hex data.

For ubuntu or debian on beagle bone black this can be done with:

sudo apt-get install libusb-1.0-0
sudo apt-get install libusb-1.0-0-dev
sudo apt-get install fxload

##Working principle
The tmsiDevice opens up a thread with realtime priority. So you will have to run the executable with sudo rights. If you do not want this you have to change the priority of the thread inside open function. Furthermore you would have to get access rights for usb device using a udev-rule.

##MATLAB/Simulink
Using the build.m script a Block for ert_linux can be build which after creating of a Simulink Block
using the generated .tlc and mex files can be used in external mode. This was tested with up to 100 Hz on a standard laptop or even a beaglebone black.
![alt tag](https://github.com/wiesener/tmsi/blob/master/tmsiSimulink.png)
