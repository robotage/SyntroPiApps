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

#include "SyntroPiCam.h"
#include "VideoDriver.h"
#include "AudioDriver.h"
#include "CamClient.h"
#include <QMessageBox>

#define RATE_TIMER_INTERVAL 2

SyntroPiCam::SyntroPiCam()
    : QMainWindow()
{
	m_camera = NULL;
    m_audio = NULL;

    SyntroUtils::syntroAppInit();

    m_client = new CamClient(this);
    m_client->resumeThread();
    startVideo();
    startAudio();
}

SyntroPiCam::~SyntroPiCam()
{
	if (m_camera) {
		delete m_camera;
		m_camera = NULL;
	}
}

void SyntroPiCam::closeEvent(QCloseEvent *)
{
	stopVideo();
    stopAudio();

    m_client->exitThread();

    SyntroUtils::syntroAppExit();
}

bool SyntroPiCam::createCamera()
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

void SyntroPiCam::startVideo()
{
	if (!m_camera) {
		if (!createCamera()) {
            QMessageBox::warning(this, "SyntroPiCam", "Error allocating camera", QMessageBox::Ok);
			return;
		}
	}

    connect(m_camera, SIGNAL(newVideo(QByteArray)), m_client, SLOT(newVideo(QByteArray)), Qt::DirectConnection);
    connect(m_camera, SIGNAL(videoFormat(int,int,int)), m_client, SLOT(videoFormat(int,int,int)));

    m_camera->resumeThread();
}

void SyntroPiCam::stopVideo()
{
	if (m_camera) {
        disconnect(m_camera, SIGNAL(newVideo(QByteArray)), m_client, SLOT(newVideo(QByteArray)));
        disconnect(m_camera, SIGNAL(videoFormat(int,int,int)), m_client, SLOT(videoFormat(int,int,int)));

        m_camera->exitThread();
		m_camera = NULL;
	}
}

void SyntroPiCam::startAudio()
{
    if (!m_audio) {
        m_audio = new AudioDriver();
        connect(m_audio, SIGNAL(newAudio(QByteArray)), m_client, SLOT(newAudio(QByteArray)), Qt::DirectConnection);
        connect(m_audio, SIGNAL(audioFormat(int, int, int)), m_client, SLOT(audioFormat(int, int, int)), Qt::QueuedConnection);
       m_audio->resumeThread();
    }
}

void SyntroPiCam::stopAudio()
{
    if (m_audio) {
        disconnect(m_audio, SIGNAL(newAudio(QByteArray)), m_client, SLOT(newAudio(QByteArray)));
        disconnect(m_audio, SIGNAL(audioFormat(int, int, int)), m_client, SLOT(audioFormat(int, int, int)));
        m_audio->exitThread();
        m_audio = NULL;
    }
}
