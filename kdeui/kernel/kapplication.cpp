/* This file is part of the KDE libraries
    Copyright (C) 1997 Matthias Kalle Dalheimer (kalle@kde.org)
    Copyright (C) 1998, 1999, 2000 KDE Team

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
        */

#include "kapplication.h"
#include "kdeversion.h"

#include <config.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtGui/QStyleFactory>
#include <QtGui/QWidget>
#include <QtGui/QCloseEvent>
#include <QtGui/QX11Info>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusConnectionInterface>

#include "kaboutdata.h"
#include "kcrash.h"
#include "kconfig.h"
#include "kcmdlineargs.h"
#include "kglobalsettings.h"
#include "kdebug.h"
#include "kglobal.h"
#include "kicon.h"
#include "kiconloader.h"
#include "klocale.h"
#include "kstandarddirs.h"
#include "kstandardshortcut.h"
#include "kurl.h"
#include "kwindowsystem.h"
#include "kde_file.h"
#include "kstartupinfo.h"
#include "kcomponentdata.h"
#include "kmainwindow.h"
#include "kmenu.h"
#include "kconfiggroup.h"
#include "kactioncollection.h"
#include "kdebugger.h"
#include "kapplication_adaptor.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

#ifdef Q_WS_X11
#  include <netwm.h>
#  include <X11/Xlib.h>
#  include <X11/Xutil.h>
#  include <X11/Xatom.h>
#  include <X11/SM/SMlib.h>
#  include <fixx11h.h>
#endif

KApplication* KApplication::KApp = 0L;

static const int s_quit_signals[] = {
    SIGTERM,
    SIGHUP,
    SIGINT,
    0
};

static void quit_handler(int sig)
{
    if (!qApp) {
        KDE_signal(sig, SIG_DFL);
        return;
    }

    if (qApp->type() == KAPPLICATION_GUI_TYPE) {
        const QWidgetList toplevelwidgets = QApplication::topLevelWidgets();
        if (!toplevelwidgets.isEmpty()) {
            kDebug() << "closing top-level main windows";
            foreach (QWidget* topwidget, toplevelwidgets) {
                if (!topwidget || !topwidget->isWindow() || !topwidget->inherits("QMainWindow")) {
                    continue;
                }
                kDebug() << "closing" << topwidget;
                if (!topwidget->close()) {
                    kDebug() << "not quiting because a top-level window did not close";
                    return;
                }
            }
            kDebug() << "all top-level main windows closed";
        }
    }
    KDE_signal(sig, SIG_DFL);
    qApp->quit();
}

static void kRegisterSessionClient(const bool enable, const QString &serviceName)
{
    if (serviceName.isEmpty()) {
        return;
    }
    QDBusInterface sessionManager(
        "org.kde.plasma-desktop", "/App", "local.PlasmaApp",
        QDBusConnection::sessionBus()
    );
    if (sessionManager.isValid()) {
        sessionManager.call(enable ? "registerClient" : "unregisterClient", serviceName);
    } else {
        kWarning() << "org.kde.plasma-desktop is not valid interface";
    }
}

/*
  Private data
 */
class KApplicationPrivate
{
public:
  KApplicationPrivate(KApplication* q, const QByteArray &cName)
      : q(q)
      , adaptor(nullptr)
      , componentData(cName)
      , startup_id("0")
      , app_started_timer(nullptr)
      , session_save(false)
      , pSessionConfig(nullptr)
      , bSessionManagement(false)
      , debugger(nullptr)
  {
  }

  KApplicationPrivate(KApplication* q, const KComponentData &cData)
      : q(q)
      , adaptor(nullptr)
      , componentData(cData)
      , startup_id("0")
      , app_started_timer(nullptr)
      , session_save(false)
      , pSessionConfig(nullptr)
      , bSessionManagement(false)
      , debugger(nullptr)
  {
  }

  KApplicationPrivate(KApplication *q)
      : q(q)
      , adaptor(nullptr)
      , componentData(KCmdLineArgs::aboutData())
      , startup_id("0")
      , app_started_timer(nullptr)
      , session_save(false)
      , pSessionConfig(nullptr)
      , bSessionManagement(false)
      , debugger(nullptr)
  {
  }

  void _k_x11FilterDestroyed();
  void _k_checkAppStartedSlot();
  void _k_aboutToQuitSlot();

  void init();
  void parseCommandLine( ); // Handle KDE arguments (Using KCmdLineArgs)

  KApplication *q;

  KApplicationAdaptor* adaptor;
  QString serviceName;
  KComponentData componentData;
  QByteArray startup_id;
  QTimer* app_started_timer;

