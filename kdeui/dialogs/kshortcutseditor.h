/*
    This file is part of the KDE libraries
    Copyright (C) 2024 Ivailo Monev <xakepa10@gmail.com>

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

#ifndef KSHORTCUTSEDITOR_H
#define KSHORTCUTSEDITOR_H

#include <kdeui_export.h>

#include <QWidget>

class KActionCollection;
class KConfigBase;
class KConfigGroup;
class KShortcutsEditorPrivate;


/**
 * @short Widget for configuration of KAccel and KGlobalAccel.
 *
 * The class takes care of all aspects of configuration shortcuts, including handling key conflicts
 * internally..
 *
 * @see KShortcutsDialog
 * @author Ivailo Monev <xakepa10@gmail.com>
 */
class KDEUI_EXPORT KShortcutsEditor : public QWidget
{
    Q_OBJECT
public:
    enum ActionType {
        /// Actions which are triggered by any keypress in a widget
        LocalAction       = 1,
        /// Actions which are triggered by any keypress in the windowing system
        GlobalAction      = 2,
        /// All actions
        AllActions        = (LocalAction | GlobalAction)
    };
    Q_DECLARE_FLAGS(ActionTypes, ActionType)

    enum LetterShortcuts {
        /// Shortcuts without a modifier are not allowed,
        /// so 'A' would not be valid, whereas 'Ctrl+A' would be.
        /// This only applies to printable characters, however.
        /// 'F1', 'Insert' etc. could still be used.
        LetterShortcutsDisallowed = 0,
        /// Letter shortcuts are allowed
        LetterShortcutsAllowed
    };

    /**
     * Constructor.
     *
     * @param collection the KActionCollection to configure
     * @param parent parent widget
     * @param actionTypes types of actions to display in this widget.
     * @param letterShortcuts set to LetterShortcutsDisallowed if unmodified alphanumeric
     *  keys ('A', '1', etc.) are not permissible shortcuts.
     */
    KShortcutsEditor(KActionCollection *collection, QWidget *parent,
                     ActionTypes actionTypes = AllActions,
                     LetterShortcuts letterShortcuts = LetterShortcutsAllowed);

    /**
     * \overload
     *
     * Creates a key chooser without a starting action collection.
     *
     * @param parent parent widget
     * @param actionTypes types of actions to display in this widget.
     * @param letterShortcuts set to LetterShortcutsDisallowed if unmodified alphanumeric
     *  keys ('A', '1', etc.) are not permissible shortcuts.
     */
    explicit KShortcutsEditor(QWidget *parent, ActionTypes actionTypes = AllActions,
                              LetterShortcuts letterShortcuts = LetterShortcutsAllowed);

    /// Destructor
    virtual ~KShortcutsEditor();

    /**
     * Are the unsaved changes?
     */
    bool isModified() const;

    /**
     * Removes all action collections from the editor
     */
    void clearCollections();

    /**
     * Insert an action collection, i.e. add all its actions to the ones
     * already associated with the KShortcutsEditor object.
     * @param title subtree title of this collection of shortcut.
     */
    void addCollection(KActionCollection *, const QString &title = QString());

    /**
     * Export the current setting to configuration @p config.
     *
     * This initializes the configuration object. This will export the global
     * configuration too.
     *
     * @param config Config object
     */
    void exportConfiguration(KConfigBase *config = nullptr) const;

    /**
     * Import the settings from configuration @p config.
     *
     * This will remove all current setting before importing. All shortcuts
     * are set to QKeySequence() prior to importing from @p config!
     *
     * @param config Config object
     */
    void importConfiguration(KConfigBase *config = nullptr);

Q_SIGNALS:
    /**
     * Emitted when an action's shortcut has been changed.
     **/
    void keyChange();

public Q_SLOTS:
    /**
     * Set all shortcuts to their default values (bindings).
     **/
    void allDefault();

private:
    Q_PRIVATE_SLOT(d, void _k_slotKeySequenceChanged())

    friend class KShortcutsDialog;
    friend class KShortcutsEditorPrivate;
    Q_DISABLE_COPY(KShortcutsEditor)
    KShortcutsEditorPrivate *const d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(KShortcutsEditor::ActionTypes)

#endif // KSHORTCUTSEDITOR_H
