/* This file is part of the KDE libraries
    Copyright (C) 1997 Stefan Taferner (taferner@alpin.or.at)
    Copyright (C) 2000 Nicolas Hadacek (haadcek@kde.org)
    Copyright (C) 2001,2002 Ellis Whitehead (ellis@kde.org)

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

#include "kstandardshortcut.h"
#include "kconfig.h"
#include "kglobal.h"
#include "klocale.h"
#include "kconfiggroup.h"
#include "kdebug.h"

#include <QKeySequence>

namespace KStandardShortcut
{

struct KStandardShortcutInfo
{
    //! The standard shortcut id. @see StandardShortcut
    StandardShortcut id;

    /** 
     * Unique name for the given accel. The name is used to save the user
     * settings. It's not representable. Use description for that.
     * @warning NEVER EVER CHANGE IT OR TRANSLATE IT!
     */
    const char* name;

    //! Context for the translation
    const char* translation_context;

    //! Localized label for user-visible display
    const char* description;

    //! The keys for this shortcut
    int cutDefault, cutDefault2;

    //! A shortcut that is created with @a cutDefault and @cutDefault2
    QKeySequence cut;

    //! If this struct is initialized. If not initialized @cut is not valid
    bool isInitialized;
};

#define CTRL(x) Qt::CTRL+Qt::Key_##x
#define SHIFT(x) Qt::SHIFT+Qt::Key_##x
#define CTRLSHIFT(x) Qt::CTRL+Qt::SHIFT+Qt::Key_##x
#define ALT(x) Qt::ALT+Qt::Key_##x
#define ALTSHIFT(x) Qt::ALT+Qt::SHIFT+Qt::Key_##x