  bool session_save;
  QString sessionKey;
  KConfig* pSessionConfig; //instance specific application config object
  bool bSessionManagement;

  KDebugger* debugger;
};

static QList< QWeakPointer< QWidget > > *x11Filter = 0;

/**
   * Installs a handler for the SIGPIPE signal. It is thrown when you write to
   * a pipe or socket that has been closed.
   * The handler is installed automatically in the constructor, but you may
   * need it if your application or component does not have a KApplication
   * instance.
   */
static void installSigpipeHandler()
{
    struct sigaction act;
    act.sa_handler = SIG_IGN;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGPIPE, &act, 0);
}

void KApplication::installX11EventFilter( QWidget* filter )
{
    if ( !filter )
        return;
    if (!x11Filter)
        x11Filter = new QList< QWeakPointer< QWidget > >;
    connect ( filter, SIGNAL(destroyed()), this, SLOT(_k_x11FilterDestroyed()) );
    x11Filter->append( filter );
}

void KApplicationPrivate::_k_x11FilterDestroyed()
{
    q->removeX11EventFilter( static_cast< const QWidget* >(q->sender()));
}

void KApplication::removeX11EventFilter( const QWidget* filter )
{
    if ( !x11Filter || !filter )
        return;
    // removeAll doesn't work, creating QWeakPointer to something that's about to be deleted aborts
    // x11Filter->removeAll( const_cast< QWidget* >( filter ));
    QMutableListIterator< QWeakPointer< QWidget > > it( *x11Filter );
    while (it.hasNext()) {
        QWeakPointer< QWidget > wp = it.next();
        if( wp.isNull() || wp.data() == filter )
            it.remove();
    }
    if ( x11Filter->isEmpty() ) {
        delete x11Filter;
        x11Filter = 0;
    }
}

bool KApplication::notify(QObject *receiver, QEvent *event)
{
    QEvent::Type t = event->type();
    if( t == QEvent::Show && receiver->isWidgetType())
    {
        QWidget* w = static_cast<QWidget*>(receiver);
#if defined Q_WS_X11
        if (w->isTopLevel() && !startupId().isEmpty()) {
            // TODO better done using window group leader?
            KStartupInfo::setWindowStartupId(w->winId(), startupId());
#endif
        }
        if (w->isTopLevel() && !( w->windowFlags() & Qt::X11BypassWindowManagerHint )
            && w->windowType() != Qt::Popup && !event->spontaneous())
        {
            if (!d->app_started_timer) {
                d->app_started_timer = new QTimer(this);
                connect(d->app_started_timer, SIGNAL(timeout()), SLOT(_k_checkAppStartedSlot()));
            }
            if (!d->app_started_timer->isActive()) {
                d->app_started_timer->setSingleShot(true);
                d->app_started_timer->start(0);
            }
        }
    }
    return QApplication::notify(receiver, event);
}

void KApplicationPrivate::_k_checkAppStartedSlot()
{
#if defined Q_WS_X11
    KStartupInfo::handleAutoAppStartedSending();
#endif

    // at this point all collections should be set, now is the time to read ther configuration
    // because it is not done anywhere else. unfortunately that magic also means any collections
    // created afterwards will need an explicit settings read
    foreach (KActionCollection* collection, KActionCollection::allCollections()) {
        collection->readSettings();
    }
}

KApplication::KApplication()
    : QApplication(KCmdLineArgs::qtArgc(), KCmdLineArgs::qtArgv()),
    d(new KApplicationPrivate(this))
{
    setApplicationName(d->componentData.componentName());
    setOrganizationDomain(d->componentData.aboutData()->organizationDomain());
    setApplicationVersion(d->componentData.aboutData()->version());
    installSigpipeHandler();
    d->init();
}

#ifdef Q_WS_X11
KApplication::KApplication(Display *dpy, Qt::HANDLE visual, Qt::HANDLE colormap)
    : QApplication(dpy, KCmdLineArgs::qtArgc(), KCmdLineArgs::qtArgv(), visual, colormap),
    d(new KApplicationPrivate(this))
{
    setApplicationName(d->componentData.componentName());
    setOrganizationDomain(d->componentData.aboutData()->organizationDomain());
    setApplicationVersion(d->componentData.aboutData()->version());
    installSigpipeHandler();
    d->init();
}

KApplication::KApplication(Display *dpy, Qt::HANDLE visual, Qt::HANDLE colormap, const KComponentData &cData)
    : QApplication(dpy, KCmdLineArgs::qtArgc(), KCmdLineArgs::qtArgv(), visual, colormap),
    d (new KApplicationPrivate(this, cData))
{
    setApplicationName(d->componentData.componentName());
    setOrganizationDomain(d->componentData.aboutData()->organizationDomain());
    setApplicationVersion(d->componentData.aboutData()->version());
    installSigpipeHandler();
    d->init();
}
#endif

