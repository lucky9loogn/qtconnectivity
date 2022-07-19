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

#include "qbluetoothdevicediscoveryagent.h"
#include "qbluetoothdevicediscoveryagent_p.h"
#include "qbluetoothaddress.h"
#include "qbluetoothuuid.h"

#include <QtBluetooth/private/qbluetoothdevicewatcher_winrt_p.h>
#include <QtBluetooth/private/qbluetoothutils_winrt_p.h>
#include <QtBluetooth/private/qtbluetoothglobal_p.h>

#include <QtCore/QLoggingCategory>
#include <QtCore/qmutex.h>
#include <QtCore/private/qfunctions_winrt_p.h>

#include <robuffer.h>
#include <wrl.h>
#include <windows.devices.enumeration.h>
#include <windows.devices.bluetooth.h>
#include <windows.foundation.collections.h>
#include <windows.storage.streams.h>

#include <windows.devices.bluetooth.advertisement.h>

#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Rfcomm.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

using WinRtBluetoothDevice = winrt::Windows::Devices::Bluetooth::BluetoothDevice;
using WinRtRfcommDeviceServicesResult = winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommDeviceServicesResult;
using WinRtAsyncOperation = winrt::Windows::Foundation::IAsyncOperation;
using WinRtAsyncStatus = winrt::Windows::Foundation::AsyncStatus;

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Devices;
using namespace ABI::Windows::Devices::Bluetooth;
using namespace ABI::Windows::Devices::Bluetooth::Advertisement;
using namespace ABI::Windows::Devices::Enumeration;
using namespace ABI::Windows::Storage::Streams;

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_WINDOWS)

#define EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED(msg, error, ret) \
    if (FAILED(hr)) { \
        emit errorOccured(error); \
        qCWarning(QT_BT_WINDOWS) << msg; \
        ret; \
    }

#define WARN_AND_RETURN_IF_FAILED(msg, ret) \
    if (FAILED(hr)) { \
        qCWarning(QT_BT_WINDOWS) << msg; \
        ret; \
    }

#define WARN_AND_CONTINUE_IF_FAILED(msg) \
    if (FAILED(hr)) { \
        qCWarning(QT_BT_WINDOWS) << msg; \
        continue; \
    }

static ManufacturerData extractManufacturerData(ComPtr<IBluetoothLEAdvertisement> ad)
{
    ManufacturerData ret;
    ComPtr<IVector<BluetoothLEManufacturerData*>> data;
    HRESULT hr = ad->get_ManufacturerData(&data);
    WARN_AND_RETURN_IF_FAILED("Could not obtain list of manufacturer data.", return ret);
    quint32 size;
    hr = data->get_Size(&size);
    WARN_AND_RETURN_IF_FAILED("Could not obtain manufacturer data's list size.", return ret);
    for (quint32 i = 0; i < size; ++i) {
        ComPtr<IBluetoothLEManufacturerData> d;
        hr = data->GetAt(i, &d);
        WARN_AND_CONTINUE_IF_FAILED("Could not obtain manufacturer data.");
        quint16 id;
        hr = d->get_CompanyId(&id);
        WARN_AND_CONTINUE_IF_FAILED("Could not obtain manufacturer data company id.");
        ComPtr<IBuffer> buffer;
        hr = d->get_Data(&buffer);
        WARN_AND_CONTINUE_IF_FAILED("Could not obtain manufacturer data set.");
        const QByteArray bufferData = byteArrayFromBuffer(buffer);
        if (ret.contains(id))
            qCWarning(QT_BT_WINDOWS) << "Company ID already present in manufacturer data.";
        ret.insert(id, bufferData);
    }
    return ret;
}

// Both constants are taken from Microsoft's docs:
// https://docs.microsoft.com/en-us/windows/uwp/devices-sensors/aep-service-class-ids
// Alternatively we could create separate watchers for paired and unpaired devices.
static const winrt::hstring ClassicDeviceSelector =
        L"System.Devices.Aep.ProtocolId:=\"{e0cbf06c-cd8b-4647-bb8a-263b43f0f974}\"";
static const winrt::hstring LowEnergyDeviceSelector =
        L"System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\"";

