// Owns all snippet widgets. Closing is always deferred through the host
// window's message queue so a snippet is never deleted while one of its own
// message handlers (e.g. its context menu) is still on the stack.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "snippet/SnippetWindow.h"

namespace snippin {

class SnippetManager {
public:
    using ContextMenuHandler = std::function<void(SnippetWindow&, POINT)>;

    void Initialize(HINSTANCE instance, HWND host, UINT closeMessage, ContextMenuHandler onContextMenu,
                    std::function<void()> onListChanged);

    SnippetWindow* Create(HWND source, const logic::Rect& regionInWindow, const logic::Rect& screenRegion,
                          bool live);

    void RequestClose(std::uint64_t id);
    void ProcessClose(std::uint64_t id);  // called by the host on |closeMessage|
    void CloseAll();

    SnippetWindow* FindById(std::uint64_t id);
    SnippetWindow* FindByHwnd(HWND hwnd);
    const std::vector<std::unique_ptr<SnippetWindow>>& All() const { return snippets_; }

    void SetOptions(const SnippetOptions& options);
    const SnippetOptions& Options() const { return options_; }

    void OnSourceDestroyed(HWND source);
    void ReassertTopmost();

    // Snippet under the cursor that accepts wheel input (not ghosted).
    SnippetWindow* HitTest(POINT screenPt);
    SnippetWindow* MostRecentGhost();
    bool AnyGhost() const;

private:
    HINSTANCE instance_ = nullptr;
    HWND host_ = nullptr;
    UINT closeMessage_ = 0;
    ContextMenuHandler onContextMenu_;
    std::function<void()> onListChanged_;
    SnippetOptions options_;
    std::vector<std::unique_ptr<SnippetWindow>> snippets_;
    std::uint64_t nextId_ = 1;
};

}  // namespace snippin
