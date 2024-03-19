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

#include "kio_curl.h"
#include "kcomponentdata.h"
#include "kmimetype.h"
#include "kremoteencoding.h"
#include "kconfiggroup.h"
#include "kdebug.h"

#include <QApplication>
#include <QHostAddress>
#include <QHostInfo>
#include <QDir>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define KIO_CURL_ERROR(CODE) \
    const QString curlerror = QString::fromAscii(curl_easy_strerror(CODE)); \
    kWarning(7103) << "curl error" << curlerror; \
    error(KIO::ERR_SLAVE_DEFINED, curlerror);

// for reference:
// https://linux.die.net/man/3/strmode
// https://datatracker.ietf.org/doc/html/rfc959
// https://curl.se/libcurl/c/pop3-stat.html
// https://curl.se/libcurl/c/CURLOPT_QUOTE.html

static inline QByteArray ftpPermissions(const int permissions)
{
    return QByteArray::number(permissions & 0777, 8);
}

static inline int ftpUserModeFromChar(const char modechar, const int rmode, const int wmode, const int xmode)
{
     mode_t result = 0;
     switch (modechar) {
         case '-': {
             break;
         }
         case 'r': {
             result |= rmode;
             break;
         }
         case 'w': {
             result |= wmode;
             break;
         }
         case 'x': {
             result |= xmode;
             break;
         }
         default: {
             kWarning(7103) << "Invalid FTP mode char" << modechar;
             break;
          }
      }
      return result;
}

static inline mode_t ftpModeFromString(const char* const modestring)
{
     mode_t result = 0;
     switch (modestring[0]) {
         case '-': {
             result |= S_IFREG;
             break;
          }
          case 'b': {
             result |= S_IFBLK;
             break;
          }
          case 'c': {
             result |= S_IFCHR;
             break;
          }
          case 'd': {
             result |= S_IFDIR;
             break;
         }
         case 'l': {
             result |= S_IFLNK;
             break;
         }
         case 'p': {
             result |= S_IFIFO;
             break;
         }
         case 's': {
             result |= S_IFSOCK;
             break;
         }
         default: {
             kWarning(7103) << "Invalid FTP mode string" << modestring;
             break;
         }
     }

     result |= ftpUserModeFromChar(modestring[1], S_IRUSR, S_IWUSR, S_IXUSR);
     result |= ftpUserModeFromChar(modestring[2], S_IRUSR, S_IWUSR, S_IXUSR);
     result |= ftpUserModeFromChar(modestring[3], S_IRUSR, S_IWUSR, S_IXUSR);

     result |= ftpUserModeFromChar(modestring[4], S_IRGRP, S_IWGRP, S_IXGRP);
     result |= ftpUserModeFromChar(modestring[5], S_IRGRP, S_IWGRP, S_IXGRP);
     result |= ftpUserModeFromChar(modestring[6], S_IRGRP, S_IWGRP, S_IXGRP);

     result |= ftpUserModeFromChar(modestring[7], S_IROTH, S_IWOTH, S_IXOTH);
     result |= ftpUserModeFromChar(modestring[8], S_IROTH, S_IWOTH, S_IXOTH);
     result |= ftpUserModeFromChar(modestring[9], S_IROTH, S_IWOTH, S_IXOTH);

     return result;
}

static inline QByteArray curlProxyBytes(const QString &proxy)
{
    const KUrl proxyurl(proxy);
    const QString proxyhost = proxyurl.host();
    if (proxyurl.port() > 0) {
        QByteArray curlproxybytes = proxyhost.toAscii();
        curlproxybytes.append(':');
        curlproxybytes.append(QByteArray::number(proxyurl.port()));
        return curlproxybytes;
    }
    return proxyhost.toAscii();
}

static inline curl_proxytype curlProxyType(const QString &proxy)
{
    const QString proxyprotocol = KUrl(proxy).protocol();

#if CURL_AT_LEAST_VERSION(7, 52, 0)
    if (proxyprotocol.startsWith(QLatin1String("https"))) {
        return CURLPROXY_HTTPS;
    }
#endif
    if (proxyprotocol.startsWith(QLatin1String("socks4"))) {
        return CURLPROXY_SOCKS4;
    } else if (proxyprotocol.startsWith(QLatin1String("socks4a"))) {
        return CURLPROXY_SOCKS4A;
    } else if (proxyprotocol.startsWith(QLatin1String("socks5"))) {
        return CURLPROXY_SOCKS5;
    }
    return CURLPROXY_HTTP;
}

static inline QString HTTPMIMEType(const QString &contenttype)
{
    const QList<QString> splitcontenttype = contenttype.split(QLatin1Char(';'));
    if (splitcontenttype.isEmpty() || splitcontenttype.at(0).isEmpty()) {
        return QString::fromLatin1("application/octet-stream");
    }
    return splitcontenttype.at(0);
}

static inline long HTTPCode(CURL *curl)
{
    long curlresponsecode = 0;
    const CURLcode curlresult = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &curlresponsecode);
    if (curlresult != CURLE_OK) {
        kWarning(7103) << "Could not get response code info" << curl_easy_strerror(curlresult);
    }
    kDebug(7103) << "HTTP error" << curlresponsecode;
    return curlresponsecode;
}