class QWinRTBluetoothDeviceDiscoveryWorker : public QObject,
        public std::enable_shared_from_this<QWinRTBluetoothDeviceDiscoveryWorker>
{
    Q_OBJECT
public:
    explicit QWinRTBluetoothDeviceDiscoveryWorker();
    ~QWinRTBluetoothDeviceDiscoveryWorker();
    void start(QBluetoothDeviceDiscoveryAgent::DiscoveryMethods methods);
    void stop();

private:
    void startDeviceDiscovery(QBluetoothDeviceDiscoveryAgent::DiscoveryMethod mode);
    void onDeviceDiscoveryFinished(IAsyncOperation<DeviceInformationCollection *> *op,
                                   QBluetoothDeviceDiscoveryAgent::DiscoveryMethod mode);
    void gatherDeviceInformation(IDeviceInformation *deviceInfo,
                                 QBluetoothDeviceDiscoveryAgent::DiscoveryMethod mode);
    void gatherMultipleDeviceInformation(quint32 deviceCount, IVectorView<DeviceInformation *> *devices,
                                         QBluetoothDeviceDiscoveryAgent::DiscoveryMethod mode);
    void setupLEDeviceWatcher();
    void leBluetoothInfoFromDeviceIdAsync(HSTRING deviceId);
    void leBluetoothInfoFromAddressAsync(quint64 address);
    HRESULT onPairedBluetoothLEDeviceFoundAsync(IAsyncOperation<BluetoothLEDevice *> *op, AsyncStatus status);
    HRESULT onBluetoothLEDeviceFoundAsync(IAsyncOperation<BluetoothLEDevice *> *op, AsyncStatus status);
    enum PairingCheck {
        CheckForPairing,
        OmitPairingCheck
    };
    HRESULT onBluetoothLEDeviceFound(ComPtr<IBluetoothLEDevice> device, PairingCheck pairingCheck);
    HRESULT onBluetoothLEDeviceFound(ComPtr<IBluetoothLEDevice> device);
    HRESULT onBluetoothLEAdvertisementReceived(IBluetoothLEAdvertisementReceivedEventArgs *args);
    HRESULT onLeServicesReceived(IAsyncOperation<GenericAttributeProfile::GattDeviceServicesResult *> *op,
                                 AsyncStatus status, QBluetoothDeviceInfo &info);

    std::shared_ptr<QBluetoothDeviceWatcherWinRT> createDeviceWatcher(winrt::hstring selector,
                                                                      int watcherId);
    void generateError(QBluetoothDeviceDiscoveryAgent::Error error, const char *msg = nullptr);
    // Bluetooth Classic handlers
    void getClassicDeviceFromId(const winrt::hstring &id);
    void handleClassicDevice(const WinRtBluetoothDevice &device);
    void handleRfcommServices(const WinRtRfcommDeviceServicesResult &servicesResult,
                              uint64_t address, const QString &name, uint32_t classOfDeviceInt);

    // invokable methods for handling finish conditions
    Q_INVOKABLE void incrementPendingDevicesCount();
    Q_INVOKABLE void decrementPendingDevicesCountAndCheckFinished();

public slots:
    void finishDiscovery();

Q_SIGNALS:
    void deviceFound(const QBluetoothDeviceInfo &info);
    void deviceDataChanged(const QBluetoothAddress &address, QBluetoothDeviceInfo::Fields,
                           qint16 rssi, ManufacturerData manufacturerData);
    void errorOccured(QBluetoothDeviceDiscoveryAgent::Error error);
    void scanFinished();

public:
    quint8 requestedModes = 0;

private slots:
    void onBluetoothDeviceFound(winrt::hstring deviceId, int watcherId);
    void onDeviceEnumerationCompleted(int watcherId);

private:
    ComPtr<IBluetoothLEAdvertisementWatcher> m_leWatcher;
    EventRegistrationToken m_leDeviceAddedToken;
    QMutex m_foundDevicesMutex;
    struct LEAdvertisingInfo {
        QList<QBluetoothUuid> services;
        ManufacturerData manufacturerData;
        qint16 rssi = 0;
    };

    QMap<quint64, LEAdvertisingInfo> m_foundLEDevicesMap;
    int m_pendingPairedDevices = 0;

    ComPtr<IBluetoothLEDeviceStatics> m_leDeviceStatics;

    static constexpr int ClassicWatcherId = 1;
    static constexpr int LowEnergyWatcherId = 2;

    std::shared_ptr<QBluetoothDeviceWatcherWinRT> m_classicWatcher;
    std::shared_ptr<QBluetoothDeviceWatcherWinRT> m_lowEnergyWatcher;
    bool m_classicScanStarted = false;
    bool m_lowEnergyScanStarted = false;
};

static void invokeIncrementPendingDevicesCount(QObject *context)
{
    QMetaObject::invokeMethod(context, "incrementPendingDevicesCount", Qt::QueuedConnection);
}

static void invokeDecrementPendingDevicesCountAndCheckFinished(QObject *context)
{
    QMetaObject::invokeMethod(context, "decrementPendingDevicesCountAndCheckFinished",
                              Qt::QueuedConnection);
}

QWinRTBluetoothDeviceDiscoveryWorker::QWinRTBluetoothDeviceDiscoveryWorker()
{
    qRegisterMetaType<QBluetoothDeviceInfo>();
    qRegisterMetaType<QBluetoothDeviceInfo::Fields>();
    qRegisterMetaType<ManufacturerData>();

    m_classicWatcher = createDeviceWatcher(ClassicDeviceSelector, ClassicWatcherId);

    HRESULT hr = GetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Devices_Bluetooth_BluetoothLEDevice).Get(), &m_leDeviceStatics);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain bluetooth le device factory",
                                           QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                           return)
}

QWinRTBluetoothDeviceDiscoveryWorker::~QWinRTBluetoothDeviceDiscoveryWorker()
{
    stop();
}

void QWinRTBluetoothDeviceDiscoveryWorker::start(QBluetoothDeviceDiscoveryAgent::DiscoveryMethods methods)
{
    requestedModes = methods;

    if (requestedModes & QBluetoothDeviceDiscoveryAgent::LowEnergyMethod) {
        startDeviceDiscovery(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
        setupLEDeviceWatcher();
    }

    if (requestedModes & QBluetoothDeviceDiscoveryAgent::ClassicMethod) {
        if (m_classicWatcher && m_classicWatcher->init()) {
            m_classicWatcher->start();
            m_classicScanStarted = true;
        } else {
            generateError(QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                          "Could not start classic device watcher");
            // do not return here, because we might still start LE scan
        }
    }
    // TODO - do the same for LE scan

    qCDebug(QT_BT_WINDOWS) << "Worker started";
}

void QWinRTBluetoothDeviceDiscoveryWorker::stop()
{
    m_classicWatcher->stop();
    if (m_leWatcher) {
        HRESULT hr = m_leWatcher->Stop();
        EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not stop le watcher",
                                               QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                               return)
        if (m_leDeviceAddedToken.value) {
            hr = m_leWatcher->remove_Received(m_leDeviceAddedToken);
            EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could remove le watcher token",
                                                   QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                                   return)
        }
    }
}

