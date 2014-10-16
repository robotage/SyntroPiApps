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

#ifndef _IMUTHREAD_H
#define	_IMUTHREAD_H

#include "SyntroLib.h"

#include "RTMath.h"
#include "RTIMULibDefs.h"

class RTIMU;
class RTIMUSettings;

Q_DECLARE_METATYPE(RTIMU_DATA);

class IMUThread : public SyntroThread
{
    Q_OBJECT

public:
    IMUThread();
    virtual ~IMUThread();

    RTIMUSettings *getSettings() { return m_settings; }

    RTIMU *getIMU() { return (RTIMU *)m_imu; }

public slots:
    void newIMU();

signals:
    void newCalData(const RTVector3& compass);
    void newIMUData(const RTIMU_DATA& data);

protected:
    void initThread();
    void finishThread();
    void timerEvent(QTimerEvent *event);

private:
    int m_timer;
    RTIMUSettings *m_settings;

    RTIMU *m_imu;
    bool m_calibrationMode;
};

#endif // _IMUTHREAD_H