// for reference:
// https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
static inline KIO::Error HTTPToKIOError(const long httpcode)
{
    switch (httpcode) {
        case 401:
        case 403:
        case 407: {
            return KIO::ERR_COULD_NOT_LOGIN;
        }
        case 408:
        case 504: {
            return KIO::ERR_SERVER_TIMEOUT;
        }
        case 500: {
            return KIO::ERR_INTERNAL_SERVER;
        }
        case 404:
        case 503: {
            return KIO::ERR_SERVICE_NOT_AVAILABLE;
        }
        default: {
            return KIO::ERR_NO_CONTENT;
        }
    }
    Q_UNREACHABLE();
}

static inline KIO::Error curlToKIOError(const CURLcode curlcode, CURL *curl)
{
    kWarning(7103) << "curl error" << curl_easy_strerror(curlcode);
    switch (curlcode) {
        case CURLE_HTTP_RETURNED_ERROR:
        case CURLE_ABORTED_BY_CALLBACK: {
            const long httpcode = HTTPCode(curl);
            return HTTPToKIOError(httpcode);
        }
        case CURLE_URL_MALFORMAT: {
            return KIO::ERR_MALFORMED_URL;
        }
#if CURL_AT_LEAST_VERSION(7, 73, 0)
        case CURLE_PROXY:
#endif
        case CURLE_COULDNT_RESOLVE_PROXY: {
            return KIO::ERR_UNKNOWN_PROXY_HOST;
        }
        case CURLE_AUTH_ERROR:
        case CURLE_LOGIN_DENIED: {
            return KIO::ERR_COULD_NOT_LOGIN;
        }
        case CURLE_REMOTE_ACCESS_DENIED: {
            return KIO::ERR_ACCESS_DENIED;
        }
        case CURLE_FILE_COULDNT_READ_FILE:
        case CURLE_READ_ERROR: {
            return KIO::ERR_COULD_NOT_READ;
        }
        case CURLE_WRITE_ERROR: {
            return KIO::ERR_COULD_NOT_WRITE;
        }
        case CURLE_OUT_OF_MEMORY: {
            return KIO::ERR_OUT_OF_MEMORY;
        }
        case CURLE_BAD_DOWNLOAD_RESUME: {
            return KIO::ERR_CANNOT_RESUME;
        }
        case CURLE_REMOTE_FILE_NOT_FOUND: {
            return KIO::ERR_DOES_NOT_EXIST;
        }
        case CURLE_GOT_NOTHING: {
            return KIO::ERR_NO_CONTENT;
        }
        case CURLE_REMOTE_DISK_FULL: {
            return KIO::ERR_DISK_FULL;
        }
        case CURLE_OPERATION_TIMEDOUT: {
            return KIO::ERR_SERVER_TIMEOUT;
        }
        case CURLE_REMOTE_FILE_EXISTS: {
            return KIO::ERR_FILE_ALREADY_EXIST;
        }
        case CURLE_COULDNT_RESOLVE_HOST: {
            return KIO::ERR_UNKNOWN_HOST;
        }
        case CURLE_COULDNT_CONNECT:
        default: {
            return KIO::ERR_COULD_NOT_CONNECT;
        }
    }
    Q_UNREACHABLE();
}

size_t curlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    CurlProtocol* curlprotocol = static_cast<CurlProtocol*>(userdata);
    if (!curlprotocol) {
        return 0;
    }
    // size should always be 1
    Q_ASSERT(size == 1);
    curlprotocol->slotData(ptr, nmemb);
    return nmemb;
}

size_t curlReadCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    CurlProtocol* curlprotocol = static_cast<CurlProtocol*>(userdata);
    if (!curlprotocol) {
        return 0;
    }
    curlprotocol->dataReq();
    QByteArray kioreadbuffer;
    const int kioreadresult = curlprotocol->readData(kioreadbuffer);
    if (kioreadbuffer.size() > nmemb) {
        kWarning(7103) << "Request data size larger than the buffer size";
        return 0;
    }
    ::memcpy(ptr, kioreadbuffer.constData(), kioreadbuffer.size() * sizeof(char));
    return kioreadresult;
}

int curlXFERCallback(void *userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    CurlProtocol* curlprotocol = static_cast<CurlProtocol*>(userdata);
    if (!curlprotocol) {
        return CURLE_BAD_FUNCTION_ARGUMENT;
    }
    if (curlprotocol->aborttransfer) {
        return CURLE_HTTP_RETURNED_ERROR;
    }
    curlprotocol->slotProgress(KIO::filesize_t(dlnow), KIO::filesize_t(dltotal));
    return CURLE_OK;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    KComponentData componentData("kio_curl", "kdelibs4");

    kDebug(7103) << "Starting" << ::getpid();

    if (argc != 2) {
        ::fprintf(stderr, "Usage: kio_curl app-socket\n");
        ::exit(-1);
    }

    CurlProtocol slave(argv[1]);
    slave.dispatchLoop();

    kDebug(7103) << "Done";
    return 0;
}

CurlProtocol::CurlProtocol(const QByteArray &app)
    : SlaveBase("curl", app),
    aborttransfer(false),
    m_emitmime(true), m_ishttp(false), m_isftp(false), m_issftp(false), m_collectdata(false),
    m_curl(nullptr), m_curlheaders(nullptr), m_curlquotes(nullptr)
{
    m_curl = curl_easy_init();
    if (!m_curl) {
        kWarning(7103) << "Could not create context";
        return;
    }
}

