X52ProMac
=========

Some toys to work with the Saitek X52 Pro on OS X. At present, there's not a lot here, but assuming I don't get distracted it should grow.

Quick Instructions
------------------

```
./build
sudo ./install
```

X52ProDaemon
------------

This is a simple daemon to keep the date & time up to date on the device. It'll update them every second, which is more than enough for our simple purposes.

Preferences
-----------

You can toggle these with the *defaults* command:
```
sudo defaults write /Library/Preferences/org.infernus.X52ProDaemon DateFormat -string ddmmyy
```

* **DateFormat** - *string* value of ddmmyy, mmddyy or yymmdd

To Do
-----

* Allow time format to be set via a properties file
* Allow MFD/LED light levels to be set via a properties file
* Write a control panel for the above


