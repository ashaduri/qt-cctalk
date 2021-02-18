# Qt-ccTalk
ccTalk protocol implementation in modern C++ / Qt.

[ccTalk](https://en.wikipedia.org/wiki/CcTalk) is a serial protocol for communication with bill validators and coin acceptors.
The protocol is in widespread use throughout the money transaction and point-of-sale industry.

## Project State
Implementation is finished, but untested (I no longer have access to ccTalk devices).

**This project is up for adoption!**

## Description

This project provides:
- High-level, type-safe C++ API for ccTalk commands.
- ccTalk device management (including serial port device management).
- Controller and worker thread management for non-blocking communication with multiple ccTalk devices.
- A test GUI application for testing ccTalk devices, inspecting sent / received commands,
and providing a source code example for ccTalk library usage.

## Structure Overview

The code is thoroughly annotated using doxygen-style comments.
Below is an overview of main components of the project:

### Class `qtcc::SerialWorker`
An instance of this class lives in a worker thread and is managed by `qtcc::CctalkLinkController` controller.
Its main responsibility is to open / close a serial port device, send ccTalk request binary data
(as received by the controller object) to the device and pass the binary response back to the controller.

### Class `qtcc::CctalkLinkController`
This class creates and manages a worker thread with a `qtcc::SerialWorker` object in it.
In user thread, it can be used to manage the serial port device, send binary requests,
and receive binary responses from a `qtcc::SerialWorker` instance, which lives in a worker thread.

### Class `qtcc::CctalkDevice`
This class provides a type-safe, high-level ccTalk command API, translating the high-level API to
low-level binary ccTalk commands. An object of this class owns a
`qtcc::CctalkLinkController` instance and uses it for communication with ccTalk devices. 

### Classes `qtcc::BillValidatorDevice` and `qtcc::CoinAcceptorDevice`
These classes simply inherit `qtcc::CctalkDevice` to help you specify different behavior
for bill validators and coin acceptors in a type-safe way.

### Directory `test_gui`
The `test_gui` directory contains a GUI application to showcase the ccTalk API and its usage in a user application.
It can also serve as an API debugger and device tester, displaying all the sent and received commands
in a log window.

#### Functions `MainWindow::runSerialThreads()`, `setUpCctalkDevices()`
These functions show how to set up and use bill validator and/or coin acceptor devices within an application.

#### Configuration
The GUI uses .ini file for ccTalk device configuration. On Linux the file is located at
`~/.config/Qt-ccTalk/Qt-ccTalk GUI.ini`.
The function `setUpCctalkDevices()` lists the supported configuration keys.
An example configuration file may look like this:
```INI
[bill_validator]
serial_device_name="/dev/ttyUSB0"
[cctalk]
show_full_response=true
```

## Copyright

Copyright: Alexander Shaduri <ashaduri@gmail.com>   
License: 3-Clause BSD License
