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

#ifndef VIDEODRIVER_H
#define VIDEODRIVER_H

#include "SyntroLib.h"
#include <QSize>
#include <QSettings>
#include <qimage.h>

class VideoDriver : public SyntroThread
{
	Q_OBJECT

public:
	VideoDriver();
	virtual ~VideoDriver();

	bool deviceExists();
	bool isDeviceOpen();

	QSize getImageSize();

public slots:
	void newCamera();

signals:
	void videoFormat(int width, int height, int frameRate);
	void newJPEG(QByteArray);
	void newFrame();
	void cameraState(QString state);

protected:
	void initThread();
	void finishThread();
	void timerEvent(QTimerEvent *event);

private:
	void loadSettings();
    void closeDevice();

	int m_width;
	int m_height;
    qreal m_frameRate;

	int m_timer;
    bool m_deviceOpen;

    bool m_captureInProgress;
};

#endif // VIDEODRIVER_H

