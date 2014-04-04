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

#ifndef NAVCLIENT_H
#define NAVCLIENT_H

#include <qmutex.h>
#include <QMutex>

#include "SyntroLib.h"

#include "RTMath.h"

#define NAVCLIENT_BACKGROUND_INTERVAL    (SYNTRO_CLOCKS_PER_SEC / 100)

class NavClient : public Endpoint
{
	Q_OBJECT

public:
    NavClient(QObject *parent);
    virtual ~NavClient();

public slots:
    void newIMUData(const RTVector3&, const RTQuaternion&, const RTVector3& gyro,
                    const RTVector3& accel, const RTVector3& compass, const uint64_t timestamp);

    void newStream();

protected:
	void appClientInit();
	void appClientExit();
	void appClientBackground();

private:
    RTVector3 m_gyro;
    RTVector3 m_accel;
    RTVector3 m_compass;

    RTVector3 m_pose;
    RTQuaternion m_stateVector;

    qint64 m_timestamp;

    bool m_newIMUData;

    int m_servicePort;

    QMutex m_navLock;


};

#endif // NAVCLIENT_H

