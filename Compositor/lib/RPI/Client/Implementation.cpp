#include <virtualinput/virtualinput.h>

#include <bcm_host.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <interfaces/IComposition.h>
#include "../../Client/Client.h"

int g_pipefd[2];
struct Message {
    uint32_t code;
    actiontype type;
};

static const char * connectorName = "/tmp/keyhandler";
static void VirtualKeyboardCallback(actiontype type , unsigned int code) {
    if (type != COMPLETED) {
        Message message;
        message.code = code;
        message.type = type;
        write(g_pipefd[1], &message, sizeof(message));
    }
}

namespace WPEFramework {

static Core::NodeId Connector () {

    string connector;
    if ((Core::SystemInfo::GetEnvironment(_T("COMPOSITOR"), connector) == false) || (connector.empty() == true)) {
        connector = _T("/tmp/compositor");
    }
    return (Core::NodeId(connector.c_str()));
}

class Display : public Compositor::IDisplay {
private:
    Display() = delete;
    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    class AccessorCompositor : public Exchange::IComposition::INotification {
    public:
        AccessorCompositor (const AccessorCompositor&) = delete;
        AccessorCompositor& operator= (const AccessorCompositor&) = delete;

    private:
        class RPCClient {
        private:
            RPCClient() = delete;
            RPCClient(const RPCClient&) = delete;
            RPCClient& operator=(const RPCClient&) = delete;

            typedef WPEFramework::RPC::InvokeServerType<4, 1> RPCService;

        public:
            RPCClient(const Core::NodeId& nodeId)
            : _client(Core::ProxyType<RPC::CommunicatorClient>::Create(nodeId))
            , _service(Core::ProxyType<RPCService>::Create(Core::Thread::DefaultStackSize())) {

                if (_client->Open(RPC::CommunicationTimeOut, _T("CompositorImplementation"), Exchange::IComposition::INotification::ID, ~0) == Core::ERROR_NONE) {
                    _client->CreateFactory<RPC::InvokeMessage>(2);
                    _client->Register(_service);
                }
                else {
                    _client.Release();
                }
            }
            ~RPCClient() {
                if (_client.IsValid() == true) {
                    _client->Unregister(_service);
                    _client->DestroyFactory<RPC::InvokeMessage>();
                    _client->Close(Core::infinite);
                    _client.Release();
                }
            }

        public:
            inline bool IsValid() const {
                return (_client.IsValid());
            }
            template <typename INTERFACE>
            INTERFACE* WaitForCompletion(const uint32_t waitTime) {
                return (_client->WaitForCompletion<INTERFACE>(waitTime));
            }

        private:
            Core::ProxyType<RPC::CommunicatorClient> _client;
            Core::ProxyType<RPCService> _service;
        };

    public:
        AccessorCompositor () 
        : _client(Connector())
        , _remote(nullptr) {
            
        }

        inline void Initialize() {
          if (_client.IsValid() == true) {
            _remote = _client.WaitForCompletion<Exchange::IComposition::INotification>(6000);
          }
  
        }

        inline void Deinitialize() {
            if (_remote != nullptr) {
                _remote->Release();
                _remote = nullptr;
            }
            TRACE_L1("Destructed the AccessorCompositor");

        }

        ~AccessorCompositor() {
        }

    public:
        void Attached(Exchange::IComposition::IClient* client) override {
            if( _remote != nullptr ) {
                _remote->Attached(client);
            }
        }

        void Detached(Exchange::IComposition::IClient* client) override {
            if( _remote != nullptr ) {
                _remote->Detached(client);
            }
        }

        BEGIN_INTERFACE_MAP(AccessorCompositor)
        INTERFACE_ENTRY(Exchange::IComposition::INotification)
        END_INTERFACE_MAP

    private:
        RPCClient _client;
        Exchange::IComposition::INotification* _remote;
    };

    class SurfaceImplementation : public Exchange::IComposition::IClient {
    public:
        SurfaceImplementation() = delete;
        SurfaceImplementation(const SurfaceImplementation&) = delete;
        SurfaceImplementation& operator=(const SurfaceImplementation&) = delete;

        SurfaceImplementation(
                Display* compositor, const std::string& name,
                const uint32_t width, const uint32_t height);
        virtual ~SurfaceImplementation();

        virtual void Opacity(const uint32_t value) override;
        virtual void ChangedGeometry(const Exchange::IComposition::Rectangle& rectangle) override;
        virtual void SetTop() override;
        virtual void SetInput() override;

        virtual string Name() const override {
            return _name;
        }
        virtual void Kill() override {
            //todo: implement
            fprintf(stderr, "Kill called!!!\n");
        }
        inline EGLNativeWindowType Native() const {
            return (static_cast<EGLNativeWindowType>(_nativeSurface));
        }
        inline int32_t Width() const {
            return _width;
        }
        inline int32_t Height() const {
            return _height;
        }
        inline void Keyboard(
                Compositor::IDisplay::IKeyboard* keyboard) {
            assert((_keyboard == nullptr) ^ (keyboard == nullptr));
            _keyboard = keyboard;
        }
        inline void SendKey(
                const uint32_t key,
                const IKeyboard::state action, const uint32_t time) {
            if (_keyboard != nullptr) {
                _keyboard->Direct(key, action);
            }
        }

