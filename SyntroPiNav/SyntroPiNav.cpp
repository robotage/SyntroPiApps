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
#include <QBoxLayout>

#include "SyntroPiNav.h"
#include "NavClient.h"
#include "SyntroAboutDlg.h"
#include "BasicSetupDlg.h"
#include "CompassCalDlg.h"
#include "SelectIMUDlg.h"
#include "SelectFusionDlg.h"
#include "IMUThread.h"
#include "RTIMU.h"
#include "AccelCalDlg.h"
#include "MagCalDlg.h"

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
    connect(ui.actionCalibrateAccelerometers, SIGNAL(triggered()), this, SLOT(onCalibrateAccelerometers()));
    connect(ui.actionCalibrateMagnetometers, SIGNAL(triggered()), this, SLOT(onCalibrateMagnetometers()));
    connect(ui.actionSelectFusionAlgorithm, SIGNAL(triggered()), this, SLOT(onSelectFusionAlgorithm()));
    connect(ui.actionSelectIMU, SIGNAL(triggered()), this, SLOT(onSelectIMU()));
    connect(m_enableGyro, SIGNAL(stateChanged(int)), this, SLOT(onEnableGyro(int)));
    connect(m_enableAccel, SIGNAL(stateChanged(int)), this, SLOT(onEnableAccel(int)));
    connect(m_enableCompass, SIGNAL(stateChanged(int)), this, SLOT(onEnableCompass(int)));
    connect(m_enableDebug, SIGNAL(stateChanged(int)), this, SLOT(onEnableDebug(int)));
    SyntroUtils::syntroAppInit();

    m_imuThread = new IMUThread();
    m_client = new NavClient(this);

    connect(m_imuThread, SIGNAL(newIMUData(const RTIMU_DATA&)),
            this, SLOT(newIMUData(const RTIMU_DATA&)), Qt::DirectConnection);

    connect(m_imuThread, SIGNAL(newIMUData(const RTIMU_DATA&)),
            m_client, SLOT(newIMUData(const RTIMU_DATA&)), Qt::DirectConnection);

    connect(this, SIGNAL(newIMU()), m_imuThread, SLOT(newIMU()));

    m_imuThread->resumeThread();
    m_client->resumeThread();

    restoreWindowState();

    setWindowTitle(QString("%1 - %2")
                   .arg(SyntroUtils::getAppType())
                   .arg(SyntroUtils::getAppName()));

    m_rateTimer = startTimer(RATE_TIMER_INTERVAL * 1000);
    m_updateTimer = startTimer(100);
    m_sampleCount = 0;
    m_fusionType->setText(RTFusion::fusionName(m_imuThread->getSettings()->m_fusionType));
}

SyntroPiNav::~SyntroPiNav()
{
}

void SyntroPiNav::onCalibrateAccelerometers()
{
    m_imuThread->getIMU()->setAccelCalibrationMode(true);

    AccelCalDlg dlg(this, m_imuThread->getSettings());

    connect(m_imuThread, SIGNAL(newIMUData(const RTIMU_DATA&)),
        &dlg, SLOT(newIMUData(const RTIMU_DATA&)), Qt::DirectConnection);


    if (dlg.exec() == QDialog::Accepted) {

    }

    disconnect(m_imuThread, SIGNAL(newIMUData(const RTIMU_DATA&)),
        &dlg, SLOT(newIMUData(const RTIMU_DATA&)));
    emit newIMU();
}

void SyntroPiNav::onCalibrateMagnetometers()
{
    m_imuThread->getIMU()->setCompassCalibrationMode(true);
    MagCalDlg dlg(this, m_imuThread->getSettings());

    connect(m_imuThread, SIGNAL(newIMUData(const RTIMU_DATA&)),
        &dlg, SLOT(newIMUData(const RTIMU_DATA&)), Qt::DirectConnection);

    if (dlg.exec() == QDialog::Accepted) {

    }
    disconnect(m_imuThread, SIGNAL(newIMUData(const RTIMU_DATA&)),
        &dlg, SLOT(newIMUData(const RTIMU_DATA&)));
    emit newIMU();
}

