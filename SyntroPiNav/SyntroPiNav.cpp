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


#include <QMessageBox>
#include <qboxlayout.h>

#include "SyntroPiNav.h"
#include "NavClient.h"
#include "SyntroAboutDlg.h"
#include "BasicSetupDlg.h"
#include "CompassCalDlg.h"

#include "IMUThread.h"
#include "RTIMUMPU9150.h"
#include "RTKalman.h"

#define RATE_TIMER_INTERVAL 2

SyntroPiNav::SyntroPiNav()
    : QMainWindow()
{
	ui.setupUi(this);

    layoutWindow();
	layoutStatusBar();

	connect(ui.actionExit, SIGNAL(triggered()), this, SLOT(close()));
	connect(ui.actionAbout, SIGNAL(triggered()), this, SLOT(onAbout()));
    connect(ui.actionBasicSetup, SIGNAL(triggered()), this, SLOT(onBasicSetup()));
    connect(ui.actionCalibrateCompass, SIGNAL(triggered()), this, SLOT(onCalibrateCompass()));
    connect(m_enableGyro, SIGNAL(stateChanged(int)), this, SLOT(onEnableGyro(int)));
    connect(m_enableAccel, SIGNAL(stateChanged(int)), this, SLOT(onEnableAccel(int)));
    connect(m_enableCompass, SIGNAL(stateChanged(int)), this, SLOT(onEnableCompass(int)));
    connect(m_enableDebug, SIGNAL(stateChanged(int)), this, SLOT(onEnableDebug(int)));
    SyntroUtils::syntroAppInit();

    m_imuThread = new IMUThread();
    m_client = new NavClient(this);

    connect(m_imuThread,
            SIGNAL(newIMUData(const RTVector3&, const RTQuaternion&, const RTVector3&,
                              const RTVector3&, const RTVector3&, const uint64_t)),
            this,
            SLOT(newIMUData(const RTVector3&, const RTQuaternion&, const RTVector3&,
                            const RTVector3&, const RTVector3&, const uint64_t)),
            Qt::DirectConnection);

    connect(m_imuThread,
            SIGNAL(newIMUData(const RTVector3&, const RTQuaternion&, const RTVector3&,
                              const RTVector3&, const RTVector3&, const uint64_t)),
            m_client,
            SLOT(newIMUData(const RTVector3&, const RTQuaternion&, const RTVector3&,
                            const RTVector3&, const RTVector3&, const uint64_t)),
            Qt::DirectConnection);


    m_imuThread->resumeThread();
    m_client->resumeThread();
	
	restoreWindowState();

	setWindowTitle(QString("%1 - %2")
                   .arg(SyntroUtils::getAppType())
                   .arg(SyntroUtils::getAppName()));

    m_rateTimer = startTimer(RATE_TIMER_INTERVAL * 1000);
    m_updateTimer = startTimer(100);
    m_sampleCount = 0;
}

SyntroPiNav::~SyntroPiNav()
{
}

void SyntroPiNav::onCalibrateCompass()
{
    CompassCalDlg dlg(this);

    m_imuThread->setCalibrationMode(true);
    connect(m_imuThread, SIGNAL(newCalData(const RTVector3&)), &dlg,
            SLOT(newCalData(const RTVector3&)), Qt::DirectConnection);

    if (dlg.exec() == QDialog::Accepted) {
        m_imuThread->newCompassCalData(dlg.getCompassMin(), dlg.getCompassMax());
        m_imuThread->setCalibrationMode(false);
    }
    disconnect(m_imuThread, SIGNAL(newCalData(const RTVector3&)), &dlg, SLOT(newCalData(const RTVector3&)));
}

void SyntroPiNav::onEnableGyro(int state)
{
    m_imuThread->getIMU()->setGyroEnable(state == Qt::Checked);
}

void SyntroPiNav::onEnableAccel(int state)
{
    m_imuThread->getIMU()->setAccelEnable(state == Qt::Checked);
}

void SyntroPiNav::onEnableCompass(int state)
{
    m_imuThread->getIMU()->setCompassEnable(state == Qt::Checked);
}

void SyntroPiNav::onEnableDebug(int state)
{
    m_imuThread->getIMU()->setDebugEnable(state == Qt::Checked);
}


