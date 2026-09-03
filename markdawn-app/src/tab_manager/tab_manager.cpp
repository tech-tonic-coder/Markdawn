#include "tab_manager.h"

#include <QDebug>
#include <QFileInfo>
#include <QStyle>

#include "document_view/document_view.h"
#include "markdawncore/document/document_model.h"
#include "markdawncore/document/toc_model.h"

using markdawn::core::DocumentModel;
using markdawn::core::LoadResult;
using markdawn::core::TocModel;

namespace markdawn::app {

TabManager::TabManager(QWidget* parent) : QTabWidget(parent) {
    setTabsClosable(true);
    connect(this, &QTabWidget::tabCloseRequested, this, &TabManager::closeTabAt);
    connect(this, &QTabWidget::currentChanged, this, &TabManager::handleCurrentChanged);
}

void TabManager::openFile(const QString& filePath) {
    // Normalize once, up front: every downstream use (duplicate-tab lookup,
    // the path handed to DocumentModel, the tab label/tooltip) is based on
    // this single absolute form. QFileInfo::absoluteFilePath() is used
    // rather than canonicalFilePath() because the latter returns an empty
    // string for a path that doesn't exist (verified against Qt's
    // documented behavior) -- which would break both the not-found error
    // path and the duplicate-tab map key.
    const QString normalizedPath = QFileInfo(filePath).absoluteFilePath();

    const auto existing = m_openTabs.constFind(normalizedPath);
    if (existing != m_openTabs.constEnd()) {
        qInfo() << "markdawn: focusing already-open tab for" << normalizedPath;
        setCurrentWidget(existing.value().view);
        return;
    }

    auto* model = new DocumentModel(this);
    const LoadResult result = model->loadFromFile(normalizedPath);

    auto* view = new DocumentView(this);
    // Owned by TabManager (this), not by view -- see the class comment in
    // tab_manager.h for why. Given a DocumentModel unconditionally, on both
    // the success and error-tab paths below: on the error path the model's
    // document is empty, so the TOC panel correctly falls back to its "no
    // headings" state on its own, with no separate branch needed here.
    auto* tocModel = new TocModel(this);
    const QString label = QFileInfo(normalizedPath).fileName();
    int index = -1;

    if (result == LoadResult::Ok) {
        view->loadFromModel(model);
        index = addTab(view, label);
        qInfo() << "markdawn: opened new tab for" << normalizedPath << "(" << count() << "tab(s) total )";
    } else {
        view->showLoadError(result, normalizedPath);
        index = addTab(view, label);
        setTabIcon(index, style()->standardIcon(QStyle::SP_MessageBoxWarning));
        qWarning() << "markdawn: opened error tab for" << normalizedPath;
    }
    tocModel->setDocumentModel(model);

    setTabToolTip(index, normalizedPath);
    // Insert into the map *before* setCurrentIndex(): setCurrentIndex()
    // emits currentChanged() synchronously when the index actually changes
    // (true for the very first tab, going from -1 to 0), which runs
    // handleCurrentChanged() immediately -- that lookup would miss this tab
    // if the map entry weren't already there.
    m_openTabs.insert(normalizedPath, {view, tocModel});
    setCurrentIndex(index);
}

void TabManager::closeTabAt(int index) {
    QWidget* page = widget(index);
    if (!page) {
        return;
    }
    // Small map (one entry per open tab); a linear scan to find the path
    // for this widget is simpler than maintaining a second, reverse-keyed
    // map just for removal, and is cheap at the tab counts this app deals
    // with.
    for (auto it = m_openTabs.begin(); it != m_openTabs.end(); ++it) {
        if (it.value().view == page) {
            it.value().tocModel->deleteLater();
            m_openTabs.erase(it);
            break;
        }
    }
    removeTab(index);
    page->deleteLater();
}

void TabManager::handleCurrentChanged(int index) {
    if (index < 0) {
        emit activeDocumentChanged(nullptr, nullptr);
        return;
    }
    QWidget* page = widget(index);
    for (auto it = m_openTabs.constBegin(); it != m_openTabs.constEnd(); ++it) {
        if (it.value().view == page) {
            emit activeDocumentChanged(it.value().tocModel, it.value().view);
            return;
        }
    }
    // Should not happen -- every tab widget is inserted into m_openTabs
    // before setCurrentIndex() can trigger this slot -- but fail safe
    // rather than emit a signal referencing a widget we have no record of.
    emit activeDocumentChanged(nullptr, nullptr);
}

} // namespace markdawn::app
