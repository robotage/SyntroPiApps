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

#include "VideoDriver.h"
#include "CamClient.h"

#include "RaspiDriver.h"

#define DEFAULT_WIDTH  640
#define DEFAULT_HEIGHT 360
#define MAXIMUM_RATE   30
#define DEFAULT_RATE   10

VideoDriver::VideoDriver() : SyntroThread("VideoDriver", "SyntroPiCam")
{
	m_width = DEFAULT_WIDTH;
	m_height = DEFAULT_HEIGHT;
	m_frameRate = DEFAULT_RATE;
    m_deviceOpen = false;
}

VideoDriver::~VideoDriver()
{

}

void VideoDriver::loadSettings()
{
	QSettings *settings = SyntroUtils::getSettings();

	settings->beginGroup(CAMERA_GROUP);

	if (!settings->contains(CAMERA_WIDTH))
		settings->setValue(CAMERA_WIDTH, DEFAULT_WIDTH);

	if (!settings->contains(CAMERA_HEIGHT))
		settings->setValue(CAMERA_HEIGHT, DEFAULT_HEIGHT);

	if (!settings->contains(CAMERA_FRAMERATE))
		settings->setValue(CAMERA_FRAMERATE, DEFAULT_RATE);

    m_width = settings->value(CAMERA_WIDTH).toInt();
    m_height = settings->value(CAMERA_HEIGHT).toInt();
    m_frameRate = settings->value(CAMERA_FRAMERATE).toInt();
    if (m_frameRate <= 0)
        m_frameRate = DEFAULT_RATE;
    if (m_frameRate > MAXIMUM_RATE)
        m_frameRate = MAXIMUM_RATE;

	settings->endGroup();

	delete settings;
}


void VideoDriver::initThread()
{
	m_timer = -1;
    newCamera();
}

void VideoDriver::finishThread()
{
	closeDevice();
}

void VideoDriver::newCamera()
{
	closeDevice();
	loadSettings();

    if (raspiInit(m_width, m_height) == 0) {
        m_deviceOpen = true;
        m_timer = startTimer(1000 / m_frameRate);
        emit cameraState("Running");
        emit videoFormat(m_width, m_height, m_frameRate);
    } else {
        m_deviceOpen = false;
    }

}

void VideoDriver::timerEvent(QTimerEvent *)
{
    int jpegLength;
    unsigned char *jpegBuffer;

    if (m_captureInProgress) {
        if (raspiFinishCapture() == 0) {
            raspiGetJpegBuffer(&jpegBuffer, &jpegLength);
            emit newJPEG(QByteArray((const char *)jpegBuffer, jpegLength));
            emit newFrame();
        }
        m_captureInProgress = false;
    }

    if (!m_captureInProgress) {
        if (raspiStartCapture() == 0)
            m_captureInProgress = true;
    }
}

void VideoDriver::closeDevice()
{
    m_captureInProgress = false;
    if (m_timer != -1)
        killTimer(m_timer);
	m_timer = -1;
    if (m_deviceOpen)
        raspiClose();
    m_deviceOpen = false;
    emit cameraState("Closed");
}

bool VideoDriver::isDeviceOpen()
{
    return m_deviceOpen;
}

QSize VideoDriver::getImageSize()
{
	return QSize(m_width, m_height);
}

