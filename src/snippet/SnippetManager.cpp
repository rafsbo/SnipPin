#include "snippet/SnippetManager.h"

#include <algorithm>

namespace snippin {

void SnippetManager::Initialize(HINSTANCE instance, HWND host, UINT closeMessage, ContextMenuHandler onContextMenu,
                                std::function<void()> onListChanged) {
    instance_ = instance;
    host_ = host;
    closeMessage_ = closeMessage;
    onContextMenu_ = std::move(onContextMenu);
    onListChanged_ = std::move(onListChanged);
}

SnippetWindow* SnippetManager::Create(HWND source, const logic::Rect& regionInWindow,
                                      const logic::Rect& screenRegion, bool live) {
    SnippetWindow::Callbacks callbacks;
    callbacks.onContextMenu = [this](SnippetWindow& s, POINT pt) {
        if (onContextMenu_) {
            onContextMenu_(s, pt);
        }
    };
    callbacks.onCloseRequested = [this](SnippetWindow& s) { RequestClose(s.Id()); };
    callbacks.onChanged = [this](SnippetWindow&) {
        if (onListChanged_) {
            onListChanged_();
        }
    };

    auto snippet = std::make_unique<SnippetWindow>(nextId_++, std::move(callbacks));
    SnippetOptions options = options_;
    options.live = live;
    if (!snippet->Create(instance_, source, regionInWindow, screenRegion, options)) {
        return nullptr;
    }
    snippets_.push_back(std::move(snippet));
    if (onListChanged_) {
        onListChanged_();
    }
    return snippets_.back().get();
}

void SnippetManager::RequestClose(std::uint64_t id) {
    ::PostMessageW(host_, closeMessage_, static_cast<WPARAM>(id), 0);
}

void SnippetManager::ProcessClose(std::uint64_t id) {
    auto it = std::find_if(snippets_.begin(), snippets_.end(),
                           [id](const std::unique_ptr<SnippetWindow>& s) { return s->Id() == id; });
    if (it == snippets_.end()) {
        return;
    }
    snippets_.erase(it);  // destructor destroys the window
    if (onListChanged_) {
        onListChanged_();
    }
}

void SnippetManager::CloseAll() {
    if (snippets_.empty()) {
        return;
    }
    snippets_.clear();
    if (onListChanged_) {
        onListChanged_();
    }
}

SnippetWindow* SnippetManager::FindById(std::uint64_t id) {
    for (auto& s : snippets_) {
        if (s->Id() == id) {
            return s.get();
        }
    }
    return nullptr;
}

SnippetWindow* SnippetManager::FindByHwnd(HWND hwnd) {
    for (auto& s : snippets_) {
        if (s->Hwnd() == hwnd) {
            return s.get();
        }
    }
    return nullptr;
}

void SnippetManager::SetOptions(const SnippetOptions& options) {
    options_ = options;
    for (auto& s : snippets_) {
        s->SetOptions(options);
    }
}

void SnippetManager::OnSourceDestroyed(HWND source) {
    for (auto& s : snippets_) {
        if (s->Source() == source && !s->IsSourceClosed()) {
            s->OnSourceDestroyed();
        }
    }
}

void SnippetManager::ReassertTopmost() {
    for (auto& s : snippets_) {
        s->ReassertTopmost();
    }
}

SnippetWindow* SnippetManager::HitTest(POINT pt) {
    // Iterate newest first: newer snippets are usually above older ones.
    for (auto it = snippets_.rbegin(); it != snippets_.rend(); ++it) {
        SnippetWindow& s = **it;
        if (s.IsGhost() || !s.Hwnd()) {
            continue;
        }
        RECT rc;
        if (::GetWindowRect(s.Hwnd(), &rc) && ::PtInRect(&rc, pt)) {
            return &s;
        }
    }
    return nullptr;
}

SnippetWindow* SnippetManager::MostRecentGhost() {
    SnippetWindow* best = nullptr;
    for (auto& s : snippets_) {
        if (s->IsGhost() && (!best || s->GhostSequence() > best->GhostSequence())) {
            best = s.get();
        }
    }
    return best;
}

bool SnippetManager::AnyGhost() const {
    return std::any_of(snippets_.begin(), snippets_.end(),
                       [](const std::unique_ptr<SnippetWindow>& s) { return s->IsGhost(); });
}

}  // namespace snippin