/** Array of predefined KStandardShortcutInfo objects, which cover all
    the "standard" accelerators. Each enum value from StandardShortcut
    should appear in this table.
*/
// STUFF WILL BREAK IF YOU DON'T READ THIS!!!
// Read the comments of the big enum in kstandardshortcut.h before you change anything!
static KStandardShortcutInfo g_infoStandardShortcut[] =
{
    // Group File,
    { AccelNone, 0      , 0                           , 0       , 0      , 0           , QKeySequence(), false },
    { Open     , "Open" , I18N_NOOP2_NOSTRIP("@action", "Open") , CTRL(O), 0           , QKeySequence(), false },
    { New      , "New"  , I18N_NOOP2_NOSTRIP("@action", "New")  , CTRL(N), 0           , QKeySequence(), false },
    { Close    , "Close", I18N_NOOP2_NOSTRIP("@action", "Close"), CTRL(W), CTRL(Escape), QKeySequence(), false },
    { Save     , "Save" , I18N_NOOP2_NOSTRIP("@action", "Save") , CTRL(S), 0           , QKeySequence(), false },
    { Print    , "Print", I18N_NOOP2_NOSTRIP("@action", "Print"), CTRL(P), 0           , QKeySequence(), false },
    { Quit     , "Quit" , I18N_NOOP2_NOSTRIP("@action", "Quit") , CTRL(Q), 0           , QKeySequence(), false },

    // Group Edit
    { Undo             , "Undo"             , I18N_NOOP2_NOSTRIP("@action", "Undo")                 , CTRL(Z)          , 0            , QKeySequence(), false },
    { Redo             , "Redo"             , I18N_NOOP2_NOSTRIP("@action", "Redo")                 , CTRLSHIFT(Z)     , 0            , QKeySequence(), false },
    { Cut              , "Cut"              , I18N_NOOP2_NOSTRIP("@action", "Cut")                  , CTRL(X)          , SHIFT(Delete), QKeySequence(), false },
    { Copy             , "Copy"             , I18N_NOOP2_NOSTRIP("@action", "Copy")                 , CTRL(C)          , CTRL(Insert) , QKeySequence(), false },
    { Paste            , "Paste"            , I18N_NOOP2_NOSTRIP("@action", "Paste")                , CTRL(V)          , SHIFT(Insert), QKeySequence(), false },
    { PasteSelection   , "Paste Selection"  , I18N_NOOP2_NOSTRIP("@action", "Paste Selection")      , CTRLSHIFT(Insert), 0            , QKeySequence(), false },

    { SelectAll        , "SelectAll"        , I18N_NOOP2_NOSTRIP("@action", "Select All")           , CTRL(A)          , 0            , QKeySequence(), false },
    { Deselect         , "Deselect"         , I18N_NOOP2_NOSTRIP("@action", "Deselect")             , CTRLSHIFT(A)     , 0            , QKeySequence(), false },
    { DeleteWordBack   , "DeleteWordBack"   , I18N_NOOP2_NOSTRIP("@action", "Delete Word Backwards"), CTRL(Backspace)  , 0            , QKeySequence(), false },
    { DeleteWordForward, "DeleteWordForward", I18N_NOOP2_NOSTRIP("@action", "Delete Word Forward")  , CTRL(Delete)     , 0            , QKeySequence(), false },

    { Find             , "Find"             , I18N_NOOP2_NOSTRIP("@action", "Find")                 , CTRL(F)          , 0            , QKeySequence(), false },
    { FindNext         , "FindNext"         , I18N_NOOP2_NOSTRIP("@action", "Find Next")            , Qt::Key_F3       , 0            , QKeySequence(), false },
    { FindPrev         , "FindPrev"         , I18N_NOOP2_NOSTRIP("@action", "Find Prev")            , SHIFT(F3)        , 0            , QKeySequence(), false },
    { Replace          , "Replace"          , I18N_NOOP2_NOSTRIP("@action", "Replace")              , CTRL(R)          , 0            , QKeySequence(), false },

    // Group Navigation
    { Home           , "Home"                 , I18N_NOOP2_NOSTRIP("@action Go to main page"      , "Home")                 , ALT(Home)       , 0                 , QKeySequence(), false },
    { Begin          , "Begin"                , I18N_NOOP2_NOSTRIP("@action Beginning of document", "Begin")                , CTRL(Home)      , 0                 , QKeySequence(), false },
    { End            , "End"                  , I18N_NOOP2_NOSTRIP("@action End of document"      , "End")                  , CTRL(End)       , 0                 , QKeySequence(), false },
    { Prior          , "Prior"                , I18N_NOOP2_NOSTRIP("@action"                      , "Prior")                , Qt::Key_PageUp  , 0                 , QKeySequence(), false },
    { Next           , "Next"                 , I18N_NOOP2_NOSTRIP("@action Opposite to Prior"    , "Next")                 , Qt::Key_PageDown, 0                 , QKeySequence(), false },

    { Up             , "Up"                   , I18N_NOOP2_NOSTRIP("@action"                      , "Up")                   , ALT(Up)         , 0                 , QKeySequence(), false },
    { Back           , "Back"                 , I18N_NOOP2_NOSTRIP("@action"                      , "Back")                 , ALT(Left)       , 0                 , QKeySequence(), false },
    { Forward        , "Forward"              , I18N_NOOP2_NOSTRIP("@action"                      , "Forward")              , ALT(Right)      , 0                 , QKeySequence(), false },
    { Reload         , "Reload"               , I18N_NOOP2_NOSTRIP("@action"                      , "Reload")               , Qt::Key_F5      , 0                 , QKeySequence(), false },

    { BeginningOfLine, "BeginningOfLine"      , I18N_NOOP2_NOSTRIP("@action"                      , "Beginning of Line")    , Qt::Key_Home    , 0                 , QKeySequence(), false },
    { EndOfLine      , "EndOfLine"            , I18N_NOOP2_NOSTRIP("@action"                      , "End of Line")          , Qt::Key_End     , 0                 , QKeySequence(), false },
    { GotoLine       , "GotoLine"             , I18N_NOOP2_NOSTRIP("@action"                      , "Go to Line")           , CTRL(G)         , 0                 , QKeySequence(), false },
    { BackwardWord   , "BackwardWord"         , I18N_NOOP2_NOSTRIP("@action"                      , "Backward Word")        , CTRL(Left)      , 0                 , QKeySequence(), false },
    { ForwardWord    , "ForwardWord"          , I18N_NOOP2_NOSTRIP("@action"                      , "Forward Word")         , CTRL(Right)     , 0                 , QKeySequence(), false },

    { AddBookmark    , "AddBookmark"          , I18N_NOOP2_NOSTRIP("@action"                      , "Add Bookmark")         , CTRL(B)         , 0                 , QKeySequence(), false },
    { ZoomIn         , "ZoomIn"               , I18N_NOOP2_NOSTRIP("@action"                      , "Zoom In")              , CTRL(Plus)      , CTRL(Equal)       , QKeySequence(), false },
    { ZoomOut        , "ZoomOut"              , I18N_NOOP2_NOSTRIP("@action"                      , "Zoom Out")             , CTRL(Minus)     , 0                 , QKeySequence(), false },
    { FullScreen     , "FullScreen"           , I18N_NOOP2_NOSTRIP("@action"                      , "Full Screen Mode")     , CTRLSHIFT(F)    , 0                 , QKeySequence(), false },

    { ShowMenubar    , "ShowMenubar"          , I18N_NOOP2_NOSTRIP("@action"                      , "Show Menu Bar")        , CTRL(M)         , 0                 , QKeySequence(), false },
    { TabNext        , "Activate Next Tab"    , I18N_NOOP2_NOSTRIP("@action"                      , "Activate Next Tab")    , CTRL(Period)    , CTRL(BracketRight), QKeySequence(), false },
    { TabPrev        , "Activate Previous Tab", I18N_NOOP2_NOSTRIP("@action"                      , "Activate Previous Tab"), CTRL(Comma)     , CTRL(BracketLeft) , QKeySequence(), false },

    // Group Help
    { Help           , "Help"                 , I18N_NOOP2_NOSTRIP("@action"                      , "Help")                 , Qt::Key_F1      , 0                 , QKeySequence(), false },
    { WhatsThis      , "WhatsThis"            , I18N_NOOP2_NOSTRIP("@action"                      , "What's This")          , SHIFT(F1)       , 0                 , QKeySequence(), false },

    // Group TextCompletion
    { TextCompletion           , "TextCompletion"           , I18N_NOOP2_NOSTRIP("@action", "Text Completion")            , CTRL(E)        , 0, QKeySequence(), false },
    { PrevCompletion           , "PrevCompletion"           , I18N_NOOP2_NOSTRIP("@action", "Previous Completion Match")  , CTRL(Up)       , 0, QKeySequence(), false },
    { NextCompletion           , "NextCompletion"           , I18N_NOOP2_NOSTRIP("@action", "Next Completion Match")      , CTRL(Down)     , 0, QKeySequence(), false },
    { SubstringCompletion      , "SubstringCompletion"      , I18N_NOOP2_NOSTRIP("@action", "Substring Completion")       , CTRL(T)        , 0, QKeySequence(), false },

    { RotateUp                 , "RotateUp"                 , I18N_NOOP2_NOSTRIP("@action", "Previous Item in List")      , Qt::Key_Up     , 0, QKeySequence(), false },
    { RotateDown               , "RotateDown"               , I18N_NOOP2_NOSTRIP("@action", "Next Item in List")          , Qt::Key_Down   , 0, QKeySequence(), false },

    { OpenRecent               , "OpenRecent"               , I18N_NOOP2_NOSTRIP("@action", "Open Recent")                , 0              , 0, QKeySequence(), false },
    { SaveAs                   , "SaveAs"                   , I18N_NOOP2_NOSTRIP("@action", "Save As")                    , CTRLSHIFT(S)   , 0, QKeySequence(), false },
    { Revert                   , "Revert"                   , I18N_NOOP2_NOSTRIP("@action", "Revert")                     , 0              , 0, QKeySequence(), false },
    { PrintPreview             , "PrintPreview"             , I18N_NOOP2_NOSTRIP("@action", "Print Preview")              , 0              , 0, QKeySequence(), false },
    { Mail                     , "Mail"                     , I18N_NOOP2_NOSTRIP("@action", "Mail")                       , 0              , 0, QKeySequence(), false },
    { Clear                    , "Clear"                    , I18N_NOOP2_NOSTRIP("@action", "Clear")                      , 0              , 0, QKeySequence(), false },
    { ActualSize               , "ActualSize"               , I18N_NOOP2_NOSTRIP("@action", "Actual Size")                , 0              , 0, QKeySequence(), false },
    { FitToPage                , "FitToPage"                , I18N_NOOP2_NOSTRIP("@action", "Fit To Page")                , 0              , 0, QKeySequence(), false },
    { FitToWidth               , "FitToWidth"               , I18N_NOOP2_NOSTRIP("@action", "Fit To Width")               , 0              , 0, QKeySequence(), false },
    { FitToHeight              , "FitToHeight"              , I18N_NOOP2_NOSTRIP("@action", "Fit To Height")              , 0              , 0, QKeySequence(), false },
    { Zoom                     , "Zoom"                     , I18N_NOOP2_NOSTRIP("@action", "Zoom")                       , 0              , 0, QKeySequence(), false },
    { Goto                     , "Goto"                     , I18N_NOOP2_NOSTRIP("@action", "Goto")                       , 0              , 0, QKeySequence(), false },
    { GotoPage                 , "GotoPage"                 , I18N_NOOP2_NOSTRIP("@action", "Goto Page")                  , 0              , 0, QKeySequence(), false },
    { DocumentBack             , "DocumentBack"             , I18N_NOOP2_NOSTRIP("@action", "Document Back")              , ALTSHIFT(Left) , 0, QKeySequence(), false },
    { DocumentForward          , "DocumentForward"          , I18N_NOOP2_NOSTRIP("@action", "Document Forward")           , ALTSHIFT(Right), 0, QKeySequence(), false },
    { EditBookmarks            , "EditBookmarks"            , I18N_NOOP2_NOSTRIP("@action", "Edit Bookmarks")             , 0              , 0, QKeySequence(), false },
    { Spelling                 , "Spelling"                 , I18N_NOOP2_NOSTRIP("@action", "Spelling")                   , 0              , 0, QKeySequence(), false },
    { ShowToolbar              , "ShowToolbar"              , I18N_NOOP2_NOSTRIP("@action", "Show Toolbar")               , 0              , 0, QKeySequence(), false },
    { ShowStatusbar            , "ShowStatusbar"            , I18N_NOOP2_NOSTRIP("@action", "Show Statusbar")             , 0              , 0, QKeySequence(), false },
    { SaveOptions              , "SaveOptions"              , I18N_NOOP2_NOSTRIP("@action", "Save Options")               , 0              , 0, QKeySequence(), false },
    { KeyBindings              , "KeyBindings"              , I18N_NOOP2_NOSTRIP("@action", "Key Bindings")               , 0              , 0, QKeySequence(), false },
    { Preferences              , "Preferences"              , I18N_NOOP2_NOSTRIP("@action", "Preferences")                , 0              , 0, QKeySequence(), false },
    { ConfigureToolbars        , "ConfigureToolbars"        , I18N_NOOP2_NOSTRIP("@action", "Configure Toolbars")         , 0              , 0, QKeySequence(), false },
    { ConfigureNotifications   , "ConfigureNotifications"   , I18N_NOOP2_NOSTRIP("@action", "Configure Notifications")    , 0              , 0, QKeySequence(), false },
    { TipofDay                 , "TipofDay"                 , I18N_NOOP2_NOSTRIP("@action", "Tip Of Day")                 , 0              , 0, QKeySequence(), false },
    { ReportBug                , "ReportBug"                , I18N_NOOP2_NOSTRIP("@action", "Report Bug")                 , 0              , 0, QKeySequence(), false },
    { SwitchApplicationLanguage, "SwitchApplicationLanguage", I18N_NOOP2_NOSTRIP("@action", "Switch Application Language"), 0              , 0, QKeySequence(), false },
    { AboutApp                 , "AboutApp"                 , I18N_NOOP2_NOSTRIP("@action", "About Application")          , 0              , 0, QKeySequence(), false },
    { AboutKDE                 , "AboutKatana"              , I18N_NOOP2_NOSTRIP("@action", "About Katana")               , 0              , 0, QKeySequence(), false },

    // Dummy entry to catch simple off-by-one errors. Insert new entries before this line.
    { AccelNone                , 0                          , 0                   , 0                             , 0              , 0, QKeySequence(), false }
};


