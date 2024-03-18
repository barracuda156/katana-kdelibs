/*  This file is part of the KDE libraries
    Copyright (C) 2022 Ivailo Monev <xakepa10@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2, as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KDELIBS_HTTP_H
#define KDELIBS_HTTP_H

#include <kurl.h>
#include <kio/slavebase.h>

#include <curl/curl.h>

class CurlProtocol : public KIO::SlaveBase
{
public:
    CurlProtocol(const QByteArray &app);
    ~CurlProtocol();

    void stat(const KUrl &url) final;
    void listDir(const KUrl &url) final;
    void get(const KUrl &url) final;
    void chmod(const KUrl &url, int permissions) final;
    void mkdir(const KUrl &url, int permissions) final;
    void del(const KUrl &url, bool isfile) final;

    void slotData(const char* curldata, const size_t curldatasize);
    void slotProgress(KIO::filesize_t received, KIO::filesize_t total);

    bool aborttransfer;

private:
    bool redirectUrl(const KUrl &url);
    bool setupCurl(const KUrl &url);
    CURLcode performCurl(const KUrl &url, KUrl *redirecturl);
    CURLcode setupAuth(const QString &username, const QString &password);
    QList<KIO::UDSEntry> udsEntries();

    bool m_emitmime;
    bool m_ishttp;
    bool m_isftp;
    bool m_issftp;
    bool m_collectdata;
    QByteArray m_writedata;
    KUrl m_url;
    CURL* m_curl;
    struct curl_slist* m_curlheaders;
    struct curl_slist* m_curlquotes;
};

#endif // KDELIBS_HTTP_H
