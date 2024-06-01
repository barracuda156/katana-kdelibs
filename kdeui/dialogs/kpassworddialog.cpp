/* This file is part of the KDE libraries
   Copyright (C) 2000 David Faure <faure@kde.org>
   Copyright (C) 2007 Olivier Goffart <ogoffart at kde.org>

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
#include "kpassworddialog.h"
#include "kcombobox.h"
#include "kconfig.h"
#include "kiconloader.h"
#include "klineedit.h"
#include "klocale.h"
#include "kconfiggroup.h"
#include "ktitlewidget.h"
#include "kpixmapwidget.h"
#include "kdebug.h"

#include <QCheckBox>
#include <QLayout>
#include <QTextDocument>
#include <QTimer>
#include <QDesktopWidget>

#include "ui_kpassworddialog.h"

/** @internal */
class KPasswordDialog::KPasswordDialogPrivate
{
public:
    KPasswordDialogPrivate(KPasswordDialog *q)
        : q(q),
        pixmapWidget(nullptr),
        commentRow(0)
    {
    }

    void actuallyAccept();

    KPasswordDialog *q;
    KPasswordDialogFlags m_flags;
    Ui_KPasswordDialog ui;
    KPixmapWidget* pixmapWidget;
    unsigned int commentRow;
};

KPasswordDialog::KPasswordDialog(QWidget *parent,
                                 const KPasswordDialogFlags flags,
                                 const KDialog::ButtonCodes otherButtons)
    : KDialog(parent),
    d(new KPasswordDialogPrivate(this))
{
    setCaption(i18n("Password"));
    setWindowIcon(KIcon("dialog-password"));
    setButtons(KDialog::Ok | KDialog::Cancel | otherButtons);
    setDefaultButton(KDialog::Ok);

    d->m_flags = flags;

    d->ui.setupUi(mainWidget());
    d->ui.errorMessage->setHidden(true);

    // Row 4: Username field
    if (d->m_flags & KPasswordDialog::ShowUsernameLine) {
        d->ui.userEdit->setFocus();
        d->ui.credentialsGroup->setFocusProxy(d->ui.userEdit);
        connect(d->ui.userEdit, SIGNAL(returnPressed()), d->ui.passEdit, SLOT(setFocus()));
    } else {
        d->ui.userNameLabel->hide();
        d->ui.userEdit->hide();
        d->ui.passEdit->setFocus();
        d->ui.credentialsGroup->setFocusProxy(d->ui.passEdit);
    }

    if (!(d->m_flags & KPasswordDialog::ShowAnonymousLoginCheckBox)) {
        d->ui.anonymousRadioButton->hide();
        d->ui.usePasswordButton->hide();
    }

    if (!(d->m_flags & KPasswordDialog::ShowKeepPassword)) {
        d->ui.keepCheckBox->hide();
    }

    if (d->m_flags & KPasswordDialog::UsernameReadOnly) {
        d->ui.userEdit->setReadOnly(true);
        d->ui.credentialsGroup->setFocusProxy(d->ui.passEdit);
    }
    d->ui.credentialsGroup->setEnabled(!anonymousMode());

    QRect desktop = QApplication::desktop()->screenGeometry(window());
    setMinimumWidth(qMin(1000, qMax(sizeHint().width(), desktop.width() / 4)));
    setPixmap(KIcon("dialog-password").pixmap(KIconLoader::SizeHuge));
}

KPasswordDialog::~KPasswordDialog()
{
    delete d;
}

void KPasswordDialog::setPixmap(const QPixmap &pixmap)
{
    if (!d->pixmapWidget) {
        d->pixmapWidget = new KPixmapWidget(mainWidget());
        d->pixmapWidget->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        d->ui.hboxLayout->insertWidget(0, d->pixmapWidget);
    }

    d->pixmapWidget->setPixmap(pixmap);
}

QPixmap KPasswordDialog::pixmap() const
{
    if (!d->pixmapWidget ) {
        return QPixmap();
    }
    return d->pixmapWidget->pixmap();
}

void KPasswordDialog::setUsername(const QString &user)
{
    d->ui.userEdit->setText(user);
    if (user.isEmpty()) {
        return;
    }
    if (d->ui.userEdit->isVisibleTo(this)) {
        d->ui.passEdit->setFocus();
    }
}

QString KPasswordDialog::username() const
{
    return d->ui.userEdit->text();
}

QString KPasswordDialog::password() const
{
    return d->ui.passEdit->text();
}

void KPasswordDialog::setAnonymousMode(bool anonymous)
{
    if (anonymous && !(d->m_flags & KPasswordDialog::ShowAnonymousLoginCheckBox)) {
        // This is an error case, but we can at least let user see what's about
        // to happen if they proceed.
        d->ui.anonymousRadioButton->setVisible(true);

        d->ui.usePasswordButton->setVisible(true);
        d->ui.usePasswordButton->setEnabled(false);
    }
    d->ui.anonymousRadioButton->setChecked(anonymous);
}

bool KPasswordDialog::anonymousMode() const
{
    return d->ui.anonymousRadioButton->isChecked();
}