/** Search for the KStandardShortcutInfo object associated with the given @p id.
    Return a dummy entry with no name and an empty shortcut if @p id is invalid.
*/
static KStandardShortcutInfo *guardedStandardShortcutInfo(StandardShortcut id)
{
    if (id >= static_cast<int>(sizeof(g_infoStandardShortcut) / sizeof(KStandardShortcutInfo)) ||
             id < 0) {
        kWarning(125) << "KStandardShortcut: id not found!";
        return &g_infoStandardShortcut[AccelNone];
    } else
        return &g_infoStandardShortcut[id];
}

/** Initialize the accelerator @p id by checking if it is overridden
    in the configuration file (and if it isn't, use the default).
    On X11, if QApplication was initialized with GUI disabled,
    the default will always be used.
*/
static void initialize(StandardShortcut id)
{
    KStandardShortcutInfo *info = guardedStandardShortcutInfo(id);

    // All three are needed.
    if (info->id!=AccelNone) {
        Q_ASSERT(info->description);
        Q_ASSERT(info->translation_context);
        Q_ASSERT(info->name);
    }

    KConfigGroup cg(KGlobal::config(), "Shortcuts");

    if(cg.hasKey(info->name)) {
        const QString s = cg.readEntry(info->name);
        info->cut = QKeySequence(s);
    } else {
        info->cut = hardcodedDefaultShortcut(id);
    }

    info->isInitialized = true;
}

