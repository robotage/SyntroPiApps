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

#ifndef SYNTROPICAM_H
#define SYNTROPICAM_H

#include <QMainWindow>

#define PRODUCT_TYPE "SyntroPiCamGS"

class CamClient;
class VideoDriver;
class AudioDriver;


class SyntroPiCam : public QMainWindow
{
	Q_OBJECT

public:
    SyntroPiCam();
    ~SyntroPiCam();

protected:
	void closeEvent(QCloseEvent *event);

private:
	void startVideo();
	void stopVideo();
    void startAudio();
    void stopAudio();
	bool createCamera();

	CamClient *m_client;
    VideoDriver *m_camera;
    AudioDriver *m_audio;
	QString m_cameraState;
	QString m_audioState;
};

#endif // SYNTROPICAM_H