void QWinRTBluetoothDeviceDiscoveryWorker::startDeviceDiscovery(QBluetoothDeviceDiscoveryAgent::DiscoveryMethod mode)
{
    HString deviceSelector;
    ComPtr<IDeviceInformationStatics> deviceInformationStatics;
    HRESULT hr = GetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Devices_Enumeration_DeviceInformation).Get(), &deviceInformationStatics);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain device information statics",
                                           QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                           return);
    if (mode == QBluetoothDeviceDiscoveryAgent::LowEnergyMethod)
        m_leDeviceStatics->GetDeviceSelector(deviceSelector.GetAddressOf());
    else
        return; // Classic scan is now implemented using C++/WinRT DeviceWatcher

    ComPtr<IAsyncOperation<DeviceInformationCollection *>> op;
    hr = deviceInformationStatics->FindAllAsyncAqsFilter(deviceSelector.Get(), &op);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not start bluetooth device discovery operation",
                                           QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                           return);
    QPointer<QWinRTBluetoothDeviceDiscoveryWorker> thisPointer(this);
    hr = op->put_Completed(
        Callback<IAsyncOperationCompletedHandler<DeviceInformationCollection *>>([thisPointer, mode](IAsyncOperation<DeviceInformationCollection *> *op, AsyncStatus status) {
        if (status == Completed && thisPointer)
            thisPointer->onDeviceDiscoveryFinished(op, mode);
        return S_OK;
    }).Get());
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not add device discovery callback",
                                           QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                           return);
}

void QWinRTBluetoothDeviceDiscoveryWorker::onDeviceDiscoveryFinished(IAsyncOperation<DeviceInformationCollection *> *op, QBluetoothDeviceDiscoveryAgent::DiscoveryMethod mode)
{
    qCDebug(QT_BT_WINDOWS) << (mode == QBluetoothDeviceDiscoveryAgent::ClassicMethod ? "BT" : "BTLE")
        << "scan completed";
    ComPtr<IVectorView<DeviceInformation *>> devices;
    HRESULT hr;
    hr = op->GetResults(&devices);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain discovery result",
                                           QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                           return);
    quint32 deviceCount;
    hr = devices->get_Size(&deviceCount);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain discovery result size",
                                           QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                           return);

    m_pendingPairedDevices += deviceCount;
    gatherMultipleDeviceInformation(deviceCount, devices.Get(), mode);
}

void QWinRTBluetoothDeviceDiscoveryWorker::gatherDeviceInformation(IDeviceInformation *deviceInfo, QBluetoothDeviceDiscoveryAgent::DiscoveryMethod mode)
{
    HString deviceId;
    HRESULT hr;
    hr = deviceInfo->get_Id(deviceId.GetAddressOf());
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain device ID",
                                           QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                           return);
    if (mode == QBluetoothDeviceDiscoveryAgent::LowEnergyMethod)
        leBluetoothInfoFromDeviceIdAsync(deviceId.Get());
}

void QWinRTBluetoothDeviceDiscoveryWorker::gatherMultipleDeviceInformation(quint32 deviceCount, IVectorView<DeviceInformation *> *devices, QBluetoothDeviceDiscoveryAgent::DiscoveryMethod mode)
{
    for (quint32 i = 0; i < deviceCount; ++i) {
        ComPtr<IDeviceInformation> device;
        HRESULT hr;
        hr = devices->GetAt(i, &device);
        EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain device",
                                               QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                               return);
        gatherDeviceInformation(device.Get(), mode);
    }
}

HRESULT QWinRTBluetoothDeviceDiscoveryWorker::onBluetoothLEAdvertisementReceived(IBluetoothLEAdvertisementReceivedEventArgs *args)
{
    quint64 address;
    HRESULT hr;
    hr = args->get_BluetoothAddress(&address);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain bluetooth address",
                                   QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                   return S_OK);
    qint16 rssi;
    hr = args->get_RawSignalStrengthInDBm(&rssi);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain signal strength",
                                   QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                   return S_OK);
    ComPtr<IBluetoothLEAdvertisement> ad;
    hr = args->get_Advertisement(&ad);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could get advertisement",
                                   QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                   return S_OK);
    const ManufacturerData manufacturerData = extractManufacturerData(ad);
    QBluetoothDeviceInfo::Fields changedFields = QBluetoothDeviceInfo::Field::None;
    ComPtr<IVector<GUID>> guids;
    hr = ad->get_ServiceUuids(&guids);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain service uuid list",
                                   QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                   return S_OK);
    quint32 size;
    hr = guids->get_Size(&size);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain service uuid list size",
                                   QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                   return S_OK);
    QList<QBluetoothUuid> serviceUuids;
    for (quint32 i = 0; i < size; ++i) {
        GUID guid;
        hr = guids->GetAt(i, &guid);
        EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain uuid",
                                       QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                       return S_OK);
        QBluetoothUuid uuid(guid);
        serviceUuids.append(uuid);
    }

    { // scope for QMutexLocker
        QMutexLocker locker(&m_foundDevicesMutex);
        // Merge newly found services with list of currently found ones
        if (m_foundLEDevicesMap.contains(address)) {
            const LEAdvertisingInfo adInfo = m_foundLEDevicesMap.value(address);
            QList<QBluetoothUuid> foundServices = adInfo.services;
            if (adInfo.rssi != rssi) {
                m_foundLEDevicesMap[address].rssi = rssi;
                changedFields.setFlag(QBluetoothDeviceInfo::Field::RSSI);
            }
            if (adInfo.manufacturerData != manufacturerData) {
                m_foundLEDevicesMap[address].manufacturerData.insert(manufacturerData);
                if (adInfo.manufacturerData != m_foundLEDevicesMap[address].manufacturerData)
                    changedFields.setFlag(QBluetoothDeviceInfo::Field::ManufacturerData);
            }
            bool newServiceAdded = false;
            for (const QBluetoothUuid &uuid : qAsConst(serviceUuids)) {
                if (!foundServices.contains(uuid)) {
                    foundServices.append(uuid);
                    newServiceAdded = true;
                }
            }
            if (!newServiceAdded) {
                if (!changedFields.testFlag(QBluetoothDeviceInfo::Field::None)) {
                    QMetaObject::invokeMethod(this, "deviceDataChanged", Qt::AutoConnection,
                                              Q_ARG(QBluetoothAddress, QBluetoothAddress(address)),
                                              Q_ARG(QBluetoothDeviceInfo::Fields, changedFields),
                                              Q_ARG(qint16, rssi),
                                              Q_ARG(ManufacturerData, manufacturerData));
                }
                return S_OK;
            }
            m_foundLEDevicesMap[address].services = foundServices;
        } else {
            LEAdvertisingInfo info;
            info.services = std::move(serviceUuids);
            info.manufacturerData = std::move(manufacturerData);
            info.rssi = rssi;
            m_foundLEDevicesMap.insert(address, info);
        }
    }
    leBluetoothInfoFromAddressAsync(address);
    return S_OK;
}