void SyntroPiNav::newIMUData(const RTVector3& pose, const RTQuaternion& state, const RTVector3& gyro,
                             const RTVector3& accel, const RTVector3& compass, const uint64_t /* timestamp */)
{
    m_pose = pose;
    m_state = state;
    m_gyro = gyro;
    m_accel = accel;
    m_compass = compass;
    m_sampleCount++;
}

void SyntroPiNav::closeEvent(QCloseEvent *)
{
    killTimer(m_rateTimer);
    killTimer(m_updateTimer);
    m_imuThread->exitThread();
    m_client->exitThread();

	saveWindowState();
    SyntroUtils::syntroAppExit();
}

void SyntroPiNav::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_updateTimer) {
        m_gyroX->setText(QString::number(m_gyro.x()));
        m_gyroY->setText(QString::number(m_gyro.y()));
        m_gyroZ->setText(QString::number(m_gyro.z()));
        m_accelX->setText(QString::number(m_accel.x()));
        m_accelY->setText(QString::number(m_accel.y()));
        m_accelZ->setText(QString::number(m_accel.z()));
        m_compassX->setText(QString::number(m_compass.x()));
        m_compassY->setText(QString::number(m_compass.y()));
        m_compassZ->setText(QString::number(m_compass.z()));

        m_poseX->setText(QString::number(m_pose.x()));
        m_poseY->setText(QString::number(m_pose.y()));
        m_poseZ->setText(QString::number(m_pose.z()));

        m_kalmanScalar->setText(QString::number(m_state.scalar()));
        m_kalmanX->setText(QString::number(m_state.x()));
        m_kalmanY->setText(QString::number(m_state.y()));
        m_kalmanZ->setText(QString::number(m_state.z()));
    } else {
        m_controlStatus->setText(m_client->getLinkState());

        float rate = (float)m_sampleCount / (float(RATE_TIMER_INTERVAL));
        m_sampleCount = 0;
        m_rateStatus->setText(QString("Sample rate: %1 per second").arg(rate));
    }
}

