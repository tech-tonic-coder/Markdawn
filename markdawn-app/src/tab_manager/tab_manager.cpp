#include "tab_manager.h"

#include <QDebug>
#include <QFileInfo>
#include <QStyle>

#include "document_view/document_view.h"
#include "markdawncore/document/document_model.h"

using markdawn::core::DocumentModel;
using markdawn::core::LoadResult;

namespace markdawn::app {

TabManager::TabManager(QWidget* parent) : QTabWidget(parent) {
    setTabsClosable(true);
    connect(this, &QTabWidget::tabCloseRequested, this, &TabManager::closeTabAt);
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

    const auto existing = m_openPaths.constFind(normalizedPath);
    if (existing != m_openPaths.constEnd()) {
        qInfo() << "markdawn: focusing already-open tab for" << normalizedPath;
        setCurrentWidget(existing.value());
        return;
    }

    auto* model = new DocumentModel(this);
    const LoadResult result = model->loadFromFile(normalizedPath);

    auto* view = new DocumentView(this);
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

    setTabToolTip(index, normalizedPath);
    setCurrentIndex(index);
    m_openPaths.insert(normalizedPath, view);
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
    for (auto it = m_openPaths.begin(); it != m_openPaths.end(); ++it) {
        if (it.value() == page) {
            m_openPaths.erase(it);
            break;
        }
    }
    removeTab(index);
    page->deleteLater();
}

} // namespace markdawn::app
