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
#include <QQueue>

#include "SyntroLib.h"

#include "RTIMULib.h"

#define NAVCLIENT_BACKGROUND_INTERVAL    (SYNTRO_CLOCKS_PER_SEC / 100)

class NavClient : public Endpoint
{
	Q_OBJECT

public:
    NavClient(QObject *parent);
    virtual ~NavClient();

public slots:
    void newIMUData(const RTIMU_DATA& data);

    void newStream();

protected:
	void appClientInit();
	void appClientExit();
	void appClientBackground();

private:
    QMutex m_navLock;
    QQueue<RTIMU_DATA> m_imuData;

    int m_servicePort;
};

#endif // NAVCLIENT_H

