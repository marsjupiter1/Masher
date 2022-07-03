# Masher
Mashing control for arduino and android

This is the start of an app that connected to an arduino that is wired to a couple of thermisters.
The idea being that a tuya plug switches a boiler on an off to keep the temperature in a range.
Currently it just allows the reading of the temperatures via a REST http interface.
Pretty useless, but having made my first android app, I want it under git control before I break it. 

The arduino app uses the repo:
https://github.com/marsjupiter1/TuyaAuth
to access and control the plug via the tuya cloud.