KApplication::KApplication(const KComponentData &cData)
    : QApplication(KCmdLineArgs::qtArgc(), KCmdLineArgs::qtArgv()),
    d (new KApplicationPrivate(this, cData))
{
    setApplicationName(d->componentData.componentName());
    setOrganizationDomain(d->componentData.aboutData()->organizationDomain());
    setApplicationVersion(d->componentData.aboutData()->version());
    installSigpipeHandler();
    d->init();
}

#ifdef Q_WS_X11
KApplication::KApplication(Display *display, int& argc, char** argv, const QByteArray& rAppName)
    : QApplication(display),
    d(new KApplicationPrivate(this, rAppName))
{
    setApplicationName(QString::fromLocal8Bit(rAppName.constData(), rAppName.size()));
    installSigpipeHandler();
    KCmdLineArgs::initIgnore(argc, argv, rAppName);
    d->init();
}
#endif

void KApplicationPrivate::init()
{
  if ((getuid() != geteuid()) ||
      (getgid() != getegid()))
  {
     fprintf(stderr, "The KDE libraries are not designed to run with suid privileges.\n");
     ::exit(127);
  }


  KApplication::KApp = q;

  // make sure the clipboard is created before setting the window icon (bug 209263)
  (void) QApplication::clipboard();

#if defined Q_WS_X11
  KStartupInfoId id = KStartupInfo::currentStartupIdEnv();
  KStartupInfo::resetStartupEnv();
  startup_id = id.id();
#endif

  parseCommandLine();

  // sanity checking, to make sure we've connected
  QDBusConnection sessionBus = QDBusConnection::sessionBus();
  QDBusConnectionInterface *bus = 0;
  if (!sessionBus.isConnected() || !(bus = sessionBus.interface())) {
      kFatal() << "Session bus not found, to circumvent this problem try the following command (with Linux and bash)\n"
               << "export $(dbus-launch)";
      ::exit(125);
  }

  extern bool s_kuniqueapplication_startCalled;
  if ( bus && !s_kuniqueapplication_startCalled ) // don't register again if KUniqueApplication did so already
  {
      QStringList parts = q->organizationDomain().split(QLatin1Char('.'), QString::SkipEmptyParts);
      QString reversedDomain;
      if (parts.isEmpty())
          reversedDomain = QLatin1String("local.");
      else
          foreach (const QString& s, parts)
          {
              reversedDomain.prepend(QLatin1Char('.'));
              reversedDomain.prepend(s);
          }
      const QString pidSuffix = QString::number( getpid() ).prepend( QLatin1String("-") );
      serviceName = reversedDomain + QCoreApplication::applicationName() + pidSuffix;
      if ( bus->registerService(serviceName) == QDBusConnectionInterface::ServiceNotRegistered ) {
          kError() << "Couldn't register name '" << serviceName << "' with DBUS - another process owns it already!";
          ::exit(126);
      }
  }
  adaptor = new KApplicationAdaptor(q);
  sessionBus.registerObject(QLatin1String("/MainApplication"), q,
                            QDBusConnection::ExportScriptableSlots |
                            QDBusConnection::ExportScriptableProperties |
                            QDBusConnection::ExportAdaptors);

  // Trigger creation of locale.
  (void) KGlobal::locale();

  KSharedConfig::Ptr config = componentData.config();
  QByteArray readOnly = qgetenv("KDE_HOME_READONLY");
  if (readOnly.isEmpty() && QCoreApplication::applicationName() != QLatin1String("kdialog"))
  {
    config->isConfigWritable(true);
  }

  if (q->type() == KAPPLICATION_GUI_TYPE)
  {
#ifdef Q_WS_X11
    // this is important since we fork() to launch the help (Matthias)
    fcntl(ConnectionNumber(QX11Info::display()), F_SETFD, FD_CLOEXEC);
#endif

    // Trigger initial settings
    KGlobalSettings::self()->activate(
        KGlobalSettings::ApplySettings | KGlobalSettings::ListenForChanges
    );
  }

  // too late to restart if the application is about to quit (e.g. if QApplication::quit() was
  // called or SIGTERM was received)
  q->connect(q, SIGNAL(aboutToQuit()), SLOT(_k_aboutToQuitSlot()));

  KApplication::quitOnSignal();
  KApplication::quitOnDisconnected();

  qRegisterMetaType<KUrl>();
  qRegisterMetaType<KUrl::List>();
}

