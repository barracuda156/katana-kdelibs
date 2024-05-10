/*
    Copyright 2024 Ivailo Monev <xakepa10@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#include "devinfoinput.h"
#include "devinfodevice.h"

#include <QtCore/QDebug>

using namespace Solid::Backends::Devinfo;

Input::Input(DevinfoDevice *device)
    : DeviceInterface(device)
{
}

Input::~Input()
{
}

QString Input::driver() const
{
    const QByteArray pnpdriver = m_device->deviceProperty(DevinfoDevice::DeviceDriver);
    return QString::fromLatin1(pnpdriver.constData(), pnpdriver.size());
}

Solid::Input::InputType Input::inputType() const
{
    const QByteArray devicename = m_device->deviceProperty(DevinfoDevice::DeviceName);
    if (devicename.contains("/atkbd")) {
        return Solid::Input::Keyboard;
    } else if (devicename.contains("/psm")) {
        return Solid::Input::Mouse;
    }
    return Solid::Input::UnknownInput;
}

#include "backends/devinfo/moc_devinfoinput.cpp"
