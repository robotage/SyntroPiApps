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

#include "SyntroLib.h"
#include "CamClient.h"
#include "SyntroUtils.h"
#include "AVMuxEncodeGS.h"

#include <qdebug.h>


CamClient::CamClient(QObject *)
    : Endpoint(CAMERA_IMAGE_INTERVAL, "CamClient")
{
    m_avmuxPort = -1;
    m_recordIndex = 0;
    m_encoder = NULL;
    m_lastCapsSend = 0;
    m_videoByteCount = 0;
    m_audioByteCount = 0;


    QSettings *settings = SyntroUtils::getSettings();

    settings->beginGroup(CAMCLIENT_STREAM_GROUP);

    if (!settings->contains(CAMCLIENT_GS_VIDEO_RATE))
        settings->setValue(CAMCLIENT_GS_VIDEO_RATE, "1000000");

    if (!settings->contains(CAMCLIENT_GS_AUDIO_RATE))
        settings->setValue(CAMCLIENT_GS_AUDIO_RATE, "64000");

    settings->endGroup();

    delete settings;
}

CamClient::~CamClient()
{
}

void CamClient::appClientInit()
{
    newStream();
}

void CamClient::appClientExit()
{
    m_encoder->exitThread();
}

void CamClient::appClientBackground()
{
    processAVQueue();
}

void CamClient::appClientConnected()
{
}

void CamClient::newVideo(QByteArray data)
{
    if (m_encoder != NULL)
        m_encoder->newVideoData(data);
}

void CamClient::newAudio(QByteArray data)
{
    if (m_encoder != NULL)
        m_encoder->newAudioData(data);
}

int CamClient::getVideoByteCount()
{
    int count;

    QMutexLocker lock(&m_videoByteCountLock);

    count = m_videoByteCount;
    m_videoByteCount = 0;
    return count;
}

int CamClient::getAudioByteCount()
{
    int count;

    QMutexLocker lock(&m_audioByteCountLock);

    count = m_audioByteCount;
    m_audioByteCount = 0;
    return count;
}


void CamClient::processAVQueue()
{
    QByteArray data;

    int videoLength, audioLength;
    SYNTRO_UC4 lengthData;
    QByteArray videoArray;
    QByteArray audioArray;
    int totalLength;
    unsigned char *ptr;
    qint64 videoTimestamp;
    qint64 audioTimestamp;
    int videoParam;
    int audioParam;

    if (m_avmuxPort == -1)
        return;

    if (!clientIsServiceActive(m_avmuxPort)) {             // just discard encoder queue
        while (m_encoder->getCompressedVideo(data, videoTimestamp, videoParam))
            ;
        while (m_encoder->getCompressedAudio(data, audioTimestamp, audioParam))
            ;
        return;
    }

    if (!m_encoder->pipelinesActive())
        return;

    if (SyntroUtils::syntroTimerExpired(QDateTime::currentMSecsSinceEpoch(), m_lastCapsSend, CAMCLIENT_CAPS_INTERVAL)) {
        sendCaps();
        m_lastCapsSend = QDateTime::currentMSecsSinceEpoch();
    }

    if (clientClearToSend(m_avmuxPort)) {

        while (m_encoder->getCompressedVideo(data, videoTimestamp, videoParam)) {
            SyntroUtils::convertIntToUC4(data.length(), lengthData);
            videoArray.append((const char *)lengthData, 4);
            videoArray.append(data);
        }
        videoLength = videoArray.length();

        while (m_encoder->getCompressedAudio(data, audioTimestamp, audioParam)) {
            SyntroUtils::convertIntToUC4(data.length(), lengthData);
            audioArray.append((const char *)lengthData, 4);
            audioArray.append(data);
        }
        audioLength = audioArray.length();

        totalLength = videoLength + audioLength;

        if (totalLength > 0) {
            SYNTRO_EHEAD *multiCast = clientBuildMessage(m_avmuxPort, sizeof(SYNTRO_RECORD_AVMUX) + totalLength);
            SYNTRO_RECORD_AVMUX *avmux = (SYNTRO_RECORD_AVMUX *)(multiCast + 1);
            SyntroUtils::avmuxHeaderInit(avmux, &m_avParams, SYNTRO_RECORDHEADER_PARAM_NORMAL, m_recordIndex++, 0, videoLength, audioLength);

            if ((audioLength > 0) && (videoLength == 0)) {
                SyntroUtils::convertInt64ToUC8(audioTimestamp, avmux->recordHeader.timestamp);
            SyntroUtils::convertIntToUC2(audioParam, avmux->recordHeader.param);
            } else {
                SyntroUtils::convertInt64ToUC8(videoTimestamp, avmux->recordHeader.timestamp);
                SyntroUtils::convertIntToUC2(videoParam, avmux->recordHeader.param);
            }

            ptr = (unsigned char *)(avmux + 1);

            if (videoLength > 0) {
                memcpy(ptr, videoArray.constData(), videoLength);
                ptr += videoLength;
            }

            if (audioLength > 0) {
                memcpy(ptr, audioArray.constData(), audioLength);
                ptr += audioLength;
            }
            clientSendMessage(m_avmuxPort, multiCast, sizeof(SYNTRO_RECORD_AVMUX) + totalLength, SYNTROLINK_MEDPRI);

            m_videoByteCountLock.lock();
            m_videoByteCount += videoLength;
            m_videoByteCountLock.unlock();
            m_audioByteCountLock.lock();
            m_audioByteCount += audioLength;
            m_audioByteCountLock.unlock();

        }
    }
}