void QWinRTBluetoothDeviceDiscoveryWorker::setupLEDeviceWatcher()
{
    HRESULT hr = RoActivateInstance(HString::MakeReference(RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEAdvertisementWatcher).Get(), &m_leWatcher);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not create advertisment watcher",
                                           QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                           return);
    hr = m_leWatcher->put_ScanningMode(BluetoothLEScanningMode_Active);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not set scanning mode",
                                           QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                           return);
    QPointer<QWinRTBluetoothDeviceDiscoveryWorker> thisPointer(this);
    hr = m_leWatcher->add_Received(
                Callback<ITypedEventHandler<BluetoothLEAdvertisementWatcher *, BluetoothLEAdvertisementReceivedEventArgs *>>(
                    [thisPointer](IBluetoothLEAdvertisementWatcher *, IBluetoothLEAdvertisementReceivedEventArgs *args) {
        if (thisPointer)
            return thisPointer->onBluetoothLEAdvertisementReceived(args);

        return S_OK;
    }).Get(), &m_leDeviceAddedToken);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not add device callback",
                                   QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                   return);
    hr = m_leWatcher->Start();
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not start device watcher",
                                   QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                   return);
}

void QWinRTBluetoothDeviceDiscoveryWorker::finishDiscovery()
{
    stop();
    emit scanFinished();
}

void QWinRTBluetoothDeviceDiscoveryWorker::onBluetoothDeviceFound(winrt::hstring deviceId, int watcherId)
{
    if (watcherId == ClassicWatcherId)
        getClassicDeviceFromId(deviceId);
    // TODO - handle LE device
}

void QWinRTBluetoothDeviceDiscoveryWorker::onDeviceEnumerationCompleted(int watcherId)
{
    qCDebug(QT_BT_WINDOWS) << (watcherId == ClassicWatcherId ? "BT" : "BTLE")
                           << "enumeration completed";
    if (watcherId == ClassicWatcherId) {
        m_classicWatcher->stop();
        m_classicScanStarted = false;
    } else if (watcherId == LowEnergyWatcherId) {
        m_lowEnergyWatcher->stop();
        m_lowEnergyScanStarted = false;
    }
    // TODO - probably reconsider this condition later
    if (!m_lowEnergyScanStarted && !m_classicScanStarted && !m_pendingPairedDevices
            && !(requestedModes & QBluetoothDeviceDiscoveryAgent::LowEnergyMethod)) {
        finishDiscovery();
    }
}

std::shared_ptr<QBluetoothDeviceWatcherWinRT>
QWinRTBluetoothDeviceDiscoveryWorker::createDeviceWatcher(winrt::hstring selector, int watcherId)
{
    auto watcher = std::make_shared<QBluetoothDeviceWatcherWinRT>(
                watcherId, selector,
                winrt::Windows::Devices::Enumeration::DeviceInformationKind::AssociationEndpoint);
    if (watcher) {
        connect(watcher.get(), &QBluetoothDeviceWatcherWinRT::deviceAdded,
                this, &QWinRTBluetoothDeviceDiscoveryWorker::onBluetoothDeviceFound,
                Qt::QueuedConnection);
        connect(watcher.get(), &QBluetoothDeviceWatcherWinRT::enumerationCompleted,
                this, &QWinRTBluetoothDeviceDiscoveryWorker::onDeviceEnumerationCompleted,
                Qt::QueuedConnection);
    }
    return watcher;
}

void QWinRTBluetoothDeviceDiscoveryWorker::generateError(
        QBluetoothDeviceDiscoveryAgent::Error error, const char *msg)
{
    emit errorOccured(error);
    qCWarning(QT_BT_WINDOWS) << msg;
}