void KPasswordDialog::setKeepPassword(bool b)
{
    d->ui.keepCheckBox->setChecked(b);
}

bool KPasswordDialog::keepPassword() const
{
    return d->ui.keepCheckBox->isChecked();
}

void KPasswordDialog::addCommentLine(const QString &label, const QString &comment)
{
    int gridMarginLeft = 0;
    int gridMarginTop = 0;
    int gridMarginRight = 0;
    int gridMarginBottom = 0;
    d->ui.formLayout->getContentsMargins(&gridMarginLeft, &gridMarginTop, &gridMarginRight, &gridMarginBottom);

    int spacing = d->ui.formLayout->horizontalSpacing();
    if (spacing < 0) {
        // same inter-column spacing for all rows, see comment in qformlayout.cpp
        spacing = style()->combinedLayoutSpacing(QSizePolicy::Label, QSizePolicy::LineEdit, Qt::Horizontal, 0, this);
    }

    QLabel* c = new QLabel(comment, mainWidget());
    c->setWordWrap(true);

    d->ui.formLayout->insertRow(d->commentRow, label, c);
    ++d->commentRow;

    // cycle through column 0 widgets and see the max width so we can set the minimum height of
    // column 2 wordwrapable labels
    int firstColumnWidth = 0;
    for (int i = 0; i < d->ui.formLayout->rowCount(); ++i) {
        QLayoutItem *li = d->ui.formLayout->itemAt(i, QFormLayout::LabelRole);
        if (li) {
            QWidget *w = li->widget();
            if (w && !w->isHidden()) {
                firstColumnWidth = qMax(firstColumnWidth, w->sizeHint().width());
            }
        }
    }
    for (int i = 0; i < d->ui.formLayout->rowCount(); ++i) {
        QLayoutItem *li = d->ui.formLayout->itemAt(i, QFormLayout::FieldRole);
        if (li) {
            QLabel *l = qobject_cast<QLabel*>(li->widget());
            if (l && l->wordWrap()) {
                int w = sizeHint().width() - firstColumnWidth - (2 * marginHint()) - gridMarginLeft - gridMarginRight - spacing;
                l->setMinimumSize( w, l->heightForWidth(w) );
            }
        }
    }
}

void KPasswordDialog::showErrorMessage(const QString &message, const ErrorType type)
{
    d->ui.errorMessage->setText(message, KTitleWidget::ErrorMessage);

    QFont bold = font();
    bold.setBold(true);
    switch (type) {
        case KPasswordDialog::PasswordError: {
            d->ui.passwordLabel->setFont(bold);
            d->ui.passEdit->clear();
            d->ui.passEdit->setFocus();
            break;
        }
        case KPasswordDialog::UsernameError: {
            if (d->ui.userEdit->isVisibleTo(this)) {
                d->ui.userNameLabel->setFont(bold);
                d->ui.userEdit->setFocus();
            }
            break;
        }
        case KPasswordDialog::FatalError: {
            d->ui.userNameLabel->setEnabled(false);
            d->ui.userEdit->setEnabled(false);
            d->ui.passwordLabel->setEnabled(false);
            d->ui.passEdit->setEnabled(false);
            d->ui.keepCheckBox->setEnabled(false);
            enableButton(KDialog::Ok, false);
            break;
        }
        default: {
            break;
        }
    }
    adjustSize();
}

void KPasswordDialog::setPrompt(const QString &prompt)
{
    d->ui.prompt->setText(prompt);
    d->ui.prompt->setWordWrap(true);
    d->ui.prompt->setMinimumHeight(d->ui.prompt->heightForWidth(width() - (2 * marginHint())));
}

QString KPasswordDialog::prompt() const
{
    return d->ui.prompt->text();
}

void KPasswordDialog::setPassword(const QString &p)
{
    d->ui.passEdit->setText(p);
}

void KPasswordDialog::setUsernameReadOnly(bool readOnly)
{
    d->ui.userEdit->setReadOnly(readOnly);
    if (readOnly && d->ui.userEdit->hasFocus()) {
        d->ui.passEdit->setFocus();
    }
}

void KPasswordDialog::accept()
{
    if (!d->ui.errorMessage->isHidden()) {
        d->ui.errorMessage->setText(QString());
    }

    // reset the font in case we had an error previously
    if (!d->ui.passwordLabel->isHidden()) {
        d->ui.passwordLabel->setFont(font());
        d->ui.userNameLabel->setFont(font());
    }

    // allow the error message, if any, to go away. checkPassword() may block for a period of time
    QTimer::singleShot(0, this, SLOT(actuallyAccept()));
}

void KPasswordDialog::KPasswordDialogPrivate::actuallyAccept()
{
    if (!q->checkPassword()) {
        return;
    }

    bool keep = (ui.keepCheckBox->isVisibleTo(q) && ui.keepCheckBox->isChecked());
    emit q->gotPassword(q->password(), keep);

    if (ui.userEdit->isVisibleTo(q)) {
        emit q->gotUsernameAndPassword(q->username(), q->password() , keep);
    }

    q->KDialog::accept();
}

bool KPasswordDialog::checkPassword()
{
    return true;
}

#include "moc_kpassworddialog.cpp"
