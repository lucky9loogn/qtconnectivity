/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2013 Javier S. Pedro <maemo@javispedro.com>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtBluetooth/QLowEnergyService>

#include <algorithm>

#include "qlowenergycontroller_p.h"
#include "qlowenergyserviceprivate_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QLowEnergyService
    \inmodule QtBluetooth
    \brief The QLowEnergyService class represents an individual service
    on a Bluetooth Low Energy Device.

    \since 5.4

    QLowEnergyService provides access to the details of Bluetooth Low Energy
    services. The class facilitates the discovery and publification of
    service details, permits reading and writing of the contained data
    and notifies about data changes.

    \section1 Service Structure

    A Bluetooth Low Energy peripheral device can contain multiple services.
    In turn each service may include further services. This class represents a
    single service of the peripheral device and is created via
    \l QLowEnergyController::createServiceObject(). The \l type() indicates
    whether this service is a primary (top-level) service or whether the
    service is part of another service. Each service may contain one or more
    characteristics and each characteristic may contain descriptors. The
    resulting structure may look like the following diagram:

    \image peripheral-structure.png Structure of a generic peripheral

    A characteristic is the principle information carrier. It has a
    \l {QLowEnergyCharacteristic::value()}{value()} and
    \l {QLowEnergyCharacteristic::value()}{properties()}
    describing the access permissions for the value. The general purpose
    of the contained descriptor is to further define the nature of the
    characteristic. For example, it might specify how the value is meant to be
    interpreted or whether it can notify the value consumer about value
    changes.

    \section1 Service Interaction

    Once a service object was created for the first time, its details are yet to
    be discovered. This is indicated by its current \l state() being \l DiscoveryRequired.
    It is only possible to retrieve the \l serviceUuid() and \l serviceName().

    The discovery of its included services, characteristics and descriptors
    is triggered when calling \l discoverDetails(). During the discovery the
    \l state() transitions from \l DiscoveryRequired via \l DiscoveringServices
    to its final \l ServiceDiscovered state. This transition is advertised via
    the \l stateChanged() signal. Once the details are known, all of the contained
    characteristics, descriptors and included services are known and can be read
    or written.

    The values of characteristics and descriptors can be retrieved via
    \l QLowEnergyCharacteristic and \l QLowEnergyDescriptor, respectively.
    However writing those attributes requires the service object. The
    \l writeCharacteristic() function attempts to write a new value to the given
    characteristic. If the write attempt is successful, the \l characteristicWritten()
    signal is emitted. A failure to write triggers the \l CharacteristicWriteError.
    Writing a descriptor follows the same pattern. Write requests are serialised.
    Issuing a second write request before the previous request has finished
    is delayed until the first write request has finished.

    \note Currently, it is not possible to send signed write or reliable write requests.

    \target notifications

    In some cases the peripheral generates value updates which
    the central is interested in receiving. In order for a characteristic to support
    such notifications it must have the \l QLowEnergyCharacteristic::Notify or
    \l QLowEnergyCharacteristic::Indicate property and a descriptor of type
    \l QBluetoothUuid::ClientCharacteristicConfiguration. Provided those conditions
    are fulfilled notifications can be enabled as shown in the following code segment:

    \snippet doc_src_qtbluetooth.cpp enable_btle_notifications

    The example shows a battery level characteristic which updates the central
    on every value change. The notifications are provided via
    the \l characteristicChanged() signal. More details about this mechanism
    are provided by the
    \l {https://developer.bluetooth.org/gatt/descriptors/Pages/DescriptorViewer.aspx?u=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml}{Bluetooth Specification}.

    \section1 Service Data Sharing

    Each QLowEnergyService instance shares its internal states and information
    with other QLowEnergyService instance of the same service. If one instance
    initiates the discovery of the service details, all remaining instances
    automatically follow. Therefore the following snippet always works:

    \snippet doc_src_qtbluetooth.cpp data_share_qlowenergyservice

    Other operations such as calls to \l writeCharacteristic(),
    writeDescriptor() or the invalidation of the service due to the
    related \l QLowEnergyController disconnecting from the device are shared
    the same way.

    \note This class is provided by Qt 5.4 as part of a Bluetooth Low Energy Tech Preview.
    Some API elements may change until the final release of the feature.

    \sa QLowEnergyController, QLowEnergyCharacteristic, QLowEnergyDescriptor
 */

/*!
    \enum QLowEnergyService::ServiceType

    This enum describes the type of the service.

    \value PrimaryService       The service is a top-level/primary service.
                                If this type flag is not set, the service is considered
                                to be a secondary service. Each service may be included
                                by another service which is indicated by IncludedService.
    \value IncludedService      The service is included by another service.
                                On some platforms, this flag cannot be determined until
                                the service that includes the current service was
                                discovered.
*/

/*!
    \enum QLowEnergyService::ServiceError

    This enum describes all possible error conditions during the service's
    existence. The \l error() function returns the last occurred error.

    \value NoError                  No error has occurred.
    \value OperationError           An operation was attempted while the service was not ready.
                                    An example might be the attempt to write to
                                    the service while it was not yet in the
                                    \l ServiceDiscovered \l state() or the service is invalid
                                    due to a loss of connection to the peripheral device.
    \value CharacteristicReadError  An attempt to read a characteristic value failed. For example,
                                    it might be triggered in response to a call to
                                    \l readCharacteristic(). This value was introduced by Qt 5.5.
    \value CharacteristicWriteError An attempt to write a new value to a characteristic
                                    failed. For example, it might be triggered when attempting
                                    to write to a read-only characteristic.
    \value DescriptorReadError      An attempt to read a descriptor value failed. For example,
                                    it might be triggered in response to a call to
                                    \l readDescriptor(). This value was introduced by Qt 5.5.
    \value DescriptorWriteError     An attempt to write a new value to a descriptor
                                    failed. For example, it might be triggered when attempting
                                    to write to a read-only descriptor.
    \value UnknownError             An unknown error occurred when interacting with the service.
                                    This value was introduced by Qt 5.5.
 */

/*!
    \enum QLowEnergyService::ServiceState

    This enum describes the \l state() of the service object.

    \value InvalidService       A service can become invalid when it looses
                                the connection to the underlying device. Even though
                                the connection may be lost it retains its last information.
                                An invalid service cannot become valid anymore even if
                                the connection to the device is re-established.
    \value DiscoveryRequired    The service details are yet to be discovered by calling \l discoverDetails().
                                The only reliable pieces of information are its
                                \l serviceUuid() and \l serviceName().
    \value DiscoveringServices  The service details are being discovered.
    \value ServiceDiscovered    The service details have been discovered.
 */

/*!
  \enum QLowEnergyService::WriteMode

  This enum describes the mode to be used when writing a characteristic value.
  The characteristic advertises its supported write modes via its
  \l {QLowEnergyCharacteristic::properties()}{properties}.

  \value WriteWithResponse      If a characteristic is written using this mode, the peripheral
                                shall send a write confirmation. If the operation is
                                successful, the confirmation is emitted via the
                                \l characteristicWritten() signal. Otherwise the
                                \l CharacteristicWriteError is emitted.
                                A characteristic must have set the
                                \l QLowEnergyCharacteristic::Write property to support this
                                write mode.

  \value WriteWithoutResponse   If a characteristic is written using this mode, the remote peripheral
                                shall not send a write confirmation. The operation's success
                                cannot be determined and the payload must not be longer than 20 bytes.
                                A characteristic must have set the
                                \l QLowEnergyCharacteristic::WriteNoResponse property to support this
                                write mode. Its adavantage is a quicker
                                write operation as it may happen in between other
                                device interactions.
 */

/*!
    \fn void QLowEnergyService::stateChanged(QLowEnergyService::ServiceState newState)

    This signal is emitted when the service's state changes. The \a newState can also be
    retrieved via \l state().

    \sa state()
 */

/*!
    \fn void QLowEnergyService::error(QLowEnergyService::ServiceError newError)

    This signal is emitted when an error occurrs. The \a newError parameter
    describes the error that occurred.
 */

/*!
    \fn void QLowEnergyService::characteristicRead(const QLowEnergyCharacteristic &characteristic, const QByteArray &value)

    This signal is emitted when the read request for \a characteristic successfully returned
    its \a value. The signal might be triggered by calling \l characteristicRead(). If
    the read operation is not successful, the \l error() signal is emitted using the
    \l CharacteristicReadError flag.

    \sa readCharacteristic()
    \since 5.5
 */

/*!
    \fn void QLowEnergyService::characteristicWritten(const QLowEnergyCharacteristic &characteristic, const QByteArray &newValue);

    This signal is emitted when the value of \a characteristic
    is successfully changed to \a newValue. The change must have been triggered
    by calling \l writeCharacteristic(). If the write operation is not successful,
    the \l error() signal is emitted using the \l CharacteristicWriteError flag.

    Since this signal is an indication of a successful write operation \a newValue
    generally matches the value that was passed to the associated
    \l writeCharacteristic() call. However, it may happen that the two values differ
    from each other. This can occur in cases when the written value is
    used by the remote device to trigger an operation and it returns some other value via
    the written and/or change notification. Such cases are very specific to the
    target device. In any case, the reception of the written signal can still be considered
    as a sign that the target device received the to-be-written value.

    \note If \l writeCharacteristic() is called using the \l WriteWithoutResponse mode,
    this signal and the \l error() are never emitted.

    \sa writeCharacteristic()
 */

/*!
    \fn void QLowEnergyService::characteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &newValue);

    This signal is emitted when the value of \a characteristic is changed
    by an event on the peripheral. The \a newValue parameter contains the
    updated value of the \a characteristic.

    The signal emission implies that change notifications must
    have been activated via the characteristic's
    \l {QBluetoothUuid::ClientCharacteristicConfiguration}{ClientCharacteristicConfiguration}
    descriptor prior to the change event on the peripheral. More details on how this might be
    done can be found further \l{notifications}{above}.
 */

/*!
    \fn void QLowEnergyService::descriptorRead(const QLowEnergyDescriptor &descriptor, const QByteArray &value)

    This signal is emitted when the read request for \a descriptor successfully returned
    its \a value. The signal might be triggered by calling \l descriptorRead(). If
    the read operation is not successful, the \l error() signal is emitted using the
    \l DescriptorReadError flag.

    \sa readDescriptor()
    \since 5.5
 */

/*!
    \fn void QLowEnergyService::descriptorWritten(const QLowEnergyDescriptor &descriptor, const QByteArray &newValue)

    This signal is emitted when the value of \a descriptor
    is successfully changed to \a newValue. The change must have been caused
    by calling \l writeDescriptor().

    \sa writeDescriptor()
 */

/*!
  \internal

  QLowEnergyControllerPrivate creates instances of this class.
  The user gets access to class instances via
  \l QLowEnergyController::services().
 */
QLowEnergyService::QLowEnergyService(QSharedPointer<QLowEnergyServicePrivate> p,
                                     QObject *parent)
    : QObject(parent),
      d_ptr(p)
{
    qRegisterMetaType<QLowEnergyService::ServiceState>();
    qRegisterMetaType<QLowEnergyService::ServiceError>();

    connect(p.data(), SIGNAL(error(QLowEnergyService::ServiceError)),
            this, SIGNAL(error(QLowEnergyService::ServiceError)));
    connect(p.data(), SIGNAL(stateChanged(QLowEnergyService::ServiceState)),
            this, SIGNAL(stateChanged(QLowEnergyService::ServiceState)));
    connect(p.data(), SIGNAL(characteristicChanged(QLowEnergyCharacteristic,QByteArray)),
            this, SIGNAL(characteristicChanged(QLowEnergyCharacteristic,QByteArray)));
    connect(p.data(), SIGNAL(characteristicWritten(QLowEnergyCharacteristic,QByteArray)),
            this, SIGNAL(characteristicWritten(QLowEnergyCharacteristic,QByteArray)));
    connect(p.data(), SIGNAL(descriptorWritten(QLowEnergyDescriptor,QByteArray)),
            this, SIGNAL(descriptorWritten(QLowEnergyDescriptor,QByteArray)));
}

/*!
    Destroys the \l QLowEnergyService instance.
 */
QLowEnergyService::~QLowEnergyService()
{
}

/*!
    Returns the UUIDs of all services which are included by the
    current service.

    The returned list is empty if this service instance's \l discoverDetails()
    was not yet called or there are no known characteristics.

    It is possible that an included service contains yet another service. Such
    second level includes have to be obtained via their relevant first level
    QLowEnergyService instance. Technically, this could create
    a circular dependency.

    \l {QLowEnergyController::createServiceObject()} should be used to obtain
    service instances for each of the UUIDs.

    \sa createServiceObject()
 */
QList<QBluetoothUuid> QLowEnergyService::includedServices() const
{
    return d_ptr->includedServices;
}

/*!
    Returns the current state of the service.

    If the device's service was instantiated for the first time, the object's
    state is \l DiscoveryRequired. The state of all service objects which
    point to the same service on the peripheral device are always equal.
    This is caused by the shared nature of the internal object data.
    Therefore any service object instance created after
    the first one has a state equal to already existing instances.

    A service becomes invalid if the \l QLowEnergyController disconnects
    from the remote device. An invalid service retains its internal state
    at the time of the disconnect event. This implies that once the service
    details are discovered they can even be retrieved from an invalid
    service. This permits scenarios where the device connection is established,
    the service details are retrieved and the device immediately disconnected
    to permit the next device to connect to the peripheral device.

    However, under normal circumstances the connection should remain to avoid
    repeated discovery of services and their details. The discovery may take
    a while and the client can subscribe to ongoing change notifications.

    \sa stateChanged()
 */
QLowEnergyService::ServiceState QLowEnergyService::state() const
{
    return d_ptr->state;
}

/*!
    Returns the type of the service.

    \note The type attribute cannot be relied upon until the service has
    reached the \l ServiceDiscovered state. This field is initialised
    with \l PrimaryService.

    \note On Android, it is not possible to determine whether a service
    is a primary or secondary service. Therefore all services
    have the \l PrimaryService flag set.

 */
QLowEnergyService::ServiceTypes QLowEnergyService::type() const
{
    return d_ptr->type;
}

/*!
    Returns the matching characteristic for \a uuid; otherwise an invalid
    characteristic.

    The returned characteristic is invalid if this service instance's \l discoverDetails()
    was not yet called or there are no characteristics with a matching \a uuid.

    \sa characteristics()
*/
QLowEnergyCharacteristic QLowEnergyService::characteristic(const QBluetoothUuid &uuid) const
{
    foreach (const QLowEnergyHandle handle, d_ptr->characteristicList.keys()) {
        if (d_ptr->characteristicList[handle].uuid == uuid)
            return QLowEnergyCharacteristic(d_ptr, handle);
    }

    return QLowEnergyCharacteristic();
}

/*!
    Returns all characteristics associated with this \c QLowEnergyService instance.

    The returned list is empty if this service instance's \l discoverDetails()
    was not yet called or there are no known characteristics.

    \sa characteristic(), state(), discoverDetails()
*/

QList<QLowEnergyCharacteristic> QLowEnergyService::characteristics() const
{
    QList<QLowEnergyCharacteristic> results;
    QList<QLowEnergyHandle> handles = d_ptr->characteristicList.keys();
    std::sort(handles.begin(), handles.end());

    foreach (const QLowEnergyHandle &handle, handles) {
        QLowEnergyCharacteristic characteristic(d_ptr, handle);
        results.append(characteristic);
    }
    return results;
}


/*!
    Returns the UUID of the service; otherwise a null UUID.
 */
QBluetoothUuid QLowEnergyService::serviceUuid() const
{
    return d_ptr->uuid;
}


/*!
    Returns the name of the service; otherwise an empty string.

    The returned name can only be retrieved if \l serviceUuid()
    is a \l {https://developer.bluetooth.org/gatt/services/Pages/ServicesHome.aspx}{well-known UUID}.
*/
QString QLowEnergyService::serviceName() const
{
    bool ok = false;
    quint16 clsId = d_ptr->uuid.toUInt16(&ok);
    if (ok) {
        QBluetoothUuid::ServiceClassUuid id
                = static_cast<QBluetoothUuid::ServiceClassUuid>(clsId);
        const QString name = QBluetoothUuid::serviceClassToString(id);
        if (!name.isEmpty())
            return name;
    }
    return qApp ?
           qApp->translate("QBluetoothServiceDiscoveryAgent", "Unknown Service") :
           QStringLiteral("Unknown Service");
}


/*!
    Initiates the discovery of the services, characteristics
    and descriptors contained by the service. The discovery process is indicated
    via the \l stateChanged() signal.

    \sa state()
 */
void QLowEnergyService::discoverDetails()
{
    Q_D(QLowEnergyService);

    if (!d->controller || d->state == QLowEnergyService::InvalidService) {
        d->setError(QLowEnergyService::OperationError);
        return;
    }

    if (d->state != QLowEnergyService::DiscoveryRequired)
        return;

    d->setState(QLowEnergyService::DiscoveringServices);

    d->controller->discoverServiceDetails(d->uuid);
}

/*!
    Returns the last occurred error or \l NoError.
 */
QLowEnergyService::ServiceError QLowEnergyService::error() const
{
    return d_ptr->lastError;
}

/*!
    Returns \c true if \a characteristic belongs to this service;
    otherwise \c false.

    A characteristic belongs to a service if \l characteristics()
    contains the \a characteristic.
 */
bool QLowEnergyService::contains(const QLowEnergyCharacteristic &characteristic) const
{
    if (characteristic.d_ptr.isNull() || !characteristic.data)
        return false;

    if (d_ptr == characteristic.d_ptr
        && d_ptr->characteristicList.contains(characteristic.attributeHandle())) {
        return true;
    }

    return false;
}


/*!
    Reads the value of \a characteristic. If the operation is successful, the
    \l characteristicRead() signal is emitted; otherwise the \l CharacteristicReadError
    is set.

    All descriptor and characteristic requests towards the same remote device are
    serialised. A queue is employed when issuing multiple requests at the same time.
    The queue does not eliminate duplicated read requests for the same characteristic.

    A characteristic can only be read if the service is in the \l ServiceDiscovered state,
    belongs to the service and is readable. If \a characteristic is readable its
    \l QLowEnergyCharacteristic::Read property is set.

    \sa characteristicRead()

    \since 5.5
 */
void QLowEnergyService::readCharacteristic(
        const QLowEnergyCharacteristic &characteristic)
{
    Q_D(QLowEnergyService);

    // not a characteristic of this service
    if (!contains(characteristic))
        return;

    if (state() != ServiceDiscovered || !d->controller) {
        d->setError(QLowEnergyService::OperationError);
        return;
    }

    //TODO
}

/*!
    Writes \a newValue as value for the \a characteristic. If the operation is successful,
    the \l characteristicWritten() signal is emitted; otherwise the \l CharacteristicWriteError
    is set.

    The \a mode parameter determines whether the remote device should send a write
    confirmation. The to-be-written \a characteristic must support the relevant
    write mode. The characteristic's supported write modes are indicated by its
    \l QLowEnergyCharacteristic::Write and \l QLowEnergyCharacteristic::WriteNoResponse
    properties.

    All descriptor and characteristic write requests towards the same remote device are
    serialised. A queue is employed when issuing multiple write requests at the same time.
    The queue does not eliminate duplicated write requests for the same characteristic.
    For example, if the same descriptor is set to the value A and immediately afterwards
    to B, the two write request are executed in the given order.

    \note Currently, it is not possible to use signed or reliable writes as defined by the
    Bluetooth specification.

    A characteristic can only be written if this service is in the \l ServiceDiscovered state,
    belongs to the service and is writable.

    \sa QLowEnergyCharacteristic::properties()
 */
void QLowEnergyService::writeCharacteristic(
        const QLowEnergyCharacteristic &characteristic,
        const QByteArray &newValue, QLowEnergyService::WriteMode mode)
{
    //TODO check behavior when writing to WriteSigned characteristic
    Q_D(QLowEnergyService);

    // not a characteristic of this service
    if (!contains(characteristic))
        return;

    if (state() != ServiceDiscovered)
        d->setError(QLowEnergyService::OperationError);

    if (!d->controller)
        return;

    // don't write if properties don't permit it
    if (mode == WriteWithResponse
            && (characteristic.properties() & QLowEnergyCharacteristic::Write))
    {
        d->controller->writeCharacteristic(characteristic.d_ptr,
                                       characteristic.attributeHandle(),
                                       newValue,
                                       true);
    } else if (mode == WriteWithoutResponse
               && (characteristic.properties() & QLowEnergyCharacteristic::WriteNoResponse)) {
        d->controller->writeCharacteristic(characteristic.d_ptr,
                                       characteristic.attributeHandle(),
                                       newValue,
                                       false);
    } else {
        d->setError(QLowEnergyService::OperationError);
    }
}

/*!
    Returns \c true if \a descriptor belongs to this service; otherwise \c false.
 */
bool QLowEnergyService::contains(const QLowEnergyDescriptor &descriptor) const
{
    if (descriptor.d_ptr.isNull() || !descriptor.data)
        return false;

    const QLowEnergyHandle charHandle = descriptor.characteristicHandle();
    if (!charHandle)
        return false;

    if (d_ptr == descriptor.d_ptr
        && d_ptr->characteristicList.contains(charHandle)
        && d_ptr->characteristicList[charHandle].descriptorList.contains(descriptor.handle()))
    {
        return true;
    }

    return false;
}

/*!
    Reads the value of \a descriptor. If the operation is successful, the
    \l descriptorRead() signal is emitted; otherwise the \l DescriptorReadError
    is set.

    All descriptor and characteristic requests towards the same remote device are
    serialised. A queue is employed when issuing multiple requests at the same time.
    The queue does not eliminate duplicated read requests for the same descriptor.

    A descriptor can only be written if the service is in the \l ServiceDiscovered state,
    belongs to the service and is readable.

    \sa descriptorRead()

    \since 5.5
 */
void QLowEnergyService::readDescriptor(
        const QLowEnergyDescriptor &descriptor)
{
    Q_D(QLowEnergyService);

    if (!contains(descriptor))
        return;

    if (state() != ServiceDiscovered || !d->controller) {
        d->setError(QLowEnergyService::OperationError);
        return;
    }

    //TODO implement QLowEnergyService::readDescriptor()
}

/*!
    Writes \a newValue as value for \a descriptor. If the operation is successful,
    the \l descriptorWritten() signal is emitted; otherwise the \l DescriptorWriteError
    is emitted.

    All descriptor and characteristic requests towards the same remote device are
    serialised. A queue is employed when issuing multiple write requests at the same time.
    The queue does not eliminate duplicated write requests for the same descriptor.
    For example, if the same descriptor is set to the value A and immediately afterwards
    to B, the two write request are executed in the given order.

    A descriptor can only be written if this service is in the \l ServiceDiscovered state,
    belongs to the service and is writable.
 */
void QLowEnergyService::writeDescriptor(const QLowEnergyDescriptor &descriptor,
                                        const QByteArray &newValue)
{
    Q_D(QLowEnergyService);

    if (!contains(descriptor))
        return;

    if (state() != ServiceDiscovered || !d->controller) {
        d->setError(QLowEnergyService::OperationError);
        return;
    }

    d->controller->writeDescriptor(descriptor.d_ptr,
                                   descriptor.characteristicHandle(),
                                   descriptor.handle(),
                                   newValue);
}

QT_END_NAMESPACE
