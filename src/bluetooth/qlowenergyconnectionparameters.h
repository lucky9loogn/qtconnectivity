/***************************************************************************
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

#ifndef QLOWENERGYCONNECTIONPARAMETERS_H
#define QLOWENERGYCONNECTIONPARAMETERS_H

#include <QtBluetooth/qtbluetoothglobal.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qshareddata.h>

QT_BEGIN_NAMESPACE

class QLowEnergyConnectionParametersPrivate;

class Q_BLUETOOTH_EXPORT QLowEnergyConnectionParameters
{
public:
    QLowEnergyConnectionParameters();
    QLowEnergyConnectionParameters(const QLowEnergyConnectionParameters &other);
    ~QLowEnergyConnectionParameters();

    QLowEnergyConnectionParameters &operator=(const QLowEnergyConnectionParameters &other);
    friend bool operator==(const QLowEnergyConnectionParameters &a,
                           const QLowEnergyConnectionParameters &b)
    {
        return equals(a, b);
    }
    friend bool operator!=(const QLowEnergyConnectionParameters &a,
                           const QLowEnergyConnectionParameters &b)
    {
        return !equals(a, b);
    }

    void setIntervalRange(double minimum, double maximum);
    double minimumInterval() const;
    double maximumInterval() const;

    void setLatency(int latency);
    int latency() const;

    void setSupervisionTimeout(int timeout);
    int supervisionTimeout() const;

    void swap(QLowEnergyConnectionParameters &other) Q_DECL_NOTHROW { qSwap(d, other.d); }

private:
    static bool equals(const QLowEnergyConnectionParameters &a,
                       const QLowEnergyConnectionParameters &b);
    QSharedDataPointer<QLowEnergyConnectionParametersPrivate> d;
};

Q_DECLARE_SHARED(QLowEnergyConnectionParameters)

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QLowEnergyConnectionParameters)

#endif // Include guard
