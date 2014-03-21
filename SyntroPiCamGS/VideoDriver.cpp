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

static VideoDriver *theDriver;

extern "C" void newCompressedVideoSegment(unsigned char *data, int length)
{
    theDriver->newCompressedDataSegment(data, length);
}

VideoDriver::VideoDriver() : SyntroThread("VideoDriver", "SyntroPiCam")
{
    theDriver = this;
	m_width = DEFAULT_WIDTH;
	m_height = DEFAULT_HEIGHT;
	m_frameRate = DEFAULT_RATE;
    m_deviceOpen = false;
 }

VideoDriver::~VideoDriver()
{

}

void VideoDriver::newCompressedDataSegment(unsigned char *data, int length)
{
    emit newVideo(QByteArray((const char *)data, length));
}

void VideoDriver::loadSettings()
{
	QSettings *settings = SyntroUtils::getSettings();

	settings->beginGroup(CAMERA_GROUP);

    if (!settings->contains(CAMERA_FRAMESIZE))
        settings->setValue(CAMERA_FRAMESIZE, CAMERA_FRAMESIZE_640x360);

	if (!settings->contains(CAMERA_FRAMERATE))
		settings->setValue(CAMERA_FRAMERATE, DEFAULT_RATE);

    switch (settings->value(CAMERA_FRAMESIZE).toInt()) {
    case CAMERA_FRAMESIZE_640x480:
        m_width = 640;
        m_height = 480;
        break;

    case CAMERA_FRAMESIZE_1280x720:
        m_width = 1280;
        m_height = 720;
         break;

    case CAMERA_FRAMESIZE_1920x1080:
        m_width = 1920;
        m_height = 1080;
         break;

    default:
        m_width = 640;
        m_height = 360;
        break;

   }

    m_frameRate = settings->value(CAMERA_FRAMERATE).toInt();
    if (m_frameRate <= 0)
        m_frameRate = DEFAULT_RATE;
    if (m_frameRate > MAXIMUM_RATE)
        m_frameRate = MAXIMUM_RATE;

	settings->endGroup();

    settings->beginGroup(CAMCLIENT_STREAM_GROUP);

    m_compressedVideoRate = settings->value(CAMCLIENT_GS_VIDEO_RATE).toInt();

    settings->endGroup();


	delete settings;
}


void VideoDriver::initThread()
{
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

    if (raspiInit(m_width, m_height, m_frameRate, m_compressedVideoRate) == 0) {
        m_deviceOpen = true;
        emit cameraState("Running");
        emit videoFormat(m_width, m_height, m_frameRate);
        raspiStartCapture();
    } else {
        m_deviceOpen = false;
    }

}

void VideoDriver::closeDevice()
{
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