void SyntroPiNav::layoutWindow()
{
    QVBoxLayout *vLayout = new QVBoxLayout();
    vLayout->setContentsMargins(3, 3, 3, 3);
    vLayout->setSpacing(3);

    vLayout->addWidget(new QLabel("Kalman state (quaternion): "));

    QHBoxLayout *dataLayout = new QHBoxLayout();
    dataLayout->addSpacing(30);
    m_kalmanScalar = new QLabel("1");
    m_kalmanScalar->setFrameStyle(QFrame::Panel);
    m_kalmanX = new QLabel("0");
    m_kalmanX->setFrameStyle(QFrame::Panel);
    m_kalmanY = new QLabel("0");
    m_kalmanY->setFrameStyle(QFrame::Panel);
    m_kalmanZ = new QLabel("0");
    m_kalmanZ->setFrameStyle(QFrame::Panel);
    dataLayout->addWidget(m_kalmanScalar);
    dataLayout->addWidget(m_kalmanX);
    dataLayout->addWidget(m_kalmanY);
    dataLayout->addWidget(m_kalmanZ);
    dataLayout->addSpacing(30);
    vLayout->addLayout(dataLayout);

    vLayout->addSpacing(10);
    vLayout->addWidget(new QLabel("Pose (radians): "));

    m_poseX = new QLabel("0");
    m_poseX->setFrameStyle(QFrame::Panel);
    m_poseY = new QLabel("0");
    m_poseY->setFrameStyle(QFrame::Panel);
    m_poseZ = new QLabel("0");
    m_poseZ->setFrameStyle(QFrame::Panel);
    dataLayout = new QHBoxLayout();
    dataLayout->addSpacing(30);
    dataLayout->addWidget(m_poseX);
    dataLayout->addWidget(m_poseY);
    dataLayout->addWidget(m_poseZ);
    dataLayout->addSpacing(137);
    vLayout->addLayout(dataLayout);

    vLayout->addSpacing(10);
    vLayout->addWidget(new QLabel("Gyros (radians/s): "));

    m_gyroX = new QLabel("0");
    m_gyroX->setFrameStyle(QFrame::Panel);
    m_gyroY = new QLabel("0");
    m_gyroY->setFrameStyle(QFrame::Panel);
    m_gyroZ = new QLabel("0");
    m_gyroZ->setFrameStyle(QFrame::Panel);
    dataLayout = new QHBoxLayout();
    dataLayout->addSpacing(30);
    dataLayout->addWidget(m_gyroX);
    dataLayout->addWidget(m_gyroY);
    dataLayout->addWidget(m_gyroZ);
    dataLayout->addSpacing(137);
    vLayout->addLayout(dataLayout);

    vLayout->addSpacing(10);
    vLayout->addWidget(new QLabel("Accelerometers (g): "));

    m_accelX = new QLabel("0");
    m_accelX->setFrameStyle(QFrame::Panel);
    m_accelY = new QLabel("0");
    m_accelY->setFrameStyle(QFrame::Panel);
    m_accelZ = new QLabel("0");
    m_accelZ->setFrameStyle(QFrame::Panel);
    dataLayout = new QHBoxLayout();
    dataLayout->addSpacing(30);
    dataLayout->addWidget(m_accelX);
    dataLayout->addWidget(m_accelY);
    dataLayout->addWidget(m_accelZ);
    dataLayout->addSpacing(137);
    vLayout->addLayout(dataLayout);

    vLayout->addSpacing(10);
    vLayout->addWidget(new QLabel("Magnetometers (uT): "));

    m_compassX = new QLabel("0");
    m_compassX->setFrameStyle(QFrame::Panel);
    m_compassY = new QLabel("0");
    m_compassY->setFrameStyle(QFrame::Panel);
    m_compassZ = new QLabel("0");
    m_compassZ->setFrameStyle(QFrame::Panel);
    dataLayout = new QHBoxLayout();
    dataLayout->addSpacing(30);
    dataLayout->addWidget(m_compassX);
    dataLayout->addWidget(m_compassY);
    dataLayout->addWidget(m_compassZ);
    dataLayout->addSpacing(137);
    vLayout->addLayout(dataLayout);

    vLayout->addSpacing(10);
    vLayout->addWidget(new QLabel("Kalman controls: "));

    m_enableGyro = new QCheckBox("Enable gyros");
    m_enableGyro->setChecked(true);
    vLayout->addWidget(m_enableGyro);

    m_enableAccel = new QCheckBox("Enable accels");
    m_enableAccel->setChecked(true);
    vLayout->addWidget(m_enableAccel);

    m_enableCompass = new QCheckBox("Enable compass");
    m_enableCompass->setChecked(true);
    vLayout->addWidget(m_enableCompass);

    m_enableDebug = new QCheckBox("Enable debug messages");
    m_enableDebug->setChecked(false);
    vLayout->addWidget(m_enableDebug);

    vLayout->addStretch(1);

    centralWidget()->setLayout(vLayout);
    setFixedSize(500, 450);

 }


void SyntroPiNav::layoutStatusBar()
{
	m_controlStatus = new QLabel(this);
	m_controlStatus->setAlignment(Qt::AlignLeft);
	ui.statusBar->addWidget(m_controlStatus, 1);

    m_rateStatus = new QLabel(this);
    m_rateStatus->setAlignment(Qt::AlignCenter | Qt::AlignLeft);
    ui.statusBar->addWidget(m_rateStatus);
}

void SyntroPiNav::saveWindowState()
{
    QSettings *settings = SyntroUtils::getSettings();

    settings->beginGroup("Window");
    settings->setValue("Geometry", saveGeometry());
    settings->setValue("State", saveState());
    settings->endGroup();

    delete settings;
}

void SyntroPiNav::restoreWindowState()
{
    QSettings *settings = SyntroUtils::getSettings();

    settings->beginGroup("Window");
    restoreGeometry(settings->value("Geometry").toByteArray());
    restoreState(settings->value("State").toByteArray());
    settings->endGroup();

    delete settings;
}

void SyntroPiNav::onAbout()
{
    SyntroAbout dlg(this);
    dlg.exec();
}

void SyntroPiNav::onBasicSetup()
{
    BasicSetupDlg dlg(this);
    dlg.exec();
}

