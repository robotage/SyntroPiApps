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

}

NavClient::~NavClient()
{
}


void NavClient::appClientInit()
{
    m_newIMUData = false;
    m_servicePort = -1;
    newStream();
}

void NavClient::appClientExit()
{

}

void NavClient::appClientBackground()
{
    QMutexLocker locker(&m_navLock);
    SYNTRO_NAVDATA data;

    if (!m_newIMUData)
        return;

    if (!clientIsServiceActive(m_servicePort) || !clientClearToSend(m_servicePort))
        return;                                             // can't send for network reasons

    SYNTRO_EHEAD *multiCast = clientBuildMessage(m_servicePort, sizeof(SYNTRO_RECORD_HEADER) + sizeof(SYNTRO_NAVDATA));
    SYNTRO_RECORD_HEADER *head = (SYNTRO_RECORD_HEADER *)(multiCast + 1);
    SyntroUtils::convertIntToUC2(SYNTRO_RECORD_TYPE_NAV, head->type);
    SyntroUtils::convertIntToUC2(SYNTRO_RECORD_TYPE_NAV_IMU, head->subType);
    SyntroUtils::convertIntToUC2(sizeof(SYNTRO_RECORD_HEADER), head->headerLength);
    SyntroUtils::convertInt64ToUC8(m_timestamp, head->timestamp);

    data.pose[0] = m_pose.x();
    data.pose[1] = m_pose.y();
    data.pose[2] = m_pose.z();

    data.state[0] = m_stateVector.scalar();
    data.state[1] = m_stateVector.x();
    data.state[2] = m_stateVector.y();
    data.state[3] = m_stateVector.z();

    data.gyro[0] = m_gyro.x();
    data.gyro[1] = m_gyro.y();
    data.gyro[2] = m_gyro.z();

    data.accel[0] = m_accel.x();
    data.accel[1] = m_accel.y();
    data.accel[2] = m_accel.z();

    data.compass[0] = m_compass.x();
    data.compass[1] = m_compass.y();
    data.compass[2] = m_compass.z();

    memcpy(head + 1, &data, sizeof(SYNTRO_NAVDATA));
    clientSendMessage(m_servicePort, multiCast, sizeof(SYNTRO_RECORD_HEADER) + sizeof(SYNTRO_NAVDATA), SYNTROLINK_MEDPRI);
    m_newIMUData = false;
}

void NavClient::newIMUData(const RTVector3& pose, const RTQuaternion& state, const RTVector3& gyro,
                           const RTVector3& accel, const RTVector3& compass, const uint64_t timestamp)
{
    QMutexLocker locker(&m_navLock);

    m_pose = pose;
    m_stateVector = state;
    m_gyro = gyro;
    m_accel = accel;
    m_compass = compass;
    m_timestamp = timestamp;
    m_newIMUData = true;
}


void NavClient::newStream()
{
    // remove the old streams

    if (m_servicePort != -1)
        clientRemoveService(m_servicePort);

    m_servicePort = clientAddService(SYNTRO_STREAMNAME_NAV, SERVICETYPE_MULTICAST, true);
}