void CamClient::newStream()
{
	// remove the old streams
	
    if (m_avmuxPort != -1)
        clientRemoveService(m_avmuxPort);
    m_avmuxPort = -1;

    if (m_encoder != NULL)
        m_encoder->exitThread();
    m_encoder = NULL;

    // and start the new streams

    QSettings *settings = SyntroUtils::getSettings();

    settings->beginGroup(CAMCLIENT_STREAM_GROUP);

    m_compressedVideoRate = settings->value(CAMCLIENT_GS_VIDEO_RATE).toInt();
    m_compressedAudioRate = settings->value(CAMCLIENT_GS_AUDIO_RATE).toInt();
    m_avmuxPort = clientAddService(SYNTRO_STREAMNAME_AVMUX, SERVICETYPE_MULTICAST, true);

    settings->endGroup();

    m_gotAudioFormat = false;
    m_gotVideoFormat = false;

    delete settings;

    m_avParams.avmuxSubtype = SYNTRO_RECORD_TYPE_AVMUX_RTP;
    m_avParams.videoSubtype = SYNTRO_RECORD_TYPE_VIDEO_RTPH264;
    m_avParams.audioSubtype = SYNTRO_RECORD_TYPE_AUDIO_RTPAAC;
    m_encoder = new AVMuxEncode(0);
    m_encoder->resumeThread();
}

void CamClient::videoFormat(int width, int height, int framerate)
{
    m_avParams.videoWidth = width;
    m_avParams.videoHeight = height;
    m_avParams.videoFramerate = framerate;
    m_gotVideoFormat = true;
    establishPipelines();
}

void CamClient::audioFormat(int sampleRate, int channels, int sampleSize)
{
    m_avParams.audioSampleRate = sampleRate;
    m_avParams.audioChannels = channels;
    m_avParams.audioSampleSize = sampleSize;
    m_gotAudioFormat = true;
    establishPipelines();
}

void CamClient::establishPipelines()
{
    if (!m_gotVideoFormat || !m_gotAudioFormat)
        return;
    m_encoder->deletePipelines();
    m_encoder->setAudioCompressionRate(m_compressedAudioRate);
    m_encoder->newPipelines(&m_avParams);
}


void CamClient::sendCaps()
{
    if (!clientIsServiceActive(m_avmuxPort) || !clientClearToSend(m_avmuxPort))
        return;

    gchar *videoCaps = m_encoder->getVideoCaps();
    gchar *audioCaps = m_encoder->getAudioCaps();

    int videoLength = 0;
    int audioLength = 0;
    int totalLength = 0;

    if (videoCaps != NULL)
        videoLength = strlen(videoCaps) + 1;

    if (audioCaps != NULL)
        audioLength = strlen(audioCaps) + 1;

    totalLength = videoLength + audioLength;

    if (totalLength == 0)
        return;

    SYNTRO_EHEAD *multiCast = clientBuildMessage(m_avmuxPort, sizeof(SYNTRO_RECORD_AVMUX) + totalLength);
    SYNTRO_RECORD_AVMUX *avmux = (SYNTRO_RECORD_AVMUX *)(multiCast + 1);
    SyntroUtils::avmuxHeaderInit(avmux, &m_avParams, SYNTRO_RECORDHEADER_PARAM_NORMAL, m_recordIndex++, 0, videoLength, audioLength);
    avmux->videoSubtype = SYNTRO_RECORD_TYPE_VIDEO_RTPCAPS;
    avmux->audioSubtype = SYNTRO_RECORD_TYPE_AUDIO_RTPCAPS;

    unsigned char *ptr = (unsigned char *)(avmux + 1);

    if (videoLength > 0) {
        memcpy(ptr, videoCaps, videoLength);
        ptr += videoLength;
    }

    if (audioLength > 0) {
        memcpy(ptr, audioCaps, audioLength);
        ptr += audioLength;
    }

    clientSendMessage(m_avmuxPort, multiCast, sizeof(SYNTRO_RECORD_AVMUX) + totalLength, SYNTROLINK_MEDPRI);
 }