CurlProtocol::~CurlProtocol()
{
    if (m_curlheaders) {
        curl_slist_free_all(m_curlheaders);
    }
    if (m_curlquotes) {
        curl_slist_free_all(m_curlquotes);
    }
    if (m_curl) {
        curl_easy_cleanup(m_curl);
    }
}

void CurlProtocol::stat(const KUrl &url)
{
    kDebug(7103) << "Stat URL" << url.prettyUrl();

    KUrl staturl(url);
    QString statfilename = QLatin1String(".");
    const QString staturlfilename = staturl.fileName();
    const QString staturlprotocol = staturl.protocol();
    if (staturlprotocol == QLatin1String("ftp") || staturlprotocol == QLatin1String("sftp")) {
        if (!staturl.path().endsWith(QDir::separator())) {
            statfilename = staturlfilename;
            staturl.setFileName(QString());
            staturl.adjustPath(KUrl::AddTrailingSlash);
        }
    }
    kDebug(7103) << "Actual stat URL" << staturl << "filename" << statfilename;

    if (!setupCurl(staturl, false)) {
        return;
    }

    if (m_isftp || m_issftp) {
        m_collectdata = true;
    }

    KUrl redirecturl;
    CURLcode curlresult = performCurl(staturl, &redirecturl);
    kDebug(7103) << "Stat result" << curlresult;
    if (curlresult != CURLE_OK) {
        const KIO::Error kioerror = curlToKIOError(curlresult, m_curl);
        error(kioerror, url.prettyUrl());
        return;
    }

    if (redirecturl.isValid()) {
        redirection(redirecturl);
        finished();
        return;
    }

    if (m_isftp || m_issftp) {
        foreach (const KIO::UDSEntry &kioudsentry, udsEntries()) {
            if (kioudsentry.stringValue(KIO::UDSEntry::UDS_NAME) == statfilename) {
                statEntry(kioudsentry);
                finished();
                return;
            }
        }
        kWarning(7103) << "Could not find entry for" << statfilename;
        error(KIO::ERR_COULD_NOT_STAT, url.prettyUrl());
        return;
    }

    QString httpmimetype;
    char *curlcontenttype = nullptr;
    curlresult = curl_easy_getinfo(m_curl, CURLINFO_CONTENT_TYPE, &curlcontenttype);
    if (curlresult == CURLE_OK) {
        httpmimetype = HTTPMIMEType(QString::fromAscii(curlcontenttype));
    } else {
        kWarning(7103) << "Could not get content type info" << curl_easy_strerror(curlresult);
    }

    curl_off_t curlfiletime = 0;
    curlresult = curl_easy_getinfo(m_curl, CURLINFO_FILETIME_T, &curlfiletime);
    if (curlresult != CURLE_OK) {
        kWarning(7103) << "Could not get filetime info" << curl_easy_strerror(curlresult);
    }

    curl_off_t curlcontentlength = 0;
    curlresult = curl_easy_getinfo(m_curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &curlcontentlength);
    if (curlresult != CURLE_OK) {
        kWarning(7103) << "Could not get content length info" << curl_easy_strerror(curlresult);
    }

    KIO::UDSEntry kioudsentry;
    kDebug(7103) << "File time" << curlfiletime;
    kDebug(7103) << "Content length" << curlcontentlength;
    kDebug(7103) << "MIME type" << httpmimetype;
    kioudsentry.insert(KIO::UDSEntry::UDS_NAME, staturlfilename);
    kioudsentry.insert(KIO::UDSEntry::UDS_SIZE, qlonglong(curlcontentlength));
    kioudsentry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME, qlonglong(curlfiletime));
    if (!httpmimetype.isEmpty()) {
        kioudsentry.insert(KIO::UDSEntry::UDS_MIME_TYPE, httpmimetype);
    }
    statEntry(kioudsentry);

    finished();
}

void CurlProtocol::listDir(const KUrl &url)
{
    kDebug(7103) << "List URL" << url.prettyUrl();

    // listing has to be done via URL ending with a slash, otherwise it is like file query
    KUrl listurl(url);
    listurl.adjustPath(KUrl::AddTrailingSlash);

    if (!setupCurl(listurl, true)) {
        return;
    }

    m_collectdata = true;

    KUrl redirecturl;
    CURLcode curlresult = performCurl(listurl, &redirecturl);
    kDebug(7103) << "List result" << curlresult;
    if (curlresult != CURLE_OK) {
        const KIO::Error kioerror = curlToKIOError(curlresult, m_curl);
        error(kioerror, url.prettyUrl());
        return;
    }

    if (redirecturl.isValid()) {
        redirection(redirecturl);
        finished();
        return;
    }

    foreach (const KIO::UDSEntry &kioudsentry, udsEntries()) {
        listEntry(kioudsentry, false);
    }
    KIO::UDSEntry kioudsentry;
    listEntry(kioudsentry, true);

    finished();
}

void CurlProtocol::get(const KUrl &url)
{
    kDebug(7103) << "Get URL" << url.prettyUrl();

    if (!setupCurl(url, false)) {
        return;
    }

    KUrl redirecturl;
    CURLcode curlresult = performCurl(url, &redirecturl);
    kDebug(7103) << "Get result" << curlresult;
    if (curlresult != CURLE_OK) {
        const KIO::Error kioerror = curlToKIOError(curlresult, m_curl);
        error(kioerror, url.prettyUrl());
        return;
    }

    if (redirecturl.isValid()) {
        redirection(redirecturl);
        finished();
        return;
    }

    finished();
}

