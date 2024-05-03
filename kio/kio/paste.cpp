/* This file is part of the KDE libraries
   Copyright (C) 2000 David Faure <faure@kde.org>

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

#include "paste.h"
#include "kio/job.h"
#include "kio/copyjob.h"
#include "kio/deletejob.h"
#include "kio/global.h"
#include "kio/netaccess.h"
#include "kio/renamedialog.h"
#include "kio/kprotocolmanager.h"
#include "jobuidelegate.h"
#include "kdialog.h"
#include "kurl.h"
#include "klocale.h"
#include "kinputdialog.h"
#include "kmessagebox.h"
#include "kmimetype.h"
#include "kcombobox.h"
#include "klineedit.h"
#include "kdebug.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QLabel>
#include <QLayout>

static KIO::Job *pasteClipboardUrls(const QMimeData* mimeData, const KUrl& destDir)
{
    const KUrl::List urls = KUrl::List::fromMimeData(mimeData, KUrl::List::PreferLocalUrls);
    if (!urls.isEmpty()) {
        const QByteArray data = mimeData->data("application/x-kde-cutselection");
        const bool move = data.isEmpty() ? false : data.at(0) == '1';
        KIO::Job *job = 0;
        if (move) {
            job = KIO::move(urls, destDir);
        } else {
            job = KIO::copy(urls, destDir);
        }
        return job;
    }
    return 0;
}

static KIO::Job* putDataAsyncTo(const KUrl& url, const QByteArray& data, QWidget* widget, KIO::JobFlags flags)
{
    KIO::Job* job = KIO::storedPut(data, url, -1, flags);
    job->ui()->setWindow(widget);
    return job;
}

static QStringList extractFormats(const QMimeData* mimeData)
{
    QStringList formats;
    Q_FOREACH(const QString& format, mimeData->formats()) {
        if (format == QLatin1String("application/x-kde-cutselection")) { // see KonqDrag
            continue;
        }
        if (format == QLatin1String("application/x-kde-suggestedfilename")) {
            continue;
        }
        if (format.startsWith(QLatin1String("application/x-qt-"))) { // Katie-internal
            continue;
        }
        if (!format.contains(QLatin1Char('/'))) { // e.g. TARGETS, MULTIPLE, TIMESTAMP
            continue;
        }
        formats.append(format);
    }
    return formats;
}

class PasteDialog : public KDialog
{
    Q_OBJECT
public:
    PasteDialog( const QString &caption, const QString &label,
                 const QString &value, const QStringList& items,
                 QWidget *parent, bool clipboard );

    QString lineEditText() const;
    int comboItem() const;
    bool clipboardChanged() const;

private Q_SLOTS:
    void slotClipboardDataChanged();

private:
    QLabel* m_label;
    KLineEdit* m_lineEdit;
    KComboBox* m_comboBox;
    bool m_clipboardChanged;
};

PasteDialog::PasteDialog(const QString &caption, const QString &label,
                         const QString &value, const QStringList &items,
                         QWidget *parent,
                         bool clipboard)
    : KDialog(parent),
    m_label(nullptr),
    m_lineEdit(nullptr),
    m_comboBox(nullptr),
    m_clipboardChanged(false)
{
    setCaption(caption );
    setButtons(KDialog::Ok | KDialog::Cancel);
    setModal(true);
    setDefaultButton(KDialog::Ok);

    QFrame *frame = new QFrame;
    setMainWidget(frame);

    QVBoxLayout *layout = new QVBoxLayout(frame);

    m_label = new QLabel(label, frame);
    layout->addWidget(m_label);

    m_lineEdit = new KLineEdit(value, frame);
    layout->addWidget(m_lineEdit);

    m_lineEdit->setFocus();
    m_label->setBuddy(m_lineEdit);

    layout->addWidget( new QLabel(i18n("Data format:"), frame));
    m_comboBox = new KComboBox(frame);
    m_comboBox->addItems(items);
    layout->addWidget(m_comboBox);

    layout->addStretch();

    // connect( m_lineEdit, SIGNAL(textChanged(QString)), SLOT(slotEditTextChanged(QString)));
    // connect(this, SIGNAL(user1Clicked()), m_lineEdit, SLOT(clear()));

    //slotEditTextChanged(value);
    setMinimumWidth(350);

    if (clipboard) {
        connect(
            QApplication::clipboard(), SIGNAL(dataChanged()),
            this, SLOT(slotClipboardDataChanged())
        );
    }
}

void PasteDialog::slotClipboardDataChanged()
{
    m_clipboardChanged = true;
}

QString PasteDialog::lineEditText() const
{
    return m_lineEdit->text();
}

int PasteDialog::comboItem() const
{
    return m_comboBox->currentIndex();
}

bool PasteDialog::clipboardChanged() const
{
    return m_clipboardChanged;
}

KIO_EXPORT bool KIO::canPasteMimeSource(const QMimeData* data)
{
    return data->hasText() || !extractFormats(data).isEmpty();
}

KIO::Job* pasteMimeDataImpl(const QMimeData* mimeData, const KUrl& destUrl,
                            const QString& dialogText, QWidget* widget,
                            bool clipboard)
{
    QByteArray ba;
    const QString suggestedFilename = QString::fromUtf8(mimeData->data("application/x-kde-suggestedfilename"));

    // Now check for plain text
    // We don't want to display a mimetype choice for a QTextDrag, those mimetypes look ugly.
    if (mimeData->hasText()) {
        ba = mimeData->text().toLocal8Bit(); // encoding OK?
    } else {
        const QStringList formats = extractFormats(mimeData);
        if (formats.isEmpty()) {
            return nullptr;
        } else if (formats.size() > 1) {
            QStringList formatLabels;
            for (int i = 0; i < formats.size(); i++) {
                const QString& fmt = formats[i];
                KMimeType::Ptr mime = KMimeType::mimeType(fmt, KMimeType::ResolveAliases);
                if (mime) {
                    formatLabels.append(i18n("%1 (%2)", mime->comment(), fmt));
                } else {
                    formatLabels.append(fmt);
                }
            }

            QString text(dialogText);
            if (text.isEmpty()) {
                text = i18n("Filename for clipboard content:");
            }
            PasteDialog dlg(QString(), text, suggestedFilename, formatLabels, widget, clipboard);

            if (dlg.exec() != KDialog::Accepted) {
                return nullptr;
            }

            if (clipboard && dlg.clipboardChanged()) {
                KMessageBox::sorry(
                    widget,
                    i18n(
                        "The clipboard has changed since you used 'paste': "
                        "the chosen data format is no longer applicable. "
                        "Please copy again what you wanted to paste."
                    )
                );
                return nullptr;
            }

            const QString result = dlg.lineEditText();
            const QString chosenFormat = formats[dlg.comboItem()];

            kDebug() << " result=" << result << " chosenFormat=" << chosenFormat;
            KUrl newUrl = destUrl;
            newUrl.addPath(result);
            // if "data" came from QClipboard, then it was deleted already - by a nice 0-seconds timer
            // In that case, get it again. Let's hope the user didn't copy something else meanwhile :/
            // #### QT4/KDE4 TODO: check that this is still the case
            if (clipboard) {
                mimeData = QApplication::clipboard()->mimeData();
            }
            ba = mimeData->data(chosenFormat);
            if (ba.isEmpty()) {
                return nullptr;
            }
            return putDataAsyncTo(newUrl, ba, widget, KIO::Overwrite);
        }
        ba = mimeData->data(formats.first());
    }
    if (ba.isEmpty()) {
        return nullptr;
    }

    bool ok = false;
    QString text(dialogText);
    if (text.isEmpty()) {
        text = i18n("Filename for clipboard content:");
    }
    QString file = KInputDialog::getText(QString(), text, suggestedFilename, &ok, widget);
    if (!ok) {
        return nullptr;
    }

    KUrl newUrl(destUrl);
    newUrl.addPath(file);

    // Check for existing destination file.
    // When we were using CopyJob, we couldn't let it do that (would expose
    // an ugly tempfile name as the source URL)
    // And now we're using a put job anyway, no destination checking included.
    if (KIO::NetAccess::exists(newUrl, KIO::NetAccess::DestinationSide, widget)) {
        kDebug(7007) << "Paste will overwrite file.  Prompting...";
        KIO::RenameDialog dlg(widget,
                              i18n("File Already Exists"),
                              destUrl.pathOrUrl(),
                              newUrl.pathOrUrl(),
                              (KIO::RenameDialog_Mode) (KIO::M_OVERWRITE | KIO::M_SINGLE) );
        KIO::RenameDialog_Result res = static_cast<KIO::RenameDialog_Result>(dlg.exec());
        if (res == KIO::R_RENAME) {
            newUrl = dlg.newDestUrl();
        } else if (res == KIO::R_CANCEL) {
            return nullptr;
        }
    }

    if (newUrl.isEmpty()) {
        return nullptr;
    }

    return putDataAsyncTo(newUrl, ba, widget, KIO::Overwrite);
}

// The main method for pasting
KIO_EXPORT KIO::Job *KIO::pasteClipboard( const KUrl& destUrl, QWidget* widget, bool move )
{
    Q_UNUSED(move);

    if (!destUrl.isValid()) {
        KMessageBox::error(widget, i18n("Malformed URL\n%1", destUrl.prettyUrl()));
        return 0;
    }

    // TODO: if we passed mimeData as argument, we could write unittests that don't
    // mess up the clipboard and that don't need QtGui.
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();

    if (KUrl::List::canDecode(mimeData)) {
        // We can ignore the bool move, KIO::paste decodes it
        KIO::Job* job = pasteClipboardUrls(mimeData, destUrl);
        if (job) {
            job->ui()->setWindow(widget);
            return job;
        }
    }

    return pasteMimeDataImpl(mimeData, destUrl, QString(), widget, true /*clipboard*/);
}

// NOTE: DolphinView::pasteInfo() has a better version of this
// (but which requires KonqFileItemCapabilities)
KIO_EXPORT QString KIO::pasteActionText()
{
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    const KUrl::List urls = KUrl::List::fromMimeData(mimeData);
    if (!urls.isEmpty()) {
        if (urls.first().isLocalFile()) {
            return i18np("&Paste File", "&Paste %1 Files", urls.count());
        }
        return i18np("&Paste URL", "&Paste %1 URLs", urls.count());
    } else if ( !mimeData->formats().isEmpty() ) {
        return i18n( "&Paste Clipboard Contents" );
    }
    return QString();
}

// The [new] main method for dropping
KIO_EXPORT KIO::Job* KIO::pasteMimeData(const QMimeData* mimeData, const KUrl& destUrl,
                                        const QString& dialogText, QWidget* widget)
{
    return pasteMimeDataImpl(mimeData, destUrl, dialogText, widget, false /*not clipboard*/);
}

#include "paste.moc"
