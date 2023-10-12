/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp -I QtCore/private/qglobal_p.h -p leadvertisingmanager1_p.h:leadvertisingmanager1.cpp org.bluez.LEAdvertisingManager1.xml --moc
 *
 * qdbusxml2cpp is Copyright (C) 2022 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#ifndef LEADVERTISINGMANAGER1_P_H
#define LEADVERTISINGMANAGER1_P_H

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

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtDBus/QtDBus>
#include <QtCore/private/qglobal_p.h>

/*
 * Proxy class for interface org.bluez.LEAdvertisingManager1
 */
class OrgBluezLEAdvertisingManager1Interface: public QDBusAbstractInterface
{
    Q_OBJECT
public:
    static inline const char *staticInterfaceName()
    { return "org.bluez.LEAdvertisingManager1"; }

public:
    OrgBluezLEAdvertisingManager1Interface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = nullptr);

    ~OrgBluezLEAdvertisingManager1Interface();

    Q_PROPERTY(uchar ActiveInstances READ activeInstances)
    inline uchar activeInstances() const
    { return qvariant_cast< uchar >(property("ActiveInstances")); }

    Q_PROPERTY(QStringList SupportedIncludes READ supportedIncludes)
    inline QStringList supportedIncludes() const
    { return qvariant_cast< QStringList >(property("SupportedIncludes")); }

    Q_PROPERTY(uchar SupportedInstances READ supportedInstances)
    inline uchar supportedInstances() const
    { return qvariant_cast< uchar >(property("SupportedInstances")); }

public Q_SLOTS: // METHODS
    inline QDBusPendingReply<> RegisterAdvertisement(const QDBusObjectPath &advertisement, const QVariantMap &options)
    {
        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(advertisement) << QVariant::fromValue(options);
        return asyncCallWithArgumentList(QStringLiteral("RegisterAdvertisement"), argumentList);
    }

    inline QDBusPendingReply<> UnregisterAdvertisement(const QDBusObjectPath &advertisement)
    {
        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(advertisement);
        return asyncCallWithArgumentList(QStringLiteral("UnregisterAdvertisement"), argumentList);
    }

Q_SIGNALS: // SIGNALS
};

namespace org {
  namespace bluez {
    using LEAdvertisingManager1 = ::OrgBluezLEAdvertisingManager1Interface;
  }
}
#endif