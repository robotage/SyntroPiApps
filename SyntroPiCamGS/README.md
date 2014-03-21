# SyntroPiCamGS

SyntroPiCamGS is a Raspberry Pi/Raspbian H.264/MP4A streaming application for use with the SyntroNet system.

Check out www.richards-tech.com and search for SyntroPiCamGS to find more instructions and advice.

### Dependencies

1. Qt4 or Qt5 development libraries and headers
2. SyntroCore libraries and headers (see the SyntroCore repo for more details)
3. The Raspberry Pi userland repo
4. gstreamer and plugins

The system dependencies can be satisfied by executing:

	sudo apt-get install cmake libqt4-dev qtcreator libasound2
	sudo apt-get install libgstreamer0.10-dev libgstreamer-plugins-base0.10-dev
	sudo apt-get install libgstreamer-plugins-bad0.10-dev gstreamer0.10-plugins-base
	sudo apt-get install gstreamer0.10-plugins-good gstreamer0.10-plugins-bad

### Fetch

	git clone https://github.com/raspberrypi/userland.git
        git clone git://github.com/richards-tech/SyntroPiCamGS.git


### Build 

The Raspberry Pi userland repo must be built and installed. Use the buildme script in the repo to do this
before building SyntroPiCamGS. Then, change to the SyntroPiCamGS directory and:

        qmake 
        make 
        sudo make install

### Run

SyntroPiCamGS is a console-mode only program. So, enter the following at a command prompt:

	SyntroPiCamGS

The first time this is run, it will default to 640 x 360, 10fps, 1Mbps max video rate, 64k bps max audio rate with the audio set to 8000 samples per second, 16 bits per sample and 2 channels. It will create a default config file, ~/.config/Syntro/SyntroPiCamGS.ini.

The can be edited with any editor (e.g. nano). The important settings are fairly obvious apart from the frame size options. Valid frame size codes are:

	0 = 640 x 360
	1 = 640 x 480
	2 = 1280 x 720
	3 = 1920 x 1080

Anything else will result in 640 x 360 being used.


#### Daemon mode

This runs SyntroPiCamGS as a background process with no console interaction.

        SyntroPiCamGS -c -d

This only works in conjunction with console mode.


