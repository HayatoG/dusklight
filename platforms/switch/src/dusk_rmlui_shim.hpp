// Minimal RmlUi forward declarations for the Switch build.
//
// On PC, Dusklight pulls in <RmlUi/Core.h> via headers like src/dusk/ui/ui.hpp.
// On Switch we disable AURORA_ENABLE_RMLUI, so those headers aren't available,
// but the public surface of dusk::ui::* still references Rml types in
// declarations (as pointers/references or as `Rml::String` typedef).
//
// This shim declares enough of `Rml::` for those declarations to parse.
// Linking succeeds because the actual implementations live in PC-only .cpp
// files that we exclude from the Switch build — the corresponding stubs in
// switch_stubs.cpp provide empty bodies.

#pragma once

#ifdef __SWITCH__

#include <string>

namespace Rml {
    using String = std::string;

    // Enum values seen in header bodies (Click, Keydown, Change, …). Real
    // values are large; we only need them to parse and compare with `!=`.
    enum class EventId : int {
        Invalid = -1,
        Click = 1, Keydown, Keyup, Change, Focus, Blur, Submit,
        Mousemove, Mousedown, Mouseup, Mouseover, Mouseout,
    };

    class Event {
    public:
        void StopPropagation() {}
        void StopImmediatePropagation() {}
        EventId GetId() const { return EventId::Invalid; }
        const String& GetType() const { static String s; return s; }
        struct Param { int dummy; };
        int GetParameter(const String&, int defv) const { return defv; }
    };

    class EventListener {
    public:
        virtual ~EventListener() = default;
        virtual void ProcessEvent(Event&) {}
        virtual void OnDetach(class Element*) {}
    };

    // Minimal Element — header bodies call IsPseudoClassSet and a few others.
    class Element {
    public:
        bool IsPseudoClassSet(const String&) const { return false; }
        bool IsPseudoClassSet(const char*) const { return false; }
        void SetPseudoClass(const String&, bool) {}
        void SetPseudoClass(const char*, bool) {}
        Element* GetParentNode() const { return nullptr; }
        Element* GetElementById(const String&) const { return nullptr; }
        Element* GetElementById(const char*) const { return nullptr; }
        String GetId() const { return {}; }
        void Focus() {}
        void Blur() {}
        void Click() {}
        void AddEventListener(EventId, EventListener*, bool = false) {}
        void RemoveEventListener(EventId, EventListener*, bool = false) {}
        bool IsVisible() const { return false; }
        void SetAttribute(const String&, const String&) {}
        String GetAttribute(const String&, const String&) const { return {}; }
        void SetInnerRML(const String&) {}
        String GetInnerRML() const { return {}; }
        void RemoveChild(Element*) {}
    };

    class ElementDocument : public Element {};
    class ElementFormControlInput : public Element {
    public:
        String GetValue() const { return {}; }
        void SetValue(const String&) {}
    };
    class Context {};
}

#endif // __SWITCH__
