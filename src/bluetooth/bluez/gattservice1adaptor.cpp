/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp -a gattservice1adaptor_p.h:gattservice1adaptor.cpp -c OrgBluezGattService1Adaptor -i bluez5_helper_p.h org.bluez.GattService1.xml
 *
 * qdbusxml2cpp is Copyright (C) 2022 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#include "gattservice1adaptor_p.h"
#include <QtCore/QMetaObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

/*
 * Implementation of adaptor class OrgBluezGattService1Adaptor
 */

OrgBluezGattService1Adaptor::OrgBluezGattService1Adaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    // constructor
    setAutoRelaySignals(true);
}

OrgBluezGattService1Adaptor::~OrgBluezGattService1Adaptor()
{
    // destructor
}

QDBusObjectPath OrgBluezGattService1Adaptor::device() const
{
    // get the value of property Device
    return qvariant_cast< QDBusObjectPath >(parent()->property("Device"));
}

QList<QDBusObjectPath> OrgBluezGattService1Adaptor::includes() const
{
    // get the value of property Includes
    return qvariant_cast< QList<QDBusObjectPath> >(parent()->property("Includes"));
}

bool OrgBluezGattService1Adaptor::primary() const
{
    // get the value of property Primary
    return qvariant_cast< bool >(parent()->property("Primary"));
}

QString OrgBluezGattService1Adaptor::uUID() const
{
    // get the value of property UUID
    return qvariant_cast< QString >(parent()->property("UUID"));
}