      operator Compositor::IDisplay::ISurface*()
        {
            return &_impl;
        }    
    
    private:
        class Implementation : public Compositor::IDisplay::ISurface {
            public:
                Implementation(SurfaceImplementation& parent)
                : _parent(parent) {

                }
                        // Lifetime management
            virtual void AddRef() const override {
                _parent.AddRef();
            }
            virtual uint32_t Release() const override {
                return _parent.Release();
            }

            // Methods
            virtual EGLNativeWindowType Native() const override {
                return _parent.Native();
            }
            virtual std::string Name() const override {
                return _parent.Name();
            }
            virtual void Keyboard(IKeyboard* keyboard) override {
                _parent.Keyboard(keyboard);
            }
            virtual int32_t Width() const override {
                return _parent.Width();
            }
            virtual int32_t Height() const override {
                return _parent.Height();
            }
            private:

                SurfaceImplementation& _parent;
        };

        BEGIN_INTERFACE_MAP(Entry)
            INTERFACE_ENTRY(Exchange::IComposition::IClient)
        END_INTERFACE_MAP

    private:
        Display& _display;
        std::string _name;
        /* const */ uint32_t _width;
        /* const */ uint32_t _height;
        uint32_t _opacity;        

        EGLSurface _nativeSurface;
        EGL_DISPMANX_WINDOW_T _nativeWindow;
        DISPMANX_DISPLAY_HANDLE_T _dispmanDisplay;
        DISPMANX_UPDATE_HANDLE_T _dispmanUpdate;
        DISPMANX_ELEMENT_HANDLE_T _dispmanElement;

        VC_RECT_T _dstRect;
        VC_RECT_T _srcRect;

        IKeyboard* _keyboard;
        Implementation _impl;

    };

public:
    Display(const std::string& displayName);
    virtual ~Display();

    virtual void AddRef() const {
    }
    virtual uint32_t Release() const {
        return (0);
    }
    virtual EGLNativeDisplayType Native() const override {
        return (static_cast<EGLNativeDisplayType>(EGL_DEFAULT_DISPLAY));
    }
    virtual const std::string& Name() const override {
        return (_displayName);
    }
    virtual int Process (const uint32_t data) override;
    virtual int FileDescriptor() const override;
    virtual ISurface* Create(
            const std::string& name,
            const uint32_t width, const uint32_t height) override;

private:
    inline void Register(SurfaceImplementation* surface);
    inline void Unregister(SurfaceImplementation* surface);

