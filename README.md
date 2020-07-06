# ESP8266-XR-Console-Remote

Code From this article : https://electronicfields.wordpress.com/2020/01/28/controlling-a-behringer-x-air-console-with-a-wemos-d1/

The Midas MR / Behringer XR digital mixing consoles are cheap and powerful. With no tangible control surface doing quick and current actions can be inconvenient.

A Wemos D1 with its onboard ESP8266 and four PB-86 push-buttons (I like their vintage-looking style) are all you need to control your console on your WiFi network.

After having fun with C# to control my XR12 console on a Windows computer, I decided to make a physical interface to do basic monitoring stuff : muting, mono listening, enabling a compressor as a leveler, etc.

I chose the Arduino environment to program my Wemos D1 using the CNMAT OSC Arduino library developed by Yotam Mann and Adrian Freed.

Be careful, some GPIOs on a ESP-12e/Wemos D1 board must match some defined states to be able to boot up.