void CurlProtocol::put(const KUrl &url, int permissions, KIO::JobFlags flags)
{
    // TODO: job flags for ftp/sftp only, check if URL exists on server
    // NOTE: CURLOPT_NEW_FILE_PERMS is documented to work only for some protocols, ftp is not one
    // of them but sftp is
    Q_UNUSED(flags);

    kDebug(7103) << "Put URL" << url.prettyUrl() << permissions << flags;

    if (!setupCurl(url, false)) {
        return;
    }

    CURLcode curlresult = CURLE_OK;
    if (m_ishttp) {
        curlresult = curl_easy_setopt(m_curl, CURLOPT_POST, 1L);
        if (curlresult != CURLE_OK) {
            KIO_CURL_ERROR(curlresult);
            return;
        }
    } else {
        curlresult = curl_easy_setopt(m_curl, CURLOPT_UPLOAD, 1L);
        if (curlresult != CURLE_OK) {
            KIO_CURL_ERROR(curlresult);
            return;
        }

        const QString putfilename = url.path();
        const QByteArray putpermissions = ftpPermissions(permissions);
        kDebug(7103) << "Filename" << putfilename << "permissions" << putpermissions;

        const QByteArray putfilenamebytes = remoteEncoding()->encode(putfilename);
        m_curlquotes = curl_slist_append(m_curlquotes, QByteArray("SITE CHMOD ") + putpermissions + " " + putfilenamebytes);
        curlresult = curl_easy_setopt(m_curl, CURLOPT_POSTQUOTE, m_curlquotes);
        if (curlresult != CURLE_OK) {
            KIO_CURL_ERROR(curlresult);
            return;
        }
    }

    KUrl redirecturl;
    curlresult = performCurl(url, &redirecturl);
    kDebug(7103) << "Put result" << curlresult;
    if (curlresult != CURLE_OK) {
        if (curlresult == CURLE_QUOTE_ERROR) {
            error(KIO::ERR_CANNOT_CHMOD, url.prettyUrl());
            return;
        }
        const KIO::Error kioerror = curlToKIOError(curlresult, m_curl);
        error(kioerror, url.prettyUrl());
        return;
    }

    if (redirecturl.isValid()) {
        redirection(redirecturl);
        finished();
        return;
    }

    finished();
}

void CurlProtocol::chmod(const KUrl &url, int permissions)
{
    kDebug(7103) << "Chmod URL" << url.prettyUrl() << permissions;

    KUrl chmodurl(url);
    QString chmodfilename = chmodurl.path();
    chmodurl.setPath(QString());
    chmodurl.adjustPath(KUrl::AddTrailingSlash);
    if (chmodfilename.isEmpty() || chmodfilename == QDir::separator()) {
        // must be the root directory
        chmodfilename = QLatin1String(".");
    }
    const QByteArray chmodpermissions = ftpPermissions(permissions);
    kDebug(7103) << "Actual chmod URL" << chmodurl << "filename" << chmodfilename << "permissions" << chmodpermissions;

    if (!setupCurl(chmodurl, true)) {
        return;
    }

    const QByteArray chmodfilenamebytes = remoteEncoding()->encode(chmodfilename);
    m_curlquotes = curl_slist_append(m_curlquotes, QByteArray("SITE CHMOD ") + chmodpermissions + " " + chmodfilenamebytes);
    CURLcode curlresult = curl_easy_setopt(m_curl, CURLOPT_QUOTE, m_curlquotes);
    if (curlresult != CURLE_OK) {
        KIO_CURL_ERROR(curlresult);
        return;
    }

    KUrl redirecturl;
    curlresult = performCurl(chmodurl, &redirecturl);
    kDebug(7103) << "Chmod result" << curlresult;
    if (curlresult != CURLE_OK) {
        if (curlresult == CURLE_QUOTE_ERROR) {
            error(KIO::ERR_CANNOT_CHMOD, url.prettyUrl());
            return;
        }
        const KIO::Error kioerror = curlToKIOError(curlresult, m_curl);
        error(kioerror, url.prettyUrl());
        return;
    }

    if (redirecturl.isValid()) {
        redirection(redirecturl);
        finished();
        return;
    }

    finished();
}