// this function executes in main worker thread
void QWinRTBluetoothDeviceDiscoveryWorker::getClassicDeviceFromId(const winrt::hstring &id)
{
    ++m_pendingPairedDevices;
    auto asyncOp = WinRtBluetoothDevice::FromIdAsync(id);
    auto thisPtr = shared_from_this();
    asyncOp.Completed([thisPtr](auto &&op, WinRtAsyncStatus status) {
        if (thisPtr) {
            if (status == WinRtAsyncStatus::Completed) {
                WinRtBluetoothDevice device = op.GetResults();
                if (device) {
                    thisPtr->handleClassicDevice(device);
                    return;
                }
            }
            // status != Completed or failed to extract result
            qCDebug(QT_BT_WINDOWS) << "Failed to get Classic device from id";
            invokeDecrementPendingDevicesCountAndCheckFinished(thisPtr.get());
        }
    });
}
\
// this is a callback - executes in a new thread
void QWinRTBluetoothDeviceDiscoveryWorker::handleClassicDevice(const WinRtBluetoothDevice &device)
{
    const uint64_t address = device.BluetoothAddress();
    const std::wstring name { device.Name() }; // via operator std::wstring_view()
    const QString btName = QString::fromStdWString(name);
    const uint32_t deviceClass = device.ClassOfDevice().RawValue();
    auto thisPtr = shared_from_this();
    auto asyncOp = device.GetRfcommServicesAsync();
    asyncOp.Completed([thisPtr, address, btName, deviceClass](auto &&op, WinRtAsyncStatus status) {
        if (thisPtr) {
            if (status == WinRtAsyncStatus::Completed) {
                auto servicesResult = op.GetResults();
                if (servicesResult) {
                    thisPtr->handleRfcommServices(servicesResult, address, btName, deviceClass);
                    return;
                }
            }
            // Failed to get services
            qCDebug(QT_BT_WINDOWS) << "Failed to get RFCOMM services for device" << btName;
            invokeDecrementPendingDevicesCountAndCheckFinished(thisPtr.get());
        }
    });
}

// this is a callback - executes in a new thread
void QWinRTBluetoothDeviceDiscoveryWorker::handleRfcommServices(
        const WinRtRfcommDeviceServicesResult &servicesResult, uint64_t address,
        const QString &name, uint32_t classOfDeviceInt)
{
    // need to perform the check even if some of the operations fails
    auto guard = qScopeGuard([this]() {
        invokeDecrementPendingDevicesCountAndCheckFinished(this);
    });
    Q_UNUSED(guard); // to suppress warning

    const auto error = servicesResult.Error();
    if (error != winrt::Windows::Devices::Bluetooth::BluetoothError::Success) {
        qCWarning(QT_BT_WINDOWS) << "Obtain device services completed with BluetoothError"
                                 << static_cast<int>(error);
        return;
    }

    const auto services = servicesResult.Services();
    QList<QBluetoothUuid> uuids;
    for (const auto &service : services) {
        const auto serviceId = service.ServiceId();
        const winrt::guid serviceUuid = serviceId.Uuid();
        // A cast from winrt::guid does not work :(
        const GUID uuid {
            serviceUuid.Data1,
            serviceUuid.Data2,
            serviceUuid.Data3,
            { serviceUuid.Data4[0], serviceUuid.Data4[1], serviceUuid.Data4[2],
              serviceUuid.Data4[3], serviceUuid.Data4[4], serviceUuid.Data4[5],
              serviceUuid.Data4[6], serviceUuid.Data4[7] }
        };
        uuids.append(QBluetoothUuid(uuid));
    }

    const QBluetoothAddress btAddress(address);

    qCDebug(QT_BT_WINDOWS) << "Discovered BT device: " << btAddress << name
                           << "Num UUIDs" << uuids.size();

    QBluetoothDeviceInfo info(btAddress, name, classOfDeviceInt);
    info.setCoreConfigurations(QBluetoothDeviceInfo::BaseRateCoreConfiguration);
    info.setServiceUuids(uuids);
    info.setCached(true);

    QMetaObject::invokeMethod(this, "deviceFound", Qt::AutoConnection,
                              Q_ARG(QBluetoothDeviceInfo, info));
}

void QWinRTBluetoothDeviceDiscoveryWorker::incrementPendingDevicesCount()
{
    ++m_pendingPairedDevices;
}

void QWinRTBluetoothDeviceDiscoveryWorker::decrementPendingDevicesCountAndCheckFinished()
{
    if ((--m_pendingPairedDevices == 0) && !m_classicScanStarted && !m_lowEnergyScanStarted
        && !(requestedModes & QBluetoothDeviceDiscoveryAgent::LowEnergyMethod)) {
        finishDiscovery();
    }
}

// "deviceFound" will be emitted at the end of the deviceFromIdOperation callback
void QWinRTBluetoothDeviceDiscoveryWorker::leBluetoothInfoFromDeviceIdAsync(HSTRING deviceId)
{
    // Note: in this method we do not need to call
    // decrementPairedDevicesAndCheckFinished() because we *do* run LE
    // scanning, so the condition in the check will always be false.
    // It's enough to just decrement m_pendingPairedDevices.
    ComPtr<IAsyncOperation<BluetoothLEDevice *>> deviceFromIdOperation;
    // on Windows 10 FromIdAsync might ask for device permission. We cannot wait here but have to handle that asynchronously
    HRESULT hr = m_leDeviceStatics->FromIdAsync(deviceId, &deviceFromIdOperation);
    if (FAILED(hr)) {
        emit errorOccured(QBluetoothDeviceDiscoveryAgent::UnknownError);
        --m_pendingPairedDevices;
        qCWarning(QT_BT_WINDOWS) << "Could not obtain bluetooth device from id";
        return;
    }
    QPointer<QWinRTBluetoothDeviceDiscoveryWorker> thisPointer(this);
    hr = deviceFromIdOperation->put_Completed(Callback<IAsyncOperationCompletedHandler<BluetoothLEDevice *>>
                                              ([thisPointer] (IAsyncOperation<BluetoothLEDevice *> *op, AsyncStatus status)
    {
        if (thisPointer) {
            if (status == Completed)
                thisPointer->onPairedBluetoothLEDeviceFoundAsync(op, status);
            else
                --thisPointer->m_pendingPairedDevices;
        }
        return S_OK;
    }).Get());
    if (FAILED(hr)) {
        emit errorOccured(QBluetoothDeviceDiscoveryAgent::UnknownError);
        --m_pendingPairedDevices;
        qCWarning(QT_BT_WINDOWS) << "Could not register device found callback";
        return;
    }
}