void SyntroPiNav::onSelectFusionAlgorithm()
{
    SelectFusionDlg dlg(m_imuThread->getSettings(), this);

    if (dlg.exec() == QDialog::Accepted) {
        emit newIMU();
        m_fusionType->setText(RTFusion::fusionName(m_imuThread->getSettings()->m_fusionType));
    }
}

void SyntroPiNav::onSelectIMU()
{
    SelectIMUDlg dlg(m_imuThread->getSettings(), this);

    if (dlg.exec() == QDialog::Accepted) {
        emit newIMU();
    }
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


void SyntroPiNav::newIMUData(const RTIMU_DATA& data)
{
    m_imuData = data;
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
        m_gyroX->setText(QString::number(m_imuData.gyro.x(), 'f', 6));
        m_gyroY->setText(QString::number(m_imuData.gyro.y(), 'f', 6));
        m_gyroZ->setText(QString::number(m_imuData.gyro.z(), 'f', 6));

        m_accelX->setText(QString::number(m_imuData.accel.x(), 'f', 6));
        m_accelY->setText(QString::number(m_imuData.accel.y(), 'f', 6));
        m_accelZ->setText(QString::number(m_imuData.accel.z(), 'f', 6));

        m_compassX->setText(QString::number(m_imuData.compass.x(), 'f', 6));
        m_compassY->setText(QString::number(m_imuData.compass.y(), 'f', 6));
        m_compassZ->setText(QString::number(m_imuData.compass.z(), 'f', 6));

        m_fusionPoseX->setText(QString::number(m_imuData.fusionPose.x() * RTMATH_RAD_TO_DEGREE, 'f', 6));
        m_fusionPoseY->setText(QString::number(m_imuData.fusionPose.y() * RTMATH_RAD_TO_DEGREE, 'f', 6));
        m_fusionPoseZ->setText(QString::number(m_imuData.fusionPose.z() * RTMATH_RAD_TO_DEGREE, 'f', 6));

        m_fusionQPoseScalar->setText(QString::number(m_imuData.fusionQPose.scalar(), 'f', 6));
        m_fusionQPoseX->setText(QString::number(m_imuData.fusionQPose.x(), 'f', 6));
        m_fusionQPoseY->setText(QString::number(m_imuData.fusionQPose.y(), 'f', 6));
        m_fusionQPoseZ->setText(QString::number(m_imuData.fusionQPose.z(), 'f', 6));

        m_accelMagnitude->setText(QString::number(m_imuData.accel.length(), 'f', 6));
        m_compassMagnitude->setText(QString::number(m_imuData.compass.length(), 'f', 6));

        if (m_imuThread->getIMU() != NULL) {
            RTVector3 residuals = m_imuThread->getIMU()->getAccelResiduals();
            m_accelResidualX->setText(QString::number(residuals.x(), 'f', 6));
            m_accelResidualY->setText(QString::number(residuals.y(), 'f', 6));
            m_accelResidualZ->setText(QString::number(residuals.z(), 'f', 6));
        }

    } else {
        m_controlStatus->setText(m_client->getLinkState());

        float rate = (float)m_sampleCount / (float(RATE_TIMER_INTERVAL));
        m_sampleCount = 0;
        m_rateStatus->setText(QString("Sample rate: %1 per second").arg(rate));

        if (m_imuThread->getIMU() == NULL) {
            m_calStatus->setText("No IMU found");
        } else {
            m_calStatus->setText(QString("Accel %1 : Compass %2 : Ellipsoid %3")
                    .arg(m_imuThread->getIMU()->getAccelCalibrationValid() ? "calibrated" : "uncalibrated")
                    .arg(m_imuThread->getIMU()->getCompassCalibrationValid() ? "calibrated" : "uncalibrated")
                    .arg(m_imuThread->getIMU()->getCompassCalibrationEllipsoidValid() ? "in use" : "not in use"));
        }

        if (m_imuThread->getIMU() != NULL) {
            m_imuType->setText(m_imuThread->getIMU()->IMUName());

            if (!m_imuThread->getIMU()->IMUGyroBiasValid())
                m_biasStatus->setText("Gyro bias being calculated - keep IMU still!");
            else
                m_biasStatus->setText("Gyro bias valid");
        }
    }
}