KApplication* KApplication::kApplication()
{
    return KApp;
}

KConfig* KApplication::sessionConfig()
{
    if (!d->pSessionConfig) {
        // create an instance specific config object
        QString configName = d->sessionKey;
        if (configName.isEmpty()) {
            configName = QString::fromLatin1("%1_%2").arg(QCoreApplication::applicationName()).arg(QCoreApplication::applicationPid());
        }
        d->pSessionConfig = new KConfig(
            QString::fromLatin1("session/%1").arg(configName),
            KConfig::SimpleConfig
        );
    }
    return d->pSessionConfig;
}

bool KApplication::saveSession()
{
    foreach (KMainWindow *window, KMainWindow::memberList()) {
        if (!window->testAttribute(Qt::WA_WState_Hidden)) {
            QCloseEvent e;
            QApplication::sendEvent(window, &e);
            if (!e.isAccepted()) {
                return false;
            }
       }
    }
    foreach (QWidget* widget, QApplication::topLevelWidgets()) {
        if (!widget || widget->isHidden() || widget->inherits("QMainWindow")) {
            continue;
        }
        QCloseEvent e;
        QApplication::sendEvent(widget, &e);
        if (!e.isAccepted()) {
            return false;
        }
    }

    d->session_save = true;
    KConfig* config = KApplication::kApplication()->sessionConfig();
    if ( KMainWindow::memberList().count() ){
        // According to Jochen Wilhelmy <digisnap@cs.tu-berlin.de>, this
        // hook is useful for better document orientation
        KMainWindow::memberList().first()->saveGlobalProperties(config);
    }
    int n = 0;
    foreach (KMainWindow* mw, KMainWindow::memberList()) {
        n++;
        mw->savePropertiesInternal(config, n);
    }
    KConfigGroup group( config, "Number" );
    group.writeEntry("NumberOfWindows", n );
    if ( d->pSessionConfig ) {
        d->pSessionConfig->sync();
    }
    d->session_save = false;
    return true;
}

void KApplication::reparseConfiguration()
{
    KGlobal::config()->reparseConfiguration();
}

void KApplication::quit()
{
    QApplication::quit();
}

void KApplication::disableSessionManagement()
{
    if (d->bSessionManagement) {
        kRegisterSessionClient(false, d->serviceName);
    }
    d->bSessionManagement = false;
}

void KApplication::enableSessionManagement()
{
    kRegisterSessionClient(true, d->serviceName);
    d->bSessionManagement = true;
}

bool KApplication::sessionSaving() const
{
    return d->session_save;
}

bool KApplication::isSessionRestored() const
{
    return !d->sessionKey.isEmpty();
}

void KApplicationPrivate::parseCommandLine( )
{
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs("kde");

    if (args && args->isSet("config"))
    {
        QString config = args->getOption("config");
        componentData.setConfigName(config);
    }

    if ( q->type() != KApplication::Tty ) {
        QString appicon;
        if (args && args->isSet("icon")
            && !args->getOption("icon").trimmed().isEmpty()
            && !KIconLoader::global()->iconPath(args->getOption("icon"), -1, true).isEmpty())
        {
            appicon = args->getOption("icon");
        }
        if(appicon.isEmpty()) {
            appicon = componentData.aboutData()->programIconName();
        }
        q->setWindowIcon(KIcon(appicon));
    }

    if (!args)
        return;

    if (qgetenv("KDE_DEBUG").isEmpty() && args->isSet("crashhandler")) {
        // setup default crash handler
        KCrash::setFlags(KCrash::Notify | KCrash::Log);
    }

#ifdef Q_WS_X11
    if (args->isSet("waitforwm")) {
        Atom type;
        (void) q->desktop(); // trigger desktop creation, we need PropertyNotify events for the root window
        int format;
        unsigned long length, after;
        unsigned char *data;
        Atom netSupported = XInternAtom(QX11Info::display(), "_NET_SUPPORTED", False);
        while (XGetWindowProperty(QX11Info::display(), QX11Info::appRootWindow(), netSupported,
                0, 1, false, AnyPropertyType, &type, &format, &length, &after, &data) != Success || !length)
        {
            if (data) {
                XFree(data);
            }
            data = nullptr;
            XEvent event;
            XWindowEvent(QX11Info::display(), QX11Info::appRootWindow(), PropertyChangeMask, &event);
        }
        if (data) {
            XFree(data);
        }
    }
#endif

    if (args->isSet("session")) {
        sessionKey = args->getOption("session");
    }

    if (args->isSet("debugger")) {
        debugger = new KDebugger();
        debugger->show();
    }
}