void CurlProtocol::mkdir(const KUrl &url, int permissions)
{
    kDebug(7103) << "Mkdir URL" << url.prettyUrl() << permissions;

    KUrl mkdirurl(url);
    QString mkdirfilename = mkdirurl.path();
    mkdirurl.setPath(QString());
    mkdirurl.adjustPath(KUrl::AddTrailingSlash);
    if (mkdirfilename.isEmpty() || mkdirfilename == QDir::separator()) {
        // must be the root directory
        mkdirfilename = QLatin1String(".");
    }
    const QByteArray mkdirpermissions = ftpPermissions(permissions);
    kDebug(7103) << "Actual mkdir URL" << mkdirurl << "filename" << mkdirfilename << "permissions" << mkdirpermissions;

    if (!setupCurl(mkdirurl, true)) {
        return;
    }

    const QByteArray mkdirfilenamebytes = remoteEncoding()->encode(mkdirfilename);
    m_curlquotes = curl_slist_append(m_curlquotes, QByteArray("MKD ") + mkdirfilenamebytes);
    m_curlquotes = curl_slist_append(m_curlquotes, QByteArray("SITE CHMOD ") + mkdirpermissions + " " + mkdirfilenamebytes);
    CURLcode curlresult = curl_easy_setopt(m_curl, CURLOPT_QUOTE, m_curlquotes);
    if (curlresult != CURLE_OK) {
        KIO_CURL_ERROR(curlresult);
        return;
    }

    KUrl redirecturl;
    curlresult = performCurl(mkdirurl, &redirecturl);
    kDebug(7103) << "Mkdir result" << curlresult;
    if (curlresult != CURLE_OK) {
        if (curlresult == CURLE_QUOTE_ERROR) {
            error(KIO::ERR_COULD_NOT_MKDIR, url.prettyUrl());
            return;
        }
        const KIO::Error kioerror = curlToKIOError(curlresult, m_curl);
        error(kioerror, url.prettyUrl());
        return;
    }

    if (redirecturl.isValid()) {
        redirection(redirecturl);
        finished();
        return;
    }

    finished();
}

void CurlProtocol::del(const KUrl &url, bool isfile)
{
    kDebug(7103) << "Delete URL" << url.prettyUrl() << isfile;

    KUrl delurl(url);
    QString delfilename = delurl.path();
    delurl.setPath(QString());
    delurl.adjustPath(KUrl::AddTrailingSlash);
    if (delfilename.isEmpty() || delfilename == QDir::separator()) {
        // must be the root directory
        delfilename = QLatin1String(".");
    }
    kDebug(7103) << "Actual Delete URL" << delurl << "filename" << delfilename;

    if (!setupCurl(delurl, true)) {
        return;
    }

    const QByteArray delfilenamebytes = remoteEncoding()->encode(delfilename);
    if (isfile) {
        m_curlquotes = curl_slist_append(m_curlquotes, QByteArray("DELE ") + delfilenamebytes);
    } else {
        m_curlquotes = curl_slist_append(m_curlquotes, QByteArray("RMD ") + delfilenamebytes);
    }
    CURLcode curlresult = curl_easy_setopt(m_curl, CURLOPT_QUOTE, m_curlquotes);
    if (curlresult != CURLE_OK) {
        KIO_CURL_ERROR(curlresult);
        return;
    }

    KUrl redirecturl;
    curlresult = performCurl(delurl, &redirecturl);
    kDebug(7103) << "Delete result" << curlresult;
    if (curlresult != CURLE_OK) {
        if (curlresult == CURLE_QUOTE_ERROR) {
            error(KIO::ERR_CANNOT_DELETE, url.prettyUrl());
            return;
        }
        const KIO::Error kioerror = curlToKIOError(curlresult, m_curl);
        error(kioerror, url.prettyUrl());
        return;
    }

    if (redirecturl.isValid()) {
        redirection(redirecturl);
        finished();
        return;
    }

    finished();
}

void CurlProtocol::slotData(const char* curldata, const size_t curldatasize)
{
    if (aborttransfer) {
        kDebug(7103) << "Transfer still in progress";
        return;
    }

    const QByteArray bytedata = QByteArray::fromRawData(curldata, curldatasize);

    if (m_emitmime) {
        m_emitmime = false;

        if (m_ishttp) {
            // if it's HTTP error do not send data and MIME, abort transfer
            const long httpcode = HTTPCode(m_curl);
            if (httpcode >= 400) {
                aborttransfer = true;
                return;
            }

            QString httpmimetype = QString::fromLatin1("application/octet-stream");
            char *curlcontenttype = nullptr;
            CURLcode curlresult = curl_easy_getinfo(m_curl, CURLINFO_CONTENT_TYPE, &curlcontenttype);
            if (curlresult == CURLE_OK) {
                httpmimetype = HTTPMIMEType(QString::fromAscii(curlcontenttype));
            } else {
                kWarning(7103) << "Could not get content type info" << curl_easy_strerror(curlresult);
            }
            mimeType(httpmimetype);
        } else {
            KMimeType::Ptr kmimetype = KMimeType::findByNameAndContent(m_url.url(), bytedata);
            // default MIME type should be returned in the worst case
            Q_ASSERT(kmimetype);
            mimeType(kmimetype->name());
        }
    }

    if (m_collectdata) {
        m_writedata.append(bytedata);
    } else {
        data(bytedata);
    }

    curl_off_t curlspeeddownload = 0;
    CURLcode curlresult = curl_easy_getinfo(m_curl, CURLINFO_SPEED_DOWNLOAD_T, &curlspeeddownload);
    if (curlresult == CURLE_OK) {
        kDebug(7103) << "Download speed" << curlspeeddownload;
        speed(ulong(curlspeeddownload));
    } else {
        kWarning(7103) << "Could not get download speed info" << curl_easy_strerror(curlresult);
    }
}

void CurlProtocol::slotProgress(const KIO::filesize_t received, const KIO::filesize_t total)
{
    kDebug(7103) << "Received" << received << "from" << total;
    processedSize(received);
    if (total > 0 && received != total) {
        totalSize(total);
    }
}

