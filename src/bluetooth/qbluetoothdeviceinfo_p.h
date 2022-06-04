/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
**
**
**
**
**
**
**
**
**
**
******************************************************************************/

#ifndef QBLUETOOTHDEVICEINFO_P_H
#define QBLUETOOTHDEVICEINFO_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qbluetoothdeviceinfo.h"
#include "qbluetoothaddress.h"
#include "qbluetoothuuid.h"

#include <QString>
#include <QtCore/qhash.h>

QT_BEGIN_NAMESPACE

class QBluetoothDeviceInfoPrivate
{
public:
    QBluetoothDeviceInfoPrivate();

    bool valid = false;
    bool cached = false;
    qint16 rssi = 1;
    quint8 minorDeviceClass = 0;

    QBluetoothAddress address;
    QString name;
    QBluetoothDeviceInfo::MajorDeviceClass majorDeviceClass = QBluetoothDeviceInfo::MiscellaneousDevice;

    QBluetoothDeviceInfo::ServiceClasses serviceClasses = QBluetoothDeviceInfo::NoService;

    QList<QBluetoothUuid> serviceUuids;
    QMultiHash<quint16, QByteArray> manufacturerData;
    QBluetoothDeviceInfo::CoreConfigurations deviceCoreConfiguration = QBluetoothDeviceInfo::UnknownCoreConfiguration;

    QBluetoothUuid deviceUuid;
};

QT_END_NAMESPACE

#endif
