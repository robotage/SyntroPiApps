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

#ifndef SYNTROPINAVCONSOLE_H
#define SYNTROPINAVCONSOLE_H

#include <QThread>

class NavClient;
class IMUThread;

class SyntroPiNavConsole : public QThread
{
	Q_OBJECT

public:
    SyntroPiNavConsole(bool daemonMode, QObject *parent);

public slots:
    void aboutToQuit();

protected:
	void run();
	void timerEvent(QTimerEvent *event);

private:
	void showHelp();
	void showStatus();

	void runConsole();
	void runDaemon();

	void registerSigHandler();
	static void sigHandler(int sig);

    NavClient *m_client;
    IMUThread *m_imuThread;
    int m_rateTimer;
	bool m_daemonMode;
	static volatile bool sigIntReceived;

};

#endif // SYNTROPINAVCONSOLE_H

