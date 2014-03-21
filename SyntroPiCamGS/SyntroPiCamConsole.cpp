//
//  Copyright (c) 2014 Scott Ellis and Richard Barnett.
//
//  This file is part of SyntroNet
//
//  SyntroNet is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  SyntroNet is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with SyntroNet.  If not, see <http://www.gnu.org/licenses/>.
//

#include "SyntroPiCamConsole.h"
#include "SyntroPiCam.h"

#include <termios.h>
#include <unistd.h>
#include <signal.h>

#include "VideoDriver.h"
#include "AudioDriver.h"
#include "CamClient.h"

#define RATE_TIMER_INTERVAL 2

volatile bool SyntroPiCamConsole::sigIntReceived = false;

SyntroPiCamConsole::SyntroPiCamConsole(bool daemonMode, QObject *parent)
	: QThread(parent)
{
	m_daemonMode = daemonMode;

    m_videoByteRate = 0;
    m_audioByteRate = 0;
	m_frameRateTimer = 0;
	m_camera = NULL;
    m_audio = NULL;

	m_client = NULL;

    m_width = 0;
    m_height = 0;
    m_framerate = 0;

	if (m_daemonMode) {
		registerSigHandler();
		
		if (daemon(1, 1)) {
			perror("daemon");
			return;
		}
	}

	connect((QCoreApplication *)parent, SIGNAL(aboutToQuit()), this, SLOT(aboutToQuit()));

    SyntroUtils::syntroAppInit();

    m_client = new CamClient(this);
	m_client->resumeThread();

    if (!m_daemonMode) {
        m_frameRateTimer = startTimer(RATE_TIMER_INTERVAL * 1000);
    }

	start();
}

void SyntroPiCamConsole::aboutToQuit()
{
    if (m_frameRateTimer) {
        killTimer(m_frameRateTimer);
        m_frameRateTimer = 0;
    }

	for (int i = 0; i < 5; i++) {
		if (wait(1000))
			break;

		if (!m_daemonMode)
			printf("Waiting for console thread to finish...\n");
	}
}

bool SyntroPiCamConsole::createCamera()
{
	if (m_camera) {
		delete m_camera;
		m_camera = NULL;
	}

    m_camera = new VideoDriver();

	if (!m_camera)
		return false;

	return true;
}

bool SyntroPiCamConsole::startVideo()
{
	if (!m_camera) {
		if (!createCamera()) {
            appLogError("Error allocating camera");
			return false;
		}
	}

	if (!m_daemonMode) {
		connect(m_camera, SIGNAL(cameraState(QString)), this, SLOT(cameraState(QString)), Qt::DirectConnection);
    }
    connect(m_camera, SIGNAL(newVideo(QByteArray)), m_client, SLOT(newVideo(QByteArray)), Qt::DirectConnection);
    connect(m_camera, SIGNAL(videoFormat(int,int,int)), this, SLOT(videoFormat(int,int,int)));
    connect(m_camera, SIGNAL(videoFormat(int,int,int)), m_client, SLOT(videoFormat(int,int,int)));

    m_camera->resumeThread();

	return true;
}

void SyntroPiCamConsole::stopVideo()
{
    if (m_camera) {
        if (!m_daemonMode) {
            disconnect(m_camera, SIGNAL(cameraState(QString)), this, SLOT(cameraState(QString)));
        }
        disconnect(m_camera, SIGNAL(newVideo(QByteArray)), m_client, SLOT(newVideo(QByteArray)));
        disconnect(m_camera, SIGNAL(videoFormat(int,int,int)), this, SLOT(videoFormat(int,int,int)));
        disconnect(m_camera, SIGNAL(videoFormat(int,int,int)), m_client, SLOT(videoFormat(int,int,int)));

        m_camera->exitThread();
        m_camera = NULL;
    }
}