CURLcode CurlProtocol::setupAuth(const QString &username, const QString &password)
{
    CURLcode curlresult = CURLE_OK;
    const QByteArray urlusernamebytes = username.toAscii();
    if (!urlusernamebytes.isEmpty()) {
        curlresult = curl_easy_setopt(m_curl, CURLOPT_USERNAME, urlusernamebytes.constData());
        if (curlresult != CURLE_OK) {
            return curlresult;
        }
    }
    const QByteArray urlpasswordbytes = password.toAscii();
    if (!urlpasswordbytes.isEmpty()) {
        curlresult = curl_easy_setopt(m_curl, CURLOPT_PASSWORD, urlpasswordbytes.constData());
        if (curlresult != CURLE_OK) {
            return curlresult;
        }
    }
    return curlresult;
}

bool CurlProtocol::setupCurl(const KUrl &url, const bool ftporsftp)
{
    if (Q_UNLIKELY(!m_curl)) {
        error(KIO::ERR_OUT_OF_MEMORY, QString::fromLatin1("Null context"));
        return false;
    }

    // curl cannot verify certs if the host is address, CURLOPT_USE_SSL set to CURLUSESSL_TRY
    // does not bypass such cases so resolving it manually
    const QHostAddress urladdress(url.host());
    if (!urladdress.isNull()) {
        const QHostInfo urlinfo = QHostInfo::fromName(url.host());
        if (urlinfo.error() == QHostInfo::NoError) {
            KUrl newurl(url);
            newurl.setHost(urlinfo.hostName());
            kDebug(7103) << "Rewrote" << url << "to" << newurl;
            redirection(newurl);
            finished();
            return false;
        } else {
            kWarning(7103) << "Could not resolve" << url.host();
        }
    }

    aborttransfer = false;
    m_emitmime = true;
    const QString urlprotocol = url.protocol();
    m_ishttp = (urlprotocol == QLatin1String("http") || urlprotocol == QLatin1String("https"));
    m_isftp = (urlprotocol == QLatin1String("ftp"));
    m_issftp = (urlprotocol == QLatin1String("sftp"));
    m_collectdata = false;
    m_writedata.clear();
    m_url = url;

    if (ftporsftp && !m_isftp && !m_issftp) {
        // only for FTP or SFTP
        error(KIO::ERR_INTERNAL, url.prettyUrl());
        return false;
    }

    curl_easy_reset(m_curl);
    curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(m_curl, CURLOPT_FILETIME, 1L);
    curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(m_curl, CURLOPT_MAXREDIRS, 100L); // proxies apparently cause a lot of redirects
    curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, SlaveBase::connectTimeout());
    // curl_easy_setopt(m_curl, CURLOPT_IGNORE_CONTENT_LENGTH, 1L); // breaks XFER info, fixes transfer of chunked content
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(m_curl, CURLOPT_READDATA, this);
    curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, curlReadCallback);
    curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 0L); // otherwise the XFER info callback is not called
    curl_easy_setopt(m_curl, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFOFUNCTION, curlXFERCallback);
    curl_easy_setopt(m_curl, CURLOPT_FAILONERROR, 1L);
    // TODO: option for this, warning?
    curl_easy_setopt(m_curl, CURLOPT_USE_SSL, (long)CURLUSESSL_TRY);
    // curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L); // debugging

    // NOTE: the URL path has to be percentage-encoded, otherwise curl will reject it if it
    // contains whitespace for example
    const QByteArray urlbytes = url.toEncoded();
    CURLcode curlresult = curl_easy_setopt(m_curl, CURLOPT_URL, urlbytes.constData());
    if (curlresult != CURLE_OK) {
        KIO_CURL_ERROR(curlresult);
        return false;
    }

#if CURL_AT_LEAST_VERSION(7, 85, 0)
    static const char* const curlprotocols = "http,https,ftp,sftp";

    curlresult = curl_easy_setopt(m_curl, CURLOPT_PROTOCOLS_STR, curlprotocols);
    if (curlresult != CURLE_OK) {
        KIO_CURL_ERROR(curlresult);
        return false;
    }

    curlresult = curl_easy_setopt(m_curl, CURLOPT_REDIR_PROTOCOLS_STR, curlprotocols);
    if (curlresult != CURLE_OK) {
        KIO_CURL_ERROR(curlresult);
        return false;
    }
#else
    // deprecated since v7.85.0
    static const long const curlprotocols = (CURLPROTO_HTTP | CURLPROTO_HTTPS | CURLPROTO_FTP | CURLPROTO_SFTP);
    curlresult = curl_easy_setopt(m_curl, CURLOPT_PROTOCOLS, curlprotocols);
    if (curlresult != CURLE_OK) {
        KIO_CURL_ERROR(curlresult);
        return false;
    }

    curlresult = curl_easy_setopt(m_curl, CURLOPT_REDIR_PROTOCOLS, curlprotocols);
    if (curlresult != CURLE_OK) {
        KIO_CURL_ERROR(curlresult);
        return false;
    }