KApplication::~KApplication()
{
    if (d->debugger) {
        delete d->debugger;
    }

    if (d->pSessionConfig) {
        delete d->pSessionConfig;
    }

    delete d;
    KApp = 0;
}


#ifdef Q_WS_X11
class KAppX11HackWidget: public QWidget
{
public:
    bool publicx11Event( XEvent * e) { return x11Event( e ); }
};
#endif



#ifdef Q_WS_X11
bool KApplication::x11EventFilter( XEvent *_event )
{
    if (x11Filter) {
        // either deep-copy or mutex
        QListIterator< QWeakPointer< QWidget > > it( *x11Filter );
        while (it.hasNext()) {
            QWeakPointer< QWidget > wp = it.next();
            if( !wp.isNull() )
                if ( static_cast<KAppX11HackWidget*>( wp.data() )->publicx11Event(_event))
                    return true;
        }
    }

    return false;
}
#endif // Q_WS_X11

void KApplication::updateUserTimestamp( int time )
{
#if defined Q_WS_X11
    if( time == 0 )
    { // get current X timestamp
        Window w = XCreateSimpleWindow( QX11Info::display(), QX11Info::appRootWindow(), 0, 0, 1, 1, 0, 0, 0 );
        XSelectInput( QX11Info::display(), w, PropertyChangeMask );
        unsigned char data[ 1 ];
        XChangeProperty( QX11Info::display(), w, XA_ATOM, XA_ATOM, 8, PropModeAppend, data, 1 );
        XEvent ev;
        XWindowEvent( QX11Info::display(), w, PropertyChangeMask, &ev );
        time = ev.xproperty.time;
        XDestroyWindow( QX11Info::display(), w );
    }
    if( QX11Info::appUserTime() == 0
        || NET::timestampCompare( time, QX11Info::appUserTime()) > 0 ) // time > appUserTime
        QX11Info::setAppUserTime(time);
    if( QX11Info::appTime() == 0
        || NET::timestampCompare( time, QX11Info::appTime()) > 0 ) // time > appTime
        QX11Info::setAppTime(time);
#endif
}

unsigned long KApplication::userTimestamp() const
{
#if defined Q_WS_X11
    return QX11Info::appUserTime();
#else
    return 0;
#endif
}

void KApplication::quitOnSignal()
{
    sigset_t handlermask;
    ::sigemptyset(&handlermask);
    int counter = 0;
    while (s_quit_signals[counter]) {
        KDE_signal(s_quit_signals[counter], quit_handler);
        ::sigaddset(&handlermask, s_quit_signals[counter]);
        counter++;
    }
    ::sigprocmask(SIG_UNBLOCK, &handlermask, NULL);
}

void KApplication::quitOnDisconnected()
{
  if (!qApp) {
    kWarning() << "KApplication::quitOnDisconnected() called before application instance is created";
    return;
  }
  QDBusConnection::sessionBus().connect(
    QString(),
    QString::fromLatin1("/org/freedesktop/DBus/Local"),
    QString::fromLatin1("org.freedesktop.DBus.Local"),
    QString::fromLatin1("Disconnected"),
    qApp, SLOT(quit())
  );
}

void KApplication::setTopWidget( QWidget *topWidget )
{
    if( !topWidget )
      return;

    // set the specified caption
    if ( !topWidget->inherits("KMainWindow") ) { // KMainWindow does this already for us
        topWidget->setWindowTitle(KGlobal::caption());
    }

#ifdef Q_WS_X11
    // set the app startup notification window property
    KStartupInfo::setWindowStartupId(topWidget->winId(), startupId());
#endif
}

QByteArray KApplication::startupId() const
{
    return d->startup_id;
}

void KApplication::setStartupId(const QByteArray &startup_id)
{
    if (startup_id == d->startup_id) {
        return;
    }
#if defined Q_WS_X11
    KStartupInfo::handleAutoAppStartedSending(); // finish old startup notification if needed
#endif
    if (startup_id.isEmpty()) {
        d->startup_id = "0";
    } else {
        d->startup_id = startup_id;
#if defined Q_WS_X11
        KStartupInfoId id;
        id.initId(startup_id);
        long timestamp = id.timestamp();
        if (timestamp != 0) {
            updateUserTimestamp(timestamp);
        }
#endif
    }
}

void KApplication::clearStartupId()
{
    d->startup_id = "0";
}

void KApplicationPrivate::_k_aboutToQuitSlot()
{
    KCrash::setFlags(KCrash::flags() & ~KCrash::AutoRestart);
    if (bSessionManagement) {
        kRegisterSessionClient(false, serviceName);
    }
}

#include "moc_kapplication.cpp"