void saveShortcut(StandardShortcut id, const QKeySequence &newShortcut)
{
    KStandardShortcutInfo *info = guardedStandardShortcutInfo(id);
    // If the action has no standard shortcut associated there is nothing to
    // save
    if (info->id == AccelNone) {
        return;
    }

    KConfigGroup cg(KGlobal::config(), "Shortcuts");

    info->cut = newShortcut;
    bool sameAsDefault = (newShortcut == hardcodedDefaultShortcut(id));

    if (sameAsDefault) {
        // If the shortcut is the equal to the hardcoded one we remove it from
        // kdeglobal if necessary and return.
        if (cg.hasKey(info->name)) {
            cg.deleteEntry(info->name, KConfig::Global|KConfig::Persistent);
        }
        return;
    }

    // Write the changed shortcut to kdeglobals
    cg.writeEntry(info->name, info->cut.toString(), KConfig::Global|KConfig::Persistent);
}

QString name(StandardShortcut id)
{
    return guardedStandardShortcutInfo(id)->name;
}

QString label(StandardShortcut id)
{
    KStandardShortcutInfo *info = guardedStandardShortcutInfo( id );
    return i18nc( info->translation_context, info->description);
}

// TODO: Add psWhatsThis entry to KStandardShortcutInfo
QString whatsThis( StandardShortcut /*id*/ )
{
//  KStandardShortcutInfo* info = guardedStandardShortcutInfo( id );
//  if( info && info->whatsThis )
//      return i18n(info->whatsThis);
//  else
        return QString();
}