#endif

    kDebug(7103) << "Metadata" << allMetaData();

    if (hasMetaData(QLatin1String("UserAgent"))) {
        const QByteArray useragentbytes = metaData("UserAgent").toAscii();
        curlresult = curl_easy_setopt(m_curl, CURLOPT_USERAGENT, useragentbytes.constData());
        if (curlresult != CURLE_OK) {
            KIO_CURL_ERROR(curlresult);
            return false;
        }
    }

    const bool noauth = (metaData("no-auth") == QLatin1String("yes"));
    if (hasMetaData(QLatin1String("UseProxy"))) {
        const QString proxystring = metaData("UseProxy");
        const QByteArray proxybytes = curlProxyBytes(proxystring);
        const curl_proxytype curlproxytype = curlProxyType(proxystring);
        kDebug(7103) << "Proxy" << proxybytes << curlproxytype;
        curlresult = curl_easy_setopt(m_curl, CURLOPT_PROXY, proxybytes.constData());
        if (curlresult != CURLE_OK) {
            KIO_CURL_ERROR(curlresult);
            return false;
        }
        curlresult = curl_easy_setopt(m_curl, CURLOPT_PROXYTYPE, curlproxytype);
        if (curlresult != CURLE_OK) {
            KIO_CURL_ERROR(curlresult);
            return false;
        }

        const bool noproxyauth = (noauth || metaData("no-proxy-auth") == QLatin1String("yes"));
        kDebug(7103) << "No proxy auth" << noproxyauth;
        curlresult = curl_easy_setopt(m_curl, CURLOPT_PROXYAUTH, noproxyauth ? CURLAUTH_NONE : CURLAUTH_ANY);
        if (curlresult != CURLE_OK) {
            KIO_CURL_ERROR(curlresult);
            return false;
        }
    }

    const bool nowwwauth = (noauth || metaData("no-www-auth") == QLatin1String("true"));
    kDebug(7103) << "No WWW auth" << nowwwauth;
    if (m_ishttp) {
        curlresult = curl_easy_setopt(m_curl, CURLOPT_HTTPAUTH, nowwwauth ? CURLAUTH_NONE : CURLAUTH_ANY);
        if (curlresult != CURLE_OK) {
            KIO_CURL_ERROR(curlresult);
            return false;
        }
    }

    curlresult = setupAuth(url.userName(), url.password());
    if (curlresult != CURLE_OK) {
        KIO_CURL_ERROR(curlresult);
        return false;
    }

    if (hasMetaData(QLatin1String("resume"))) {
        Q_ASSERT(sizeof(qlonglong) == sizeof(curl_off_t));
        const qlonglong resumeoffset = metaData(QLatin1String("resume")).toLongLong();
        curlresult = curl_easy_setopt(m_curl, CURLOPT_RESUME_FROM_LARGE, curl_off_t(resumeoffset));
        if (curlresult != CURLE_OK) {
            KIO_CURL_ERROR(curlresult);
            return false;
        } else {
            canResume();
        }
    }

    if (m_curlheaders) {
        curl_slist_free_all(m_curlheaders);
        m_curlheaders = nullptr;
    }
    if (m_ishttp) {
        if (hasMetaData(QLatin1String("Languages"))) {
            m_curlheaders = curl_slist_append(m_curlheaders, QByteArray("Accept-Language: ") + metaData("Languages").toAscii());
        }

        if (hasMetaData(QLatin1String("Charsets"))) {
            m_curlheaders = curl_slist_append(m_curlheaders, QByteArray("Accept-Charset: ") + metaData("Charsets").toAscii());
        }

        if (hasMetaData(QLatin1String("accept"))) {
            m_curlheaders = curl_slist_append(m_curlheaders, QByteArray("Accept: ") + metaData("accept").toAscii());
        }

        if (hasMetaData(QLatin1String("Authorization"))) {
            m_curlheaders = curl_slist_append(m_curlheaders, QByteArray("Authorization: ") + metaData("Authorization").toAscii());
        }

        curlresult = curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_curlheaders);
        if (curlresult != CURLE_OK) {
            curl_slist_free_all(m_curlheaders);
            m_curlheaders = nullptr;
            KIO_CURL_ERROR(curlresult);
            return false;
        }
    }

    if (m_curlquotes) {
        curl_slist_free_all(m_curlquotes);
        m_curlquotes = nullptr;
    }
    if (m_isftp || m_issftp) {
        // NOTE: this is stored in kio_ftprc
        const long disablepassivemode = config()->readEntry("DisablePassiveMode", false);
        kDebug(7103) << "Disable passive mode" << disablepassivemode;
        curlresult = curl_easy_setopt(m_curl, CURLOPT_FTP_SKIP_PASV_IP, disablepassivemode);
        if (curlresult != CURLE_OK) {
            KIO_CURL_ERROR(curlresult);
            return false;
        }
    }

    return true;
}