// "deviceFound" will be emitted at the end of the deviceFromAdressOperation callback
void QWinRTBluetoothDeviceDiscoveryWorker::leBluetoothInfoFromAddressAsync(quint64 address)
{
    ComPtr<IAsyncOperation<BluetoothLEDevice *>> deviceFromAddressOperation;
    // on Windows 10 FromBluetoothAddressAsync might ask for device permission. We cannot wait
    // here but have to handle that asynchronously
    HRESULT hr = m_leDeviceStatics->FromBluetoothAddressAsync(address, &deviceFromAddressOperation);
    if (FAILED(hr)) {
        emit errorOccured(QBluetoothDeviceDiscoveryAgent::UnknownError);
        qCWarning(QT_BT_WINDOWS) << "Could not obtain bluetooth device from address";
        return;
    }
    QPointer<QWinRTBluetoothDeviceDiscoveryWorker> thisPointer(this);
    hr = deviceFromAddressOperation->put_Completed(Callback<IAsyncOperationCompletedHandler<BluetoothLEDevice *>>
                                                   ([thisPointer](IAsyncOperation<BluetoothLEDevice *> *op, AsyncStatus status)
    {
        if (status == Completed && thisPointer)
            thisPointer->onBluetoothLEDeviceFoundAsync(op, status);
        return S_OK;
    }).Get());
    if (FAILED(hr)) {
        emit errorOccured(QBluetoothDeviceDiscoveryAgent::UnknownError);
        qCWarning(QT_BT_WINDOWS) << "Could not register device found callback";
        return;
    }
}

HRESULT QWinRTBluetoothDeviceDiscoveryWorker::onPairedBluetoothLEDeviceFoundAsync(IAsyncOperation<BluetoothLEDevice *> *op, AsyncStatus status)
{
    --m_pendingPairedDevices;
    if (status != AsyncStatus::Completed)
        return S_OK;

    ComPtr<IBluetoothLEDevice> device;
    HRESULT hr;
    hr = op->GetResults(&device);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain bluetooth le device",
                                   QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                   return S_OK);
    return onBluetoothLEDeviceFound(device);
}

HRESULT QWinRTBluetoothDeviceDiscoveryWorker::onBluetoothLEDeviceFoundAsync(IAsyncOperation<BluetoothLEDevice *> *op, AsyncStatus status)
{
    if (status != AsyncStatus::Completed)
        return S_OK;

    ComPtr<IBluetoothLEDevice> device;
    HRESULT hr;
    hr = op->GetResults(&device);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain bluetooth le device",
                                   QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                   return S_OK);
    return onBluetoothLEDeviceFound(device);
}

static void invokeDeviceFoundWithDebug(QWinRTBluetoothDeviceDiscoveryWorker *worker,
                                       const QBluetoothDeviceInfo &info)
{
    qCDebug(QT_BT_WINDOWS) << "Discovered BTLE device: " << info.address() << info.name()
                           << "Num UUIDs" << info.serviceUuids().count() << "RSSI:" << info.rssi()
                           << "Num manufacturer data" << info.manufacturerData().count();

    QMetaObject::invokeMethod(worker, "deviceFound", Qt::AutoConnection,
                              Q_ARG(QBluetoothDeviceInfo, info));
}

