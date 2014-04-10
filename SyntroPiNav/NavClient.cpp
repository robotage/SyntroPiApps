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

#include "SyntroLib.h"
#include "NavClient.h"
#include "SyntroNavDefs.h"

#include <qdebug.h>

NavClient::NavClient(QObject *)
    : Endpoint(NAVCLIENT_BACKGROUND_INTERVAL, "NavClient")
{
    m_servicePort = -1;
}

NavClient::~NavClient()
{
}


void NavClient::appClientInit()
{
    newStream();
}

void NavClient::appClientExit()
{

}

void NavClient::appClientBackground()
{
    QMutexLocker locker(&m_navLock);
    SYNTRO_NAVDATA data;
    RTIMU_DATA localData;
    int recordCount;
    int totalLength;
    int validFields;

    if (m_imuData.empty())
        return;

    if (m_servicePort == -1)
        return;

    if (!clientIsServiceActive(m_servicePort) || !clientClearToSend(m_servicePort))
        return;                                             // can't send for network reasons

    recordCount = m_imuData.count();
    totalLength = sizeof(SYNTRO_NAVDATA) * recordCount;

    SYNTRO_EHEAD *multiCast = clientBuildMessage(m_servicePort, sizeof(SYNTRO_RECORD_HEADER) + totalLength);
    SYNTRO_RECORD_HEADER *head = (SYNTRO_RECORD_HEADER *)(multiCast + 1);
    SyntroUtils::convertIntToUC2(SYNTRO_RECORD_TYPE_NAV, head->type);
    SyntroUtils::convertIntToUC2(SYNTRO_RECORD_TYPE_NAV_IMU, head->subType);
    SyntroUtils::convertIntToUC2(sizeof(SYNTRO_RECORD_HEADER), head->headerLength);
    SyntroUtils::convertInt64ToUC8(SyntroClock(), head->timestamp);

    for (int record = 0; record < recordCount; record++) {

        localData = m_imuData.dequeue();

        validFields = 0;

        if (localData.fusionPoseValid)
            validFields |= SYNTRO_NAVDATA_VALID_FUSIONPOSE;
        if (localData.fusionQPoseValid)
            validFields |= SYNTRO_NAVDATA_VALID_FUSIONQPOSE;
        if (localData.gyroValid)
            validFields |= SYNTRO_NAVDATA_VALID_GYRO;
        if (localData.accelValid)
            validFields |= SYNTRO_NAVDATA_VALID_ACCEL;
        if (localData.compassValid)
            validFields |= SYNTRO_NAVDATA_VALID_COMPASS;
        if (localData.compassValid)
            validFields |= SYNTRO_NAVDATA_VALID_PRESSURE;
        if (localData.compassValid)
            validFields |= SYNTRO_NAVDATA_VALID_TEMPERATURE;
        if (localData.compassValid)
            validFields |= SYNTRO_NAVDATA_VALID_HUMIDITY;

        SyntroUtils::convertIntToUC2(validFields, data.validFields);

        data.fusionPose[0] = localData.fusionPose.x();
        data.fusionPose[1] = localData.fusionPose.y();
        data.fusionPose[2] = localData.fusionPose.z();

        data.fusionQPose[0] = localData.fusionQPose.scalar();
        data.fusionQPose[1] = localData.fusionQPose.x();
        data.fusionQPose[2] = localData.fusionQPose.y();
        data.fusionQPose[3] = localData.fusionQPose.z();

        data.gyro[0] = localData.gyro.x();
        data.gyro[1] = localData.gyro.y();
        data.gyro[2] = localData.gyro.z();

        data.accel[0] = localData.accel.x();
        data.accel[1] = localData.accel.y();
        data.accel[2] = localData.accel.z();

        data.compass[0] = localData.compass.x();
        data.compass[1] = localData.compass.y();
        data.compass[2] = localData.compass.z();

        data.pressure = localData.pressure;
        data.temperature = localData.temperature;
        data.humidity = localData.humidity;

        SyntroUtils::convertInt64ToUC8(localData.timestamp, data.timestamp);

        memcpy(((SYNTRO_NAVDATA *)(head + 1)) + record, &data, sizeof(SYNTRO_NAVDATA));
    }
    clientSendMessage(m_servicePort, multiCast, sizeof(SYNTRO_RECORD_HEADER) + totalLength, SYNTROLINK_MEDPRI);
}

void NavClient::newIMUData(const RTIMU_DATA& data)
{
    QMutexLocker locker(&m_navLock);
    if ((m_servicePort == -1) || !clientIsServiceActive(m_servicePort)) {
        // can't send for network reasons so dump queue
        m_imuData.clear();
        return;
    }

    m_imuData.enqueue(data);

    if (m_imuData.count() > 50) {
        // stop queue getting stupidly big
        m_imuData.dequeue();
        qDebug() << "NavClient queue overflow";
    }
}


void NavClient::newStream()
{
    // remove the old streams

    if (m_servicePort != -1)
        clientRemoveService(m_servicePort);

    m_servicePort = clientAddService(SYNTRO_STREAMNAME_NAV, SERVICETYPE_MULTICAST, true);
}