    const std::string _displayName;
    mutable Core::CriticalSection _adminLock;
    void* _virtualkeyboard;
    std::list<SurfaceImplementation*> _surfaces;
    Exchange::IComposition::INotification* _accessorCompositor;
};

Display::SurfaceImplementation::SurfaceImplementation(
        Display* display,
        const std::string& name,
        const uint32_t width, const uint32_t height)
: Exchange::IComposition::IClient()
, _display(*display)
, _name(name)
, _width(width)
, _height(height)
, _opacity(255) 
, _keyboard(nullptr)
, _impl(*this) {
    TRACE_L1("Created client named: %s", _name.c_str());

    graphics_get_display_size(0, &_width, &_height);
    VC_DISPMANX_ALPHA_T alpha = {
            DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS,
            255,
            0
    };
    vc_dispmanx_rect_set(&_dstRect, 0, 0, _width, _height);
    vc_dispmanx_rect_set(&_srcRect,
            0, 0, (_width << 16), (_height << 16));

    _dispmanDisplay = vc_dispmanx_display_open(0);
    _dispmanUpdate = vc_dispmanx_update_start(0);
    _dispmanElement = vc_dispmanx_element_add(
            _dispmanUpdate,
            _dispmanDisplay,
            0,
            &_dstRect,
            0 /*src*/,
            &_srcRect,
            DISPMANX_PROTECTION_NONE,
            &alpha /*alpha*/,
            0 /*clamp*/,
            DISPMANX_NO_ROTATE);
    vc_dispmanx_update_submit_sync(_dispmanUpdate);

    _nativeWindow.element = _dispmanElement;
    _nativeWindow.width = _width;
    _nativeWindow.height = _height;
    _nativeSurface = static_cast<EGLSurface>(&_nativeWindow);

    _display.Register(this);
}

Display::SurfaceImplementation::~SurfaceImplementation() {

    TRACE_L1("Destructing client named: %s", _name.c_str());

    _dispmanUpdate = vc_dispmanx_update_start(0);
    vc_dispmanx_element_remove(_dispmanUpdate, _dispmanElement);
    vc_dispmanx_update_submit_sync(_dispmanUpdate);
    vc_dispmanx_display_close(_dispmanDisplay);

    _display.Unregister(this);
}

void Display::SurfaceImplementation::Opacity(
        const uint32_t value) {

    ASSERT (value <= Exchange::IComposition::maxOpacity);

    _opacity = (value > Exchange::IComposition::maxOpacity) ? Exchange::IComposition::maxOpacity : value;

    _dispmanUpdate = vc_dispmanx_update_start(0);
    vc_dispmanx_element_change_attributes(_dispmanUpdate,
            _dispmanElement,
            (1 << 1),
            0,
            _opacity,
            &_dstRect,
            &_srcRect,
            0,
            DISPMANX_NO_ROTATE);
    vc_dispmanx_update_submit_sync(_dispmanUpdate);
}

void Display::SurfaceImplementation::ChangedGeometry(const Exchange::IComposition::Rectangle& rectangle) {
    vc_dispmanx_rect_set(&_dstRect, rectangle.x, rectangle.y, rectangle.width, rectangle.height);
    vc_dispmanx_rect_set(&_srcRect,
            0, 0, (_width << 16), (_height << 16));

    _dispmanUpdate = vc_dispmanx_update_start(0);
    vc_dispmanx_element_change_attributes(_dispmanUpdate,
            _dispmanElement,
            (1 << 2),
            0,
            _opacity,
            &_dstRect,
            &_srcRect,
            0,
            DISPMANX_NO_ROTATE);
    vc_dispmanx_update_submit_sync(_dispmanUpdate);
}

void Display::SurfaceImplementation::SetTop() {
    _dispmanUpdate = vc_dispmanx_update_start(0);
    vc_dispmanx_element_change_layer(_dispmanUpdate, _dispmanElement, 0);
    vc_dispmanx_update_submit_sync(_dispmanUpdate);
}

void Display::SurfaceImplementation::SetInput() {
}

Display::Display(const std::string& name)
: _displayName(name)
, _adminLock()
, _virtualkeyboard(nullptr)
, _accessorCompositor(Core::Service<AccessorCompositor>::Create<AccessorCompositor>())  {

    static_cast<AccessorCompositor*>(_accessorCompositor)->Initialize();

    bcm_host_init();

    if (pipe(g_pipefd) == -1) {
        g_pipefd[0] = -1;
        g_pipefd[1] = -1;
    }
    _virtualkeyboard = Construct(
            name.c_str(), connectorName, VirtualKeyboardCallback);
    if (_virtualkeyboard == nullptr) {
        fprintf(stderr, "Initialization of virtual keyboard failed!!!\n");
        fflush(stderr);
    }
}

Display::~Display() {

    if (_virtualkeyboard != nullptr) {
        Destruct(_virtualkeyboard);
    }
  static_cast<AccessorCompositor*>(_accessorCompositor)->Deinitialize();
  }

int Display::Process(const uint32_t data) {
    Message message;
    if ((data != 0) && (g_pipefd[0] != -1) &&
            (read(g_pipefd[0], &message, sizeof(message)) > 0)) {

        _adminLock.Lock();
        std::list<SurfaceImplementation*>::iterator index(_surfaces.begin());
        while (index != _surfaces.end()) {
            // RELEASED  = 0,
            // PRESSED   = 1,
            // REPEAT    = 2,

            (*index)->SendKey (message.code, (message.type == 0 ? IDisplay::IKeyboard::released : IDisplay::IKeyboard::pressed), time(nullptr));
            index++;
        }
        _adminLock.Unlock();
    }
    return (0);
}

int Display::FileDescriptor() const {
    return (g_pipefd[0]);
}

Compositor::IDisplay::ISurface* Display::Create(
        const std::string& name, const uint32_t width, const uint32_t height) {

    Compositor::IDisplay::ISurface* retval = *(Core::Service<SurfaceImplementation>::Create<SurfaceImplementation>(this, name, width, height));

   return retval;
   }

inline void Display::Register(Display::SurfaceImplementation* surface) {
    _adminLock.Lock();
    std::list<SurfaceImplementation*>::iterator index(
            std::find(_surfaces.begin(), _surfaces.end(), surface));
    if (index == _surfaces.end()) {
        _surfaces.push_back(surface);
        ASSERT (surface != nullptr);
        if ( _accessorCompositor != nullptr) {
               _accessorCompositor->Attached(surface);
        }
    }
    _adminLock.Unlock();
}

inline void Display::Unregister(Display::SurfaceImplementation* surface) {
    _adminLock.Lock();
    std::list<SurfaceImplementation*>::iterator index(
            std::find(_surfaces.begin(), _surfaces.end(), surface));
    if (index != _surfaces.end()) {
        ASSERT (surface != nullptr);
        if ( _accessorCompositor != nullptr) {
            _accessorCompositor->Detached(surface);
        }
        _surfaces.erase(index);
    }
    _adminLock.Unlock();
}  


Compositor::IDisplay* Compositor::IDisplay::Instance(const string& displayName) {
    static Display myDisplay(displayName);
        Compositor::IDisplay* retval = (&myDisplay);
         return (&myDisplay);
}

}