HRESULT QWinRTBluetoothDeviceDiscoveryWorker::onBluetoothLEDeviceFound(ComPtr<IBluetoothLEDevice> device)
{
    if (!device) {
        qCDebug(QT_BT_WINDOWS) << "onBluetoothLEDeviceFound: No device given";
        return S_OK;
    }

    UINT64 address;
    HString name;
    HRESULT hr = device->get_BluetoothAddress(&address);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain bluetooth address",
                                           QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                           return S_OK);
    hr = device->get_Name(name.GetAddressOf());
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain device name",
                                           QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                           return S_OK);
    const QString btName = QString::fromWCharArray(WindowsGetStringRawBuffer(name.Get(), nullptr));

    ComPtr<IBluetoothLEDevice2> device2;
    hr = device.As(&device2);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not cast device",
                                           QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                           return S_OK);
    ComPtr<IDeviceInformation> deviceInfo;
    hr = device2->get_DeviceInformation(&deviceInfo);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain device info",
                                           QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                           return S_OK);
    if (!deviceInfo) {
        qCDebug(QT_BT_WINDOWS) << "onBluetoothLEDeviceFound: Could not obtain device information";
        return S_OK;
    }
    ComPtr<IDeviceInformation2> deviceInfo2;
    hr = deviceInfo.As(&deviceInfo2);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain cast device info",
                                           QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                           return S_OK);
    ComPtr<IDeviceInformationPairing> pairing;
    hr = deviceInfo2->get_Pairing(&pairing);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain pairing information",
                                           QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                           return S_OK);
    boolean isPaired;
    hr = pairing->get_IsPaired(&isPaired);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain pairing status",
                                           QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                           return S_OK);

    const LEAdvertisingInfo adInfo = m_foundLEDevicesMap.value(address);
    const ManufacturerData manufacturerData = adInfo.manufacturerData;
    const qint16 rssi = adInfo.rssi;

    QBluetoothDeviceInfo info(QBluetoothAddress(address), btName, 0);
    info.setCoreConfigurations(QBluetoothDeviceInfo::LowEnergyCoreConfiguration);
    info.setRssi(rssi);
    for (quint16 key : manufacturerData.keys())
        info.setManufacturerData(key, manufacturerData.value(key));
    info.setCached(true);

    // Use the services obtained from the advertisement data if the device is not paired
    if (!isPaired) {
        info.setServiceUuids(adInfo.services);
        invokeDeviceFoundWithDebug(this, info);
    } else {
        ComPtr<IBluetoothLEDevice3> device3;
        hr = device.As(&device3);
        EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Failed to obtain IBluetoothLEDevice3 instance",
                                               QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                               return S_OK);

        ComPtr<IAsyncOperation<GenericAttributeProfile::GattDeviceServicesResult *>> servicesOp;
        hr = device3->GetGattServicesAsync(&servicesOp);
        EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Failed to execute async services request",
                                               QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                               return S_OK);

        QPointer<QWinRTBluetoothDeviceDiscoveryWorker> thisPtr(this);
        hr = servicesOp->put_Completed(
                Callback<IAsyncOperationCompletedHandler<
                    GenericAttributeProfile::GattDeviceServicesResult *>>([thisPtr, info](
                        IAsyncOperation<GenericAttributeProfile::GattDeviceServicesResult *> *op,
                        AsyncStatus status) mutable {
                            if (thisPtr)
                                thisPtr->onLeServicesReceived(op, status, info);
                            return S_OK;
                        }).Get());
        EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not add LE services discovery callback",
                                               QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                               return S_OK);
    }

    return S_OK;
}

HRESULT QWinRTBluetoothDeviceDiscoveryWorker::onLeServicesReceived(
        IAsyncOperation<GenericAttributeProfile::GattDeviceServicesResult *> *op,
        AsyncStatus status, QBluetoothDeviceInfo &info)
{
    if (status != AsyncStatus::Completed) {
        qCWarning(QT_BT_WINDOWS) << "LE service request finished with status"
                                 << static_cast<int>(status);
        return S_OK;
    }

    ComPtr<GenericAttributeProfile::IGattDeviceServicesResult> servicesResult;
    HRESULT hr = op->GetResults(&servicesResult);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not get async operation result for LE services",
                                           QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                           return S_OK);

    GenericAttributeProfile::GattCommunicationStatus commStatus;
    hr = servicesResult->get_Status(&commStatus);
    EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain services status",
                                           QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                           return S_OK);

    if (commStatus == GenericAttributeProfile::GattCommunicationStatus_Success) {
        IVectorView<GenericAttributeProfile::GattDeviceService *> *deviceServices;
        hr = servicesResult->get_Services(&deviceServices);
        EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain gatt service list",
                                               QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                               return S_OK);
        uint serviceCount;
        hr = deviceServices->get_Size(&serviceCount);
        EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain gatt service list size",
                                               QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                               return S_OK);
        QList<QBluetoothUuid> uuids;
        for (uint i = 0; i < serviceCount; ++i) {
            ComPtr<GenericAttributeProfile::IGattDeviceService> service;
            hr = deviceServices->GetAt(i, &service);
            EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain gatt service",
                                                   QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                                   return S_OK);
            GUID uuid;
            hr = service->get_Uuid(&uuid);
            EMIT_WORKER_ERROR_AND_RETURN_IF_FAILED("Could not obtain uuid",
                                                   QBluetoothDeviceDiscoveryAgent::Error::UnknownError,
                                                   return S_OK);
            uuids.append(QBluetoothUuid(uuid));
        }
        info.setServiceUuids(uuids);
    } else {
        qCWarning(QT_BT_WINDOWS) << "Obtaining LE services finished with status"
                                 << static_cast<int>(commStatus);
    }
    invokeDeviceFoundWithDebug(this, info);

    return S_OK;
}

QBluetoothDeviceDiscoveryAgentPrivate::QBluetoothDeviceDiscoveryAgentPrivate(
                const QBluetoothAddress &deviceAdapter,
                QBluetoothDeviceDiscoveryAgent *parent)
    :   q_ptr(parent)
{
    Q_UNUSED(deviceAdapter);
    mainThreadCoInit(this);
}

QBluetoothDeviceDiscoveryAgentPrivate::~QBluetoothDeviceDiscoveryAgentPrivate()
{
    disconnectAndClearWorker();
    mainThreadCoUninit(this);
}

bool QBluetoothDeviceDiscoveryAgentPrivate::isActive() const
{
    return worker != nullptr;
}

QBluetoothDeviceDiscoveryAgent::DiscoveryMethods QBluetoothDeviceDiscoveryAgent::supportedDiscoveryMethods()
{
    return (ClassicMethod | LowEnergyMethod);
}

