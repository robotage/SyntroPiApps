# SyntroPiCam

SyntroPiCam is a Raspberry Pi/Raspbian MJPEG streaming application for use with the SyntroNet system.

Check out www.richards-tech.com and search for SyntroPiCam to find more instructions and advice.

### Dependencies

1. Qt4 or Qt5 development libraries and headers
2. SyntroCore libraries and headers 
3. The Raspberry Pi userland repo

The system dependencies can be satisfied by executing:

	sudo apt-get install cmake libqt4-dev qtcreator libasound2

### Fetch

	git clone https://github.com/raspberrypi/userland.git
        git clone git://github.com/richards-tech/SyntroPiCam.git


### Build 

The Raspberry Pi userland repo must be built and installed. Use the buildme script in the repo to do this
before building SyntroPiCam. Then, change to the SyntroPiCam directory and:

        qmake 
        make 
        sudo make install

### Run

#### GUI mode

        SyntroPiCam

Note that the frame rate is limited to around 6fps at the moment. This is being investigated.

The stream can be viewed with one or more instances of the SyntroView app - see www.richards-tech.com for more details. SyntroView is supported on many platforms including Windows, Mac OS X, Ubuntu and (soon) Android.

#### Console mode

Use this mode when you have no display on the local system or in the
typical case where you only want to watch the streamed video on a 
remote system using SyntroView or another SyntroNet app.

        SyntroPiCam -c


Enter h to get some help.


#### Daemon mode

This runs SyntroPiCam as a background process with no console interaction.

        SyntroPiCam -c -d

This only works in conjunction with console mode.


### Configuration

SyntroPiCam writes a configuration file to the ~/.config/Syntro directory, 
so if you run it from your home directory like this

        ~$ SyntroPiCam &

Then you will get a file called ~/.config/Syntro/SyntroPiCam.ini like this created

        scott@hex:~$ cat  SyntroPiCam.ini

		[General]
		appName=hex
		appType=SyntroPiCam
		componentType=SyntroPiCam
		controlRevert=0
		heartbeatInterval=1
		heartbeatTimeout=5
		localControl=0
		localControlPriority=0
		runtimeAdapter=
		runtimePath=

		[Camera]
		device=0
		format=MJPG
		frameRate=30
		height=360
		streamName=video
		width=640

		[Logging]
		diskLog=true
		logKeep=5
		logLevel=info
		netLog=true

        ....

This can be edited or else modified via the dialogs in GUI mode.