void SyntroPiCamConsole::startAudio()
{
    m_audio = new AudioDriver();
    connect(m_audio, SIGNAL(newAudio(QByteArray)), m_client, SLOT(newAudio(QByteArray)), Qt::DirectConnection);
    connect(m_audio, SIGNAL(audioFormat(int, int, int)), m_client, SLOT(audioFormat(int, int, int)), Qt::QueuedConnection);
    connect(m_audio, SIGNAL(audioState(QString)), this, SLOT(audioState(QString)), Qt::DirectConnection);
    m_audio->resumeThread();
}

void SyntroPiCamConsole::stopAudio()
{
    disconnect(m_audio, SIGNAL(newAudio(QByteArray)), m_client, SLOT(newAudio(QByteArray)));
    disconnect(m_audio, SIGNAL(audioFormat(int, int, int)), m_client, SLOT(audioFormat(int, int, int)));
    connect(m_audio, SIGNAL(audioState(QString)), this, SLOT(audioState(QString)));

    m_audio->exitThread();
    m_audio = NULL;
}

void SyntroPiCamConsole::videoFormat(int width, int height, int framerate)
{
    m_width = width;
    m_height = height;
    m_framerate = framerate;
}

void SyntroPiCamConsole::cameraState(QString state)
{
    m_cameraState = state;
}

void SyntroPiCamConsole::audioState(QString state)
{
    m_audioState = state;
}

void SyntroPiCamConsole::timerEvent(QTimerEvent *)
{
    m_videoByteRate = (double)m_client->getVideoByteCount() / (double)RATE_TIMER_INTERVAL;
    m_audioByteRate = (double)m_client->getAudioByteCount() / (double)RATE_TIMER_INTERVAL;
}

void SyntroPiCamConsole::showHelp()
{
	printf("\nOptions are:\n\n");
	printf("  h - Show help\n");
	printf("  s - Show status\n");
	printf("  x - Exit\n");
}

void SyntroPiCamConsole::showStatus()
{    
	printf("\nStatus: %s\n", qPrintable(m_client->getLinkState()));

    if (m_cameraState == "Running") {
        printf("Video byte rate : %f bytes per second\n", m_videoByteRate);
        printf("Audio byte rate : %f bytes per second\n", m_audioByteRate);
    } else {
        printf("Camera state    : %s\n", qPrintable(m_cameraState));
    }
    printf("Frame size is   : %d x %d\n", m_width, m_height);
    printf("Frame rate is   : %d\n", m_framerate);
    printf("Audio state is  : %s\n", qPrintable(m_audioState));
}

void SyntroPiCamConsole::run()
{
	if (m_daemonMode)
		runDaemon();
	else
		runConsole();

    stopVideo();
    stopAudio();

    m_client->exitThread();
    SyntroUtils::syntroAppExit();
	QCoreApplication::exit();
}

void SyntroPiCamConsole::runConsole()
{
	struct termios	ctty;

	tcgetattr(fileno(stdout), &ctty);
	ctty.c_lflag &= ~(ICANON);
	tcsetattr(fileno(stdout), TCSANOW, &ctty);

	bool grabbing = startVideo();
    startAudio();

	while (grabbing) {
		printf("\nEnter option: ");

        switch (tolower(getchar()))	
		{
		case 'h':
			showHelp();
			break;

		case 's':
			showStatus();
			break;

		case 'x':
			printf("\nExiting\n");
			grabbing = false;		
			break;

		case '\n':
			continue;
		}
	}
}

void SyntroPiCamConsole::runDaemon()
{
	startVideo();
    startAudio();

    while (!SyntroPiCamConsole::sigIntReceived)
		msleep(100); 
}

void SyntroPiCamConsole::registerSigHandler()
{
	struct sigaction sia;

	bzero(&sia, sizeof sia);
    sia.sa_handler = SyntroPiCamConsole::sigHandler;

	if (sigaction(SIGINT, &sia, NULL) < 0)
		perror("sigaction(SIGINT)");
}

void SyntroPiCamConsole::sigHandler(int)
{
    SyntroPiCamConsole::sigIntReceived = true;
}
