X52ProMac
=========

An in-progress effort to create basic tools to manage the Saitek X52 Pro under OS X.

Quick Instructions
------------------

Eventually I'll write an installation package, but in the meantime:

```
./build
sudo ./install
```

X52ProDaemon
------------

This will keep the date & time up to date on the device. It'll update them every second, which is more than enough for our simple purposes.

It will also set the brightness on the LEDs; the MFD; and the blink status of the clutch/hat buttons, and update these if the preferences are changed.

What This Won't Cover
---------------------

This is intended to cover the non-gameplay parts of the X52, so it won't ever cover button mappings or such. Use [ControllerMate](http://www.controllermate.com), it's great :-)

Preferences
-----------

You can toggle these with the *defaults* command:
```
sudo defaults write /Library/Preferences/org.infernus.X52ProDaemon DateFormat -string ddmmyy
```

* **DateFormat** - *string* value of ddmmyy (default), mmddyy or yymmdd
* **ClockType** - *string* value of 12 or 24 (default)
* **MFDBrightness** - **int** value of 0 - 128 (default)
* **LEDBrightness** - **int** value of 0 - 128 (default)
* **BlinkClutch** - **boolean** value of true / false (default)

To Do
-----

* Write a control panel for the above and to allow control of the various LEDs