const QKeySequence &shortcut(StandardShortcut id)
{
    KStandardShortcutInfo *info = guardedStandardShortcutInfo(id);

    if (!info->isInitialized)
        initialize(id);

    return info->cut;
}

StandardShortcut find(const QKeySequence &seq)
{
    if (!seq.isEmpty()) {
        for(uint i = 0; i < sizeof(g_infoStandardShortcut) / sizeof(KStandardShortcutInfo); i++) {
            StandardShortcut id = g_infoStandardShortcut[i].id;
            if (id != AccelNone) {
                if (!g_infoStandardShortcut[i].isInitialized) {
                    initialize(id);
                }
                if (g_infoStandardShortcut[i].cut.matches(seq) != QKeySequence::NoMatch) {
                    return id;
                }
            }
        }
    }
    return AccelNone;
}

StandardShortcut find(const char *keyName)
{
    for (uint i = 0; i < sizeof(g_infoStandardShortcut) / sizeof(KStandardShortcutInfo); i++) {
        if (qstrcmp(g_infoStandardShortcut[i].name, keyName)) {
            return g_infoStandardShortcut[i].id;
        }
    }
    return AccelNone;
}

QKeySequence hardcodedDefaultShortcut(StandardShortcut id)
{
    KStandardShortcutInfo *info = guardedStandardShortcutInfo(id);
    return QKeySequence(info->cutDefault, info->cutDefault2);
}

