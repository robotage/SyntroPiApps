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

#ifndef SYNTROPICAMCONSOLE_H
#define SYNTROPICAMCONSOLE_H

#include <QThread>

class CamClient;
class VideoDriver;
class AudioDriver;

class SyntroPiCamConsole : public QThread
{
	Q_OBJECT

public:
    SyntroPiCamConsole(bool daemonMode, QObject *parent);

public slots:
	void cameraState(QString);
	void aboutToQuit();
    void videoFormat(int width, int height, int frameRate);
    void audioState(QString);

protected:
	void run();
	void timerEvent(QTimerEvent *event);

private:
	bool createCamera();
	bool startVideo();
	void stopVideo();
    void startAudio();
    void stopAudio();
	void showHelp();
	void showStatus();

	void runConsole();
	void runDaemon();

	void registerSigHandler();
	static void sigHandler(int sig);

	CamClient *m_client;
    VideoDriver *m_camera;
    AudioDriver *m_audio;

    QString m_cameraState;
    QString m_audioState;
    int m_frameRateTimer;
    double m_videoByteRate;
    double m_audioByteRate;
	bool m_daemonMode;
	static volatile bool sigIntReceived;

    int m_width;
    int m_height;
    int m_framerate;

};

#endif // SYNTROPICAMCONSOLE_H

