#ifndef D3DAPP_H
#define D3DAPP_H

#include <iostream>
#include <sstream>
#include <cassert>

#include "D3d11.h"

#include <../GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <../GLFW/glfw3native.h>

#define SafeRelease(obj) { if (obj) { obj->Release(); obj = nullptr; } }

class GameTimer
{
	double mSecondsPerCount;
	double mDeltaTime;
	__int64 mBaseTime;
	__int64 mPauseTime;
	__int64 mStopTime;
	__int64 mPrevTime;
	__int64 mCurrTime;
	bool mStopped;

public:
	GameTimer();

	float TotalTime() const;
	float DeltaTime() const;

	void Reset();
	void Start();
	void Stop();
	void Tick();
};

class D3DApp
{
public:
	D3DApp();
	virtual ~D3DApp();

	int Run();

	virtual bool Init();
	virtual void OnResize();
	virtual void UpdateScene(float dt) = 0;
	virtual void DrawScene() = 0;

protected:
	bool InitMainWindow();
	bool InitDirect3D();

	void CalculateFrameStats();

	GameTimer mTimer;
	bool mAppPaused;

	// D3D11 stuff
	ID3D11Device* mDevice;
	ID3D11DeviceContext* mContext;
	bool m4xMSAAEnabled;
	UINT m4xMSAAQuality;
	IDXGISwapChain* mSwapChain;
	ID3D11Texture2D* mDepthStencilBuffer;
	ID3D11RenderTargetView* mRenderTargetView;
	ID3D11DepthStencilView* mDepthStencilView;
	D3D11_VIEWPORT mViewport;

	// window stuff
	GLFWwindow* mMainWindow;
	int mMainWindowWidth;
	int mMainWindowHeight;
	std::string mMainWindowTitle;
	bool mResizing;
};

GameTimer::GameTimer() :
	mSecondsPerCount(0),
	mDeltaTime(-1),
	mBaseTime(0),
	mPauseTime(0),
	mPrevTime(0),
	mCurrTime(0),
	mStopped(false)
{
	__int64 freq;
	QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
	mSecondsPerCount = 1.0 / (double)freq;
}

void GameTimer::Tick()
{
	if (mStopped)
	{
		mDeltaTime = 0;
		return;
	}

	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	mCurrTime = currTime;

	mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;
	mPrevTime = mCurrTime;

	if (mDeltaTime < 0)
	{
		mDeltaTime = 0;
	}
}

float GameTimer::DeltaTime() const
{
	return (float)mDeltaTime;
}

void GameTimer::Reset()
{
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	mBaseTime = currTime;
	mPrevTime = currTime;
	mStopTime = 0;
	mStopped = false;
}

void GameTimer::Stop()
{
	if (!mStopped)
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		mStopTime = currTime;
		mStopped = true;
	}
}

void GameTimer::Start()
{
	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	if (mStopped)
	{
		mPauseTime += (currTime - mStopTime);
		mPrevTime = currTime;
		mStopTime = 0;
		mStopped = false;
	}
}

float GameTimer::TotalTime() const
{
	return (float)((((mStopped ? mStopTime : mCurrTime) - mPauseTime) - mBaseTime) * mSecondsPerCount);
}

D3DApp::D3DApp() :
	mAppPaused(false),
	mResizing(false),
	m4xMSAAEnabled(false),
	m4xMSAAQuality(0),

	mDevice(nullptr),
	mContext(nullptr),
	mSwapChain(nullptr),
	mDepthStencilBuffer(nullptr),
	mRenderTargetView(nullptr),
	mDepthStencilView(nullptr),

	mMainWindow(nullptr),
	mMainWindowWidth(800),
	mMainWindowHeight(600),
	mMainWindowTitle("D3D11 Application")
{
	ZeroMemory(&mViewport, sizeof(D3D11_VIEWPORT));
}

D3DApp::~D3DApp()
{
	SafeRelease(mRenderTargetView);
	SafeRelease(mDepthStencilView);
	SafeRelease(mDepthStencilBuffer);
	SafeRelease(mSwapChain);
	if (mContext) mContext->ClearState();
	SafeRelease(mContext);
	SafeRelease(mDevice);

	glfwDestroyWindow(mMainWindow);
	glfwTerminate();
}

int D3DApp::Run()
{
	mTimer.Reset();

	while (!glfwWindowShouldClose(mMainWindow))
	{
		glfwPollEvents();

		mTimer.Tick();

		if (!mAppPaused)
		{
			CalculateFrameStats();
			UpdateScene(mTimer.DeltaTime());
			DrawScene();
		}
		else
		{
			Sleep(100);
		}
	}

	return 0;
}

