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

#ifndef SYNTROPINAV_H
#define SYNTROPINAV_H

#include <QMainWindow>
#include <QLabel>
#include <QCheckBox>

#include "ui_SyntroPiNav.h"

#include "RTMath.h"
#include "RTIMULibDefs.h"

#define PRODUCT_TYPE "SyntroPiNav"

class NavClient;
class IMUThread;

class SyntroPiNav : public QMainWindow
{
    Q_OBJECT

public:
    SyntroPiNav();
    ~SyntroPiNav();

public slots:
    void onAbout();
    void onBasicSetup();
    void onCalibrateAccelerometers();
    void onCalibrateMagnetometers();
    void onSelectFusionAlgorithm();
    void onSelectIMU();
    void onEnableGyro(int);
    void onEnableAccel(int);
    void onEnableCompass(int);
    void onEnableDebug(int);
    void newIMUData(const RTIMU_DATA&);

signals:
    void newIMU();

protected:
    void timerEvent(QTimerEvent *event);
    void closeEvent(QCloseEvent *event);

private:
    void layoutStatusBar();
    void layoutWindow();
    void saveWindowState();
    void restoreWindowState();
    QLabel *getFixedPanel(QString text);

    IMUThread *m_imuThread;

    //  Qt GUI stuff

    Ui::SyntroPiNavClass ui;

    QLabel *m_fusionQPoseScalar;
    QLabel *m_fusionQPoseX;
    QLabel *m_fusionQPoseY;
    QLabel *m_fusionQPoseZ;

    QLabel *m_fusionPoseX;
    QLabel *m_fusionPoseY;
    QLabel *m_fusionPoseZ;

    QLabel *m_gyroX;
    QLabel *m_gyroY;
    QLabel *m_gyroZ;

    QLabel *m_accelX;
    QLabel *m_accelY;
    QLabel *m_accelZ;
    QLabel *m_accelMagnitude;
    QLabel *m_accelResidualX;
    QLabel *m_accelResidualY;
    QLabel *m_accelResidualZ;

    QLabel *m_compassX;
    QLabel *m_compassY;
    QLabel *m_compassZ;
    QLabel *m_compassMagnitude;

    QLabel *m_fusionType;
    QCheckBox *m_enableGyro;
    QCheckBox *m_enableAccel;
    QCheckBox *m_enableCompass;
    QCheckBox *m_enableDebug;

    QLabel *m_imuType;
    QLabel *m_biasStatus;
    QLabel *m_calStatus;
    QLabel *m_rateStatus;
    QLabel *m_controlStatus;

    NavClient *m_client;

    int m_rateTimer;
    int m_updateTimer;

    RTIMU_DATA m_imuData;

    int m_sampleCount;
};

#endif // SYNTROPINAV_H