void QBluetoothDeviceDiscoveryAgentPrivate::start(QBluetoothDeviceDiscoveryAgent::DiscoveryMethods methods)
{
    if (worker)
        return;

    worker = std::make_shared<QWinRTBluetoothDeviceDiscoveryWorker>();
    discoveredDevices.clear();
    connect(worker.get(), &QWinRTBluetoothDeviceDiscoveryWorker::deviceFound,
            this, &QBluetoothDeviceDiscoveryAgentPrivate::registerDevice);
    connect(worker.get(), &QWinRTBluetoothDeviceDiscoveryWorker::deviceDataChanged,
            this, &QBluetoothDeviceDiscoveryAgentPrivate::updateDeviceData);
    connect(worker.get(), &QWinRTBluetoothDeviceDiscoveryWorker::errorOccured,
            this, &QBluetoothDeviceDiscoveryAgentPrivate::onErrorOccured);
    connect(worker.get(), &QWinRTBluetoothDeviceDiscoveryWorker::scanFinished,
            this, &QBluetoothDeviceDiscoveryAgentPrivate::onScanFinished);
    worker->start(methods);

    if (lowEnergySearchTimeout > 0 && methods & QBluetoothDeviceDiscoveryAgent::LowEnergyMethod) { // otherwise no timeout and stop() required
        if (!leScanTimer) {
            leScanTimer = new QTimer(this);
            leScanTimer->setSingleShot(true);
        }
        connect(leScanTimer, &QTimer::timeout,
            worker.get(), &QWinRTBluetoothDeviceDiscoveryWorker::finishDiscovery);
        leScanTimer->setInterval(lowEnergySearchTimeout);
        leScanTimer->start();
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::stop()
{
    Q_Q(QBluetoothDeviceDiscoveryAgent);
    if (worker) {
        worker->stop();
        disconnectAndClearWorker();
        emit q->canceled();
    }
    if (leScanTimer)
        leScanTimer->stop();
}

void QBluetoothDeviceDiscoveryAgentPrivate::registerDevice(const QBluetoothDeviceInfo &info)
{
    Q_Q(QBluetoothDeviceDiscoveryAgent);

    for (QList<QBluetoothDeviceInfo>::iterator iter = discoveredDevices.begin();
        iter != discoveredDevices.end(); ++iter) {
        if (iter->address() == info.address()) {
            qCDebug(QT_BT_WINDOWS) << "Updating device" << iter->name() << iter->address();
            // merge service uuids
            QList<QBluetoothUuid> uuids = iter->serviceUuids();
            uuids.append(info.serviceUuids());
            const QSet<QBluetoothUuid> uuidSet(uuids.begin(), uuids.end());
            if (iter->serviceUuids().count() != uuidSet.count())
                iter->setServiceUuids(uuidSet.values().toVector());
            if (iter->coreConfigurations() != info.coreConfigurations())
                iter->setCoreConfigurations(QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration);
            return;
        }
    }

    discoveredDevices << info;
    emit q->deviceDiscovered(info);
}

void QBluetoothDeviceDiscoveryAgentPrivate::updateDeviceData(const QBluetoothAddress &address,
                                                             QBluetoothDeviceInfo::Fields fields,
                                                             qint16 rssi,
                                                             ManufacturerData manufacturerData)
{
    if (fields.testFlag(QBluetoothDeviceInfo::Field::None))
        return;

    Q_Q(QBluetoothDeviceDiscoveryAgent);
    for (QList<QBluetoothDeviceInfo>::iterator iter = discoveredDevices.begin();
        iter != discoveredDevices.end(); ++iter) {
        if (iter->address() == address) {
            qCDebug(QT_BT_WINDOWS) << "Updating data for device" << iter->name() << iter->address();
            if (fields.testFlag(QBluetoothDeviceInfo::Field::RSSI))
                iter->setRssi(rssi);
            if (fields.testFlag(QBluetoothDeviceInfo::Field::ManufacturerData))
                for (quint16 key : manufacturerData.keys())
                    iter->setManufacturerData(key, manufacturerData.value(key));
            emit q->deviceUpdated(*iter, fields);
            return;
        }
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::onErrorOccured(QBluetoothDeviceDiscoveryAgent::Error e)
{
    Q_Q(QBluetoothDeviceDiscoveryAgent);
    lastError = e;
    emit q->errorOccurred(e);
}

void QBluetoothDeviceDiscoveryAgentPrivate::onScanFinished()
{
    Q_Q(QBluetoothDeviceDiscoveryAgent);
    disconnectAndClearWorker();
    emit q->finished();
}

void QBluetoothDeviceDiscoveryAgentPrivate::disconnectAndClearWorker()
{
    if (!worker)
        return;

    disconnect(worker.get(), &QWinRTBluetoothDeviceDiscoveryWorker::scanFinished,
               this, &QBluetoothDeviceDiscoveryAgentPrivate::onScanFinished);
    disconnect(worker.get(), &QWinRTBluetoothDeviceDiscoveryWorker::deviceFound,
               this, &QBluetoothDeviceDiscoveryAgentPrivate::registerDevice);
    disconnect(worker.get(), &QWinRTBluetoothDeviceDiscoveryWorker::deviceDataChanged,
               this, &QBluetoothDeviceDiscoveryAgentPrivate::updateDeviceData);
    disconnect(worker.get(), &QWinRTBluetoothDeviceDiscoveryWorker::errorOccured,
               this, &QBluetoothDeviceDiscoveryAgentPrivate::onErrorOccured);
    if (leScanTimer) {
        disconnect(leScanTimer, &QTimer::timeout,
                   worker.get(), &QWinRTBluetoothDeviceDiscoveryWorker::finishDiscovery);
    }

    worker = nullptr;
}

QT_END_NAMESPACE

#include <qbluetoothdevicediscoveryagent_winrt.moc>
