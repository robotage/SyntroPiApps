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

#ifndef CAMCLIENT_H
#define CAMCLIENT_H

#include <qmutex.h>

#include "SyntroLib.h"


//  Settings keys

//----------------------------------------------------------
//	Stream group

//	Camera group

//		Settings keys

#define CAMERA_GROUP					 "CameraGroup"

#define	CAMERA_FRAMESIZE				 "FrameSize"
#define	CAMERA_FRAMERATE				 "FrameRate"

#define CAMERA_FRAMESIZE_640x360            0
#define CAMERA_FRAMESIZE_640x480            1
#define CAMERA_FRAMESIZE_1280x720           2
#define CAMERA_FRAMESIZE_1920x1080          3


// group name for stream-related entries

#define	CAMCLIENT_STREAM_GROUP            "StreamGroup"

#define CAMCLIENT_GS_VIDEO_RATE           "GSVideoRate"
#define CAMCLIENT_GS_AUDIO_RATE           "GSAudioRate"

#define CAMCLIENT_CAPS_INTERVAL           5000              // interval between caps sends


#define	CAMERA_IMAGE_INTERVAL	((qint64)SYNTRO_CLOCKS_PER_SEC/40)


typedef struct
{
    QByteArray data;                                        // the data
    qint64 timestamp;                                       // the timestamp
} CLIENT_QUEUEDATA;

class AVMuxEncode;

class CamClient : public Endpoint
{
	Q_OBJECT

public:
    CamClient(QObject *parent);
    virtual ~CamClient();
    int getVideoByteCount();
    int getAudioByteCount();

public slots:
	void newStream();
    void newVideo(QByteArray);
    void newAudio(QByteArray);
    void videoFormat(int width, int height, int framerate);
    void audioFormat(int sampleRate, int channels, int sampleSize);

protected:
	void appClientInit();
	void appClientExit();
	void appClientConnected();								// called when endpoint is connected to SyntroControl
	void appClientBackground();
     int m_avmuxPort;                                        // the local port assigned to the avmux service

private:
    void processAVQueue();                                  // processes the compressed video and audio data

    bool dequeueVideoFrame(QByteArray& videoData, qint64& timestamp);
    bool dequeueAudioFrame(QByteArray& audioData, qint64& timestamp);

    int m_videoByteCount;
    QMutex m_videoByteCountLock;

    int m_audioByteCount;
    QMutex m_audioByteCountLock;

    int m_recordIndex;                                      // increments for every avmux record constructed

    SYNTRO_AVPARAMS m_avParams;                             // used to hold stream parameters

    bool m_gotVideoFormat;
    bool m_gotAudioFormat;

    int m_compressedVideoRate;
    int m_compressedAudioRate;

    AVMuxEncode *m_encoder;

    void establishPipelines();
    void sendCaps();

    qint64 m_lastCapsSend;

};

#endif // CAMCLIENT_H

