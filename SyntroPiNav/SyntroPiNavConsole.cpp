//
//  Copyright (c) 2014 richards-tech.
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

#include "SyntroPiNavConsole.h"
#include "SyntroPiNav.h"
#include "NavClient.h"

#include "IMUThread.h"
#include "RTKalman.h"
#include "RTIMUMPU9150.h"

#include <termios.h>
#include <unistd.h>
#include <signal.h>

#define RATE_TIMER_INTERVAL 2

volatile bool SyntroPiNavConsole::sigIntReceived = false;

SyntroPiNavConsole::SyntroPiNavConsole(bool daemonMode, QObject *parent)
	: QThread(parent)
{
	m_daemonMode = daemonMode;

    m_rateTimer = 0;
	m_client = NULL;

	if (m_daemonMode) {
		registerSigHandler();
		
		if (daemon(1, 1)) {
			perror("daemon");
			return;
		}
	}

	connect((QCoreApplication *)parent, SIGNAL(aboutToQuit()), this, SLOT(aboutToQuit()));

    SyntroUtils::syntroAppInit();

    m_imuThread = new IMUThread();
    m_imuThread->resumeThread();

    m_client = new NavClient(this);

    connect(m_imuThread,
            SIGNAL(newIMUData(const RTVector3&, const RTQuaternion&, const RTVector3&,
                              const RTVector3&, const RTVector3&, const uint64_t)),
            m_client,
            SLOT(newIMUData(const RTVector3&, const RTQuaternion&, const RTVector3&,
                            const RTVector3&, const RTVector3&, const uint64_t)),
            Qt::DirectConnection);

	m_client->resumeThread();

    if (!m_daemonMode) {
        m_rateTimer = startTimer(RATE_TIMER_INTERVAL * 1000);
    }

	start();
}

void SyntroPiNavConsole::aboutToQuit()
{
    if (m_rateTimer) {
        killTimer(m_rateTimer);
        m_rateTimer = 0;
    }

    for (int i = 0; i < 2; i++) {
		if (wait(1000))
			break;

		if (!m_daemonMode)
			printf("Waiting for console thread to finish...\n");
	}
}

void SyntroPiNavConsole::timerEvent(QTimerEvent *)
{
}

void SyntroPiNavConsole::showHelp()
{
	printf("\nOptions are:\n\n");
	printf("  h - Show help\n");
	printf("  s - Show status\n");
	printf("  x - Exit\n");
}

void SyntroPiNavConsole::showStatus()
{    
	printf("\nStatus: %s\n", qPrintable(m_client->getLinkState()));

}

void SyntroPiNavConsole::run()
{
	if (m_daemonMode)
		runDaemon();
	else
		runConsole();

    m_imuThread->exitThread();
    m_client->exitThread();
    SyntroUtils::syntroAppExit();
	QCoreApplication::exit();
}

void SyntroPiNavConsole::runConsole()
{
	struct termios	ctty;

	tcgetattr(fileno(stdout), &ctty);
	ctty.c_lflag &= ~(ICANON);
	tcsetattr(fileno(stdout), TCSANOW, &ctty);

    bool running = true;

    while (running) {
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
            running = false;
			break;

		case '\n':
			continue;
		}
	}
}

void SyntroPiNavConsole::runDaemon()
{
    while (!SyntroPiNavConsole::sigIntReceived)
		msleep(100); 
}

void SyntroPiNavConsole::registerSigHandler()
{
	struct sigaction sia;

	bzero(&sia, sizeof sia);
    sia.sa_handler = SyntroPiNavConsole::sigHandler;

	if (sigaction(SIGINT, &sia, NULL) < 0)
		perror("sigaction(SIGINT)");
}

void SyntroPiNavConsole::sigHandler(int)
{
    SyntroPiNavConsole::sigIntReceived = true;
}
