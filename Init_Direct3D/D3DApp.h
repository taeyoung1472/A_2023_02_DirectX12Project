#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "../Common/d3dUtil.h"
#include "../Common/GameTimer.h"

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

// using namespaces
using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace Microsoft::WRL;

class D3DApp
{
protected:
    D3DApp(HINSTANCE hInstance);
    D3DApp(const D3DApp& rhs) = delete;
    D3DApp& operator=(const D3DApp& rhs) = delete;
    virtual ~D3DApp();

public:
    static D3DApp* GetApp();

    HINSTANCE AppInst()const;
    HWND      MainWnd()const;
    float     AspectRatio()const;

    int Run();

    virtual bool Initialize();
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    virtual void CreateDsvDescriptorHeaps();
    virtual void OnResize();
    virtual void Update(const GameTimer& gt) = 0;
    virtual void Draw(const GameTimer& gt) = 0;
    virtual void DrawBegin(const GameTimer& gt) = 0;
    virtual void DrawEnd(const GameTimer& gt) = 0;

    // Convenience overrides for handling mouse input.
    virtual void OnMouseDown(WPARAM btnState, int x, int y) { }
    virtual void OnMouseUp(WPARAM btnState, int x, int y) { }
    virtual void OnMouseMove(WPARAM btnState, int x, int y) { }

protected:
    bool InitMainWindow();

    void CalculateFrameStats();

protected:
    bool InitDirect3D();
    void FlushCommandQueue();
    void CreateCommandObjects();
    void CreateSwapChain();
    void CreateRtvDescriptorHeaps();
    void CreateViewport();

public:
    ID3D12Resource* CurrentBackBuffer()const;
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;

protected:
    static D3DApp* mApp;

    HINSTANCE mhAppInst = nullptr; // application instance handle
    HWND      mhMainWnd = nullptr; // main window handle
    bool      mAppPaused = false;  // is the application paused?
    bool      mMinimized = false;  // is the application minimized?
    bool      mMaximized = false;  // is the application maximized?
    bool      mResizing = false;   // are the resize bars being dragged?
    bool      mFullscreenState = false;// fullscreen enabled

    std::wstring mMainWndCaption = L"DirectX12 Project";

    int mClientWidth = 1920;
    int mClientHeight = 1080;

    // Used to keep track of the �delta-time?and game time (?.4).
    GameTimer mTimer;

protected:
    ComPtr<IDXGIFactory4>   mdxgiFactory;
    ComPtr<ID3D12Device>    md3dDevice;

    ComPtr<ID3D12Fence>     mFence;
    UINT64                  mCurrentFence = 0;

    // ������ ũ�� ���
    UINT                    mRtvDescriptorSize = 0;
    UINT                    mDsvDescriptorSize = 0;
    UINT                    mCbvSrvUavDescriptorSize = 0;

    // 4X MSAA
    bool                    m4xMsaaState = false;
    UINT                    m4xMsaaQuality = 0;

    D3D_DRIVER_TYPE         md3DDriverType = D3D_DRIVER_TYPE_HARDWARE;
    DXGI_FORMAT             mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT             mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // ��� ť, ��� ����Ʈ, ��� �Ҵ��� ����
    ComPtr<ID3D12CommandQueue>          mCommandQueue;
    ComPtr<ID3D12CommandAllocator>      mDirectCmdListAlloc;
    ComPtr<ID3D12GraphicsCommandList>   mCommandList;

    // ���� ü��
    ComPtr<IDXGISwapChain>              mSwapChain;

    static const int SwapChainBufferCount = 2;
    int                                 mCurrBackBuffer = 0;
    ComPtr<ID3D12Resource>              mSwapChainBuffer[SwapChainBufferCount];

    ComPtr<ID3D12DescriptorHeap>        mRtvHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE         mRtvView[SwapChainBufferCount] = {};

    // ���� ���ٽ� ����
    ComPtr<ID3D12Resource>              mDepthStencilBuffer;
    ComPtr<ID3D12DescriptorHeap>        mDsvHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE         mDsvView = {};

    // ����Ʈ
    D3D12_VIEWPORT                      mScreenViewport;
    D3D12_RECT                          mScissorRect;
};

