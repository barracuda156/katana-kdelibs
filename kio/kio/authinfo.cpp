/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000-2001 Dawit Alemayehu <adawit@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "authinfo.h"
#include "kdebug.h"

using namespace KIO;

//////

AuthInfo::AuthInfo()
    : readOnly(false),
    keepPassword(false),
    anonymousMode(false),
    hideUserName(false)
{
}

AuthInfo::AuthInfo(const AuthInfo &info)
{
    url = info.url;
    username = info.username;
    password = info.password;
    prompt = info.prompt;
    caption = info.caption;
    comment = info.comment;
    commentLabel = info.commentLabel;
    readOnly = info.readOnly;
    keepPassword = info.keepPassword;
    anonymousMode = info.anonymousMode;
    hideUserName = info.hideUserName;
}

AuthInfo& AuthInfo::operator=(const AuthInfo &info)
{
    url = info.url;
    username = info.username;
    password = info.password;
    prompt = info.prompt;
    caption = info.caption;
    comment = info.comment;
    commentLabel = info.commentLabel;
    readOnly = info.readOnly;
    keepPassword = info.keepPassword;
    anonymousMode = info.anonymousMode;
    hideUserName = info.hideUserName;
    return *this;
}

/////

QDataStream& KIO::operator<<(QDataStream &s, const AuthInfo &a)
{
    s << a.url << a.username << a.password << a.prompt << a.caption
      << a.comment << a.commentLabel << a.readOnly << a.keepPassword
      << a.anonymousMode << a.hideUserName;
    return s;
}

QDataStream& KIO::operator>>(QDataStream &s, AuthInfo &a)
{
    s >> a.url >> a.username >> a.password >> a.prompt >> a.caption
      >> a.comment >> a.commentLabel >> a.readOnly >> a.keepPassword
      >> a.anonymousMode >> a.hideUserName;
    return s;
}
