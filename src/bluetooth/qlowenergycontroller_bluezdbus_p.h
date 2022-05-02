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

#ifndef QLOWENERGYCONTROLLERPRIVATEDBUS_P_H
#define QLOWENERGYCONTROLLERPRIVATEDBUS_P_H


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

#include "qlowenergycontroller.h"
#include "qlowenergycontrollerbase_p.h"

#include <QtDBus/QDBusObjectPath>

class OrgBluezAdapter1Interface;
class OrgBluezBattery1Interface;
class OrgBluezDevice1Interface;
class OrgBluezGattCharacteristic1Interface;
class OrgBluezGattDescriptor1Interface;
class OrgBluezGattService1Interface;
class OrgFreedesktopDBusObjectManagerInterface;
class OrgFreedesktopDBusPropertiesInterface;

QT_BEGIN_NAMESPACE

class QDBusPendingCallWatcher;

class QLowEnergyControllerPrivateBluezDBus final : public QLowEnergyControllerPrivate
{
    Q_OBJECT
public:
    QLowEnergyControllerPrivateBluezDBus();
    ~QLowEnergyControllerPrivateBluezDBus() override;

    void init() override;
    void connectToDevice() override;
    void disconnectFromDevice() override;

    void discoverServices() override;
    void discoverServiceDetails(const QBluetoothUuid &service,
                                QLowEnergyService::DiscoveryMode mode) override;

    void readCharacteristic(
                const QSharedPointer<QLowEnergyServicePrivate> service,
                const QLowEnergyHandle charHandle) override;
    void readDescriptor(
                const QSharedPointer<QLowEnergyServicePrivate> service,
                const QLowEnergyHandle charHandle,
                const QLowEnergyHandle descriptorHandle) override;

    void writeCharacteristic(
                const QSharedPointer<QLowEnergyServicePrivate> service,
                const QLowEnergyHandle charHandle,
                const QByteArray &newValue,
                QLowEnergyService::WriteMode writeMode) override;
    void writeDescriptor(
                const QSharedPointer<QLowEnergyServicePrivate> service,
                const QLowEnergyHandle charHandle,
                const QLowEnergyHandle descriptorHandle,
                const QByteArray &newValue) override;

    void startAdvertising(
                const QLowEnergyAdvertisingParameters &params,
                const QLowEnergyAdvertisingData &advertisingData,
                const QLowEnergyAdvertisingData &scanResponseData) override;
    void stopAdvertising() override;

    void requestConnectionUpdate(
                const QLowEnergyConnectionParameters & params) override;
    void addToGenericAttributeList(
                        const QLowEnergyServiceData &service,
                        QLowEnergyHandle startHandle) override;

    int mtu() const override;

    QLowEnergyService *addServiceHelper(const QLowEnergyServiceData &service) override;


private:
    void connectToDeviceHelper();
    void resetController();

    void scheduleNextJob();

private slots:
    void devicePropertiesChanged(const QString &interface, const QVariantMap &changedProperties,
                                 const QStringList &invalidatedProperties);
    void characteristicPropertiesChanged(QLowEnergyHandle charHandle, const QString &interface,
                                    const QVariantMap &changedProperties,
                                    const QStringList &invalidatedProperties);
    void interfacesRemoved(const QDBusObjectPath &objectPath, const QStringList &interfaces);

    void onCharReadFinished(QDBusPendingCallWatcher *call);
    void onDescReadFinished(QDBusPendingCallWatcher *call);
    void onCharWriteFinished(QDBusPendingCallWatcher *call);
    void onDescWriteFinished(QDBusPendingCallWatcher *call);
private:
    OrgBluezAdapter1Interface* adapter{};
    OrgBluezDevice1Interface* device{};
    OrgFreedesktopDBusObjectManagerInterface* managerBluez{};
    OrgFreedesktopDBusPropertiesInterface* deviceMonitor{};

    bool pendingConnect = false;
    bool disconnectSignalRequired = false;

    struct GattCharacteristic
    {
        QSharedPointer<OrgBluezGattCharacteristic1Interface> characteristic;
        QSharedPointer<OrgFreedesktopDBusPropertiesInterface> charMonitor;
        QList<QSharedPointer<OrgBluezGattDescriptor1Interface>> descriptors;
    };

    struct GattService
    {
        QString servicePath;
        QList<GattCharacteristic> characteristics;

        bool hasBatteryService = false;
        QSharedPointer<OrgBluezBattery1Interface> batteryInterface;
    };

    QHash<QBluetoothUuid, GattService> dbusServices;
    QLowEnergyHandle runningHandle = 1;

    struct GattJob {
        enum JobFlag {
            Unset                   = 0x00,
            CharRead                = 0x01,
            CharWrite               = 0x02,
            DescRead                = 0x04,
            DescWrite               = 0x08,
            ServiceDiscovery        = 0x10,
            LastServiceDiscovery    = 0x20
        };
        Q_DECLARE_FLAGS(JobFlags, JobFlag)

        JobFlags flags = GattJob::Unset;
        QLowEnergyHandle handle;
        QByteArray value;
        QLowEnergyService::WriteMode writeMode = QLowEnergyService::WriteWithResponse;
        QSharedPointer<QLowEnergyServicePrivate> service;
    };

    QList<GattJob> jobs;
    bool jobPending = false;

    void prepareNextJob();
    void discoverBatteryServiceDetails(GattService &dbusData,
                                       QSharedPointer<QLowEnergyServicePrivate> serviceData);
    void executeClose(QLowEnergyController::Error newError);
};

QT_END_NAMESPACE

#endif // QLOWENERGYCONTROLLERPRIVATEDBUS_P_H
