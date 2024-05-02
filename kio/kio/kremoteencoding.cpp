/* This file is part of the KDE libraries
   Copyright (C) 2003 Thiago Macieira <thiago.macieira@kdemail.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kremoteencoding.h"

#include <QTextConverter>
#include <kdebug.h>

class KRemoteEncodingPrivate
{
public:
    KRemoteEncodingPrivate()
        : converter(nullptr)
    {
    }

    ~KRemoteEncodingPrivate()
    {
        delete converter;
    }

    QByteArray name;
    QTextConverter* converter;
};

KRemoteEncoding::KRemoteEncoding(const char *name)
    : d(new KRemoteEncodingPrivate())
{
    setEncoding(name);
}

KRemoteEncoding::~KRemoteEncoding()
{
    delete d;
}

QString KRemoteEncoding::decode(const QByteArray& name) const
{
    d->converter->reset();
    const QString result = d->converter->toUnicode(name);
    if (d->converter->hasFailure()) {
        // fallback in case of decoding failure
        return QString::fromLatin1(name.constData(), name.size());
    }
    return result;
}

QByteArray KRemoteEncoding::encode(const QString& name) const
{
    d->converter->reset();
    const QByteArray result = d->converter->fromUnicode(name);
    if (d->converter->hasFailure()) {
        return name.toLatin1();
    }
    return result;
}

QByteArray KRemoteEncoding::encode(const KUrl &url) const
{
    return encode(url.path());
}

QByteArray KRemoteEncoding::directory(const KUrl &url, bool ignore_trailing_slash) const
{
    QString dir = url.directory(ignore_trailing_slash ? KUrl::KUrl::RemoveTrailingSlash : KUrl::LeaveTrailingSlash);
    return encode(dir);
}

QByteArray KRemoteEncoding::fileName(const KUrl &url) const
{
    return encode(url.fileName());
}

const char *KRemoteEncoding::encoding() const
{
    return d->name.constData();
}

void KRemoteEncoding::setEncoding(const char *name)
{
    delete d->converter;
    d->name = name;
    if (d->name.isEmpty()) {
        d->name = "UTF-8";
    }
    d->converter = new QTextConverter(d->name);
    kDebug() << "setting encoding to" << d->name;
}