void SyntroPiNav::layoutWindow()
{
    QVBoxLayout *vLayout = new QVBoxLayout();
    vLayout->setContentsMargins(3, 3, 3, 3);
    vLayout->setSpacing(3);

    QHBoxLayout *imuLayout = new QHBoxLayout();
    vLayout->addLayout(imuLayout);
    imuLayout->addWidget(new QLabel("IMU type: "));
    m_imuType = new QLabel();
    imuLayout->addWidget(m_imuType);
    imuLayout->setStretch(1, 1);

    QHBoxLayout *biasLayout = new QHBoxLayout();
    vLayout->addLayout(biasLayout);
    biasLayout->addWidget(new QLabel("Gyro bias status: "));
    m_biasStatus = new QLabel();
    biasLayout->addWidget(m_biasStatus);
    biasLayout->setStretch(1, 1);

    vLayout->addSpacing(10);

    vLayout->addWidget(new QLabel("Fusion state (quaternion): "));

    QHBoxLayout *dataLayout = new QHBoxLayout();
    dataLayout->setAlignment(Qt::AlignLeft);
    m_fusionQPoseScalar = getFixedPanel("1");
    m_fusionQPoseX = getFixedPanel("0");
    m_fusionQPoseY = getFixedPanel("0");
    m_fusionQPoseZ = getFixedPanel("0");
    dataLayout->addSpacing(30);
    dataLayout->addWidget(m_fusionQPoseScalar);
    dataLayout->addWidget(m_fusionQPoseX);
    dataLayout->addWidget(m_fusionQPoseY);
    dataLayout->addWidget(m_fusionQPoseZ);
    vLayout->addLayout(dataLayout);

    vLayout->addSpacing(10);
    vLayout->addWidget(new QLabel("Pose - roll, pitch, yaw (degrees): "));

    m_fusionPoseX = getFixedPanel("0");
    m_fusionPoseY = getFixedPanel("0");
    m_fusionPoseZ = getFixedPanel("0");
    dataLayout = new QHBoxLayout();
    dataLayout->setAlignment(Qt::AlignLeft);
    dataLayout->addSpacing(30);
    dataLayout->addWidget(m_fusionPoseX);
    dataLayout->addWidget(m_fusionPoseY);
    dataLayout->addWidget(m_fusionPoseZ);
    vLayout->addLayout(dataLayout);

    vLayout->addSpacing(10);
    vLayout->addWidget(new QLabel("Gyros (radians/s): "));

    m_gyroX = getFixedPanel("0");
    m_gyroY = getFixedPanel("0");
    m_gyroZ = getFixedPanel("0");
    dataLayout = new QHBoxLayout();
    dataLayout->setAlignment(Qt::AlignLeft);
    dataLayout->addSpacing(30);
    dataLayout->addWidget(m_gyroX);
    dataLayout->addWidget(m_gyroY);
    dataLayout->addWidget(m_gyroZ);
    vLayout->addLayout(dataLayout);

    vLayout->addSpacing(10);
    vLayout->addWidget(new QLabel("Accelerometers (g): "));

    m_accelX = getFixedPanel("0");
    m_accelY = getFixedPanel("0");
    m_accelZ = getFixedPanel("0");
    dataLayout = new QHBoxLayout();
    dataLayout->addSpacing(30);
    dataLayout->setAlignment(Qt::AlignLeft);
    dataLayout->addWidget(m_accelX);
    dataLayout->addWidget(m_accelY);
    dataLayout->addWidget(m_accelZ);
    vLayout->addLayout(dataLayout);

    vLayout->addSpacing(10);
    vLayout->addWidget(new QLabel("Accelerometer magnitude (g): "));

    m_accelMagnitude = getFixedPanel("0");
    dataLayout = new QHBoxLayout();
    dataLayout->addSpacing(30);
    dataLayout->addWidget(m_accelMagnitude);
    dataLayout->setAlignment(Qt::AlignLeft);
    vLayout->addLayout(dataLayout);

    vLayout->addSpacing(10);
    vLayout->addWidget(new QLabel("Accelerometer residuals (g): "));

    m_accelResidualX = getFixedPanel("0");
    m_accelResidualY = getFixedPanel("0");
    m_accelResidualZ = getFixedPanel("0");
    dataLayout = new QHBoxLayout();
    dataLayout->addSpacing(30);
    dataLayout->setAlignment(Qt::AlignLeft);
    dataLayout->addWidget(m_accelResidualX);
    dataLayout->addWidget(m_accelResidualY);
    dataLayout->addWidget(m_accelResidualZ);
    vLayout->addLayout(dataLayout);

    vLayout->addSpacing(10);
    vLayout->addWidget(new QLabel("Magnetometers (uT): "));

    m_compassX = getFixedPanel("0");
    m_compassY = getFixedPanel("0");
    m_compassZ = getFixedPanel("0");
    dataLayout = new QHBoxLayout();
    dataLayout->setAlignment(Qt::AlignLeft);
    dataLayout->addSpacing(30);
    dataLayout->addWidget(m_compassX);
    dataLayout->addWidget(m_compassY);
    dataLayout->addWidget(m_compassZ);
    vLayout->addLayout(dataLayout);

    vLayout->addSpacing(10);
    vLayout->addWidget(new QLabel("Compass magnitude (uT): "));

    m_compassMagnitude = getFixedPanel("0");
    dataLayout = new QHBoxLayout();
    dataLayout->addSpacing(30);
    dataLayout->addWidget(m_compassMagnitude);
    dataLayout->setAlignment(Qt::AlignLeft);
    vLayout->addLayout(dataLayout);

    vLayout->addSpacing(10);

    QHBoxLayout *fusionBox = new QHBoxLayout();
    QLabel *fusionTypeLabel = new QLabel("Fusion algorithm: ");
    fusionBox->addWidget(fusionTypeLabel);
    fusionTypeLabel->setMaximumWidth(150);
    m_fusionType = new QLabel();
    fusionBox->addWidget(m_fusionType);
    vLayout->addLayout(fusionBox);

    vLayout->addSpacing(10);

    QHBoxLayout *calBox = new QHBoxLayout();
    QLabel *calLabel = new QLabel("Calibration status: ");
    calBox->addWidget(calLabel);
    calLabel->setMaximumWidth(150);
    m_calStatus = new QLabel();
    calBox->addWidget(m_calStatus);
    vLayout->addLayout(calBox);

    vLayout->addSpacing(10);


    vLayout->addWidget(new QLabel("Fusion controls: "));

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
    setMinimumSize(650, 650);
}

QLabel* SyntroPiNav::getFixedPanel(QString text)
{
    QLabel *label = new QLabel(text);
    label->setFrameStyle(QFrame::Panel);
    label->setFixedSize(QSize(100, 16));
    return label;
}

void SyntroPiNav::layoutStatusBar()
{
    m_controlStatus = new QLabel(this);
    m_controlStatus->setAlignment(Qt::AlignLeft);
    ui.statusBar->addWidget(m_controlStatus, 1);


    m_rateStatus = new QLabel(this);
    m_rateStatus->setAlignment(Qt::AlignCenter | Qt::AlignRight);
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