// NOTE: redirection is done so that the URL in navigation is corrected, notably its user and
// password part
CURLcode CurlProtocol::performCurl(const KUrl &url, KUrl *redirecturl)
{
    CURLcode curlresult = curl_easy_perform(m_curl);
    const QString urlusername = url.userName();
    KIO::AuthInfo kioauthinfo;
    kioauthinfo.url = url;
    kioauthinfo.username = urlusername;
    kioauthinfo.password = url.password();
    if (curlresult != CURLE_OK) {
        KIO::Error kioerror = curlToKIOError(curlresult, m_curl);
        if (kioerror == KIO::ERR_COULD_NOT_LOGIN) {
            kDebug(7103) << "Authorizing from cache" << url.prettyUrl();
            if (checkCachedAuthentication(kioauthinfo)) {
                curlresult = setupAuth(kioauthinfo.username, kioauthinfo.password);
                if (curlresult != CURLE_OK) {
                    return curlresult;
                }
                curlresult = curl_easy_perform(m_curl);
                kioerror = curlToKIOError(curlresult, m_curl);
                if (kioerror != KIO::ERR_COULD_NOT_LOGIN) {
                    kDebug(7103) << "Going to redirect for cache authorization";
                    KUrl newurl(url);
                    newurl.setUserName(kioauthinfo.username);
                    newurl.setPassword(kioauthinfo.password);
                    *redirecturl = newurl;
                }
            }
        }
    }

    if (curlresult != CURLE_OK) {
        KIO::Error kioerror = curlToKIOError(curlresult, m_curl);
        if (kioerror == KIO::ERR_COULD_NOT_LOGIN) {
            kDebug(7103) << "Authorizing" << url.prettyUrl();
            kioauthinfo.keepPassword = true;
            kioauthinfo.prompt = i18n("You need to supply a username and a password to access this URL.");
            kioauthinfo.commentLabel = i18n("URL:");
            kioauthinfo.comment = i18n("<b>%1</b>", url.prettyUrl());
            if (openPasswordDialog(kioauthinfo)) {
                curlresult = setupAuth(kioauthinfo.username, kioauthinfo.password);
                if (curlresult != CURLE_OK) {
                    return curlresult;
                }
                if (kioauthinfo.keepPassword) {
                    kDebug(7103) << "Caching authorization";
                    cacheAuthentication(kioauthinfo);
                }
                curlresult = curl_easy_perform(m_curl);
                kioerror = curlToKIOError(curlresult, m_curl);
                if (kioerror != KIO::ERR_COULD_NOT_LOGIN) {
                    kDebug(7103) << "Going to redirect for authorization";
                    KUrl newurl(url);
                    newurl.setUserName(kioauthinfo.username);
                    newurl.setPassword(kioauthinfo.password);
                    *redirecturl = newurl;
                }
            }
        }
    }

    return curlresult;
}

QList<KIO::UDSEntry> CurlProtocol::udsEntries()
{
    QList<KIO::UDSEntry> result;

    kDebug(7103) << "Encoding" << remoteEncoding()->encoding();

    static const QByteArray linkseparator = QByteArray("->");
    foreach(const QByteArray &line, m_writedata.split('\n')) {
        if (line.isEmpty()) {
            continue;
        }

        QList<QByteArray> lineparts;
        foreach (const QByteArray &linepart, line.split(' ')) {
            if (linepart.isEmpty()) {
                continue;
            }
            lineparts.append(linepart);
        }
        // qDebug() << Q_FUNC_INFO << lineparts;

        // basic validation
        if (lineparts.size() < 9) {
            kWarning(7103) << "Invalid FTP data line" << line;
            continue;
        }

        // take out the link parts, if any
        QByteArray ftplinkpath;
        const int linkseparatorindex = lineparts.indexOf(linkseparator);
        if (linkseparatorindex > 0) {
            foreach (const QByteArray &linkpart, lineparts.mid(linkseparatorindex)) {
                ftplinkpath.append(linkpart);
                ftplinkpath.append(' ');
            }
            ftplinkpath.chop(1);
            lineparts = lineparts.mid(0, linkseparatorindex);
        }

        // another validation just in case
        if (lineparts.size() < 9) {
            kWarning(7103) << "Invalid FTP data line" << line;
            continue;
        }

        // now take out everything but the filepath parts
        const QByteArray ftpmode = lineparts.at(0);
        const QByteArray ftpowner = lineparts.at(2);
        const QByteArray ftpgroup = lineparts.at(3);
        const qlonglong ftpsize = lineparts.at(4).toLongLong();
        lineparts = lineparts.mid(8);

        // and finally the filepath parts
        QByteArray ftpfilepath;
        foreach (const QByteArray &filepart, lineparts) {
            ftpfilepath.append(filepart);
            ftpfilepath.append(' ');
        }
        ftpfilepath.chop(1);

        // qDebug() << Q_FUNC_INFO << ftpmode << ftpowner << ftpgroup << ftpsize << ftpfilepath << ftplinkpath;

        KIO::UDSEntry kioudsentry;
        const mode_t stdmode = ftpModeFromString(ftpmode);
        kioudsentry.insert(KIO::UDSEntry::UDS_NAME, remoteEncoding()->decode(ftpfilepath));
        kioudsentry.insert(KIO::UDSEntry::UDS_FILE_TYPE, stdmode & S_IFMT);
        kioudsentry.insert(KIO::UDSEntry::UDS_ACCESS, stdmode & 07777);
        kioudsentry.insert(KIO::UDSEntry::UDS_SIZE, ftpsize);
        kioudsentry.insert(KIO::UDSEntry::UDS_USER, QString::fromLatin1(ftpowner));
        kioudsentry.insert(KIO::UDSEntry::UDS_GROUP, QString::fromLatin1(ftpgroup));
        if (!ftplinkpath.isEmpty()) {
            // link paths to current path causes KIO to do strange things
            if (ftplinkpath.at(0) != '.' && ftplinkpath.size() != 1) {
                kioudsentry.insert(KIO::UDSEntry::UDS_LINK_DEST, remoteEncoding()->decode(ftplinkpath));
            }
            if (ftpsize <= 0) {
                kioudsentry.insert(KIO::UDSEntry::UDS_GUESSED_MIME_TYPE, QString::fromLatin1("application/x-zerosize"));
            }
        }
        result.append(kioudsentry);
    }
    // at this point the transfer should be complete, release the memory
    m_writedata.clear();
    return result;
}