bool D3DApp::Init()
{
	if (!InitMainWindow()) return false;
	if (!InitDirect3D()) return false;
	return true;
}

bool D3DApp::InitMainWindow()
{
	if (glfwInit() == GLFW_FALSE) return false;

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	mMainWindow = glfwCreateWindow(mMainWindowWidth, mMainWindowHeight, mMainWindowTitle.c_str(), nullptr, nullptr);

	return mMainWindow;
}

bool D3DApp::InitDirect3D()
{
	D3D_FEATURE_LEVEL featureLevel;

	HRESULT hr = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		D3D11_CREATE_DEVICE_DEBUG,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&mDevice,
		&featureLevel,
		&mContext
	);

	if (FAILED(hr))
	{
		std::cout << "D3D11CreateDevice FAILED" << std::endl;
		return false;
	}

	if (featureLevel < D3D_FEATURE_LEVEL_11_0)
	{
		std::cout << "featureLevel < D3D_FEATURE_LEVEL_11_0" << std::endl;
		return false;
	}

	mDevice->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &m4xMSAAQuality);
	assert(m4xMSAAQuality > 0);

	DXGI_SWAP_CHAIN_DESC desc;
	desc.BufferDesc.Width = mMainWindowWidth;
	desc.BufferDesc.Height = mMainWindowHeight;
	desc.BufferDesc.RefreshRate.Numerator = 60;
	desc.BufferDesc.RefreshRate.Denominator = 1;
	desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	if (m4xMSAAEnabled)
	{
		desc.SampleDesc.Count = 4;
		desc.SampleDesc.Quality = m4xMSAAQuality - 1;
	}
	else
	{
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
	}

	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = 1;
	desc.OutputWindow = glfwGetWin32Window(mMainWindow);
	desc.Windowed = true;
	desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	desc.Flags = 0;

	IDXGIDevice* dxgiDevice = nullptr;
	mDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);

	IDXGIAdapter* dxgiAdapter = nullptr;
	dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&dxgiAdapter);

	IDXGIFactory* dxgiFactory = nullptr;
	dxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&dxgiFactory);

	dxgiFactory->CreateSwapChain(mDevice, &desc, &mSwapChain);

	SafeRelease(dxgiDevice);
	SafeRelease(dxgiAdapter);
	SafeRelease(dxgiFactory);

	OnResize();

	return true;
}

void D3DApp::OnResize()
{
	assert(mDevice);
	assert(mContext);
	assert(mSwapChain);

	SafeRelease(mRenderTargetView);
	SafeRelease(mDepthStencilView);
	SafeRelease(mDepthStencilBuffer);

	// resize the swap chain and create the render target view

	mSwapChain->ResizeBuffers(
		1,
		mMainWindowWidth,
		mMainWindowHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		0
	);

	ID3D11Texture2D* backBuffer = nullptr;
	mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
	mDevice->CreateRenderTargetView(backBuffer, nullptr, &mRenderTargetView);
	SafeRelease(backBuffer);

	// create the depth-stencil buffer and view

	D3D11_TEXTURE2D_DESC desc;
	desc.Width = mMainWindowWidth;
	desc.Height = mMainWindowHeight;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	if (m4xMSAAEnabled)
	{
		desc.SampleDesc.Count = 4;
		desc.SampleDesc.Quality = m4xMSAAQuality - 1;
	}
	else
	{
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
	}

	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	mDevice->CreateTexture2D(&desc, 0, &mDepthStencilBuffer);
	mDevice->CreateDepthStencilView(mDepthStencilBuffer, 0, &mDepthStencilView);

	// bind the render target and depth-stencil views to the pipeline

	mContext->OMSetRenderTargets(1, &mRenderTargetView, mDepthStencilView);

	// set the viewport

	mViewport.TopLeftX = 0;
	mViewport.TopLeftY = 0;
	mViewport.Width = mMainWindowWidth;
	mViewport.Height = mMainWindowHeight;
	mViewport.MinDepth = 0;
	mViewport.MaxDepth = 1;

	mContext->RSSetViewports(1, &mViewport);
}

void D3DApp::CalculateFrameStats()
{
	static std::size_t frameCount = 0;
	static float timeElapsed = 0;

	frameCount++;

	if ((mTimer.TotalTime() - timeElapsed) >= 1)
	{
		float fps = frameCount; // fps = frameCount / 1 second
		float frameTime = 1000 / fps; // in ms

		std::ostringstream oss;
		oss << mMainWindowTitle << " | FPS: " << fps << " | Frame Time: " << frameTime << " (ms)";
		glfwSetWindowTitle(mMainWindow, oss.str().c_str());

		frameCount = 0;
		timeElapsed += 1;
	}
}

#endif // D3DAPP_H