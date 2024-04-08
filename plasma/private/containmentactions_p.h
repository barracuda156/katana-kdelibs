/*
 *   Copyright (c) 2009 Chani Armitage <chani@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef PLASMA_CONTAINMENTACTIONSPRIVATE_H
#define PLASMA_CONTAINMENTACTIONSPRIVATE_H

#include <plasma/plasma.h>

namespace Plasma
{

class ContainmentActionsPrivate
{
public:
    ContainmentActionsPrivate(KService::Ptr service, ContainmentActions *containmentActions) :
        q(containmentActions),
        containmentActionsDescription(service),
        initialized(false),
        needsConfig(false),
        containment(nullptr)
    {
    };

    ContainmentActions *q;
    KPluginInfo containmentActionsDescription;
    KServiceAction mode;
    bool initialized;
    bool needsConfig;
    Containment *containment;
};

} // namespace Plasma

#endif //PLASMA_CONTAINMENTACTIONSPRIVATE_H