const QKeySequence& open()                  { return shortcut( Open ); }
const QKeySequence& openNew()               { return shortcut( New ); }
const QKeySequence& close()                 { return shortcut( Close ); }
const QKeySequence& save()                  { return shortcut( Save ); }
const QKeySequence& print()                 { return shortcut( Print ); }
const QKeySequence& quit()                  { return shortcut( Quit ); }
const QKeySequence& cut()                   { return shortcut( Cut ); }
const QKeySequence& copy()                  { return shortcut( Copy ); }
const QKeySequence& paste()                 { return shortcut( Paste ); }
const QKeySequence& pasteSelection()        { return shortcut( PasteSelection ); }
const QKeySequence& deleteWordBack()        { return shortcut( DeleteWordBack ); }
const QKeySequence& deleteWordForward()     { return shortcut( DeleteWordForward ); }
const QKeySequence& undo()                  { return shortcut( Undo ); }
const QKeySequence& redo()                  { return shortcut( Redo ); }
const QKeySequence& find()                  { return shortcut( Find ); }
const QKeySequence& findNext()              { return shortcut( FindNext ); }
const QKeySequence& findPrev()              { return shortcut( FindPrev ); }
const QKeySequence& replace()               { return shortcut( Replace ); }
const QKeySequence& home()                  { return shortcut( Home ); }
const QKeySequence& begin()                 { return shortcut( Begin ); }
const QKeySequence& end()                   { return shortcut( End ); }
const QKeySequence& beginningOfLine()       { return shortcut( BeginningOfLine ); }
const QKeySequence& endOfLine()             { return shortcut( EndOfLine ); }
const QKeySequence& prior()                 { return shortcut( Prior ); }
const QKeySequence& next()                  { return shortcut( Next ); }
const QKeySequence& backwardWord()          { return shortcut( BackwardWord ); }
const QKeySequence& forwardWord()           { return shortcut( ForwardWord ); }
const QKeySequence& gotoLine()              { return shortcut( GotoLine ); }
const QKeySequence& addBookmark()           { return shortcut( AddBookmark ); }
const QKeySequence& tabNext()               { return shortcut( TabNext ); }
const QKeySequence& tabPrev()               { return shortcut( TabPrev ); }
const QKeySequence& fullScreen()            { return shortcut( FullScreen ); }
const QKeySequence& zoomIn()                { return shortcut( ZoomIn ); }
const QKeySequence& zoomOut()               { return shortcut( ZoomOut ); }
const QKeySequence& help()                  { return shortcut( Help ); }
const QKeySequence& completion()            { return shortcut( TextCompletion ); }
const QKeySequence& prevCompletion()        { return shortcut( PrevCompletion ); }
const QKeySequence& nextCompletion()        { return shortcut( NextCompletion ); }
const QKeySequence& rotateUp()              { return shortcut( RotateUp ); }
const QKeySequence& rotateDown()            { return shortcut( RotateDown ); }
const QKeySequence& substringCompletion()   { return shortcut( SubstringCompletion ); }
const QKeySequence& whatsThis()             { return shortcut( WhatsThis ); }
const QKeySequence& reload()                { return shortcut( Reload ); }
const QKeySequence& selectAll()             { return shortcut( SelectAll ); }
const QKeySequence& up()                    { return shortcut( Up ); }
const QKeySequence& back()                  { return shortcut( Back ); }
const QKeySequence& forward()               { return shortcut( Forward ); }
const QKeySequence& showMenubar()           { return shortcut( ShowMenubar ); }

}
